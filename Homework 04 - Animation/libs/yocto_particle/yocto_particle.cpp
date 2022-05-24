//
// Implementation for Yocto/Particle.
//

//
// LICENSE:
//
// Copyright (c) 2020 -- 2020 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//

#include "yocto_particle.h"

#include <yocto/yocto_geometry.h>
#include <yocto/yocto_sampling.h>
#include <yocto/yocto_shape.h>

#include <unordered_set>

// -----------------------------------------------------------------------------
// SIMULATION DATA AND API
// -----------------------------------------------------------------------------
namespace yocto {

particle_scene make_ptscene(const scene_data& ioscene, const particle_params& params) {
  // scene
  auto ptscene = particle_scene{};

  // shapes
  static auto velocity = unordered_map<string, float>{{"floor", 0}, {"particles", 1}, {"cloth", 0}, {"collider", 0}};
  auto iid = 0;
  for (auto& ioinstance : ioscene.instances) {
    auto& ioshape = ioscene.shapes[ioinstance.shape];
    auto& iomaterial_name = ioscene.material_names[ioinstance.material];
    if (iomaterial_name == "particles") {
        add_particles(ptscene, iid++, ioshape.points, ioshape.positions, ioshape.radius, 1, 1);
    } else if (iomaterial_name == "cloth") {
        auto nverts = (int)ioshape.positions.size();
        add_cloth(ptscene, iid++, ioshape.quads, ioshape.positions, ioshape.normals, ioshape.radius, 0.5, 1 / 8000.0, {nverts - 1, nverts - (int)sqrt((float)nverts)});
    } else if (iomaterial_name == "collider") {
        add_collider(ptscene, iid++, ioshape.triangles, ioshape.quads, ioshape.positions, ioshape.normals, ioshape.radius);
    } else if (iomaterial_name == "floor") {
        add_collider(ptscene, iid++, ioshape.triangles, ioshape.quads, ioshape.positions, ioshape.normals, ioshape.radius);
    } else {
        throw std::invalid_argument{"unknown material " + iomaterial_name};
    }
  }

  // done
  return ptscene;
}

void update_ioscene(scene_data& ioscene, const particle_scene& ptscene) {
  for (auto& ptshape : ptscene.shapes) {
    auto& ioshape = ioscene.shapes[ptshape.shape];
    get_positions(ptshape, ioshape.positions);
    get_normals(ptshape, ioshape.normals);
  }
}

void flatten_scene(scene_data& ioscene) {
  for (auto& ioinstance : ioscene.instances) {
    auto& shape = ioscene.shapes[ioinstance.shape];
    for (auto& position : shape.positions)
      position = transform_point(ioinstance.frame, position);
    for (auto& normal : shape.normals)
      normal = transform_normal(ioinstance.frame, normal);
    ioinstance.frame = identity3x4f;
  }
}

// Scene creation
int add_particles(particle_scene& scene, int shape_id, const vector<int>& points, const vector<vec3f>& positions, const vector<float>& radius, float mass, float random_velocity) {
  auto& shape             = scene.shapes.emplace_back();
  shape.shape             = shape_id;
  shape.points            = points;
  shape.initial_positions = positions;
  shape.initial_normals.assign(shape.positions.size(), {0, 0, 1});
  shape.initial_radius = radius;
  shape.initial_invmass.assign(positions.size(), 1 / (mass * positions.size()));
  shape.initial_velocities.assign(positions.size(), {0, 0, 0});
  shape.emit_rngscale = random_velocity;
  return (int)scene.shapes.size() - 1;
}
int add_cloth(particle_scene& scene, int shape_id, const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<float>& radius, float mass, float coeff,
    const vector<int>& pinned) {
  auto& shape             = scene.shapes.emplace_back();
  shape.shape             = shape_id;
  shape.quads             = quads;
  shape.initial_positions = positions;
  shape.initial_normals   = normals;
  shape.initial_radius    = radius;
  shape.initial_invmass.assign(positions.size(), 1 / (mass * positions.size()));
  shape.initial_velocities.assign(positions.size(), {0, 0, 0});
  shape.initial_pinned = pinned;
  shape.spring_coeff   = coeff;
  return (int)scene.shapes.size() - 1;
}
int add_collider(particle_scene& scene, int shape_id,
    const vector<vec3i>& triangles, const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<float>& radius) {
  auto& collider     = scene.colliders.emplace_back();
  collider.shape     = shape_id;
  collider.quads     = quads;
  collider.triangles = triangles;
  collider.positions = positions;
  collider.normals   = normals;
  collider.radius    = radius;
  return (int)scene.colliders.size() - 1;
}

// Set shapes
void set_velocities(
    particle_shape& shape, const vec3f& velocity, float random_scale) {
  shape.emit_velocity = velocity;
  shape.emit_rngscale = random_scale;
}

// Get shape properties
void get_positions(const particle_shape& shape, vector<vec3f>& positions) {
  positions = shape.positions;
}
void get_normals(const particle_shape& shape, vector<vec3f>& normals) {
  normals = shape.normals;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// SIMULATION DATA AND API
// -----------------------------------------------------------------------------
namespace yocto {

// Init simulation
void init_simulation(particle_scene& scene, const particle_params& params) {
    // YOUR CODE GOES HERE
    auto seed_variance = 0;
    for (auto& particle : scene.shapes) {
        //Setting valori iniziali
        particle.emit_rng = make_rng(params.seed, (seed_variance++) * 2 + 1);
        particle.invmass = particle.initial_invmass;
        particle.normals = particle.initial_normals;
        particle.positions = particle.initial_positions;
        particle.radius = particle.initial_radius;
        particle.velocities = particle.initial_velocities;
        
        //Setting forze iniziali. Puliamo le forze già presenti e creiamo un vettore lungo quanto le posizioni con forze {0.0f, 0.0f, 0.0f}
        particle.forces.clear();
        for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++)
            particle.forces.push_back(zero3f);

        //Setting invmass delle particelle pinned
        for (auto pin : particle.initial_pinned)
            particle.invmass[pin] = 0;

        //Setting velocità
        for (auto& velocity : particle.velocities)
            velocity += sample_sphere(rand2f(particle.emit_rng)) * particle.emit_rngscale * rand1f(particle.emit_rng);

        //Setting springs
        particle.springs.clear();
        if (particle.spring_coeff > 0) {
            for (auto& edge : get_edges(particle.quads)) //Spring per ogni edge
                particle.springs.push_back({edge.x, edge.y, distance(particle.positions[edge.x], particle.positions[edge.y]), particle.spring_coeff});
            for (auto& quad : particle.quads) {
                particle.springs.push_back({quad.x, quad.z, distance(particle.positions[quad.x], particle.positions[quad.z]), particle.spring_coeff});
                particle.springs.push_back({quad.y, quad.w, distance(particle.positions[quad.y], particle.positions[quad.w]), particle.spring_coeff});
            }
        }
        
        //Setting bvh
        for (auto& collider : scene.colliders) {
            if (collider.quads.size() > 0)
                collider.bvh = make_quads_bvh(collider.quads, collider.positions, collider.radius);
            else
                collider.bvh = make_triangles_bvh(collider.triangles, collider.positions, collider.radius);
        }
    }
}

// check if a point is inside a collider
bool collide_collider(const particle_collider& collider, const vec3f& position, vec3f& hit_position, vec3f& hit_normal) {
    // YOUR CODE GOES HERE
    auto ray = ray3f{position, vec3f{0.0f, 1.0f, 0.0f}};
    shape_intersection intersection;
    if (collider.quads.size() > 0)
        intersection = intersect_quads_bvh(collider.bvh, collider.quads, collider.positions, ray);
    else
        intersection = intersect_triangles_bvh(collider.bvh, collider.triangles, collider.positions, ray);

    if (!intersection.hit)
        return false;

    //Calcoliamo hit_position e hit_normal
    if (collider.quads.size() > 0) {
        auto quad = collider.quads[intersection.element];
        hit_position = interpolate_quad(collider.positions[quad.x], collider.positions[quad.y], collider.positions[quad.z], collider.positions[quad.w], intersection.uv);
        hit_normal = normalize(interpolate_quad(collider.normals[quad.x], collider.normals[quad.y], collider.normals[quad.z], collider.normals[quad.w], intersection.uv));
    }
    else {
        auto triangle = collider.triangles[intersection.element];
        hit_position = interpolate_triangle(collider.positions[triangle.x], collider.positions[triangle.y], collider.positions[triangle.z], intersection.uv);
        hit_normal = normalize(interpolate_triangle(collider.normals[triangle.x], collider.normals[triangle.y], collider.normals[triangle.z], intersection.uv));
    }
    return dot(hit_normal, ray.d) > 0;
}

// simulate mass-spring
void simulate_massspring(particle_scene& scene, const particle_params& params) {
    // YOUR CODE GOES HERE
    // SAVE OLD POSITIONS
    for (auto& particle : scene.shapes)
        particle.old_positions = particle.positions;
    
    // COMPUTE DYNAMICS
    for (auto& particle : scene.shapes) 
        for (int msstep = 0; msstep < params.mssteps; msstep++) {
            auto delta_dt = params.deltat / params.mssteps;

            //Compute Forces
            for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
                if (!particle.invmass[pos_ind]) continue;
                particle.forces[pos_ind] = vec3f{ 0.0f, -params.gravity, 0.0f } / particle.invmass[pos_ind];
            }

            for (auto& spring : particle.springs) {
                auto& position_zero = particle.positions[spring.vert0];
                auto& position_one = particle.positions[spring.vert1];
                auto invmass = particle.invmass[spring.vert0] + particle.invmass[spring.vert1];
                if (!invmass) continue;
                auto delta_pos = position_one - position_zero;
                auto delta_vel = particle.velocities[spring.vert1] - particle.velocities[spring.vert0];
                auto spring_dir = normalize(delta_pos);
                auto spring_len = length(delta_pos);
                auto force = spring_dir * (spring_len / spring.rest - 1.0f) / (spring.coeff * invmass);
                force += dot(delta_vel / spring.rest, spring_dir) * spring_dir / (spring.coeff * 1000 * invmass);
                particle.forces[spring.vert0] += force;
                particle.forces[spring.vert1] -= force;
            }

            //Update State
            for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
                if (!particle.invmass[pos_ind]) continue;
                particle.velocities[pos_ind] += delta_dt * particle.forces[pos_ind] * particle.invmass[pos_ind];
                particle.positions[pos_ind] += delta_dt * particle.velocities[pos_ind];
            }
        }

    // HANDLE COLLISIONS
    for (auto& particle : scene.shapes) 
        for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
            if (!particle.invmass[pos_ind]) continue;
            for (auto& collider : scene.colliders) {
                auto hit_position = zero3f;
                auto hit_normal = zero3f;
                if (collide_collider(collider, particle.positions[pos_ind], hit_position, hit_normal)) {
                    particle.positions[pos_ind] = hit_position + hit_normal * 0.005f;
                    auto projection = dot(particle.velocities[pos_ind], hit_normal);
                    particle.velocities[pos_ind] = (particle.velocities[pos_ind] - projection * hit_normal) * (1 - params.bounce.x) - projection * hit_normal * (1.0f - params.bounce.y);
                }
            }
        }

    // VELOCITY FILTER
    for (auto& particle : scene.shapes) 
        for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
            if (!particle.invmass[pos_ind]) continue;
            particle.velocities[pos_ind] *= (1.0f - params.dumping * params.deltat);
            if (length(particle.velocities[pos_ind]) < params.minvelocity)
                particle.velocities[pos_ind] = {0.0f, 0.0f, 0.0f};
        }

    // RECOMPUTE NORMALS
    for (auto& particle : scene.shapes) {
        if (particle.quads.size() > 0)
            particle.normals = quads_normals(particle.quads, particle.positions);
        else
            particle.normals = triangles_normals(particle.triangles, particle.positions);
    }
}

// simulate pbd
void simulate_pbd(particle_scene& scene, const particle_params& params) {
    // YOUR CODE GOES HERE
    // SAVE OLD POSITIONS
    for (auto& particle : scene.shapes)
        particle.old_positions = particle.positions;

    // PREDICT POSITIONS
    for (auto& particle : scene.shapes)
        for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
            if (!particle.invmass[pos_ind]) continue;
            particle.velocities[pos_ind] += vec3f{0.0f, -params.gravity, 0.0f} * params.deltat;
            particle.positions[pos_ind] += particle.velocities[pos_ind] * params.deltat;
        }

    // COMPUTE COLLISIONS
    for (auto& particle : scene.shapes) { //clear collisions list
        particle.collisions.clear();
        for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
            if (!particle.invmass[pos_ind]) continue;
            for (auto& collider : scene.colliders) {
                auto hit_position = zero3f;
                auto hit_normal = zero3f;
                if (!collide_collider(collider, particle.positions[pos_ind], hit_position, hit_normal)) continue;
                particle.collisions.push_back(particle_collision{pos_ind, hit_position, hit_normal});
            }
        }
    }

    // SOLVE CONSTRAINTS
    for (auto& particle : scene.shapes) 
        for (int pdbstep = 0; pdbstep < params.pdbsteps; pdbstep++) {
            //Solve springs
            for (auto& spring : particle.springs) {
                auto& position_zero = particle.positions[spring.vert0];
                auto& position_one = particle.positions[spring.vert1];
                auto invmass = particle.invmass[spring.vert0] + particle.invmass[spring.vert1];
                if (!invmass) continue;
                auto direction = position_one - position_zero;
                auto original_length = length(direction);
                direction /= original_length;
                auto lambda = (1.0f - spring.coeff) * (original_length - spring.rest) / invmass;
                particle.positions[spring.vert0] += particle.invmass[spring.vert0] * lambda * direction;
                particle.positions[spring.vert1] -= particle.invmass[spring.vert1] * lambda * direction;
            }
            //Solve collisions
            for (auto& collision : particle.collisions) {
                auto& coll_part = particle.positions[collision.vert];
                if (!particle.invmass[collision.vert]) continue;
                auto projection = dot(coll_part - collision.position, collision.normal);
                if (projection >= 0) continue;
                coll_part += -projection * collision.normal;
            }
        }

    // COMPUTE VELOCITIES
    for (auto& particle : scene.shapes) 
        for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
            if (!particle.invmass[pos_ind]) continue;
            particle.velocities[pos_ind] = (particle.positions[pos_ind] - particle.old_positions[pos_ind]) / params.deltat;
            if (params.windy) {
                if(params.favourable)
                    particle.velocities[pos_ind] += (params.tailwind * params.wind_str) * params.deltat;
                else
                    particle.velocities[pos_ind] += (params.upwind * params.wind_str) * params.deltat;
            }
        }

    // VELOCITY FILTER
    for (auto& particle : scene.shapes) 
        for (int pos_ind = 0; pos_ind < particle.positions.size(); pos_ind++) {
            if (!particle.invmass[pos_ind]) continue;
            particle.velocities[pos_ind] *= (1.0f - params.dumping * params.deltat);
            if (length(particle.velocities[pos_ind]) < params.minvelocity)
                particle.velocities[pos_ind] = vec3f{ 0.0f, 0.0f, 0.0f };
        }

    // RECOMPUTE NORMALS
    for (auto& particle : scene.shapes) {
        if (particle.quads.size() > 0)
            particle.normals = quads_normals(particle.quads, particle.positions);
        else
            particle.normals = triangles_normals(particle.triangles, particle.positions);
    }
}

// Simulate one step
void simulate_frame(particle_scene& scene, const particle_params& params) {
  switch (params.solver) {
    case particle_solver_type::mass_spring:
      return simulate_massspring(scene, params);
    case particle_solver_type::position_based:
      return simulate_pbd(scene, params);
    default: throw std::invalid_argument("unknown solver");
  }
}

// Simulate the whole sequence
void simulate_frames(particle_scene& scene, const particle_params& params, progress_callback progress_cb) {
  // handle progress
  auto progress = vec2i{0, 1 + (int)params.frames};

  if (progress_cb) progress_cb("init simulation", progress.x++, progress.y);
  init_simulation(scene, params);

  for (auto idx = 0; idx < params.frames; idx++) {
    if (progress_cb) progress_cb("simulate frames", progress.x++, progress.y);
    simulate_frame(scene, params);
  }

  if (progress_cb) progress_cb("simulate frames", progress.x++, progress.y);
}

}  // namespace yocto

//
// Implementation for Yocto/RayTrace.
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2021 Fabio Pellacini
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

#include "yocto_raytrace.h"

#include <yocto/yocto_cli.h>
#include <yocto/yocto_geometry.h>
#include <yocto/yocto_parallel.h>
#include <yocto/yocto_sampling.h>
#include <yocto/yocto_shading.h>
#include <yocto/yocto_shape.h>

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR SCENE EVALUATION
// -----------------------------------------------------------------------------
namespace yocto {

// Generates a ray from a camera for yimg::image plane coordinate uv and
// the lens coordinates luv.
static ray3f eval_camera(const camera_data& camera, const vec2f& uv) {
  auto film = camera.aspect >= 1
                  ? vec2f{camera.film, camera.film / camera.aspect}
                  : vec2f{camera.film * camera.aspect, camera.film};
  auto q    = transform_point(camera.frame,
      {film.x * (0.5f - uv.x), film.y * (uv.y - 0.5f), camera.lens});
  auto e    = transform_point(camera.frame, {0, 0, 0});
  return {e, normalize(e - q)};
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR PATH TRACING
// -----------------------------------------------------------------------------
namespace yocto {

    //-------------------------------
    // Funzione per effetto wet
    //-------------------------------
    
    vec3f mixImpl(vec3f x, vec3f y, float a) {
        return x * (1 - a) + y * a;
    }

    vec3f saturation(vec3f vec, float t) {
        vec3f x_value = {dot(vec, vec3f{0.2126f, 0.7152f, 0.0722f})};
        return mixImpl(x_value, vec, t);
    }

    vec4f shade_wet(vec3f color, vec2f uv, const raytrace_params& params) {
        color = sqrt(color);
        color = saturation(color, 1.7f);
        auto new_uv = uv / vec2f{(float) params.resolution, params.resolution * 300.0f / 720.0f} * 2.0f - 1.0f;
        float vgn = smoothstep(1.2f, 0.7f, abs(new_uv.y)) * smoothstep(1.1f, 0.8f, abs(new_uv.x));
        color *= 1.0f - (1.0f - vgn) * 0.15f;
        return rgb_to_rgba(color);
    }

    //--------------------------------------------------------
    // Funzioni per ray-tracing dei crediti base + refractive
    //--------------------------------------------------------

    /// <summary>
    /// Gestione della reflectance usata per il materiale refractive, come definita nel capitolo 10.4 di Raytracing In One Weekend
    /// </summary>
    /// <param name="cosine">Coseno</param>
    /// <param name="ior">Indice di rifrattività</param>
    /// <returns>Float per checking della rifrattività</returns>
    float reflectance(double cosine, float ior) {
        auto r_zero = pow((1 - ior) / (1 + ior), 2);
        return r_zero + (1 - r_zero) * pow((1 - cosine), 5);
    }

// Raytrace renderer.
    static vec4f shade_raytrace(const scene_data& scene, const bvh_scene& bvh, const ray3f& ray, int bounce, rng_state& rng, const raytrace_params& params) {
        // YOUR CODE GOES HERE ----
        auto intersection = intersect_bvh(bvh, scene, ray);
        if (intersection.hit) {
            //Ricaviamo instance, shape e materiale dall'intersezione
            auto& instance = scene.instances[intersection.instance];
            auto& shape = scene.shapes[instance.shape];
            auto& material = eval_material(scene, instance, intersection.element, intersection.uv);
            //Ricaviamo come da traccia posizione, normale e radiance
            auto position = transform_point(instance.frame, eval_position(shape, intersection.element, intersection.uv));
            auto normal = transform_direction(instance.frame, eval_normal(shape, intersection.element, intersection.uv));
            auto radiance = rgb_to_rgba(material.emission);

            //-----------------------
            // Gestione dell'opacità
            //------------------------
            if (rand1f(rng) < 1 - material.opacity)
                radiance += shade_raytrace(scene, bvh, ray3f{ position, ray.d }, bounce + 1, rng, params);
            
            if (bounce >= params.bounces) //Definiamo il caso "zero" che conclude la ricorsione
                return radiance;
            else {

                //------------------------------------------------------------------------
                // Gestione della normale, principalmente usata per hair, bath ed ecosys
                //------------------------------------------------------------------------
                if (!shape.points.empty()) {
                    normal = -ray.d;
                }
                else if (!shape.lines.empty()) {
                    normal = orthonormalize(-ray.d, normal);
                }
                else if (!shape.triangles.empty()) {
                    if (dot(-ray.d, normal) < 0) {
                        normal = -normal;
                    }
                }

                //---------------------------------------------
                // Gestione dell'effetto Wet/Lucid sulla scena
                //---------------------------------------------
                if (params.wet) {
                    auto exponent = 2 / pow(material.roughness, 2);
                    auto wet_normal = sample_hemisphere_cospower(exponent, normal, rand2f(rng));
                    if (material.type != material_type::transparent) {
                        if (material.roughness == 0.0f) {
                            auto incoming = reflect(-ray.d, wet_normal);
                            if (instance.material == 0)
                                return radiance += rgb_to_rgba(material.color) * shade_wet(material.color, intersection.uv, params) * shade_raytrace(scene, bvh, ray3f{ position, incoming }, bounce + 1, rng, params) * 0.75f;
                            else
                                return radiance += rgb_to_rgba(material.color) * shade_wet(material.color, intersection.uv, params) * shade_raytrace(scene, bvh, ray3f{ position, incoming }, bounce + 1, rng, params) * 0.30f;
                        }
                        else {
                            auto halfway = sample_hemisphere_cospower(exponent, wet_normal, rand2f(rng));
                            auto incoming = reflect(-ray.d, halfway);
                            if (instance.material == 0)
                                return radiance += rgb_to_rgba(material.color) * shade_wet(material.color, intersection.uv, params) * shade_raytrace(scene, bvh, ray3f{ position, incoming }, bounce + 1, rng, params) * 0.75f;
                            else
                                return radiance += rgb_to_rgba(material.color) * shade_wet(material.color, intersection.uv, params) * shade_raytrace(scene, bvh, ray3f{ position, incoming }, bounce + 1, rng, params) * 0.30f;
                        }
                    }
                }
                switch (material.type) {// Controllo del materiale dell'intersezione
                    //-------------------------
                    // Gestione materiale Matte
                    //-------------------------
                    case material_type::matte: {
                      //Calcolo della indirect illumination
                      auto indirect_incoming = sample_hemisphere(normal, rand2f(rng));
                      radiance += (2 * pi) * rgb_to_rgba(material.color) / pi * shade_raytrace(scene, bvh, ray3f{position, indirect_incoming}, bounce + 1, rng, params) * dot(normal, indirect_incoming);
                      //Calcolo materiale matte
                      auto incoming = sample_hemisphere_cos(normal, rand2f(rng)); 
                      return radiance += rgb_to_rgba(material.color) / pi * shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params) * dot(normal, incoming);
                    } break;

                    //-----------------------------
                    // Gestione materiali metallici
                    //-----------------------------
                    case material_type::reflective: {
                        //Calcoliamo la nuova normale da utilizzare
                        auto exponent = 2 / pow(material.roughness, 2);
                        auto metal_normal = sample_hemisphere_cospower(exponent, normal, rand2f(rng));
                        if (material.roughness == 0.0f) {// Caso di metalli lucidi
                            auto incoming = reflect(-ray.d, metal_normal);
                            return radiance += rgb_to_rgba(material.color) * shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                        } else {  // Caso di metalli rough
                            auto halfway  = sample_hemisphere_cospower(exponent, metal_normal, rand2f(rng));
                            auto incoming = reflect(-ray.d, halfway);
                            return radiance += rgb_to_rgba(material.color) * shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                        }
                    } break;

                    //-------------------------------------------
                    // Gestione materiali plastici - rough plastic
                    //-------------------------------------------
                    case material_type::glossy: {
                        //Calcoliamo l'halfway da utilizzare
                        auto exponent = 2 / pow(material.roughness, 2);
                        auto halfway  = sample_hemisphere_cospower(exponent, normal, rand2f(rng));
                        if (rand1f(rng) < fresnel_schlick(vec3f{0.04}, halfway, -ray.d).x) { //Utilizziamo la riflessione
                            auto incoming = reflect(-ray.d, halfway);
                            return radiance += shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                        } else { //utilizziamo la diffusa
                            auto incoming = sample_hemisphere_cos(normal, rand2f(rng));
                            return radiance += rgb_to_rgba(material.color) * shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                        }
                    } break;

                    //-----------------------------------------
                    // Gestione materiali polished dielectrics
                    //-----------------------------------------
                    case material_type::transparent: {  // Materiali polished dialectrics
                        if (rand1f(rng) < fresnel_schlick(vec3f{0.04}, normal, -ray.d).x) {
                            auto incoming = reflect(-ray.d, normal);
                            return radiance += shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                        } else {
                            auto incoming = ray.d;
                            return radiance += rgb_to_rgba(material.color) * shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                        }
                    } break;

                    //---------------------------------
                    // Gestione materiale refractive
                    //---------------------------------
                    case material_type::refractive: {
                        if (rand1f(rng) < fresnel_schlick(vec3f{0.04}, normal, -ray.d).x) { //Effettua riflessione come in un polished dialectrics
                            auto incoming = reflect(-ray.d, normal);
                            return radiance += shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                        }
                        else { //Effettua rifrazione
                            auto cos_theta = fmin(dot(-ray.d, normal), 1.0);
                            auto sin_theta = sqrt(1.0 - pow(cos_theta, 2));
                            if (dot(normal, -ray.d) < 0) { //Check per invertire ior e normale se necessario
                                material.ior = 1.0 / material.ior;
                                normal = -normal;
                            }     
                            if (material.ior * sin_theta <= 1 || reflectance(cos_theta, material.ior) < rand1f(rng)) {  // Check per utilizzare la riflessione o la rifrazione
                                auto incoming = refract(-ray.d, normal, material.ior);
                                return radiance += rgb_to_rgba(material.color) * shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                            }
                            else {
                                auto incoming = reflect(-ray.d, normal);
                                return radiance += shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params);
                            }
                        }
                    } break;
                }
            }
        }
        //In caso non ci sia hit calcoliamo l'illuminazione dell'ambiente
        return rgb_to_rgba(eval_environment(scene, ray.d));
    }

// Matte renderer.
static vec4f shade_matte(const scene_data& scene, const bvh_scene& bvh, const ray3f& ray, int bounce, rng_state& rng, const raytrace_params& params) {
    // YOUR CODE GOES HERE ----
    auto intersection = intersect_bvh(bvh, scene, ray);
    if (intersection.hit) {
        auto& instance = scene.instances[intersection.instance];
        auto& shape = scene.shapes[instance.shape];
        auto& material = eval_material(scene, instance, intersection.element, intersection.uv);
        //Ricaviamo come da traccia posizione e normale
        auto position = transform_point(instance.frame, eval_position(shape, intersection.element, intersection.uv));
        auto normal = transform_direction(instance.frame, eval_normal(shape, intersection.element, intersection.uv));
        auto radiance = rgb_to_rgba(material.emission);
        if (bounce >= params.bounces)
            return radiance;
        else {
            auto incoming = sample_hemisphere_cos(normal, rand2f(rng));
            return radiance += rgb_to_rgba(material.color) / pi * shade_matte(scene, bvh, ray3f{ position, incoming }, bounce + 1, rng, params) * dot(normal, incoming);
        }
    }
    return rgb_to_rgba(eval_environment(scene, ray.d));
}

// Eyelight renderer.
static vec4f shade_eyelight(const scene_data& scene, const bvh_scene& bvh, const ray3f& ray, int bounce, rng_state& rng, const raytrace_params& params) {
    // YOUR CODE GOES HERE ----
    bvh_intersection intersection = intersect_bvh(bvh, scene, ray);
    if (intersection.hit) {
        //Ricaviamo instance e shape dall'intersezione
        auto& instance = scene.instances[intersection.instance];
        auto& shape = scene.shapes[instance.shape];
        //Effettuiamo un lookup del colore del materiale attraverso l'indice dell'instance
        auto material_color = rgb_to_rgba(scene.materials[instance.material].color);
        //Calcoliamo la normale 
        auto normal = transform_direction(instance.frame, eval_normal(shape, intersection.element, intersection.uv));
        //Restituiamo il calcolo definito nelle slide per ottenere l'effetto di illuminazione da una telecamera
        return material_color * dot(normal, -ray.d);
    }
    return {0.0f};
}

static vec4f shade_normal(const scene_data& scene, const bvh_scene& bvh,  const ray3f& ray, int bounce, rng_state& rng, const raytrace_params& params) {
    // YOUR CODE GOES HERE ----
    auto intersection = intersect_bvh(bvh, scene, ray);
    if (intersection.hit) {
        //Ricaviamo instance e shape dall'intersezione
        auto& instance = scene.instances[intersection.instance];
        auto& shape = scene.shapes[instance.shape];
        //Calcoliamo la normale attraverso la funzione sottostante, trasformandola in colore effettuando le operazioni successive * 0.5f + 0.5f
        return rgb_to_rgba(transform_direction(instance.frame, eval_normal(shape, intersection.element, intersection.uv) * 0.5f + 0.5f));
    }
    return {0.0f};
}

static vec4f shade_texcoord(const scene_data& scene, const bvh_scene& bvh, const ray3f& ray, int bounce, rng_state& rng, const raytrace_params& params) {
    // YOUR CODE GOES HERE ----
    auto intersection = intersect_bvh(bvh, scene, ray);
    if (intersection.hit) {
        //Ricaviamo instance dall'intersezione
        auto& instance = scene.instances[intersection.instance];
        //Calcoliamo le texture coordinates trasformandole in colore forzandole nel range [0, 1] con la funzione fmod
        vec2f texcoord = eval_texcoord(scene, instance, intersection.element, intersection.uv);
        return rgb_to_rgba({fmod(texcoord.x, 1), fmod(texcoord.y, 1), 0.0f});
    }
    return {0.0f};
}

static vec4f shade_color(const scene_data& scene, const bvh_scene& bvh, const ray3f& ray, int bounce, rng_state& rng, const raytrace_params& params) {
    // YOUR CODE GOES HERE ----
    auto intersection = intersect_bvh(bvh, scene, ray);
    if (intersection.hit) {
        //Ricaviamo instance dall'intersezione
        auto& instance = scene.instances[intersection.instance];
        //Effettuiamo un "lookup" del colore del materiale attraverso gli indici presenti in instance
        return rgb_to_rgba(eval_material(scene, instance, intersection.element, intersection.uv).color);
    }
    return {0.0f};
}

// Trace a single ray from the camera using the given algorithm.
using raytrace_shader_func = vec4f (*)(const scene_data& scene,
    const bvh_scene& bvh, const ray3f& ray, int bounce, rng_state& rng,
    const raytrace_params& params);
static raytrace_shader_func get_shader(const raytrace_params& params) {
  switch (params.shader) {
    case raytrace_shader_type::raytrace: return shade_raytrace;
    case raytrace_shader_type::matte: return shade_matte;
    case raytrace_shader_type::eyelight: return shade_eyelight;
    case raytrace_shader_type::normal: return shade_normal;
    case raytrace_shader_type::texcoord: return shade_texcoord;
    case raytrace_shader_type::color: return shade_color;
    default: {
      throw std::runtime_error("sampler unknown");
      return nullptr;
    }
  }
}

// Build the bvh acceleration structure.
bvh_scene make_bvh(const scene_data& scene, const raytrace_params& params) {
  return make_bvh(scene, false, false, params.noparallel);
}

// Init a sequence of random number generators.
raytrace_state make_state(
    const scene_data& scene, const raytrace_params& params) {
  auto& camera = scene.cameras[params.camera];
  auto  state  = raytrace_state{};
  if (camera.aspect >= 1) {
    state.width  = params.resolution;
    state.height = (int)round(params.resolution / camera.aspect);
  } else {
    state.height = params.resolution;
    state.width  = (int)round(params.resolution * camera.aspect);
  }
  state.samples = 0;
  state.image.assign(state.width * state.height, {0, 0, 0, 0});
  state.hits.assign(state.width * state.height, 0);
  state.rngs.assign(state.width * state.height, {});
  auto rng_ = make_rng(1301081);
  for (auto& rng : state.rngs) {
    rng = make_rng(961748941ull, rand1i(rng_, 1 << 31) / 2 + 1);
  }
  return state;
}

// Progressively compute an image by calling trace_samples multiple times.
void raytrace_samples(raytrace_state& state, const scene_data& scene,
    const bvh_scene& bvh, const raytrace_params& params) {
  if (state.samples >= params.samples) return;
  auto& camera = scene.cameras[params.camera];
  auto  shader = get_shader(params);
  state.samples += 1;
  if (params.samples == 1) {
    for (auto idx = 0; idx < state.width * state.height; idx++) {
      auto i = idx % state.width, j = idx / state.width;
      auto u = (i + 0.5f) / state.width, v = (j + 0.5f) / state.height;
      auto ray      = eval_camera(camera, {u, v});
      auto radiance = shader(scene, bvh, ray, 0, state.rngs[idx], params);
      if (!isfinite(radiance)) radiance = {0, 0, 0};
      state.image[idx] += radiance;
      state.hits[idx] += 1;
    }
  } else if (params.noparallel) {
    for (auto idx = 0; idx < state.width * state.height; idx++) {
      auto i = idx % state.width, j = idx / state.width;
      auto u        = (i + rand1f(state.rngs[idx])) / state.width,
           v        = (j + rand1f(state.rngs[idx])) / state.height;
      auto ray      = eval_camera(camera, {u, v});
      auto radiance = shader(scene, bvh, ray, 0, state.rngs[idx], params);
      if (!isfinite(radiance)) radiance = {0, 0, 0};
      state.image[idx] += radiance;
      state.hits[idx] += 1;
    }
  } else {
    parallel_for(state.width * state.height, [&](int idx) {
      auto i = idx % state.width, j = idx / state.width;
      auto u        = (i + rand1f(state.rngs[idx])) / state.width,
           v        = (j + rand1f(state.rngs[idx])) / state.height;
      auto ray      = eval_camera(camera, {u, v});
      auto radiance = shader(scene, bvh, ray, 0, state.rngs[idx], params);
      if (!isfinite(radiance)) radiance = {0, 0, 0};
      state.image[idx] += radiance;
      state.hits[idx] += 1;
    });
  }
}

// Check image type
static void check_image(
    const color_image& image, int width, int height, bool linear) {
  if (image.width != width || image.height != height)
    throw std::invalid_argument{"image should have the same size"};
  if (image.linear != linear)
    throw std::invalid_argument{
        linear ? "expected linear image" : "expected srgb image"};
}

// Get resulting render
color_image get_render(const raytrace_state& state) {
  auto image = make_image(state.width, state.height, true);
  get_render(image, state);
  return image;
}
void get_render(color_image& image, const raytrace_state& state) {
  check_image(image, state.width, state.height, true);
  auto scale = 1.0f / (float)state.samples;
  for (auto idx = 0; idx < state.width * state.height; idx++) {
    image.pixels[idx] = state.image[idx] * scale;
  }
}

}  // namespace yocto

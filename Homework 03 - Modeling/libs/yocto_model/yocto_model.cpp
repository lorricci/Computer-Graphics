//
// Implementation for Yocto/Model
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

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "yocto_model.h"

#include <yocto/yocto_sampling.h>

#include "ext/perlin-noise/noise1234.h"

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::array;
using std::string;
using std::vector;
using namespace std::string_literals;

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR EXAMPLE OF PROCEDURAL MODELING
// -----------------------------------------------------------------------------
namespace yocto {

float noise(const vec3f& p) { return ::noise3(p.x, p.y, p.z); }
vec2f noise2(const vec3f& p) {
  return {noise(p + vec3f{0, 0, 0}), noise(p + vec3f{3, 7, 11})};
}
vec3f noise3(const vec3f& p) {
  return {noise(p + vec3f{0, 0, 0}), noise(p + vec3f{3, 7, 11}),
      noise(p + vec3f{13, 17, 19})};
}
float fbm(const vec3f& p, int octaves) {
  auto sum    = 0.0f;
  auto weight = 1.0f;
  auto scale  = 1.0f;
  for (auto octave = 0; octave < octaves; octave++) {
    sum += weight * fabs(noise(p * scale));
    weight /= 2;
    scale *= 2;
  }
  return sum;
}
float turbulence(const vec3f& p, int octaves) {
  auto sum    = 0.0f;
  auto weight = 1.0f;
  auto scale  = 1.0f;
  for (auto octave = 0; octave < octaves; octave++) {
    sum += weight * fabs(noise(p * scale));
    weight /= 2;
    scale *= 2;
  }
  return sum;
}
float ridge(const vec3f& p, int octaves) {
  auto sum    = 0.0f;
  auto weight = 0.5f;
  auto scale  = 1.0f;
  for (auto octave = 0; octave < octaves; octave++) {
    sum += weight * (1 - fabs(noise(p * scale))) * (1 - fabs(noise(p * scale)));
    weight /= 2;
    scale *= 2;
  }
  return sum;
}

void add_polyline(shape_data& shape, const vector<vec3f>& positions,
    const vector<vec4f>& colors, float thickness = 0.0001f) {
  auto offset = (int)shape.positions.size();
  shape.positions.insert(
      shape.positions.end(), positions.begin(), positions.end());
  shape.colors.insert(shape.colors.end(), colors.begin(), colors.end());
  shape.radius.insert(shape.radius.end(), positions.size(), thickness);
  for (auto idx = 0; idx < positions.size() - 1; idx++) {
    shape.lines.push_back({offset + idx, offset + idx + 1});
  }
}

void sample_shape(vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texcoords, const shape_data& shape, int num) {
  auto triangles  = shape.triangles;
  auto qtriangles = quads_to_triangles(shape.quads);
  triangles.insert(triangles.end(), qtriangles.begin(), qtriangles.end());
  auto cdf = sample_triangles_cdf(triangles, shape.positions);
  auto rng = make_rng(19873991);
  for (auto idx = 0; idx < num; idx++) {
    auto [elem, uv] = sample_triangles(cdf, rand1f(rng), rand2f(rng));
    auto q          = triangles[elem];
    positions.push_back(interpolate_triangle(
        shape.positions[q.x], shape.positions[q.y], shape.positions[q.z], uv));
    normals.push_back(normalize(interpolate_triangle(
        shape.normals[q.x], shape.normals[q.y], shape.normals[q.z], uv)));
    if (!texcoords.empty()) {
      texcoords.push_back(interpolate_triangle(shape.texcoords[q.x],
          shape.texcoords[q.y], shape.texcoords[q.z], uv));
    } else {
      texcoords.push_back(uv);
    }
  }
}
//-----------------------------
// FUNZIONI PER EXTRA CREDIT
// ----------------------------

float voronoi_distance(vec3f position) {
    auto rng = make_rng(172784);
    vec3f floored = vec3f{floor(position.x), floor(position.y), floor(position.z)};
    vec3f fracted = vec3f{position.x - floor(position.x), position.y - floor(position.y), position.z - floor(position.z)};
    vec3f mb;
    vec3f mr;
    float result = 8.0f;
    
    for (float k = -1; k <= 1; k++)
        for (float j = -1; j <= 1; j++) 
            for (float i = -1; i <= 1; i++) {
                vec3f b = vec3f{i, j, k};
                vec3f r = b + rand3f(rng) - fracted;
                float d = dot(r, r);
                if (d < result) {
                    result = d;
                    mr = r;
                    mb = b;
                }
            }

    result = 8.0f;
    for(float k = -2; k <= 2; k++)
        for (float j = -2; j <= 2; j++)
            for (float i = -2; i <= 2; i++) {
                vec3f b = mb + vec3f{i, j, k};
                vec3f r = b + rand3f(rng) - fracted;
                float d = dot(0.5 * (mr + r), normalize(r - mr));
                result = min(result, d);
            }

    return result;
}

float voronoi_distance(vec2f position) {
    auto rng = make_rng(172784);
    vec2f floored = vec2f{floor(position.x), floor(position.y)};
    vec2f fracted = vec2f{position.x - floor(position.x), position.y - floor(position.y)};
    vec2f mb;
    vec2f mr;
    float result = 8.0f;

    for (float j = -1; j <= 1; j++)
        for(float i = -1; i <= 1; i++) {
                vec2f b = vec2f{i, j};
                vec2f r = b + rand2f(rng) - fracted;
                float d = dot(r, r);
                if (d < result) {
                    result = d;
                    mr = r;
                    mb = b;
                }
            }

    result = 8.0f;
    for (float j = -2; j <= 2; j++)
        for (float i = -2; i <= 2; i++) {
            vec2f b = mb + vec2f{i, j};
            vec2f r = b + rand2f(rng) - fracted;
            float d = dot(0.5 * (mr + r), normalize(r - mr));
            result = min(result, d);
        }

    return result;
}

float cellnoise(vec3f position) {
    return 1.0f - smoothstep(0.0f, 0.05f, voronoi_distance(position));
}

float cellnoise(vec2f position) {
    return 1.0f - smoothstep(0.0f, 0.05f, voronoi_distance(position));
}

vec2f hash2f(vec2f position) { 
    position = vec2f{dot(position, vec2f{127.1f, 311.7f}), dot(position, vec2f{269.5f, 183.3f})};
    auto to_fract = vec2f{sin(position.x), sin(position.y)} * 43758.5453f;
    return vec2f{ to_fract.x - floor(to_fract.x), to_fract.y - floor(to_fract.y)};
}

vec3f hash2f(vec3f position) {
    position = vec3f{dot(position, vec3f{127.1f, 311.7f}), dot(position, vec3f{269.5f, 183.3f})};
    auto to_fract = vec3f{sin(position.x), sin(position.y), sin(position.z)} *43758.5453f;
    return vec3f{to_fract.x - floor(to_fract.x), to_fract.y - floor(to_fract.y), to_fract.z - floor(to_fract.z)};
}

float smoothvoronoi(vec2f position) {
    vec2f floored = vec2f{floor(position.x), floor(position.y)};
    vec2f fracted = vec2f{position.x - floor(position.x), position.y - floor(position.y)};

    float res = 0.0f;
    for (float j = -1; j <= 1; j++)
        for (float i = -1; i <= 1; i++) {
            vec2f b = vec2f{i, j};
            vec2f r = b - fracted + hash2f(floored + b);
            float d = dot(r, r);

            res += 1.0f / pow(d, 8.0f);
        }
    return pow(1.0f / res, 1.0f / 16.0f);
}

float smoothvoronoi(vec3f position) {
    vec3f floored = vec3f{floor(position.x), floor(position.y), floor(position.z)};
    vec3f fracted = vec3f{position.x - floor(position.x), position.y - floor(position.y), position.z - floor(position.z)};

    float res = 0.0;
    for(float k = -1; k <= 1; k++)
        for (float j = -1; j <= 1; j++)
            for (float i = -1; i <= 1; i++) {
                vec3f b = vec3f{i, j, k};
                vec3f r = b - fracted + hash2f(floored + b);
                float d = dot(r, r);

                res += 1.0 / pow(d, 8.0f);
            }
    return pow(1.0f / res, 1.0f / 16.0f);
}
//-----------------------------
// FUNZIONI PER CREDITI BASE
//-----------------------------

void make_terrain(shape_data& shape, const terrain_params& params) {
  // YOUR CODE GOES HERE
  for (auto ind = 0; ind < shape.positions.size(); ind++) {
    shape.positions[ind] += shape.normals[ind] * ridge(shape.positions[ind] * params.scale, params.octaves) * params.height * (1 - length(shape.positions[ind] - params.center) / params.size);
    if (shape.positions[ind].y / params.height < 0.30f)
      shape.colors.push_back(params.bottom);
    else if (shape.positions[ind].y / params.height < 0.60f)
      shape.colors.push_back(params.middle);
    else
      shape.colors.push_back(params.top);
  }
  compute_normals(shape.normals, shape);
}

void make_displacement(shape_data& shape, const displacement_params& params) {
  // YOUR CODE GOES HERE
  for (auto ind = 0; ind < shape.positions.size(); ind++) {
    auto old_pos = shape.positions[ind];
    float noise;
    if (params.cellnoise) {//Applicazione di cellnoise
        if (params.tridimensional) //Applicazione a tre dimensioni
            noise = cellnoise(shape.positions[ind] * params.scale);
        else //Applicazione a due dimensioni
            noise = cellnoise(vec2f{ shape.positions[ind].x, shape.positions[ind].y } *params.scale);
    }
    else if (params.smoothvoronoi) {//Applicazione di smooth voronoi
        if (params.tridimensional)
            noise = smoothvoronoi(shape.positions[ind] * params.scale);
        else
            noise = smoothvoronoi(vec2f{shape.positions[ind].x, shape.positions[ind].y} * params.scale);
    }
    else //Applicazione di turbulence noise
        noise = turbulence(shape.positions[ind] * params.scale, params.octaves);
    if (params.surface) { //Applica il noise a tutta la superficie
        shape.positions[ind] += shape.normals[ind] * (noise * params.height);
        shape.colors.push_back(interpolate_line(params.bottom, params.top, distance(old_pos, shape.positions[ind]) / params.height));
    }
    else { //Applica il noise solo come texture 
        auto new_pos = shape.positions[ind] + shape.normals[ind] * (noise * params.height);
        shape.colors.push_back(interpolate_line(params.bottom, params.top, distance(old_pos, new_pos) / params.height));
    }
  }
  compute_normals(shape.normals, shape);
}

void make_hair(shape_data& hair, const shape_data& shape, const hair_params& params) {
  // YOUR CODE GOES HERE
  // Operiamo su una copia della shape
  shape_data shape_copy = shape;
  sample_shape(shape_copy.positions, shape_copy.normals, shape_copy.texcoords, shape_copy, params.num);
  auto rng = make_rng(172784);
  for (auto ind = 0; ind < shape_copy.positions.size(); ind++) {
    if (rand1f(rng) > params.density) continue; //Se un random è maggiore della densità, saltiamo questa iterazione
    //Definiamo due nuovi vettori che conterranno la posizione ed il colore da applicare ad hair
    vector<vec3f> positions;
    vector<vec4f> colors;
    //Mettiamo al loro interno la posizione ed il colore iniziale
    positions.push_back(shape_copy.positions[ind]);
    colors.push_back(params.bottom);

    //Calcoliamo la prima normale da usare, questa varierà in step successivi
    auto normal = shape_copy.normals[ind];

    for (auto curr_step = 0; curr_step < params.steps; curr_step++) { //Componiamo "steps" segmenti per la creazione del capello
        //Calcoliamo il vertice spostato di length/step lungo la normale, perturbato da noise (scalato) e moltiplicato per il fattore strength di hair
        //Spostiamo poi il risultato lungo l'asse y per la gravità
        vec3f vertex = positions[curr_step] + (params.lenght / params.steps) * normal + noise3(positions[curr_step] * params.scale) * params.strength;
        vertex.y -= params.gravity;

        //Aggiorniamo la normale nel caso siano presenti più steps
        normal = normalize(vertex - positions[curr_step]);

        //Aggiungiamo la posizione calcolata ed il colore variato tra bottom e top in base alla distanza tra il nuovo vertice e la posizione originale
        positions.push_back(vertex);
        colors.push_back(interpolate_line(params.bottom, params.top, distance(vertex, shape_copy.positions[ind]) / params.lenght));
    }
    //Sovrascriviamo l'ultimo colore ed aggiungiamo il capello appena creato alla shape hair
    colors[params.steps] = params.top;
    add_polyline(hair, positions, colors);
  }
  //Calcoliamo ora le nuove tangenti
  lines_tangents(lines_tangents(hair.lines, hair.positions), hair.lines, hair.positions);
}

void make_grass(scene_data& scene, const instance_data& object, const vector<instance_data>& grasses, const grass_params& params) {
  // YOUR CODE GOES HERE
  // Istanziamo il randomizer con il seed dato nella documentazione
  auto rng = make_rng(172784);

  //Ricaviamo la  shape dalla scena ed effettuiamo un sampling dei punti
  auto shape = scene.shapes[object.shape];
  sample_shape(shape.positions, shape.normals, shape.texcoords, shape, params.num);
  for (auto ind = 0; ind < shape.positions.size(); ind++) {
    auto grass = grasses[rand1i(rng, grasses.size())];
    if (rand1f(rng) > params.density) continue; //Se un random è maggiore della densità, saltiamo questa iterazione
    //Costruiamo >il frame di grass come riportato nelle slide
    grass.frame.y = shape.normals[ind];
    grass.frame.x = normalize(vec3f{1.0f, 0.0f, 0.0f} - dot(vec3f{1.0f, 0.0f, 0.0f}, grass.frame.y) * grass.frame.y);
    grass.frame.z = cross(grass.frame.x, grass.frame.y);
    grass.frame.o = shape.positions[ind];
    
    //Effettuiamo lo scaling per un valore [0.9, 1.0)
    auto scaling = 0.9f + rand1f(rng) * 0.1f;
    grass.frame *= scaling_frame(vec3f{scaling, scaling, scaling});
    
    // Ruotiamo l'asse y di un valore [0, 2pi)
    auto y_angle = rand1f(rng) * 2.0f * pif;
    grass.frame *= rotation_frame(grass.frame.y, y_angle);

    //Ruotiamo l'asse z di un valore [0.1, 0.2)
    auto z_angle = 0.1f + rand1f(rng) * 0.1f;
    grass.frame *= rotation_frame(grass.frame.z, z_angle);

    //Aggiungiamo infine l'istanza alla nostra scena
    scene.instances.push_back(grass);
  }
}

}  // namespace yocto

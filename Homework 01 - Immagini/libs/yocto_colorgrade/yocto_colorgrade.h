//
// Yocto/Grade: Tiny library for color grading.
//

//
// LICENSE:
//
// Copyright (c) 2020 -- 2020 Fabio Pellacini
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#ifndef _YOCTO_COLORGRADE_
#define _YOCTO_COLORGRADE_

#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>

// -----------------------------------------------------------------------------
// COLOR GRADING FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Color grading parameters
struct grade_params {
  float exposure   = 0.0f;
  bool  filmic     = false;
  bool  srgb       = true;
  vec3f tint       = vec3f{1, 1, 1};
  float saturation = 0.5f;
  float contrast   = 0.5f;
  float vignette   = 0.0f;
  float grain      = 0.0f;
  int   mosaic     = 0;
  int   grid       = 0;
  bool  predthermal = false;
  float sigma       = 0.0f;
  bool  crosshatching = false;
  // Diversi fattori di luminosit� che determinano quando le hatch lines
  // "compaiono"
  float hatch_1 = 0.8f;
  float hatch_2 = 0.6f;
  float hatch_3 = 0.3f;
  float hatch_4 = 0.15f;

  // Vicinanza tra le diverse hatch lines (density) e larghezza delle hatch lines (width)
  float density = 10.0f;
  float width   = 1.0f;
  // Specifica se i valori che verrano generati per l'hatching terranno conto
  // del colore o se verr� usato il solo grey-scaling
  bool color_hatches = false;

  // Kernel offset
  float k_offset = 1.0f;
};

// Grading functions
color_image grade_image(const color_image& image, const grade_params& params);

};  // namespace yocto

#endif

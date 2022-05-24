//
// Implementation for Yocto/Grade.
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

#include "yocto_colorgrade.h"

#include <yocto/yocto_color.h>
#include <yocto/yocto_sampling.h>
#include <list>

// -----------------------------------------------------------------------------
// COLOR GRADING FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {
	//-------------------------------------------------------------------------------------------------
	// fUNZIONI PER GRADING DI BASE DELL'IMMAGINE
	//-------------------------------------------------------------------------------------------------
	
	/// <summary>
	/// Implementiamo la funzione che applica le trasformazioni di ToneMapping.
	/// </summary>
	/// <param name="rgb">Vettore di float, corrisponde all'rgb del pixel</param>
	/// <param name="params">Parametri di grading</param>
	/// <returns>Il canale RGB del pixel dopo aver effettuato le correzioni</returns>
	vec3f applyToneMapping(vec3f rgb, const grade_params& params) {
		//Applichiamo l'exposure compensation
          if (params.exposure != 0) rgb *= pow(2, params.exposure);
		  //Applichiamo la filmic correction
          if (params.filmic != 0) {
            rgb *= 0.6;
            rgb = (pow(rgb, 2) * 2.51 + rgb * 0.03) / (pow(rgb, 2) * 2.43 + rgb * 0.59 + 0.14);
		  }
		  //Applichiamo l'srgb color space
          if (params.srgb != 0) rgb = pow(rgb, 1 / 2.2); 
		  //Applichiamo il clamp
          rgb = clamp(rgb, 0, 1);
          return rgb;
    }

	/// <summary>
	/// Applica la correzione di Color Tint.
	/// </summary>
	/// <param name="rgb">Canale RGB del pixel</param>
	/// <param name="params">Parametri di grading</param>
	/// <returns>Canale RGB del pixel dopo aver effettuato la correzione</returns>
	vec3f applyColorTint(vec3f rgb, const grade_params& params) {
		rgb *= params.tint;
		return rgb;
	}
    
	/// <summary>
	/// Applichiamo un effetto Saturation. La variabile g viene calcolata come indicato
	/// </summary>
	/// <param name="rgb">Canale RGB del pixel</param>
	/// <param name="params">Parametri di grading</param>
	/// <returns>Canale RGB del pixel dopo aver effettuato la correzione</returns>
	vec3f applySaturation(vec3f rgb, const grade_params& params) {
          auto g = (rgb.x + rgb.y + rgb.z) / 3;
          rgb = g + (rgb - g) * (params.saturation * 2);
          return rgb;
	}

	/// <summary>
	/// Applichiamo il Contrast richiamando la funzione buil-in gain della libreria yocto.
	/// </summary>
	/// <param name="rgb">Canale RGB del pixel</param>
	/// <param name="params">Parametri di grading</param>
	/// <returns>Canale RGB del pixel dopo aver effettuato la correzione</returns>
	vec3f applyContrast(vec3f rgb, const grade_params& params) {
          rgb = gain(rgb, 1 - params.contrast);
          return rgb;
	}

	/// <summary>
	/// Applichiamo un effetto Vignette calcolando alcuni valori definiti dalla traccia in base alle coordinate del pixel e la grandezza
	/// dell'immagine. Usiamo poi una funzione built-in smoothstep con i valori calcolati.
	/// </summary>
	/// <param name="rgb">Canale RGB del pixel</param>
	/// <param name="params">Parametri di grading</param>
	/// <param name="coords">Coordinate del pixel su cui applicare la funzione</param>
	/// <param name="size">Size dell'immagine</param>
	/// <returns>Canale RGB del pixel dopo aver effettuato la correzione</returns>
	vec3f applyVignette(vec3f rgb, const grade_params& params, vec2f coords, vec2f size) {
          auto vr = 1 - params.vignette;
          auto r  = length(coords - size / 2) / length(size / 2);
		rgb *= (1 - smoothstep(vr, 2 * vr, r));
		return rgb;
	}

	/// <summary>
	/// Applichiamo un effetto mosaico, spostando i pixel calcolati secondo una funzione data dalla traccia. 
	/// </summary>
	/// <param name="params">Parametri di grading</param>
	/// <param name="to_grade">Immagine su cui effettuare il grading</param>
	void applyMosaicEffect(const grade_params& params, color_image& to_grade) {
		for (auto i = 0; i < to_grade.width; i++) {
			for (auto j = 0; j < to_grade.height; j++) {
				vec2i index     = {i, j};
				vec2i new_index = {i - (i % params.mosaic), j - (j % params.mosaic)};
				auto rgb = xyz(to_grade[new_index]);
				to_grade[index] = {rgb.x, rgb.y, rgb.z};
            }
        }
    }

	/// <summary>
	/// Applichiamo un effetto griglia, la trasformazione viene calcolata in base alla posizione attuale del canale pixel preso in esame.
	/// </summary>
	/// <param name="params">Parametri di grading</param>
	/// <param name="to_grade">Immagine su cui effettuare il grading</param>
	void applyGridEffect(const grade_params& params, color_image& to_grade) {
      for (auto i = 0; i < to_grade.width; i++) {
        for (auto j = 0; j < to_grade.height; j++) {
          vec2i index = {i, j};
          auto  rgb = xyz(to_grade[index]);
          rgb = (0 == i % params.grid || 0 == j % params.grid) ? 0.5 * rgb : rgb;
          to_grade[index] = {rgb.x, rgb.y, rgb.z};
        }
      }
    }
	
	/// <summary>
	/// Calcola un valore heat come definito su Shader Toy - Predator Thermal Vision III
	/// </summary>
	/// <param name="smoothValue">Variabile usata nel calcolo della funzione smoothstep</param>
	/// <returns>Un vec3f heat, contenente i valori rgb modificati secondo il filtro</returns>
	vec3f calcHeat(float smoothValue) {
      auto  value = 1 - smoothValue;
      auto  heat_factor_one = 0.5f + 0.5f * smoothstep(0.0f, 0.1f, value);
      vec3f heat_factor_two = {smoothstep(0.5f, 0.3f, value), value < 0.3f ? smoothstep(0.0f, 0.3f, value) : smoothstep(1.0f, 0.6f, value), smoothstep(0.4f, 0.6f, value)};
      auto heat = heat_factor_one * heat_factor_two;
      return heat;
    }

	/// <summary>
	/// Applica una visione termica ispirata al film cult Predator
	/// </summary>
	/// <param name="to_grade">Immagine su cui applicare la visione termica Predator</param>
	/// <param name="to_grade_size">Size dell'immagine</param>
	/// <param name="rgb">Canale RGB del pixel</param>
	/// <param name="coords">Coordinate del pixel</param>
	/// <returns>Canale RGB modificato con le operazioni di heat</returns>
	vec3f predatorThermalVision(vec2f to_grade_size, vec3f rgb, vec2f coords) { 
		//Preleviamo qualche valore di setup, le coordinate sono come l'index, ma castate come floats
        vec2f rg = {rgb.x, rgb.y};
        vec2f uv = coords / to_grade_size;
        auto smoothValue = smoothstep(rgb.z, 0.0f, distance(rg, uv));
		//Applichiamo la funzione di Heat
        auto thermalRGB = calcHeat(smoothValue);
        return thermalRGB;
	}

	//------------------------------------------------------------------------------------
	//FUNZIONI PER GAUSSIAN BLUR
	//------------------------------------------------------------------------------------

	/// <summary>
	/// Effettua una normalizzazione in base al fattore Sigma specificato
	/// </summary>
	/// <param name="x">Indice di iterazione sulla grandezza del kernel</param>
	/// <param name="sigma">Fattore di blurring</param>
	/// <returns>Valore di costruzione del kernel</returns>
	float normpdf(float x, float sigma) { 
		return 0.39894 * exp(-0.5f * x * (x/(sigma * sigma))) / sigma;
	}

	/// <summary>
	/// Applica una funzione di Gaussian Blur all'immagine. La grandezza del kernel è di 6.
	/// </summary>
	/// <param name="to_grade">Immagine su cui applicare il Gaussian Blur</param>
	/// <param name="to_grade_size">Size dell'immagine</param>
	/// <param name="coords">Coordinate del pixel</param>
	/// <param name="params">Parametri di grading</param>
	/// <returns>Canale RGB modificato con le operazioni di Gaussian Bluring</returns>
	vec3f gaussianBlur(const color_image& to_grade, vec2f to_grade_size, vec2f coords, const grade_params& params) {
		//Dichiariamo prima alcune grandezze per il kernel
		const int mSize = 11;
        const int kernSize = (mSize - 1) / 2;
		float Z = 0.0f;

		//Creiamo il kernel 1 - D
        float kernel[mSize];
        for (auto j = 0; j <= kernSize; j++) {
			kernel[kernSize + j] = normpdf(j, params.sigma);
			kernel[kernSize - j] = normpdf(j, params.sigma);
		}

		//Otteniamo il fattore di normalizzazione
        for (auto j = 0; j < mSize; j++) 
			Z += kernel[j];

		//Applichiamo il blur in base al kernel calcolato prima
        vec4f final_color = {0.0f};
        for (auto i = -kernSize; i <= kernSize; i++)
          for (auto j = -kernSize; j <= kernSize; j++) {
            vec2f flIJ = {i, j};
            final_color += kernel[kernSize + j] * kernel[kernSize + i] * eval_image(to_grade, (coords + flIJ) / to_grade_size);
          }
		//Estraiamo l'rgb dal colore finale calcolato e restituiamolo in output
        final_color /= Z * Z;
        vec3f rgb = {final_color.x, final_color.y, final_color.z};
        return rgb;
	}

	//------------------------------------------------------------------------------------
	// FUNZIONI PER IL CROSS-HATCHING via EDGE DETECTION FILTER
	// -----------------------------------------------------------------------------------

	/// <summary>
	/// Implementa la funzione per il calcolo del valore x modulo y.
	/// </summary>
	/// <param name="x_value">Valore x</param>
	/// <param name="y_value">Valore y</param>
	/// <returns>Valore di x modulo y, implementato con la funzione floor</returns>
	float modOpenImplementation(float x_value, float y_value) {
          return x_value - (y_value * floor(x_value / y_value));
	}

	/// <summary>
	/// Effettua dei controlli successivi sui valori hatch definiti in base al grey-scaling/rgb usage. Per ogni controllo su hatch viene effettuato il calcolo
	/// di un nuovo valore, altrimenti viene restituito il valore di default {1.0f, 1.0f, 1.0f}
	/// </summary>
	/// <param name="brightness">Brightness calcolata nella funzione principale</param>
	/// <param name="coords">Coordinate del pixel</param>
	/// <param name="params">Parametri di grading</param>
	/// <param name="tex_rgb">Canale RGB del vettore sRGB calcolato nella funzione principale</param>
	/// <param name="h1br">Soglia 1 di hatching</param>
	/// <param name="h2br">Soglia 2 di hatching</param>
	/// <param name="h3br">Soglia 3 di hatching</param>
	/// <param name="h4br">Soglia 4 di hatching</param>
	/// <returns>Colore risultante</returns>
	vec3f brightnessVSHatches(float brightness, vec2f coords, const grade_params& params, vec3f tex_rgb, float h1br, float h2br, float h3br, float h4br) {
        vec3f res = {1.0, 1.0, 1.0};
        //Controllo primo hatch
		if (brightness < params.hatch_1) {
			float x_value = coords.x + coords.y;
            float y_value = params.density;
			if (modOpenImplementation(x_value, y_value) <= params.width) {
				if (params.color_hatches) 
					res = {tex_rgb.x * h1br, tex_rgb.y * h1br, tex_rgb.z * h1br};
				else
                    res = {h1br};
            }
        }
        //Controllo secondo hatch
        if (brightness < params.hatch_2) {
			float x_value = coords.x - coords.y;
			float y_value = params.density;
			if (modOpenImplementation(x_value, y_value) <= params.width) {
				if (params.color_hatches)
					res = {tex_rgb.x * h2br, tex_rgb.y * h2br, tex_rgb.z * h2br};
				else
					res = {h2br};
			}
        }
        //Controllo terzo hatch
        if (brightness < params.hatch_3) {
			float x_value = coords.x + coords.y - (params.density * 0.5f);
			float y_value = params.density;
			if (modOpenImplementation(x_value, y_value) <= params.width) {
				if (params.color_hatches)
					res = {tex_rgb.x * h3br, tex_rgb.y * h3br, tex_rgb.z * h3br};
				else
					res = {h3br};
			}
        }
        //Controllo quarto hatch
        if (brightness < params.hatch_4) {
			float x_value = coords.x - coords.y - (params.density * 0.5f);
			float y_value = params.density;
			if (modOpenImplementation(x_value, y_value) <= params.width) {
				if (params.color_hatches)
					res = {tex_rgb.x * h4br, tex_rgb.y * h4br, tex_rgb.z * h4br};
				else
					res = {h4br};
			}
        }
        return res;
	}
	
	/// <summary>
	/// Applica un effetto CrossHatching all'immagine in input. Il settaggio dei valori necessari al CrossHatching viene prima fatto
	/// attraverso una versione semplificata dell'algoritmo di Edge Detection Sobel.
	/// </summary>
	/// <param name="to_grade">Immagine su cui applicare il CrossHatching</param>
	/// <param name="to_grade_size">Dimensioni dell'immagine</param>
	/// <param name="coords">Coordinate del pixel corrente</param>
	/// <param name="params">Parametri di grading</param>
	/// <returns>Colore modificato dall'effetto Crosshatching</returns>
	vec3f applyCrossHatching(const color_image& to_grade, vec2f to_grade_size, vec2f coords, const grade_params& params) {
		//Definiamo i valori di brightness in base a se utilizziamo grey-scaling o colored-hatches
        float hatch_1_brightness = 0.0;
        float hatch_2_brightness = 0.0;
        float hatch_3_brightness = 0.0;
        float hatch_4_brightness = 0.0;
		if (params.color_hatches) {
			hatch_1_brightness = 0.8;
			hatch_2_brightness = 0.6;
            hatch_3_brightness = 0.3;
            hatch_4_brightness = 0.0;
        }
		//Definiamo altri valori al "riposizionamento" per creare l'effetto cross-hatching
        float ratio = to_grade_size.y / to_grade_size.x;
        vec2f new_coords = {coords.x / to_grade_size.x, coords.y / to_grade_size.x};
        vec2f uv = {new_coords.x, new_coords.y / ratio};

		//Definiamo dei valori riguardanti i canali rgb e le loro variazioni per l'effetto cross-hatching
        auto tex = eval_image(to_grade, uv);
        float brightness = (tex.x * 0.2126) + (tex.y * 0.7152) + (tex.z * 0.0722); 

		//Nel caso includiamo i colori, calcoliamo il delta (canale rgb più luminoso meno quello luminoso). Se è il delta è troppo piccolo
		//usiamo il normale grayscaling con tex = {1.0, 1.0, 1.0, alpha}
		if (params.color_hatches) {
			float dimmest = min(min(tex.x, tex.y), tex.z);
			float brightest = max(max(tex.x, tex.y), tex.z);
			float delta = brightest - dimmest;
          if (delta > 0.1)
            tex *= (1.0 / brightest);
          else {
            tex.x = 1.0;
            tex.y = 1.0;
            tex.z = 1.0;
		  }
		}
        //Effettuiamo ora diverse correzioni, in base al valore brightness calcolato prima rispetto ai valori hatch predefiniti. Per comodità di lettura
		//questo procedimento è definito nella funzione brightnessVSHatches
        vec3f tex_rgb = {tex.x, tex.y, tex.z};
        vec3f res = brightnessVSHatches(brightness, coords, params, tex_rgb, hatch_1_brightness, hatch_2_brightness, hatch_3_brightness, hatch_4_brightness);
        return res;
	}

	//------------------------------------------------------------------------------------
	//FUNZIONE DI GRADING PRINCIPALE
	//------------------------------------------------------------------------------------
	color_image grade_image(const color_image& image, const grade_params& params) {
		// PUT YOUR CODE HERE
		//Ricaviamo prima alcuni parametri utili per le operazioni di gradazione dell'immagine
		auto to_grade = image;
		vec2f to_grade_size = {to_grade.width, to_grade.height}; 
		auto rng = make_rng(172784);
		for (auto i = 0; i < to_grade.width; i++) {
          for (auto j = 0; j < to_grade.height; j++) {
            // Per ogni pixel, estraiamo il corrispondente canale RGB e applichiamo le trasformazioni indicate
            vec2i index = {i, j};
            auto  rgb = xyz(to_grade[index]);
			//Le trasformazioni restituiranno sempre il canale rgb modificato, per allinearsi alle specifiche di "design ideale" del codice 
			//usando la libreria yocto, come discusso in classe (passaggio di raw data a fine funzione)
            // Applichiamo la sequenza di operazioni del tone-mapping
            rgb = applyToneMapping(rgb, params);
            // Applichiamo la funzione di Color Tint
            rgb = applyColorTint(rgb, params);
            // Applichiamo la funzione di Saturazione
            rgb = applySaturation(rgb, params);
            // Applichiamo la funzione di Contrast
            rgb = applyContrast(rgb, params);
            // Applichiamo la funzione di Vignette. Utilizziamo un vec2f invece che un vec2i per questione di tipazione nel calcolo della funzione
            vec2f coords = {i, j};
            rgb = applyVignette(rgb, params, coords, to_grade_size);
            // Applichiamo la funzione di Film Grain. Non è in una funzione a parte per problemi di generazione del valore randomico
            rgb += (rand1f(rng) - 0.5) * params.grain;
			//Applichiamo la funzione di Predator Thermal Vision
            if (params.predthermal)	rgb = predatorThermalVision(to_grade_size, rgb, coords);
            if (params.sigma != 0) rgb = gaussianBlur(to_grade, to_grade_size, coords, params);
            if (params.crosshatching) rgb = applyCrossHatching(to_grade, to_grade_size, coords, params);
            // Assegnamo all'immagine le operazioni effettuate sui canali RGB
            to_grade[index] = {rgb.x, rgb.y, rgb.z};
          }
        }
		//Le successive operazioni vengono effettuate in cicli a parte, per evitare di causare problemi alle modifiche del colore
		
		// Applichiamo la funzione di Mosaic Effect
		if (params.mosaic != 0) applyMosaicEffect(params, to_grade);
        // Applichiamo la funzione di Grid Effect
        if (params.grid != 0) applyGridEffect(params, to_grade);
		return to_grade;
	}	
}  // namespace yocto
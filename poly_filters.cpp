/*
  Copyright 2020 Loki Davison <loki@polyeffects.com>
  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.
  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "lv2/core/lv2.h"

/** Include standard C headers */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define clamp(x, lower, upper) (fmax(lower, fmin(x, upper)))

#define FILTER_URI "http://polyeffects.com/lv2/polyfilter#"
/* 6.28318548 / 48000 */
#define CONST0 0.0001308996975

/**
   In code, ports are referred to by index.  An enumeration of port indices
   should be defined for readability.
*/
typedef enum {
    CUTOFF_PARAM = 0,
    Q_PARAM = 1,
    CUTOFF_CV = 2,
    Q_CV = 3,
    INPUT_1 = 4,
    OUTPUT_1 = 5, // overheim lowpass
    OUTPUT_HPF = 6, // hpf
    OUTPUT_BPF = 7, // bpf
    OUTPUT_BSF = 8, // bsf
} PortIndex;

typedef enum {
    DIODE_LADDER = 0,
    KORG_HPF = 1,
    KORG_LPF = 2,
    MOOG_LADDER = 3,
    MOOG_HALF_LADDER = 4,
    OBERHEIM = 5,
} FilterTypes;
/**
   Every plugin defines a private structure for the plugin instance.  All data
   associated with a plugin instance is stored here, and is available to
   every instance method.  In this simple plugin, only port buffers need to be
   stored, since there is no additional instance data.
*/
typedef struct {
	// Port buffers
    const float* cutoff_param;
    const float* q_param;
    const float* cutoff_cv;
    const float* q_cv;
    const float* input_1;
    float* output_1;
    float* output_hpf;
    float* output_bpf;
    float* output_bsf;

	/* float algorithm_smooth = 0.5f; */
    /* float timbre_smooth = 0.5f; */
    /* float level1_smooth = 0.5f; */
    /* float level2_smooth = 0.5f; */

	float fRec1[2];
	float fRec2[2];
	float fRec3[2];
	float fRec4[2];
	float fRec5[2];
	float fRec6[2];

    FilterTypes filter_type = DIODE_LADDER;

} Filter ;


static float faustpower2_f(float value) {
	return (value * value);
	
}
static float faustpower10_f(float value) {
	return (((((((((value * value) * value) * value) * value) * value) * value) * value) * value) * value);
	
}
static float faustpower3_f(float value) {
	return ((value * value) * value);
	
}
static float faustpower4_f(float value) {
	return (((value * value) * value) * value);
	
}

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	Filter* amp = (Filter*)calloc(1, sizeof(Filter));

	// set feature mode for the difference modules.
	if (!strcmp (descriptor->URI, FILTER_URI "diode_ladder")) {
		amp->filter_type = DIODE_LADDER;
	} else if (!strcmp (descriptor->URI, FILTER_URI "korg_hpf")) {
		amp->filter_type = KORG_HPF;
	} else if (!strcmp (descriptor->URI, FILTER_URI "korg_lpf")) {
		amp->filter_type = KORG_LPF;
	} else if (!strcmp (descriptor->URI, FILTER_URI "moog_ladder")) {
		amp->filter_type = MOOG_LADDER;
	} else if (!strcmp (descriptor->URI, FILTER_URI "moog_half_ladder")) {
		amp->filter_type = MOOG_HALF_LADDER;
	} else if (!strcmp (descriptor->URI, FILTER_URI "oberheim")) {
		amp->filter_type = OBERHEIM;
    }

	return (LV2_Handle)amp;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	Filter* amp = (Filter*)instance;

    switch ((PortIndex)port) {

        case CUTOFF_PARAM:
            amp->cutoff_param = (const float*)data;
            break;
        case Q_PARAM:
            amp->q_param = (const float*)data;
            break;
        case CUTOFF_CV:
            amp->cutoff_cv = (const float*)data;
            break;
        case Q_CV:
            amp->q_cv = (const float*)data;
            break;
        case INPUT_1:
            amp->input_1 = (const float*)data;
            break;
        case OUTPUT_1:
            amp->output_1 = (float*)data;
            break;
        case OUTPUT_HPF:
            amp->output_hpf = (float*)data;
            break;
        case OUTPUT_BPF:
            amp->output_bpf = (float*)data;
            break;
        case OUTPUT_BSF:
            amp->output_bsf = (float*)data;
            break;
		default:
			break;
	}
}

/**
   The `activate()` method is called by the host to initialise and prepare the
   plugin instance for running.  The plugin must reset all internal state
   except for buffer locations set by `connect_port()`.  Since this plugin has
   no other internal state, this method does nothing.
   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void
activate(LV2_Handle instance)
{
}

/**
   The `run()` method is the main process function of the plugin.  It processes
   a block of audio in the audio context.  Since this plugin is
   `lv2:hardRTCapable`, `run()` must be real-time safe, so blocking (e.g. with
   a mutex) or memory allocation are not allowed.
*/
static void
run(LV2_Handle instance, uint32_t n_samples)
{
	Filter* const amp = (Filter*)instance;

    const float cutoff_param = *(amp->cutoff_param);
    const float q_param = *(amp->q_param);
    const float* cutoff_cv = amp->cutoff_cv;
    const float* q_cv = amp->q_cv;
    const float* input_1 = amp->input_1;
    float* output_1 = amp->output_1;
    float* output_hpf = amp->output_hpf;
    float* output_bpf = amp->output_bpf;
    float* output_bsf = amp->output_bsf;

    float* fRec1 = amp->fRec1;
    float* fRec2 = amp->fRec2;
    float* fRec3 = amp->fRec3;
    float* fRec4 = amp->fRec4;
    float* fRec5 = amp->fRec5;
    float* fRec6 = amp->fRec6;

    // Diode ladder

	if (amp->filter_type == DIODE_LADDER)
    {
		for (uint32_t i = 0; (i < n_samples); i = (i + 1)) {
            float fSlow0 = (0.00100000005f * clamp(float(cutoff_param)+cutoff_cv[i], 0.0f, 1.0f));
            float fSlow1 = (clamp(float(q_param)+(q_cv[i]*25.0f), 0.7072f, 25.0f) + -0.707000017f);
            float fSlow2 = (0.00514551532f * fSlow1);
			fRec5[0] = (fSlow0 + (0.999000013f * fRec5[1]));
			float fTemp0 = std::tan((CONST0 * std::pow(10.0f, ((3.0f * fRec5[0]) + 1.0f))));
			float fTemp1 = input_1[i];
			float fTemp2 = (17.0f - (9.69999981f * faustpower10_f(fRec5[0])));
			float fTemp3 = (fTemp0 + 1.0f);
			float fTemp4 = ((0.5f * ((fRec1[1] * fTemp0) / fTemp3)) + fRec2[1]);
			float fTemp5 = ((fTemp0 * (1.0f - (0.25f * (fTemp0 / fTemp3)))) + 1.0f);
			float fTemp6 = ((fTemp0 * fTemp4) / fTemp5);
			float fTemp7 = (0.5f * fTemp6);
			float fTemp8 = (fTemp7 + fRec3[1]);
			float fTemp9 = ((fTemp0 * (1.0f - (0.25f * (fTemp0 / fTemp5)))) + 1.0f);
			float fTemp10 = ((fTemp0 * fTemp8) / fTemp9);
			float fTemp11 = (fTemp10 + fRec4[1]);
			float fTemp12 = (fTemp5 * fTemp9);
			float fTemp13 = ((fTemp0 * (1.0f - (0.5f * (fTemp0 / fTemp9)))) + 1.0f);
			float fTemp14 = faustpower2_f(fTemp0);
			float fTemp15 = (fTemp3 * fTemp5);
			float fTemp16 = ((fTemp0 * ((((((1.5f * (fTemp1 * (1.0f - (0.333333343f * faustpower2_f(fTemp1))))) + (fSlow1 * ((fTemp2 * (((0.0f - ((0.0411641225f * fRec1[1]) + (0.0205820613f * fTemp6))) - (0.0205820613f * fTemp10)) - (0.00514551532f * ((faustpower3_f(fTemp0) * fTemp11) / (fTemp12 * fTemp13))))) / fTemp3))) * ((0.5f * (fTemp14 / (fTemp9 * fTemp13))) + 1.0f)) / ((fSlow2 * ((faustpower4_f(fTemp0) * fTemp2) / ((fTemp15 * fTemp9) * fTemp13))) + 1.0f)) + ((fTemp8 + (0.5f * ((fTemp0 * fTemp11) / fTemp13))) / fTemp9)) - fRec4[1])) / fTemp3);
			float fTemp17 = ((fTemp0 * ((0.5f * (((fRec4[1] + fTemp16) * ((0.25f * (fTemp14 / fTemp12)) + 1.0f)) + ((fTemp4 + (0.5f * fTemp10)) / fTemp5))) - fRec3[1])) / fTemp3);
			float fTemp18 = ((fTemp0 * ((0.5f * (((fRec3[1] + fTemp17) * ((0.25f * (fTemp14 / fTemp15)) + 1.0f)) + ((fRec1[1] + fTemp7) / fTemp3))) - fRec2[1])) / fTemp3);
			float fTemp19 = ((fTemp0 * ((0.5f * (fRec2[1] + fTemp18)) - fRec1[1])) / fTemp3);
			float fRec0 = (fRec1[1] + fTemp19);
			fRec1[0] = (fRec1[1] + (2.0f * fTemp19));
			fRec2[0] = (fRec2[1] + (2.0f * fTemp18));
			fRec3[0] = (fRec3[1] + (2.0f * fTemp17));
			fRec4[0] = (fRec4[1] + (2.0f * fTemp16));
			output_1[i] = float(fRec0);
			fRec5[1] = fRec5[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			
		}
		
	}
    else if (amp->filter_type == KORG_HPF)
    {
		for (uint32_t i = 0; (i < n_samples); i = (i + 1)) {
            float fSlow0 = (0.00100000005f * clamp(float(cutoff_param)+cutoff_cv[i], 0.0f, 1.0f));
            float fSlow1 = (0.215215757f * clamp(float(q_param)+(q_cv[i]*10.0f), 0.5, 10.0f) + -0.707000017f);

			float fTemp0 = float(input_1[i]);
			fRec4[0] = (fSlow0 + (0.999000013f * fRec4[1]));
			float fTemp1 = std::tan((CONST0 * std::pow(10.0f, ((3.0f * fRec4[0]) + 1.0f))));
			float fTemp2 = ((fTemp0 - fRec3[1]) * fTemp1);
			float fTemp3 = (fTemp1 + 1.0f);
			float fTemp4 = (fTemp1 / fTemp3);
			float fTemp5 = ((fTemp0 - (fRec3[1] + (((fTemp2 - fRec1[1]) - (fRec2[1] * (0.0f - fTemp4))) / fTemp3))) / (1.0f - (fSlow1 * ((fTemp1 * (1.0f - fTemp4)) / fTemp3))));
			float fRec0 = fTemp5;
			float fTemp6 = (fSlow1 * fTemp5);
			float fTemp7 = ((fTemp1 * (fTemp6 - fRec2[1])) / fTemp3);
			fRec1[0] = (fRec1[1] + (2.0f * ((fTemp1 * (fTemp6 - (fTemp7 + (fRec1[1] + fRec2[1])))) / fTemp3)));
			fRec2[0] = (fRec2[1] + (2.0f * fTemp7));
			fRec3[0] = (fRec3[1] + (2.0f * (fTemp2 / fTemp3)));
			output_1[i] = float(fRec0);
			fRec4[1] = fRec4[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
		}
	}
    else if (amp->filter_type == KORG_LPF)
    {
		for (uint32_t i = 0; (i < n_samples); i = (i + 1)) {
            float fSlow0 = (0.00100000005f * clamp(float(cutoff_param)+cutoff_cv[i], 0.0f, 1.0f));
            float fSlow1 = (0.215215757f * clamp(float(q_param)+(q_cv[i]*10.0f), 0.5, 10.0f) + -0.707000017f);
			fRec4[0] = (fSlow0 + (0.999000013f * fRec4[1]));
			float fTemp0 = std::tan((CONST0 * std::pow(10.0f, ((3.0f * fRec4[0]) + 1.0f))));
			float fTemp1 = ((float(input_1[i]) - fRec3[1]) * fTemp0);
			float fTemp2 = (fTemp0 + 1.0f);
			float fTemp3 = (1.0f - (fTemp0 / fTemp2));
			float fTemp4 = ((fTemp0 * ((((fRec3[1] + ((fTemp1 + (fSlow1 * (fRec1[1] * fTemp3))) / fTemp2)) + (fRec2[1] * (0.0f - (1.0f / fTemp2)))) / (1.0f - (fSlow1 * ((fTemp0 * fTemp3) / fTemp2)))) - fRec1[1])) / fTemp2);
			float fTemp5 = (fRec1[1] + fTemp4);
			float fRec0 = fTemp5;
			fRec1[0] = (fRec1[1] + (2.0f * fTemp4));
			fRec2[0] = (fRec2[1] + (2.0f * ((fTemp0 * ((fSlow1 * fTemp5) - fRec2[1])) / fTemp2)));
			fRec3[0] = (fRec3[1] + (2.0f * (fTemp1 / fTemp2)));
			output_1[i] = float(fRec0);
			fRec4[1] = fRec4[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			
		}
	}
    else if (amp->filter_type == MOOG_LADDER)
    {
		for (uint32_t i = 0; (i < n_samples); i = (i + 1)) {
            float fSlow0 = (0.00100000005f * clamp(float(cutoff_param)+cutoff_cv[i], 0.0f, 1.0f));
            float fSlow1 = (0.0411641225f * clamp(float(q_param)+(q_cv[i]*25.0f), 0.7072, 25.0f) + -0.707000017f);

			fRec5[0] = (fSlow0 + (0.999000013f * fRec5[1]));
			float fTemp0 = std::tan((CONST0 * std::pow(10.0f, ((3.0f * fRec5[0]) + 1.0f))));
			float fTemp1 = (3.9000001f - (0.899999976f * std::pow(fRec5[0], 0.200000003f)));
			float fTemp2 = (fTemp0 + 1.0f);
			float fTemp3 = ((fTemp0 * (((float(input_1[i]) - (fSlow1 * (fTemp1 * (((fRec1[1] + (fRec2[1] * fTemp0)) + (faustpower2_f(fTemp0) * fRec3[1])) + (faustpower3_f(fTemp0) * fRec4[1]))))) / ((fSlow1 * (fTemp1 * faustpower4_f(fTemp0))) + 1.0f)) - fRec4[1])) / fTemp2);
			float fTemp4 = ((fTemp0 * ((fRec4[1] + fTemp3) - fRec3[1])) / fTemp2);
			float fTemp5 = ((fTemp0 * ((fRec3[1] + fTemp4) - fRec2[1])) / fTemp2);
			float fTemp6 = ((fTemp0 * ((fRec2[1] + fTemp5) - fRec1[1])) / fTemp2);
			float fRec0 = (fRec1[1] + fTemp6);
			fRec1[0] = (fRec1[1] + (2.0f * fTemp6));
			fRec2[0] = (fRec2[1] + (2.0f * fTemp5));
			fRec3[0] = (fRec3[1] + (2.0f * fTemp4));
			fRec4[0] = (fRec4[1] + (2.0f * fTemp3));
			output_1[i] = float(fRec0);
			fRec5[1] = fRec5[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
		}
	}
    else if (amp->filter_type == MOOG_HALF_LADDER)
    {
		for (uint32_t i = 0; (i < n_samples); i = (i + 1)) {
            float fSlow0 = (0.00100000005f * clamp(float(cutoff_param)+cutoff_cv[i], 0.0f, 1.0f));
            float fSlow1 = (clamp(float(q_param)+(q_cv[i]*25.0f), 0.7072, 25.0f) + -0.707000017f);
            float fSlow2 = (0.082328245f * fSlow1);

			fRec4[0] = (fSlow0 + (0.999000013f * fRec4[1]));
			float fTemp0 = std::tan((CONST0 * std::pow(10.0f, ((3.0f * fRec4[0]) + 1.0f))));
			float fTemp1 = (fTemp0 + 1.0f);
			float fTemp2 = ((2.0f * (fTemp0 / fTemp1)) + -1.0f);
			float fTemp3 = ((fTemp0 * (((float(input_1[i]) + (fSlow1 * (((0.0f - ((0.16465649f * fRec1[1]) + (0.082328245f * (fRec2[1] * fTemp2)))) - (0.082328245f * (((fTemp0 * fTemp2) * fRec3[1]) / fTemp1))) / fTemp1))) / ((fSlow2 * ((faustpower2_f(fTemp0) * fTemp2) / faustpower2_f(fTemp1))) + 1.0f)) - fRec3[1])) / fTemp1);
			float fTemp4 = ((fTemp0 * ((fRec3[1] + fTemp3) - fRec2[1])) / fTemp1);
			float fTemp5 = (fRec2[1] + fTemp4);
			float fTemp6 = ((fTemp0 * (fTemp5 - fRec1[1])) / fTemp1);
			float fRec0 = ((2.0f * (fRec1[1] + fTemp6)) - fTemp5);
			fRec1[0] = (fRec1[1] + (2.0f * fTemp6));
			fRec2[0] = (fRec2[1] + (2.0f * fTemp4));
			fRec3[0] = (fRec3[1] + (2.0f * fTemp3));
			output_1[i] = float(fRec0);
			fRec4[1] = fRec4[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			
		}
	}
    else if (amp->filter_type == OBERHEIM)
    {
		for (uint32_t i = 0; (i < n_samples); i = (i + 1)) {
            float fSlow0 = (0.00100000005f * clamp(float(cutoff_param)+cutoff_cv[i], 0.0f, 1.0f));
            float fSlow1 = (1.0f / clamp(float(q_param)+(q_cv[i]*10.0f), 0.5, 10.0f));
			fRec6[0] = (fSlow0 + (0.999000013f * fRec6[1]));
			float fTemp0 = std::tan((CONST0 * std::pow(10.0f, ((3.0f * fRec6[0]) + 1.0f))));
			float fTemp1 = (fSlow1 + fTemp0);
			float fTemp2 = (float(input_1[i]) - (fRec4[1] + (fRec5[1] * fTemp1)));
			float fTemp3 = ((fTemp0 * fTemp1) + 1.0f);
			float fTemp4 = ((fTemp0 * fTemp2) / fTemp3);
			float fTemp5 = fmax(-1.0f, fmin(1.0f, (fRec5[1] + fTemp4)));
			float fTemp6 = (1.0f - (0.333333343f * faustpower2_f(fTemp5)));
			float fTemp7 = ((fTemp0 * fTemp5) * fTemp6);
			float fRec0 = (fRec4[1] + fTemp7);
			float fTemp8 = (fTemp2 / fTemp3);
			float fRec1 = fTemp8;
			float fTemp9 = (fTemp5 * fTemp6);
			float fRec2 = fTemp9;
			float fRec3 = (fTemp7 + (fRec4[1] + fTemp8));
			fRec4[0] = (fRec4[1] + (2.0f * fTemp7));
			fRec5[0] = (fTemp4 + fTemp9);
			output_bsf[i] = float(fRec3);
			output_bpf[i] = float(fRec2);
			output_hpf[i] = float(fRec1);
			output_1[i] = float(fRec0);
			fRec6[1] = fRec6[0];
			fRec4[1] = fRec4[0];
			fRec5[1] = fRec5[0];
		}
	}
}


/**
   The `deactivate()` method is the counterpart to `activate()`, and is called by
   the host after running the plugin.  It indicates that the host will not call
   `run()` again until another call to `activate()` and is mainly useful for more
   advanced plugins with ``live'' characteristics such as those with auxiliary
   processing threads.  As with `activate()`, this plugin has no use for this
   information so this method does nothing.
   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void
deactivate(LV2_Handle instance)
{
}

/**
   Destroy a plugin instance (counterpart to `instantiate()`).
   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void
cleanup(LV2_Handle instance)
{
	Filter* const amp = (Filter*)instance;
	free(instance);
}

/**
   The `extension_data()` function returns any extension data supported by the
   plugin.  Note that this is not an instance method, but a function on the
   plugin descriptor.  It is usually used by plugins to implement additional
   interfaces.  This plugin does not have any extension data, so this function
   returns NULL.
   This method is in the ``discovery'' threading class, so no other functions
   or methods in this plugin library will be called concurrently with it.
*/
static const void*
extension_data(const char* uri)
{
	return NULL;
}

/**
   Every plugin must define an `LV2_Descriptor`.  It is best to define
   descriptors statically to avoid leaking memory and non-portable shared
   library constructors and destructors to clean up properly.








*/

static const LV2_Descriptor descriptor0 = {
	FILTER_URI "diode_ladder",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor1 = {
	FILTER_URI "korg_hpf",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor2 = {
	FILTER_URI "korg_lpf",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor3 = {
	FILTER_URI "moog_ladder",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor4 = {
	FILTER_URI "moog_half_ladder",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor5 = {
	FILTER_URI "oberheim",
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};



/**
   The `lv2_descriptor()` function is the entry point to the plugin library.  The
   host will load the library and call this function repeatedly with increasing
   indices to find all the plugins defined in the library.  The index is not an
   indentifier, the URI of the returned descriptor is used to determine the
   identify of the plugin.
   This method is in the ``discovery'' threading class, so no other functions
   or methods in this plugin library will be called concurrently with it.
*/
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor0;
		case 1:
			return &descriptor1;
		case 2:
			return &descriptor2;
		case 3:
			return &descriptor3;
		case 4:
			return &descriptor4;
		case 5:
			return &descriptor5;
		default:
			return NULL;
	}
}

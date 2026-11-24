// Entry points for the WebAssembly build, for callers that work in plain numbers: JavaScript can't fill in an
// hippel_settings without knowing how the compiler laid it out. The rest of the API is exported as it is.
#include "hippelreplay.h"

static hippel_settings make_settings(int sample_rate, int stereo_separation, int interpolation, int volume_ramping, int loop_count,
                                   int amiga_model, int led_filter) {
	hippel_settings settings;
	hippel_default_settings(&settings);
	settings.sample_rate = sample_rate;
	settings.stereo_separation = stereo_separation;
	settings.interpolation = static_cast<hippel_interpolation>(interpolation);
	settings.volume_ramping = volume_ramping;
	settings.loop_count = loop_count;
	settings.amiga_model = static_cast<hippel_amiga_model>(amiga_model);
	settings.led_filter = static_cast<hippel_led_filter>(led_filter);
	return settings;
}

extern "C" {

// Opens a song at this rate with the default settings, but without the Amiga output filters (as libopenmpt plays
// without its Amiga emulation). Returns null if it can't.
hippel_module *hippel_wasm_open(const void *data, size_t size, int sample_rate) {
	const hippel_settings settings = make_settings(sample_rate, 100, HIPPEL_INTERPOLATION_NONE, 2000, 0, HIPPEL_AMIGA_NONE, HIPPEL_LED_OFF);
	return hippel_open(data, size, &settings, nullptr);
}

// Changes the settings of an open song: amiga_model is an hippel_amiga_model, led_filter an hippel_led_filter. They take
// effect at once (the sample rate at the next subsong selection). Returns 0 on success.
int hippel_wasm_configure(hippel_module *module, int sample_rate, int stereo_separation, int interpolation, int volume_ramping, int loop_count,
                        int amiga_model, int led_filter) {
	const hippel_settings settings = make_settings(sample_rate, stereo_separation, interpolation, volume_ramping, loop_count, amiga_model, led_filter);
	return hippel_set_settings(module, &settings);
}

}

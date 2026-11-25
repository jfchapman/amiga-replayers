// Entry points for the WebAssembly build, for callers that work in plain numbers: JavaScript can't fill in an
// amigareplay_settings without knowing how the compiler laid it out. The rest of the API is exported as it is.
#include "amigareplay.h"

static amigareplay_settings make_settings(int sample_rate, int stereo_separation, int interpolation, int volume_ramping, int loop_count,
                                          int amiga_model, int led_filter) {
	amigareplay_settings settings;
	amigareplay_default_settings(&settings, sizeof settings);
	settings.sample_rate = sample_rate;
	settings.stereo_separation = stereo_separation;
	settings.interpolation = static_cast<amigareplay_interpolation>(interpolation);
	settings.volume_ramping = volume_ramping;
	settings.loop_count = loop_count;
	settings.amiga_model = static_cast<amigareplay_amiga_model>(amiga_model);
	settings.led_filter = static_cast<amigareplay_led_filter>(led_filter);
	return settings;
}

extern "C" {

// amigareplay_probe, of a file's data alone.
int amigareplay_wasm_probe(const void *data, size_t size) {
	const amigareplay_file file = {nullptr, data, size};
	return amigareplay_probe(&file);
}

// Opens a song at this rate with the default settings, but without the Amiga output filters (as libopenmpt plays
// without its Amiga emulation). Returns null if it can't. Each file is three 32-bit numbers: where its name is (or 0),
// where its data is, and its size.
amigareplay_module *amigareplay_wasm_open(const amigareplay_file *files, int count, int sample_rate) {
	const amigareplay_settings settings =
		make_settings(sample_rate, 100, AMIGAREPLAY_INTERPOLATION_NONE, 2000, 0, AMIGAREPLAY_AMIGA_NONE, AMIGAREPLAY_LED_OFF);
	return amigareplay_open(files, count, &settings, nullptr, nullptr);
}

// Changes the settings of an open song (amiga_model is an amigareplay_amiga_model, led_filter an
// amigareplay_led_filter), which take effect at once. Returns 0 on success.
int amigareplay_wasm_configure(amigareplay_module *module, int sample_rate, int stereo_separation, int interpolation, int volume_ramping,
                               int loop_count, int amiga_model, int led_filter) {
	const amigareplay_settings settings =
		make_settings(sample_rate, stereo_separation, interpolation, volume_ramping, loop_count, amiga_model, led_filter);
	return amigareplay_set_settings(module, &settings);
}

}

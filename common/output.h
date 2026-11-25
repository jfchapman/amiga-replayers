// What every replayer's Paula ends in: the Amiga's output filters, stereo separation and gain. Also the
// interpolation between samples.
#pragma once

namespace amiga {

const double kPaulaClock = 3546895.0;  // PAL colour clock, Hz

// Paula can't fetch samples any faster than this.
const int kMinPeriod = 113;

// The same values as each library's own enums.
enum class Interpolation { None, Linear, Cubic };
enum class Model { A500, A1200, None };

// Cubic (Catmull-Rom) through sm, s0, s1 and s2, at t from s0 to s1.
inline float cubic(float sm, float s0, float s1, float s2, float t) {
	const float a = -0.5f * sm + 1.5f * s0 - 1.5f * s1 + 0.5f * s2;
	const float b = sm - 2.5f * s0 + 2.0f * s1 - 0.5f * s2;
	const float d = -0.5f * sm + 0.5f * s1;
	return ((a * t + b) * t + d) * t + s0;
}

class Output {
public:
	void reset() { filter_[0] = filter_[1] = FilterState(); }
	void set(int sample_rate, Model model, int separation, float gain);
	void set_led(bool on) { led_ = on; }

	// Adds a frame, the sum of each side's voices, into out[0] and out[1].
	void add(float left, float right, float *out) {
		// The filters are on the two analogue outputs, so they come before the stereo separation.
		const float l = filter(0, left * base_), r = filter(1, right * base_);
		const float mix = 0.5f + 0.5f * sep_, cross = 0.5f - 0.5f * sep_;
		out[0] += l * mix + r * cross;
		out[1] += r * mix + l * cross;
	}

private:
	float filter(int side, float x);

	float sep_ = 1.0f;
	float base_ = 0.5f;  // two full-scale voices per side reach 1.0

	// The output filters, per side: the fixed one-pole low-pass, then the two-pole LED filter.
	double fixed_coef_ = 0;  // 0 = off
	bool led_ = false;
	double led_b0_ = 0, led_a1_ = 0, led_a2_ = 0;
	struct FilterState {
		double fixed = 0, led1 = 0, led2 = 0;
	};
	FilterState filter_[2];
};

}  // namespace amiga

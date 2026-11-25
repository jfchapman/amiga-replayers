#include "output.h"

#include <algorithm>
#include <cmath>

namespace amiga {

namespace {

const double kPi = 3.14159265358979323846;

// Output filter corners in Hz: the commonly quoted measurements of the machines.
const double kA500Corner = 4900;
const double kA1200Corner = 28867;
const double kLedCorner = 3275;
const double kLedQ = 0.7071;  // Butterworth

}  // namespace

void Output::set(int sample_rate, Model model, int separation, float gain) {
	// One pole, matched to the analogue filter.
	const double corner = model == Model::A500 ? kA500Corner : model == Model::A1200 ? kA1200Corner : 0;
	fixed_coef_ = corner > 0 ? 1 - std::exp(-2 * kPi * corner / sample_rate) : 0;
	// Two poles, by the bilinear transform with the corner prewarped.
	const double k = std::tan(kPi * kLedCorner / sample_rate);
	const double norm = 1 / (1 + k / kLedQ + k * k);
	led_b0_ = k * k * norm;
	led_a1_ = 2 * (k * k - 1) * norm;
	led_a2_ = (1 - k / kLedQ + k * k) * norm;

	sep_ = std::clamp(separation, 0, 100) / 100.0f;
	base_ = 0.5f * gain;
}

float Output::filter(int side, float x) {
	FilterState &f = filter_[side];
	double y = x;
	if (fixed_coef_ > 0) {
		f.fixed += fixed_coef_ * (y - f.fixed);
		y = f.fixed;
	}
	if (led_) {
		const double in = y;
		y = led_b0_ * in + f.led1;
		f.led1 = 2 * led_b0_ * in - led_a1_ * y + f.led2;
		f.led2 = led_b0_ * in - led_a2_ * y;
	}
	return float(y);
}

}  // namespace amiga

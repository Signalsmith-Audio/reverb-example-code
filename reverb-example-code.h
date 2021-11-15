#include "dsp/delay.h"

struct Passthrough {
	double processSample(double sample) {
		return sample;
	}
};

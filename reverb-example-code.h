#include "dsp/delay.h"

#include "./mix-matrix.h"

#include <cstdlib>

double randomInRange(double low, double high) {
	// There are better randoms than this, and you should use them instead 😛
	double unitRand = rand()/double(RAND_MAX);
	return low + unitRand*(high - low);
}

struct Passthrough {
	void configure(double) {}
	
	double process(double sample) {
		return sample;
	}
};

// Despite the mess, this is a simple delay class which rounds to the nearest sample.
using Delay = signalsmith::delay::Delay<double, signalsmith::delay::InterpolatorNearest>;

struct SingleChannelFeedback {
	double delayMs = 80;
	double decayGain = 0.85;

	int delaySamples;
	Delay delay;
	
	void configure(double sampleRate) {
		delaySamples = delayMs*0.001*sampleRate;
		delay.resize(delaySamples + 1);
		delay.reset(); // Start with all 0s
	}
	
	double process(double input) {
		double delayed = delay.read(delaySamples);
		
		double sum = input + delayed*decayGain;
		delay.write(sum);
		
		return delayed;
	}
};


template<int channels=8>
struct MultiChannelFeedback {
	using Array = std::array<double, channels>;

	double delayMsLow = 50, delayMsHigh = 150;
	double decayGain = 0.85;

	std::array<int, channels> delaySamples;
	std::array<Delay, channels> delays;
	
	void configure(double sampleRate) {
		double delaySamplesLow = delayMsLow*0.001*sampleRate;
		double delaySamplesHigh = delayMsHigh*0.001*sampleRate;
		for (int c = 0; c < channels; ++c) {
			delaySamples[c] = randomInRange(delaySamplesLow, delaySamplesHigh);
			delays[c].resize(delaySamples[c] + 1);
			delays[c].reset();
		}
	}
	
	Array process(Array input) {
		Array delayed;
		for (int c = 0; c < channels; ++c) {
			delayed[c] = delays[c].read(delaySamples[c]);
		}
		
		for (int c = 0; c < channels; ++c) {
			double sum = input[c] + delayed[c]*decayGain;
			delays[c].write(sum);
		}
		
		return delayed;
	}
};

template<int channels=8>
struct MultiChannelMixedFeedback {
	using Array = std::array<double, channels>;
	double delayMsLow = 100, delayMsHigh = 200;
	double decayGain = 0.85;

	std::array<int, channels> delaySamples;
	std::array<Delay, channels> delays;
	
	void configure(double sampleRate) {
		double delaySamplesLow = delayMsLow*0.001*sampleRate;
		double delaySamplesHigh = delayMsHigh*0.001*sampleRate;
		for (int c = 0; c < channels; ++c) {
			delaySamples[c] = randomInRange(delaySamplesLow, delaySamplesHigh);
			delays[c].resize(delaySamples[c] + 1);
			delays[c].reset();
		}
	}
	
	Array process(Array input) {
		Array delayed;
		for (int c = 0; c < channels; ++c) {
			delayed[c] = delays[c].read(delaySamples[c]);
		}
		
		// Mix using a Householder matrix
		Array mixed = delayed;
		Householder<double, channels>::inPlace(mixed.data());
		
		for (int c = 0; c < channels; ++c) {
			double sum = input[c] + mixed[c]*decayGain;
			delays[c].write(sum);
		}
		
		return delayed;
	}
};

template<int channels=8>
struct DiffusionStep {
	using Array = std::array<double, channels>;
	double delayMsRange = 50;
	
	DiffusionStep(double delayMsRange) : delayMsRange(delayMsRange) {}

	std::array<int, channels> delaySamples;
	std::array<Delay, channels> delays;
	std::array<bool, channels> flipPolarity;
	
	void configure(double sampleRate) {
		double delaySamplesRange = delayMsRange*0.001*sampleRate;
		for (int c = 0; c < channels; ++c) {
			delaySamples[c] = randomInRange(0, delaySamplesRange);
			delays[c].resize(delaySamples[c] + 1);
			delays[c].reset();
			flipPolarity[c] = rand()%2;
		}
	}
	
	Array process(Array input) {
		// Delay
		Array delayed;
		for (int c = 0; c < channels; ++c) {
			delays[c].write(input[c]);
			delayed[c] = delays[c].read(delaySamples[c]);
		}
		
		// Mix with a Hadamard matrix
		Array mixed = delayed;
		Hadamard<double, channels>::inPlace(mixed.data());

		// Flip some polarities
		for (int c = 0; c < channels; ++c) {
			if (flipPolarity[c]) mixed[c] *= -1;
		}

		return mixed;
	}
};

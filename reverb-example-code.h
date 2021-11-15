#include "dsp/delay.h"

#include "./mix-matrix.h"

#include <cstdlib>

double randomInRange(double low, double high) {
	// There are better randoms than this, and you should use them instead 😛
	double unitRand = rand()/double(RAND_MAX);
	return low + unitRand*(high - low);
}

// This is a simple delay class which rounds to a whole number of samples.
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

	double delayMs = 150;
	double decayGain = 0.85;

	std::array<int, channels> delaySamples;
	std::array<Delay, channels> delays;
	
	void configure(double sampleRate) {
		double delaySamplesBase = delayMs*0.001*sampleRate;
		for (int c = 0; c < channels; ++c) {
			// Distribute delay times exponentially between delayMs and 2*delayMs
			double r = c*1.0/channels;
			delaySamples[c] = std::pow(2, r)*delaySamplesBase;
			
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
	double delayMs = 150;
	double decayGain = 0.85;

	std::array<int, channels> delaySamples;
	std::array<Delay, channels> delays;
	
	void configure(double sampleRate) {
		double delaySamplesBase = delayMs*0.001*sampleRate;
		for (int c = 0; c < channels; ++c) {
			double r = c*1.0/channels;
			delaySamples[c] = std::pow(2, r)*delaySamplesBase;
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
	
	std::array<int, channels> delaySamples;
	std::array<Delay, channels> delays;
	std::array<bool, channels> flipPolarity;
	
	void configure(double sampleRate) {
		double delaySamplesRange = delayMsRange*0.001*sampleRate;
		for (int c = 0; c < channels; ++c) {
			double rangeLow = delaySamplesRange*c/channels;
			double rangeHigh = delaySamplesRange*(c + 1)/channels;
			delaySamples[c] = randomInRange(rangeLow, rangeHigh);
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

template<int channels=8, int stepCount=4>
struct DiffuserEqualLengths {
	using Array = std::array<double, channels>;

	using Step = DiffusionStep<channels>;
	std::array<Step, stepCount> steps;

	DiffuserEqualLengths(double totalDiffusionMs) {
		for (auto &step : steps) {
			step.delayMsRange = totalDiffusionMs/stepCount;
		}
	}
	
	void configure(double sampleRate) {
		for (auto &step : steps) step.configure(sampleRate);
	}
	
	Array process(Array samples) {
		for (auto &step : steps) {
			samples = step.process(samples);
		}
		return samples;
	}
};

template<int channels=8, int stepCount=4>
struct DiffuserHalfLengths {
	using Array = std::array<double, channels>;

	using Step = DiffusionStep<channels>;
	std::array<Step, stepCount> steps;

	DiffuserHalfLengths(double diffusionMs) {
		for (auto &step : steps) {
			diffusionMs *= 0.5;
			step.delayMsRange = diffusionMs;
		}
	}
	
	void configure(double sampleRate) {
		for (auto &step : steps) step.configure(sampleRate);
	}
	
	Array process(Array samples) {
		for (auto &step : steps) {
			samples = step.process(samples);
		}
		return samples;
	}
};

template<int channels=8, int diffusionSteps=4>
struct BasicReverb {
	using Array = std::array<double, channels>;
	
	MultiChannelMixedFeedback<channels> feedback;
	DiffuserHalfLengths<channels, diffusionSteps> diffuser;
	double dry, wet;

	BasicReverb(double roomSizeMs, double rt60, double dry=0, double wet=1) : diffuser(roomSizeMs), dry(dry), wet(wet) {
		feedback.delayMs = roomSizeMs;

		// How long does our signal take to go around the feedback loop?
		double typicalLoopMs = roomSizeMs*1.5;
		// How many times will it do that during our RT60 period?
		double loopsPerRt60 = rt60/(typicalLoopMs*0.001);
		// This tells us how many dB to reduce per loop
		double dbPerCycle = -60/loopsPerRt60;

		feedback.decayGain = std::pow(10, dbPerCycle*0.05);
	}
	
	void configure(double sampleRate) {
		feedback.configure(sampleRate);
		diffuser.configure(sampleRate);
	}
	
	Array process(Array input) {
		Array diffuse = diffuser.process(input);
		Array longLasting = feedback.process(diffuse);
		Array output;
		for (int c = 0; c < channels; ++c) {
			output[c] = dry*input[c] + wet*longLasting[c];
		}
		return output;
	}
};

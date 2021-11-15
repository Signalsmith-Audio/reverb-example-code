#include "reverb-example-code.h"

#include "./wav.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <array>

class ExampleRunner {
	Wav inputWav;
	std::string outputPrefix;

	template<class Processor>
	void monoProcessor(std::string name, Processor &&processor) {
		Wav outputWav;
		outputWav.sampleRate = inputWav.sampleRate;
		outputWav.channels = 1;
		
		srand(1);
		processor.configure(outputWav.sampleRate);
		
		outputWav.samples.resize(inputWav.samples.size());
		for (size_t i = 0; i < inputWav.samples.size(); ++i) {
			outputWav.samples[i] = processor.process(inputWav.samples[i]);
		}
		outputWav.write(outputPrefix + name + ".wav");
		if (!outputWav.result) {
			std::cerr << outputWav.result.reason << "\n";
			std::exit(1);
		}
	}

	template<int channels, class Processor>
	void multiProcessor(std::string name, Processor &&processor) {
		static_assert(channels%2 == 0, "there must be an even number of channels");

		Wav outputWav;
		outputWav.sampleRate = inputWav.sampleRate;
		outputWav.channels = 2;
		
		srand(1);
		processor.configure(outputWav.sampleRate);
		
		outputWav.samples.resize(inputWav.samples.size()*2);
		for (size_t i = 0; i < inputWav.samples.size(); ++i) {
			double value = inputWav.samples[i];

			// Duplicate mono input into multiple channels
			std::array<double, channels> array;
			for (int c = 0; c < channels; ++c) array[c] = value;
			
			array = processor.process(array);
			
			// Mix down into stereo for example output
			double left = 0, right = 0;
			for (int c = 0; c < channels; c += 2) {
				left += array[c];
				right += array[c + 1];
			}
			
			constexpr double scaling = 2.0/channels;
			outputWav.samples[2*i] = left*scaling;
			outputWav.samples[2*i + 1] = right*scaling;
		}
		outputWav.write(outputPrefix + name + ".wav");
		if (!outputWav.result) {
			std::cerr << outputWav.result.reason << "\n";
			std::exit(1);
		}
	}
public:
	ExampleRunner(std::string inputFile, std::string outputPrefix, double durationSeconds=0) : inputWav(inputFile), outputPrefix(outputPrefix) {
		if (!inputWav.result) {
			std::cerr << inputWav.result.reason << "\n";
			std::exit(1);
		}
		if (inputWav.channels != 1) {
			std::cerr << "Input WAV must be mono.\n";
			std::exit(1);
		}
		
		// Pad if needed
		int durationSamples = durationSeconds*inputWav.sampleRate;
		if ((int)inputWav.samples.size() < durationSamples) {
			inputWav.samples.resize(durationSamples, 0);
		}
	}

	void runExamples() {
		monoProcessor("passthrough", Passthrough());
		monoProcessor("single-channel-feedback", SingleChannelFeedback());
		multiProcessor<4>("multi-channel-feedback-4", MultiChannelFeedback<4>());
		multiProcessor<8>("multi-channel-feedback-8", MultiChannelFeedback<8>());
		multiProcessor<4>("multi-channel-feedback-householder-4", MultiChannelMixedFeedback<4>());
		multiProcessor<8>("multi-channel-feedback-householder-8", MultiChannelMixedFeedback<8>());
	}
};

int main(int argc, const char **argv) {
	if (argc < 3) {
		std::cerr << "Missing: input.wav output-prefix-\n";
		return 1;
	}
	ExampleRunner runner(argv[1], argv[2], 3);
	runner.runExamples();
}

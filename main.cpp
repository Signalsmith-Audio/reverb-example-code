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
		
		int durationSamples = inputWav.samples.size()/inputWav.channels;
		outputWav.samples.resize(durationSamples);
		for (int i = 0; i < durationSamples; ++i) {
			outputWav.samples[i] = processor.process(inputWav.samples[i*inputWav.channels]);
		}
		outputWav.write(outputPrefix + name + ".wav");
		if (!outputWav.result) {
			std::cerr << outputWav.result.reason << "\n";
			std::exit(1);
		}
	}

	template<int channels, bool averageOutputs=true, class Processor>
	void multiProcessor(std::string name, Processor &&processor) {
		static_assert(channels%2 == 0, "there must be an even number of channels");

		Wav outputWav;
		outputWav.sampleRate = inputWav.sampleRate;
		outputWav.channels = 2;
		
		srand(1);
		processor.configure(outputWav.sampleRate);
		
		int durationSamples = inputWav.samples.size()/inputWav.channels;
		outputWav.samples.resize(durationSamples*2);
		for (int i = 0; i < durationSamples; ++i) {

			// Duplicate input channels as many times as needed
			std::array<double, channels> array;
			for (int c = 0; c < channels; ++c) {
				int inputChannel = c%inputWav.channels;
				array[c] = inputWav.samples[i*inputWav.channels + inputChannel];
			}
			
			array = processor.process(array);
			
			if (averageOutputs) {
				// Mix down into stereo for example output
				double left = 0, right = 0;
				for (int c = 0; c < channels; c += 2) {
					left += array[c];
					right += array[c + 1];
				}
				
				constexpr double scaling = 2.0/channels;
				outputWav.samples[2*i] = left*scaling;
				outputWav.samples[2*i + 1] = right*scaling;
			} else {
				outputWav.samples[2*i] = array[0];
				outputWav.samples[2*i + 1] = array[1];
			}
		}
		outputWav.write(outputPrefix + name + ".wav");
		if (!outputWav.result) {
			std::cerr << outputWav.result.reason << "\n";
			std::exit(1);
		}
	}
	
	void extendDuration(double seconds) {
		int extraSamples = seconds*inputWav.sampleRate;
		inputWav.samples.resize(inputWav.samples.size() + extraSamples, 0);
	}
public:
	ExampleRunner(std::string inputFile, std::string outputPrefix) : inputWav(inputFile), outputPrefix(outputPrefix) {
		if (!inputWav.result) {
			std::cerr << inputWav.result.reason << "\n";
			std::exit(1);
		}
	}

	void runExamples() {
		extendDuration(3);
		monoProcessor("single-channel-feedback", SingleChannelFeedback());
		multiProcessor<4>("multi-channel-feedback-4", MultiChannelFeedback<4>());
		multiProcessor<8>("multi-channel-feedback-8", MultiChannelFeedback<8>());
		multiProcessor<4>("multi-channel-feedback-householder-4", MultiChannelMixedFeedback<4>());
		multiProcessor<8>("multi-channel-feedback-householder-8", MultiChannelMixedFeedback<8>());
		multiProcessor<4, false>("diffuser-equal-4-3", DiffuserHalfLengths<4, 3>(200));
		multiProcessor<8, false>("diffuser-equal-8-4", DiffuserHalfLengths<8, 4>(200));
		multiProcessor<8, false>("diffuser-equal-8-6-long", DiffuserEqualLengths<8, 6>(3000));
		multiProcessor<8, false>("diffuser-halves-8-6-long", DiffuserHalfLengths<8, 6>(3000));
		extendDuration(1);
		multiProcessor<8, true>("reverb-basic-8-short", BasicReverb<8, 4>(50, 2.5));
		extendDuration(2);
		multiProcessor<8, true>("reverb-basic-8-mix", BasicReverb<8, 4>(100, 3, 1, 0.25));
		extendDuration(2);
		multiProcessor<8, true>("reverb-basic-8-long", BasicReverb<8, 4>(100, 10));
	}
};

int main(int argc, const char **argv) {
	if (argc < 3) {
		std::cerr << "Missing: input.wav output-prefix-\n";
		return 1;
	}
	ExampleRunner runner(argv[1], argv[2]);
	runner.runExamples();
}

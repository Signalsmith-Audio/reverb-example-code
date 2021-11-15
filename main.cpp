#include "reverb-example-code.h"

#include "./wav.h"

#include <iostream>
#include <string>
#include <cstdlib>

class ExampleRunner {
	Wav inputWav;
	std::string outputPrefix;

	template<class Processor>
	void monoProcessor(std::string name, Processor &&processor) {
		Wav outputWav;
		outputWav.sampleRate = inputWav.sampleRate;
		outputWav.channels = 1;
		
		outputWav.samples.resize(inputWav.samples.size());
		for (size_t i = 0; i < inputWav.samples.size(); ++i) {
			outputWav.samples[i] = processor.processSample(inputWav.samples[i]);
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
	}
};

int main(int argc, const char **argv) {
	if (argc < 3) {
		std::cerr << "Missing: input.wav output-prefix-\n";
		return 1;
	}
	ExampleRunner runner(argv[1], argv[2], 5);
	runner.runExamples();
}

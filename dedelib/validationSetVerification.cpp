#include "flatPCoeffProcessing.h"

#include "supercomputerJobs.h"
#include "bottomBufferCreator.h"

/*static void runValidationBottomBufferCreator(
	unsigned int Variables,
	PCoeffProcessingContext& context
) {
	constexpr size_t BOTTOM_BUF_CREATOR_COUNT = 16;
	std::atomic<const JobTopInfo*> jobTopAtomic;
	context.topsAreReady.wait();
	jobTopAtomic.store(&context.tops[0]);
	const JobTopInfo* jobTopEnd = jobTopAtomic.load() + context.tops.size();

	struct ThreadInfo {
		unsigned int Variables;
		std::atomic<const JobTopInfo*>* curStartingJobTop;
		const JobTopInfo* jobTopsEnd;
		PCoeffProcessingContext* context;
		size_t coreComplex;
	};

	auto threadFunc = [](void* data) -> void* {
		ThreadInfo* ti = (ThreadInfo*) data;
		std::string threadName = "BotBufCrea " + std::to_string(ti->coreComplex);
		setThreadName(threadName.c_str());
		runBottomBufferCreatorNoAlloc(ti->Variables, *ti->curStartingJobTop, ti->jobTopsEnd, *ti->context, ti->coreComplex, *ti->links);
		pthread_exit(NULL);
		return NULL;
	};

	ThreadInfo threadDatas[BOTTOM_BUF_CREATOR_COUNT];
	for(size_t coreComplex = 0; coreComplex < BOTTOM_BUF_CREATOR_COUNT; coreComplex++) {
		size_t socket = coreComplex / 8;
		ThreadInfo& ti = threadDatas[coreComplex];
		ti.Variables = Variables;
		ti.curStartingJobTop = &jobTopAtomic;
		ti.jobTopsEnd = jobTopEnd;
		ti.context = &context;
		ti.coreComplex = coreComplex;
	}

	PThreadBundle threads = spreadThreads(BOTTOM_BUF_CREATOR_COUNT, CPUAffinityType::COMPLEX, threadDatas, threadFunc, 1);

	std::cout << "\033[33m[BottomBufferCreator] Copied Links to second socket buffer\033[39m\n" << std::flush;

	threads.join();

	std::cout << "\033[33m[BottomBufferCreator] All Threads finished! Closing output queue\033[39m\n" << std::flush;

	context.inputQueue.close();
}



std::function<std::vector<JobTopInfo>()> loadCheckTopsFromValidationFile(const char* fileName) {
	return [fileName]() -> std::vector<JobTopInfo> {
		ValidationFileData vData;
		vData.readFromFile(fileName);
		// todo
	};
}

void validationBottomBufferCreator(unsigned int Variables, PCoeffProcessingContext& context) {
	
}

void validationBufferValidationPipeline(unsigned int Variables, const std::function<std::vector<JobTopInfo>()>& validationTopLoader, void(*processor)(PCoeffProcessingContext&)) {
	std::atomic<bool> hadError;
	hadError.store(false);
	auto errorBufFunc = [&](const OutputBuffer& outBuf, const char* moduleThatFoundError, bool recoverable) {
		hadError = true;

		std::cerr << "Error in buffer for top: " + std::to_string(outBuf.originalInputData.getTop()) + "\n" << std::flush;
	};

	pcoeffPipeline(Variables, validationTopLoader, processor, nullptr, errorBufFunc, );

	if(hadError.load()) {
		std::cerr << "Errors Occured, Exiting erroneously!\n" << std::endl;
		exit(1);
	}
}*/


#include "supercomputerJobs.h"

#include <filesystem>
#include <random>
#include <algorithm>

#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "crossPlatformIntrinsics.h"
#include "pcoeffValidator.h"

void checkFileDone(std::ifstream& file) {
	if(file.peek() != EOF) {
		std::cerr << "End of file not reached. Invalid file!" << std::endl;
		std::abort();
	}
}

static std::string getComputeIdentifier() {
	char hostNameData[HOST_NAME_MAX+1];
	gethostname(hostNameData, HOST_NAME_MAX+1);
	return std::string(hostNameData);
}

static std::string computeFileName(const std::string& jobID, const std::string& extention) {
	std::string result = "j" + jobID + extention;
	return result;
}

static std::string computeFolderPath(std::string computeFolder, const char* folder) {
	if(computeFolder[computeFolder.size() - 1] != '/') computeFolder.append("/");
	computeFolder.append(folder);
	computeFolder.append("/");
	return computeFolder;
}

static std::string computeFilePath(std::string computeFolder, const char* folder, const std::string& jobID, const std::string& extention) {
	return computeFolderPath(computeFolder, folder) + computeFileName(jobID, extention);
}

static std::string getValidationFilePath(std::string computeFolder, const std::string& computeID) {
	return computeFolderPath(computeFolder, "validation") + "v" + computeID + ".validation";
}




size_t getValidationBufByteSize(unsigned int Variables) {
	return sizeof(ValidationData) * VALIDATION_BUFFER_SIZE(Variables);
}
size_t getTopBitsetByteSize(unsigned int Variables) {
	return (mbfCounts[Variables] + 7) / 8;
}

struct ValidationFileData{
	unsigned int Variables;
	std::unique_ptr<ValidationData[]> savedValidationBuffer;
	std::unique_ptr<uint8_t[]> savedTopsBitset;

	ValidationFileData() = default;
	ValidationFileData(unsigned int Variables) : 
		Variables(Variables),
		savedValidationBuffer(new ValidationData[VALIDATION_BUFFER_SIZE(Variables)]),
		savedTopsBitset(new uint8_t[getTopBitsetByteSize(Variables)]) {}

	void initializeZero() {
		memset(savedValidationBuffer.get(), 0, getValidationBufByteSize(Variables));
		memset(savedTopsBitset.get(), 0, getTopBitsetByteSize(Variables));
	}

	void readFromFile(FILE* validationFile) {
		size_t bufferSizeInBytes = getValidationBufByteSize(Variables);
		size_t savedTopsBitsetBytes = getTopBitsetByteSize(Variables);
		size_t allValidationRead = fread(static_cast<void*>(savedValidationBuffer.get()), 1, bufferSizeInBytes, validationFile);
		if(allValidationRead != bufferSizeInBytes) {
			perror("ERROR");
			std::cerr << "Validation file is too short in validation part! Aborting!\n" << std::flush;
			std::abort();
		}
		size_t allBitsetRead = fread(static_cast<void*>(savedTopsBitset.get()), 1, savedTopsBitsetBytes, validationFile);
		if(allBitsetRead != savedTopsBitsetBytes) {
			perror("ERROR");
			std::cerr << "Validation file is too short in top bitset part! Aborting!\n" << std::flush;
			std::abort();
		}
	}
	void writeToFile(FILE* validationFile) const {
		size_t bufferSizeInBytes = getValidationBufByteSize(Variables);
		size_t savedTopsBitsetBytes = getTopBitsetByteSize(Variables);
		size_t allValidationWritten = fwrite(static_cast<const void*>(savedValidationBuffer.get()), 1, bufferSizeInBytes, validationFile);
		if(allValidationWritten != bufferSizeInBytes) {
			perror("ERROR");
			std::cerr << "Could not write full validation for some reason! Aborting!\n" << std::flush;
			std::abort();
		}
		size_t allBitsetWritten = fwrite(static_cast<const void*>(savedTopsBitset.get()), 1, savedTopsBitsetBytes, validationFile);
		if(allBitsetWritten != savedTopsBitsetBytes) {
			perror("ERROR");
			std::cerr << "Could not write full bitset for some reason! Aborting!\n" << std::flush;
			std::abort();
		}
	}

	bool isTopPresent(NodeIndex topIdx) const {
		if(topIdx < mbfCounts[Variables]) {
			uint8_t topByte = savedTopsBitset[topIdx / 8];
			uint8_t topBitMask = 0b00000001 << (topIdx % 8);
			return (topByte & topBitMask) != 0;
		} else {
			std::cerr << "Error: attempting to query top out of range for this ValidationFile! Aborting!\n" << std::flush;
			std::abort();
		}
	}

	void checkHasAllTops() const {
		bool fault = false;

		size_t leftoverTopBits = mbfCounts[Variables] % 8;
		size_t fullTopBytes = (mbfCounts[Variables] - leftoverTopBits) / 8;
		for(size_t i = 0; i < fullTopBytes; i++) {
			uint8_t topsByte = this->savedTopsBitset[i];
			while(topsByte != 0b11111111) {
				int missingOne = ctz8(~topsByte);
				std::cerr << "Error: Missing top " + std::to_string(i * 8 + missingOne) + " again! Aborting!\n" << std::flush;
				topsByte |= (1 << missingOne);
				fault = true;
			}
		}

		for(size_t finalBit = 0; finalBit < leftoverTopBits; finalBit++) {
			NodeIndex finalTopIdx = fullTopBytes * 8 + finalBit;
			if(!this->isTopPresent(finalTopIdx)) {
				std::cerr << "Error: Missing top " + std::to_string(finalTopIdx) + " again! Aborting!\n" << std::flush;
				fault = true;
			}
		}

		if(fault) {
			std::abort();
		}
	}

	void addTop(NodeIndex topIdx) {
		if(topIdx < mbfCounts[Variables]) {
			uint8_t& topByte = savedTopsBitset[topIdx / 8];
			uint8_t topBitMask = 0b00000001 << (topIdx % 8);
			if((topByte & topBitMask) == 0) {
				topByte |= topBitMask;
			} else {
				std::cerr << "Error: Attempting to readd the validation results for top " + std::to_string(topIdx) + " again! Aborting!\n" << std::flush;
				std::abort();
			}
		} else {
			std::cerr << "Error: attempting to query top out of range for this ValidationFile! Aborting!\n" << std::flush;
			std::abort();
		}
	}

	// Returns the number of tops added
	size_t mergeIntoThis(const ValidationFileData& other) {
		assert(this->Variables == other.Variables);
		
		std::cout << "Checking for top duplication...\n" << std::flush;
		bool fault = false;
		size_t totalTopsInOther = 0;
		for(size_t i = 0; i < getTopBitsetByteSize(Variables); i++) {
			uint8_t& thisTopBits = this->savedTopsBitset[i];
			uint8_t otherTopBits = other.savedTopsBitset[i];
			totalTopsInOther += popcnt8(otherTopBits);

			uint8_t duplicateTops = thisTopBits & otherTopBits;
			thisTopBits |= otherTopBits;

			while(duplicateTops != 0) {
				int duplicateTopIdx = ctz8(duplicateTops);
				duplicateTops &= ~(0x00000001 << duplicateTopIdx);
				std::cerr << "ERROR: Duplicate top in both validation files! Top " + std::to_string(i * 8 + duplicateTopIdx) + " is duplicated!\n" << std::flush;
				fault = true;
			}
		}

		if(fault) {
			std::cerr << "ERROR: Duplicate tops found! Aborting!\n" << std::flush;
			std::abort();
		}

		std::cout << "No duplicate tops found!\nAdding validation data...\n" << std::flush;
		for(size_t i = 0; i < VALIDATION_BUFFER_SIZE(Variables); i++) {
			this->savedValidationBuffer[i].dualBetaSum += other.savedValidationBuffer[i].dualBetaSum;
		}
		return totalTopsInOther;
	}

	std::unique_ptr<ValidationData[]> finalize() && {
		this->checkHasAllTops();

		return std::move(this->savedValidationBuffer);
	}
};

void checkValidationFileExists(const std::string& computeFolder, const std::string& computeID) {
	std::string validationFileName = getValidationFilePath(computeFolder, computeID);

	if(!std::filesystem::exists(validationFileName)) {
		std::cerr << "Validation file " + validationFileName + " does not exist! Aborting!\n" << std::flush;
		std::abort();
	} else {
		std::cout << "Confirmed Validation file " + validationFileName + " found!\n" << std::flush;
	}
}






static void initializeValidationFile(const ValidationFileData& initialFileData, const std::string& validationFileName) {
	if(std::filesystem::exists(validationFileName)) {
		std::cerr << "Validation file already exists! Abort!\n" << std::flush;
		std::abort();
	}
	FILE* validationFile = fopen(validationFileName.c_str(), "wb");

	initialFileData.writeToFile(validationFile);

	if(fclose(validationFile) != 0) {
		perror("ERROR");
		std::cerr << "Failed to close Validation File! Abort!\n" << std::flush;
		std::abort();
	}
}

void initializeValidationFiles(unsigned int Variables, std::string computeFolder, const std::vector<std::string>& computeIDs) {
	std::cout << "Generating initial validation file Dedekind Number D(" << (Variables + 2) << "). \nPath: " << computeFolder << std::endl;

	if(!std::filesystem::exists(computeFolderPath(computeFolder, "validation"))) {
		std::cerr << "The Project folder " + computeFolder + "does not yet exist! You must create it first!\n" << std::flush;
		std::abort();
	}

	ValidationFileData initialData(Variables);
	initialData.initializeZero();

	std::cout << "Validation files:\n";
	for(const std::string& computeID : computeIDs) {
		std::string validationFileName = getValidationFilePath(computeFolder, computeID);

		if(std::filesystem::exists(validationFileName)) {
			std::cerr << "Validation file already exists! Abort!\n";
			std::abort();
		}
	}

	for(const std::string& computeID : computeIDs) {
		std::cout << "Initialize " + computeID + "\n" << std::flush;
		initializeValidationFile(initialData, getValidationFilePath(computeFolder, computeID));
	}
	std::cout << "Finished generating all validation files!\n" << std::flush;
}

static void writeJobToFile(unsigned int Variables, const std::string& jobFileName, const std::vector<JobTopInfo>& topVector) {
	std::ofstream jobFile(jobFileName, std::ios::binary);

	serializeU32(Variables, jobFile);
	serializePODVector(topVector, jobFile);
}

static void addJobTop(std::vector<JobTopInfo>& jobVector, NodeIndex newTop, const FlatNode* flatNodes) {
	JobTopInfo newVal;
	newVal.top = newTop;
	newVal.topDual = flatNodes[newTop].dual;
	jobVector.push_back(std::move(newVal));
}

void initializeComputeProject(unsigned int Variables, std::string computeFolder, size_t numberOfJobs, size_t topsPerBatch) {
	std::cout << "Generating a supercomputing project for computing Dedekind Number D(" << (Variables + 2) << "). Split across " << numberOfJobs << " jobs. \nPath: " << computeFolder << std::endl;
	
	if(std::filesystem::exists(computeFolder)) {
		std::cerr << "The compute path already exists! (" << computeFolder << ") Choose a different compute folder!\nTermintating to avoid data loss. " << std::endl;
		std::abort();
	}

	std::cout << "Generating directories: " << computeFolder << "/\n    jobs/\n    working/\n    finished/\n    results/\n    logs/\n    validation/\n" << std::flush;
	std::filesystem::create_directory(computeFolder);
	std::filesystem::create_directory(computeFolderPath(computeFolder, "jobs"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "working"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "finished"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "results"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "logs"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "validation"));

	size_t numberOfMBFsToProcess = mbfCounts[Variables];
	size_t numberOfCompleteBatches = numberOfMBFsToProcess / topsPerBatch;

	std::cout << "Collecting the " << numberOfMBFsToProcess << " MBFs for the full computation..." << std::endl;

	std::vector<size_t> completeBatchIndices;
	for(size_t i = 0; i < numberOfCompleteBatches; i++) {
		completeBatchIndices.push_back(i);
	}

	std::default_random_engine generator;
	std::shuffle(completeBatchIndices.begin(), completeBatchIndices.end(), generator);

	std::cout << "Generating " << numberOfJobs << " job files of ~" << (numberOfMBFsToProcess / numberOfJobs) << " MBFs per job..." << std::endl;

	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);

	size_t currentBatchIndex = 0;
	for(size_t jobI = 0; jobI < numberOfJobs; jobI++) {
		size_t jobsLeft = numberOfJobs - jobI;
		size_t topBatchesLeft = numberOfCompleteBatches - currentBatchIndex;
		size_t numberOfBatchesInThisJob = topBatchesLeft / jobsLeft;

		std::vector<JobTopInfo> topIndices;
		topIndices.reserve(numberOfBatchesInThisJob * topsPerBatch);
		for(size_t i = 0; i < numberOfBatchesInThisJob; i++) {
			std::uint32_t curBatchStart = completeBatchIndices[currentBatchIndex++] * topsPerBatch;
			for(size_t j = 0; j < topsPerBatch; j++) {
				addJobTop(topIndices, curBatchStart + j, flatNodes);
			}
		}
		// Add remaining tops to job 0
		if(jobI == 0) {
			size_t firstUnbatchedTop = numberOfCompleteBatches * topsPerBatch;
			for(size_t topI = firstUnbatchedTop; topI < numberOfMBFsToProcess; topI++) {
				addJobTop(topIndices, topI, flatNodes);
			}
		}

		writeJobToFile(Variables, computeFilePath(computeFolder, "jobs", std::to_string(jobI), ".job"), topIndices);
	}

	std::cout << "Generated " << numberOfJobs << " job files of ~" << (numberOfMBFsToProcess / numberOfJobs) << " MBFs per job." << std::endl;
}

static std::vector<JobTopInfo> loadJob(unsigned int Variables, const std::string& computeFolder, const std::string& computeID, const std::string& jobID) {
	std::string jobFile = computeFilePath(computeFolder, "jobs", jobID, ".job");
	std::string workingFile = computeFilePath(computeFolder, "working", jobID, "_" + computeID + ".job");
	std::string resultsFile = computeFilePath(computeFolder, "results", jobID, "_" + computeID + ".results");
	
	if(std::filesystem::exists(workingFile)) {
		std::cerr << "Temporary file (" << workingFile << ") already exists! Job " << jobID << " is already running!";
		std::abort();
	}
	if(std::filesystem::exists(resultsFile)) {
		std::cerr << "Results file (" << resultsFile << ") already exists! Job " << jobID << " has already terminated!";
		std::abort();
	}
	std::filesystem::rename(jobFile, workingFile); // Throws filesystem::filesystem_error on error
	std::cout << "Moved job to working directory: " << jobFile << "=>" << workingFile << std::endl;

	std::ifstream jobIn(workingFile, std::ios::binary);

	unsigned int fileVariables = deserializeU32(jobIn);
	if(Variables != fileVariables) {
		std::cerr << "File for incorrect Dedekind Target! Specified Target: D(" + std::to_string(Variables + 2) + "), target from file: D(" + std::to_string(fileVariables + 2) + ")" << std::endl;
		std::abort();
	}

	std::vector<JobTopInfo> topIndices = deserializePODVector<JobTopInfo>(jobIn);
	checkFileDone(jobIn);

	return topIndices;
}

// Updates the validation file atomically
static void addValidationDataToFile(unsigned int Variables, const std::string& computeFolder, const std::string& computeID, const ResultProcessorOutput& computationResults) {
	std::string validationFileName = getValidationFilePath(computeFolder, computeID);

	ValidationFileData fileData(Variables);

	FILE* validationFile = fopen(validationFileName.c_str(), "rb+");
	if(validationFile == NULL) {
		std::cerr << "Validation file " + validationFileName + " does not exist, yet it existed at the start! Aborting!\n" << std::flush;
		std::abort();
	}
	std::cout << "Reading Validation file " + validationFileName + "\n" << std::flush;
	fileData.readFromFile(validationFile);
	std::cout << "Validation file " + validationFileName + " read properly\nAdding new information to buffer...\n" << std::flush;

	for(BetaResult result : computationResults.results) {
		fileData.addTop(result.topIndex);
	}
	for(size_t i = 0; i < VALIDATION_BUFFER_SIZE(Variables); i++) {
		fileData.savedValidationBuffer[i].dualBetaSum += computationResults.validationBuffer[i].dualBetaSum;
	}

	rewind(validationFile); // reset to start of file

	std::cout << "Buffer updated\nWriting Validation file " + validationFileName + "\n" << std::flush;
	fileData.writeToFile(validationFile);
	std::cout << "Validation file " + validationFileName + " written properly\n" << std::flush;

	fclose(validationFile);
	std::cout << "Validation file " + validationFileName + " closed.\n" << std::flush;
}

struct __attribute((packed)) PackedResultData {
	uint32_t nodeIndex;
	u128 brA;
	u128 brB;
	uint64_t szDownA;
	uint64_t szDownB;
};
static void serializeBetaResults(const std::vector<BetaResult>& results, std::ofstream& resultsFile) {
	std::unique_ptr<PackedResultData[]> packedFileData(new PackedResultData[results.size()]);

	for(size_t idx = 0; idx < results.size(); idx++) {
		const BetaResult& i = results[idx];
		const BetaSumPair& ip = i.dataForThisTop;
		PackedResultData& o = packedFileData[idx];

		o.nodeIndex = i.topIndex;
		o.brA = ip.betaSum.betaSum;
		o.brB = ip.betaSumDualDedup.betaSum;
		o.szDownA = ip.betaSum.countedIntervalSizeDown;
		o.szDownB = ip.betaSumDualDedup.countedIntervalSizeDown;
	}

	serializeU64(results.size(), resultsFile);
	resultsFile.write(reinterpret_cast<const char*>(packedFileData.get()), results.size() * sizeof(PackedResultData));
}
static std::vector<BetaResult> deserializeBetaResults(std::ifstream& resultsFile) {
	size_t size = deserializeU64(resultsFile);
	std::unique_ptr<PackedResultData[]> packedFileData(new PackedResultData[size]);
	resultsFile.read(reinterpret_cast<char*>(packedFileData.get()), size * sizeof(PackedResultData));
	std::vector<BetaResult> results(size);

	for(size_t idx = 0; idx < size; idx++) {
		BetaResult& i = results[idx];
		BetaSumPair& ip = i.dataForThisTop;
		const PackedResultData& o = packedFileData[idx];

		i.topIndex = o.nodeIndex;
		ip.betaSum.betaSum = o.brA;
		ip.betaSumDualDedup.betaSum = o.brB;
		ip.betaSum.countedIntervalSizeDown = o.szDownA;
		ip.betaSumDualDedup.countedIntervalSizeDown = o.szDownB;
	}
	return results;
}

static void saveResults(unsigned int Variables, const std::string& computeFolder, const std::string& jobID, const std::string& computeID, const std::vector<BetaResult>& betaResults) {
	std::string workingFile = computeFilePath(computeFolder, "working", jobID, "_" + computeID + ".job");
	std::string finishedFile = computeFilePath(computeFolder, "finished", jobID, "_" + computeID + ".job");
	std::string resultsFile = computeFilePath(computeFolder, "results", jobID, "_" + computeID + ".results");

	if(std::filesystem::exists(finishedFile)) {
		std::cerr << "Results file (" << finishedFile << ") already exists! Job " << jobID << " has already terminated!";
		std::abort();
	}
	if(std::filesystem::exists(resultsFile)) {
		std::cerr << "Results file (" << resultsFile << ") already exists! Job " << jobID << " has already terminated!";
		std::abort();
	}
	
	{
		std::ofstream resultsOut(resultsFile, std::ios::binary);
		serializeU32(Variables, resultsOut);
		serializeBetaResults(betaResults, resultsOut);
	}
	std::filesystem::rename(workingFile, finishedFile); // Throws filesystem::filesystem_error on error
	// At this point the job is committed and finished!
}

void processJob(unsigned int Variables, const std::string& computeFolder, const std::string& jobID, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&, const void*[2])) {
	std::string computeID = methodName + "_" + getComputeIdentifier();
	checkValidationFileExists(computeFolder, computeID);

	std::future<std::vector<JobTopInfo>> topsToProcessFuture = std::async(loadJob, Variables, computeFolder, computeID, jobID);

	std::cout << "Starting Computation..." << std::endl;
	
	ResultProcessorOutput pipelineOutput = pcoeffPipeline(Variables, topsToProcessFuture, processorFunc/*, threadPoolBufferValidator<7>*/);
	std::vector<BetaResult>& betaResults = pipelineOutput.results;
	


	/*if(betaResults.size() != topsToProcess.size()) {
		std::cerr << "This can't happen! Number of tops processed isn't number of inputs tops!" << std::endl;
		std::abort();
	}*/

	/*std::cout << "Original top indices: " << std::endl;
	for(unsigned int idx : topsToProcess) {
		std::cout << idx << ", ";
	}
	std::cout << std::endl;*/

	std::cout << "Saving " << betaResults.size() << " results\n" << std::flush;

	addValidationDataToFile(Variables, computeFolder, computeID, pipelineOutput);

	std::sort(betaResults.begin(), betaResults.end(), [](BetaResult a, BetaResult b) -> bool {return a.topIndex < b.topIndex;});
	saveResults(Variables, computeFolder, jobID, computeID, betaResults);
}


std::vector<BetaResult> readResultsFile(unsigned int Variables, const std::filesystem::path& filePath) {
	std::ifstream resultFileIn(filePath, std::ios::binary);

	unsigned int fileVariables = deserializeU32(resultFileIn);
	if(Variables != fileVariables) {
		std::cerr << "Results File for incorrect Dedekind Target! Specified Target: D(" + std::to_string(Variables + 2) + "), target from file: D(" + std::to_string(fileVariables + 2) + ")" << std::endl;
		std::abort();
	}

	std::vector<BetaResult> result = deserializeBetaResults(resultFileIn);
	checkFileDone(resultFileIn);
	return result;
}

std::vector<BetaSumPair> collectAllResultFiles(unsigned int Variables, const std::string& computeFolder) {
	BetaResultCollector collector(Variables);

	std::string resultsDirectory = computeFolderPath(computeFolder, "results");
	for(const auto& file : std::filesystem::directory_iterator(resultsDirectory)) {
		std::filesystem::path path = file.path();
		std::string filePath = path.string();
		if(file.is_regular_file() && filePath.substr(filePath.find_last_of(".")) == ".results") {
			// Results file
			std::cout << "Reading results file " << filePath << std::endl;
			std::vector<BetaResult> results = readResultsFile(Variables, path);
			collector.addBetaResults(results);
		}
	}

	return collector.getResultingSums();
}

std::unique_ptr<ValidationData[]> collectAllValidationFiles(unsigned int Variables, const std::string& computeFolder) {
	ValidationFileData summer(Variables);

	summer.initializeZero();

	std::string validationDirectory = computeFolderPath(computeFolder, "validation");

	ValidationFileData currentlyAddingFile(Variables);
	for(const auto& file : std::filesystem::directory_iterator(validationDirectory)) {
		std::filesystem::path path = file.path();
		std::string filePath = path.string();
		if(file.is_regular_file() && filePath.substr(filePath.find_last_of(".")) == ".validation") {
			// Results file
			std::cout << "Reading validation file " + filePath << std::endl;
			FILE* validationFile = fopen(filePath.c_str(), "rb");
			currentlyAddingFile.readFromFile(validationFile);
			size_t numberOfTopsInOther = summer.mergeIntoThis(currentlyAddingFile);
			std::cout << "Successfully added validation file " + filePath + ", added " + std::to_string(numberOfTopsInOther) + " tops." << std::endl;
		}
	}

	return std::move(summer).finalize();
}

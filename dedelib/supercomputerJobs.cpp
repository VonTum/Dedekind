#include "supercomputerJobs.h"

#include <filesystem>
#include <random>
#include <algorithm>

#include <unistd.h>
#include <limits.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "crossPlatformIntrinsics.h"

// Utilities for easily working with syscall open
static void check(int retVal, const char* err) {
	if(retVal != 0) {
		perror(err);
		std::abort();
	}
}

static void checkRead(int fd, void* outBuf, size_t readSize, const char* err) {
	while(readSize != 0) {
		ssize_t readCount = read(fd, outBuf, readSize); // have to repeat reads, because for some godawful reason read is limited in size
		if(readCount == -1) {
			perror(err);
			std::abort();
		} else {
			readSize -= readCount;
			reinterpret_cast<char*&>(outBuf) += readCount;
		}
	}
}

static void checkWrite(int fd, const void* buf, size_t writeSize, const char* err) {
	while(writeSize != 0) {
		ssize_t writeCount = write(fd, buf, writeSize); // have to repeat writes, because for some godawful reason write is limited in size
		if(writeCount == -1) {
			perror(err);
			std::abort();
		} else {
			writeSize -= writeCount;
			reinterpret_cast<const char*&>(buf) += writeCount;
		}
	}
}

static int checkOpen(const char* file, int flags, const char* err) {
	int fd = open64(file, flags);
	//std::cout << "Opening file " << file << " with flags=" << flags << ": fd=" << fd << std::endl;
	if(fd == -1) {
		perror(err);
		std::abort();
	}
	return fd;
}

static int checkCreate(const char* file) {
	int fd = open64(file, O_WRONLY|O_CREAT, 0600); // read and write permission
	std::cout << "Creating file " << file << ": fd=" << fd << std::endl;
	if(fd == -1) {
		std::string err = std::string("Failed to create file ") + file + "\n";
		perror(err.c_str());
		std::abort();
	}
	return fd;
}


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




size_t getTopBitsetByteSize(unsigned int Variables) {
	return (mbfCounts[Variables] + 7) / 8;
}

ValidationFileData::ValidationFileData(unsigned int Variables) : 
	Variables(Variables),
	memory(new char[VALIDATION_BUFFER_SIZE(Variables)*sizeof(ValidationData) + getTopBitsetByteSize(Variables)]) {
		
	savedValidationBuffer = reinterpret_cast<ValidationData*>(this->memory.get());
	savedTopsBitset = reinterpret_cast<uint8_t*>(savedValidationBuffer + VALIDATION_BUFFER_SIZE(Variables));
}

size_t ValidationFileData::getTotalMemorySize() const {
	return sizeof(ValidationData) * VALIDATION_BUFFER_SIZE(Variables) + getTopBitsetByteSize(Variables);
}

void ValidationFileData::initializeZero() {
	memset(memory.get(), 0, getTotalMemorySize());
}

void ValidationFileData::readFromFile(int validationFD) {
	checkRead(validationFD, memory.get(), getTotalMemorySize(), "Validation file is too short! ");
}
void ValidationFileData::writeToFile(int validationFD) const {
	checkWrite(validationFD, memory.get(), getTotalMemorySize(), "Could not write full validation file for some reason! ");
}

bool ValidationFileData::isTopPresent(NodeIndex topIdx) const {
	if(topIdx < mbfCounts[Variables]) {
		uint8_t topByte = savedTopsBitset[topIdx / 8];
		uint8_t topBitMask = 0b00000001 << (topIdx % 8);
		return (topByte & topBitMask) != 0;
	} else {
		std::cerr << "Error: attempting to query top out of range for this ValidationFile! Aborting!\n" << std::flush;
		std::abort();
	}
}

void ValidationFileData::checkHasAllTops() const {
	bool fault = false;

	for(NodeIndex topIdx = 0; topIdx < mbfCounts[Variables]; topIdx++) {
		if(!this->isTopPresent(topIdx)) {
			std::cerr << "Error: Missing top " + std::to_string(topIdx) + " again! Aborting!\n" << std::flush;
			fault = true;
		}
	}

	if(fault) {
		std::abort();
	}
}

void ValidationFileData::addTop(NodeIndex topIdx) {
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
size_t ValidationFileData::mergeIntoThis(const ValidationFileData& other) {
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

static void checkValidationFileExists(const std::string& validationFileName) {
	if(!std::filesystem::exists(validationFileName)) {
		std::cerr << "Validation file " + validationFileName + " does not exist! Aborting!\n" << std::flush;
		std::abort();
	} else {
		std::cout << "Confirmed Validation file " + validationFileName + " found!\n" << std::flush;
	}
}




static void checkNotExists(const std::string& fileName) {
	if(std::filesystem::exists(fileName)) {
		std::cerr << "File " + fileName + " already exists! Aborting!\n" << std::flush;
		std::abort();
	}
}

static void initializeValidationFile(const ValidationFileData& initialFileData, const std::string& validationFileName) {
	checkNotExists(validationFileName);

	std::cout << "Creating validation file " << validationFileName << std::endl;
	int validationFD = checkCreate(validationFileName.c_str());

	initialFileData.writeToFile(validationFD);

	check(fsync(validationFD), "Failed to fsync Validation File! ");
	check(close(validationFD), "Failed to close Validation File! ");
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

		checkNotExists(validationFileName);
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

void initializeComputeProject(unsigned int Variables, std::string computeFolder, size_t numberOfJobs, size_t numberOfJobsToActuallyGenerate) {
	if(numberOfJobsToActuallyGenerate != numberOfJobs) {
		std::cerr << "WARNING initializeComputeProject: GENERATING ONLY PARTIAL JOBS:" << numberOfJobsToActuallyGenerate << '/' << numberOfJobs << " -> INVALID COMPUTE PROJECT!\n" << std::flush;
	}

	constexpr size_t TOP_CLUSTER_SIZE = 8; // Cluster tops to make bot buffer generation more efficient

	std::cout << "Generating a supercomputing project for computing Dedekind Number D(" << (Variables + 2) << "). Split across " << numberOfJobs << " jobs. \nPath: " << computeFolder << std::endl;
	
	if(std::filesystem::exists(computeFolder)) {
		std::cerr << "The compute path already exists! (" << computeFolder << ") Choose a different compute folder!\nTermintating to avoid data loss. " << std::endl;
		std::abort();
	}

	std::cout << "Generating directories: " << computeFolder << "/\n    jobs/\n    working/\n    critical/\n    finished/\n    results/\n    logs/\n    validation/\n\n" << std::flush;
	std::filesystem::create_directory(computeFolder);
	std::filesystem::create_directory(computeFolderPath(computeFolder, "jobs"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "working"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "critical"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "finished"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "results"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "logs"));
	std::filesystem::create_directory(computeFolderPath(computeFolder, "validation"));

	size_t numberOfMBFsToProcess = mbfCounts[Variables];
	size_t numberOfCompleteBatches = numberOfMBFsToProcess / TOP_CLUSTER_SIZE;

	std::cout << "Collecting the " << numberOfMBFsToProcess << " MBFs for the full computation..." << std::endl;

	std::vector<size_t> completeBatchIndices;
	for(size_t i = 0; i < numberOfCompleteBatches; i++) {
		completeBatchIndices.push_back(i);
	}

	std::default_random_engine generator;
	std::shuffle(completeBatchIndices.begin(), completeBatchIndices.end(), generator);

	std::cout << "Generating " << numberOfJobsToActuallyGenerate << " job files of ~" << (numberOfMBFsToProcess / numberOfJobs) << " MBFs per job..." << std::endl;

	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);

	size_t currentBatchIndex = 0;
	for(size_t jobI = 0; jobI < numberOfJobsToActuallyGenerate; jobI++) {
		size_t jobsLeft = numberOfJobs - jobI;
		size_t topBatchesLeft = numberOfCompleteBatches - currentBatchIndex;
		size_t numberOfBatchesInThisJob = topBatchesLeft / jobsLeft;

		std::vector<JobTopInfo> topIndices;
		topIndices.reserve(numberOfBatchesInThisJob * TOP_CLUSTER_SIZE);
		for(size_t i = 0; i < numberOfBatchesInThisJob; i++) {
			std::uint32_t curBatchStart = completeBatchIndices[currentBatchIndex++] * TOP_CLUSTER_SIZE;
			for(size_t j = 0; j < TOP_CLUSTER_SIZE; j++) {
				addJobTop(topIndices, curBatchStart + j, flatNodes);
			}
		}
		// Add remaining tops to job 0
		if(jobI == 0) {
			size_t firstUnbatchedTop = numberOfCompleteBatches * TOP_CLUSTER_SIZE;
			for(size_t topI = firstUnbatchedTop; topI < numberOfMBFsToProcess; topI++) {
				addJobTop(topIndices, topI, flatNodes);
			}
		}

		writeJobToFile(Variables, computeFilePath(computeFolder, "jobs", std::to_string(jobI), ".job"), topIndices);
	}

	std::cout << "Generated " << numberOfJobsToActuallyGenerate << " job files of ~" << (numberOfMBFsToProcess / numberOfJobs) << " MBFs per job." << std::endl;
}

static std::vector<JobTopInfo> loadJob(unsigned int Variables, const std::string& workingFile) {
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
static void addValidationDataToFile(unsigned int Variables, const std::string& validationFileName, const std::string& validationFileNameTmp, const ResultProcessorOutput& computationResults) {
	ValidationFileData fileData(Variables);

	std::cout << "Opening Validation file " + validationFileName + "\n" << std::flush;
	int validationFD = checkOpen(validationFileName.c_str(), O_RDONLY, "Validation file does not exist, yet it existed at the start! ");

	std::cout << "Reading Validation file " + validationFileName + "\n" << std::flush;
	fileData.readFromFile(validationFD);
	std::cout << "Validation file " + validationFileName + " read properly\nAdding new information to buffer...\n" << std::flush;
	check(close(validationFD), "Failed to close validation file! ");

	for(BetaResult result : computationResults.results) {
		fileData.addTop(result.topIndex);
	}
	for(size_t i = 0; i < VALIDATION_BUFFER_SIZE(Variables); i++) {
		fileData.savedValidationBuffer[i].dualBetaSum += computationResults.validationBuffer[i].dualBetaSum;
	}

	//check(lseek64(validationFD, 0, SEEK_SET), "Error resetting to start of validation file! "); // reset to start of file

	int validationOutFD = checkCreate(validationFileNameTmp.c_str());

	std::cout << "Buffer updated\nWriting Validation file " + validationFileNameTmp + "\n" << std::flush;
	fileData.writeToFile(validationOutFD);
	std::cout << "Validation file " + validationFileNameTmp + " written properly\n" << std::flush;

	check(fsync(validationOutFD), "Failed to fsync validationTMP file! ");
	check(close(validationOutFD), "Failed to close validationTMP file! ");
	std::cout << "Validation file " + validationFileNameTmp + " closed.\n" << std::flush;
}

struct PackedResultData {
	uint32_t nodeIndex;
	uint32_t padding4 = 0xFFFFFFFF;
	uint64_t padding8 = 0xFFFFFFFFFFFFFFFF;
	u128 brA;
	u128 brB;
	uint64_t szDownA;
	uint64_t szDownB;
};

static std::unique_ptr<PackedResultData[]> packResults(const std::vector<BetaResult>& results) {
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

	return packedFileData;
}

static std::vector<BetaResult> unpackResults(const PackedResultData* packedFileData, size_t resultCount) {
	std::vector<BetaResult> results(resultCount);

	for(size_t idx = 0; idx < resultCount; idx++) {
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

union ResultsFileHeader {
	struct {
		uint64_t Variables;
		uint64_t resultCount;
	};
	char charData[16];
};

static void saveResults(unsigned int Variables, const std::string& resultsFile, const std::vector<BetaResult>& betaResults) {
	ResultsFileHeader header;
	header.Variables = Variables;
	header.resultCount = betaResults.size();
	
	int resultsFD = checkCreate(resultsFile.c_str());
	checkWrite(resultsFD, &header, sizeof(ResultsFileHeader), "Result file write failed! ");
	std::unique_ptr<PackedResultData[]> packedResults = packResults(betaResults);
	checkWrite(resultsFD, packedResults.get(), sizeof(PackedResultData) * betaResults.size(), "Result file write failed! ");
	check(fsync(resultsFD), "Result file sync failed! ");
	check(close(resultsFD), "Result file close failed! ");
}

void processJob(unsigned int Variables, const std::string& computeFolder, const std::string& jobID, const std::string& methodName, void (*processorFunc)(PCoeffProcessingContext&), void*(*validator)(void*)) {
	std::string computeID = methodName + "_" + getComputeIdentifier();

	std::string validationFileName = getValidationFilePath(computeFolder, computeID);
	std::string jobFile = computeFilePath(computeFolder, "jobs", jobID, ".job");
	std::string workingFile = computeFilePath(computeFolder, "working", jobID, "_" + computeID + ".job");
	std::string criticalFile = computeFilePath(computeFolder, "critical", jobID, "_" + computeID + ".job");
	std::string finishedFile = computeFilePath(computeFolder, "finished", jobID, "_" + computeID + ".job");
	std::string resultsFile = computeFilePath(computeFolder, "results", jobID, "_" + computeID + ".results");

	checkValidationFileExists(validationFileName);
	
	checkNotExists(workingFile);
	checkNotExists(criticalFile);
	checkNotExists(finishedFile);
	checkNotExists(resultsFile);
	std::filesystem::rename(jobFile, workingFile); // Throws filesystem::filesystem_error on error
	std::cout << "Moved job to working directory: " << jobFile << "=>" << workingFile << std::endl;

	std::cout << "Starting Computation..." << std::endl;
	
	ResultProcessorOutput pipelineOutput = pcoeffPipeline(Variables, [&]() -> std::vector<JobTopInfo> {return loadJob(Variables, workingFile);}, processorFunc, validator);
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
	
	std::sort(betaResults.begin(), betaResults.end(), [](BetaResult a, BetaResult b) -> bool {return a.topIndex < b.topIndex;});
	std::cout << "Saving " << betaResults.size() << " results\n" << std::flush;

	std::string validationFileNameTmp = validationFileName + "_tmp";
	addValidationDataToFile(Variables, validationFileName, validationFileNameTmp, pipelineOutput);

	// Check files again, just to be sure
	checkNotExists(finishedFile);
	checkNotExists(criticalFile);
	checkNotExists(resultsFile);

	std::cout << "Entering critical section: " << workingFile << "=>" << criticalFile << std::endl;
	std::filesystem::rename(workingFile, criticalFile); // Throws filesystem::filesystem_error on error

	saveResults(Variables, resultsFile, betaResults);
	std::filesystem::rename(validationFileNameTmp, validationFileName); // Commit the new validation file, Throws filesystem::filesystem_error on error

	std::filesystem::rename(criticalFile, finishedFile); // Throws filesystem::filesystem_error on error
	std::cout << "Finished critical section, everything comitted! " << criticalFile << "=>" << finishedFile << std::endl;
}

static std::pair<std::string, std::string> parseFileName(const std::filesystem::path& filePath, const std::string& extention) {
	std::string fileName = filePath.filename().string();

	if(fileName[0] != 'j' || fileName.substr(fileName.length()-extention.length()) != extention) {
		std::cerr << "Invalid job filename " + filePath.string() + "?\n";
		std::abort();
	}
		
	size_t underscoreIdx = fileName.find('_');
	std::string jobID = fileName.substr(1/*cut initial j*/, underscoreIdx-1);
	std::string deviceID = fileName.substr(underscoreIdx + 1, fileName.length()-extention.length()-underscoreIdx-1);
	return std::make_pair(jobID, deviceID);
}

void resetUnfinishedJobs(const std::string& computeFolder) {
	std::string jobsFolder = computeFolderPath(computeFolder, "jobs");
	std::string workingFolder = computeFolderPath(computeFolder, "working");

	std::ofstream log("resetJobsLog.txt", std::ios::app);

	std::string totalResetString = "";

	for(std::filesystem::directory_entry unfinishedJob : std::filesystem::directory_iterator(workingFolder)) {
		std::pair<std::string, std::string> jobDevicePair = parseFileName(unfinishedJob.path(), ".job");

		log << "Job " + jobDevicePair.first + " reset from node " + jobDevicePair.second << "\n";

		totalResetString.append(jobDevicePair.first + ",");

		std::filesystem::rename(unfinishedJob.path().string(), jobsFolder + "j" + jobDevicePair.first + ".job");
	}

	std::cout << "Resulting new sbatch array: --array=" + totalResetString.substr(0, totalResetString.length() - 1) << std::endl;
}

std::vector<BetaResult> readResultsFile(unsigned int Variables, const std::filesystem::path& filePath) {
	ResultsFileHeader header;
	int resultsFD = checkOpen(filePath.c_str(), O_RDONLY, "Failed to open results file! ");
	checkRead(resultsFD, &header, sizeof(ResultsFileHeader), "Results read failed! ");

	if(Variables != header.Variables) {
		std::cerr << "Results File for incorrect Dedekind Target! Specified Target: D(" + std::to_string(Variables + 2) + "), target from file: D(" + std::to_string(header.Variables + 2) + ")" << std::endl;
		std::abort();
	}

	std::unique_ptr<PackedResultData[]> packedData(new PackedResultData[header.resultCount]);
	checkRead(resultsFD, packedData.get(), sizeof(PackedResultData) * header.resultCount, "Results read failed! ");
	check(close(resultsFD), "Failed to close results file! ");

	std::vector<BetaResult> result = unpackResults(packedData.get(), header.resultCount);
	return result;
}

struct NamedResultFile {
	std::string jobID;
	std::string nodeID;
	std::filesystem::path filePath;
	std::vector<BetaResult> results;
};

static std::vector<NamedResultFile> loadAllResultsFiles(unsigned int Variables, const std::string& computeFolder) {
	std::vector<NamedResultFile> allResultFileContents;

	std::string resultsDirectory = computeFolderPath(computeFolder, "results");
	for(const auto& file : std::filesystem::directory_iterator(resultsDirectory)) {
		std::filesystem::path path = file.path();
		std::string filePath = path.string();
		if(file.is_regular_file() && filePath.substr(filePath.find_last_of(".")) == ".results") {
			// Results file
			std::cout << "Reading results file " << filePath << std::endl;
			std::vector<BetaResult> results = readResultsFile(Variables, path);

			std::pair<std::string, std::string> jobDevicePair = parseFileName(path, ".results");
			
			NamedResultFile entry;
			entry.jobID = jobDevicePair.first;
			entry.nodeID = jobDevicePair.second;
			entry.filePath = path;
			entry.results = std::move(results);

			allResultFileContents.push_back(std::move(entry));
		} else {
			std::cerr << "Unknown file found, expected results file: " << filePath << std::endl;
		}
	}

	return allResultFileContents;
}

BetaResultCollector collectAllResultFiles(unsigned int Variables, const std::string& computeFolder) {
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
		} else {
			std::cerr << "Unknown file found, expected results file: " << filePath << std::endl;
		}
	}

	return collector;
}

ValidationFileData collectAllValidationFiles(unsigned int Variables, const std::string& computeFolder) {
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
			int validationFD = checkOpen(filePath.c_str(), O_RDONLY, "Failed to open validation file! ");
			currentlyAddingFile.readFromFile(validationFD);
			size_t numberOfTopsInOther = summer.mergeIntoThis(currentlyAddingFile);
			std::cout << "Successfully added validation file " + filePath + ", added " + std::to_string(numberOfTopsInOther) + " tops." << std::endl;
			check(close(validationFD), "Failed to close validation file! ");
		} else {
			std::cerr << "Unknown file found, expected validation file: " << filePath << std::endl;
		}
	}

	return summer;
}

void checkProjectResultsIdentical(unsigned int Variables, const std::string& computeFolderA, const std::string& computeFolderB) {
	std::vector<NamedResultFile> resultsA = loadAllResultsFiles(Variables, computeFolderA);
	std::sort(resultsA.begin(), resultsA.end(), [](const NamedResultFile& a, const NamedResultFile& b) -> bool {return a.jobID < b.jobID;});
	std::vector<NamedResultFile> resultsB = loadAllResultsFiles(Variables, computeFolderB);
	std::sort(resultsB.begin(), resultsB.end(), [](const NamedResultFile& a, const NamedResultFile& b) -> bool {return a.jobID < b.jobID;});
	
	bool success = true;

	std::cout << "Loaded all result files\nComparing...\n" << std::flush;
	for(size_t i = 0; i < resultsA.size(); i++) {
		if(resultsA[i].jobID != resultsB[i].jobID) {
			std::cerr << "\033[31mMissing result file? " + resultsA[i].filePath.string() + " <-> " + resultsB[i].filePath.string() << "\033[39m\n" << std::flush;
			std::abort();
		}

		const std::vector<BetaResult>& rA = resultsA[i].results;
		const std::vector<BetaResult>& rB = resultsB[i].results;
		if(rA.size() != rB.size()) {
			std::cerr << "\033[31mResult files of unequal size! " + resultsA[i].filePath.string() + " <-> " + resultsB[i].filePath.string() << "\033[39m\n" << std::flush;
			success = false;
		}
		if(rA != rB) {
			std::cerr << "\033[31mNonmatching result files! " + resultsA[i].filePath.string() + " <-> " + resultsB[i].filePath.string() << "\033[39m\n" << std::flush;
			for(size_t topI = 0; topI < rA.size(); topI++) {
				const BetaResult& rrA = rA[topI];
				const BetaResult& rrB = rB[topI];

				if(rrA != rrB) {
					std::cerr << "\033[31mElement " + std::to_string(topI) + ">\n        " + toString(rrA) + "\n    <-> " + toString(rrB) + "\033[39m\n" << std::flush;
				}
			}
			success = false;
		}
	}

	ValidationFileData validationA = collectAllValidationFiles(Variables, computeFolderA);
	ValidationFileData validationB = collectAllValidationFiles(Variables, computeFolderB);

	std::cout << "Checking validation files tops are identical...\n" << std::flush;
	for(NodeIndex topI = 0; topI < mbfCounts[Variables]; topI++) {
		if(validationA.isTopPresent(topI) != validationB.isTopPresent(topI)) {
			if(validationA.isTopPresent(topI)) {
				std::cerr << "\033[31mTop " + std::to_string(topI) + " present in " + computeFolderA + " but not in " + computeFolderB + "!\033[39m\n" << std::flush;
			} else {
				std::cerr << "\033[31mTop " + std::to_string(topI) + " not present in " + computeFolderA + " but is present in " + computeFolderB + "!\033[39m\n" << std::flush;
			}
			success = false;
		}
	}

	std::cout << "Checking validation bottoms...\n" << std::flush;
	for(NodeIndex botI = 0; botI < VALIDATION_BUFFER_SIZE(Variables); botI++) {
		BetaSum vA = validationA.savedValidationBuffer[botI].dualBetaSum;
		BetaSum vB = validationB.savedValidationBuffer[botI].dualBetaSum;
		if(vA != vB) {
			std::cout << "\033[31mMismatching value for bot " + std::to_string(botI) + ": " + toString(vA) + " <-> " + toString(vB) + "\033[39m\n" << std::flush;
			success = false;
		}
	}

	if(success) {
		std::cout << "\033[32mAll files checked. Projects " + computeFolderA + " and " + computeFolderB + " are identical!\033[39m\n" << std::flush;
	}
}


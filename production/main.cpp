#include <iostream>
#include <thread>
#include <sstream>

#include "commands.h"

#include "../dedelib/cmdParser.h"
#include "../dedelib/timeTracker.h"
#include "../dedelib/configure.h"

/*
Correct numbers
	0: 2
	1: 3
	2: 6
	3: 20
	4: 168
	5: 7581
	6: 7828354
	7: 2414682040998
	8: 56130437228687557907788
	9: ??????????????????????????????????????????
*/

static void listCommands();
static void listFlags() {
	std::cout << "--dataDir <dir>: Set data directory" << std::endl;

	std::cout << "-mmap: Use Memory Mapping, Linux-only" << std::endl;
	std::cout << "-mmap_populate: Enable Memory Map Populating, Linux-only" << std::endl;
	std::cout << "-mmap_hugetlb: Enable Memory Map HugeTLB, Linux-only, requires hugetlb compatible filesystem" << std::endl;
	std::cout << "-mmap_2MB: Enable Memory Map HugeTLB with page size 2MB, Linux-only, requires hugetlb compatible filesystem" << std::endl;
	std::cout << "-mmap_1GB: Enable Memory Map HugeTLB with page size 1GB, Linux-only, requires hugetlb compatible filesystem" << std::endl;
}
static void helpCMD() {
	std::cout << "Commands: " << std::endl;
	listCommands();
	std::cout << "Flags: " << std::endl;
	listFlags();
}

static CommandSet helpCommands{"Help", {
	{"listCommands", listCommands},
	{"listFlags", listFlags},
	{"help", helpCMD}
}, {}};

static const CommandSet* knownCommandSets[]{&helpCommands, &miscCommands, &dataGenCommands, &flatCommands, &testSetCommands, &processingCommands, &codeGenCommands, &bottomBufferCommands};

static void listCommands() {
	for(const CommandSet* cmdSet : knownCommandSets) {
		std::cout << cmdSet->name << ":" << std::endl;
		for(const auto& cmd : cmdSet->commands) {
			std::cout << "  " << cmd.first << std::endl;
		}
		for(const auto& cmd : cmdSet->commandsWithArg) {
			std::cout << "  " << cmd.first << ":<arg>" << std::endl;
		}
	}
}


inline void runCommandNoArg(const std::string& cmd) {
	for(const CommandSet* cmdSet : knownCommandSets) {
		auto found = cmdSet->commands.find(cmd);
		if(found != cmdSet->commands.end()) {
			std::cout << "running " << cmd.c_str() << std::endl;
			TimeTracker timer;
			(*found).second();
			return;
		}
	}
	std::cout << "Command not found!" << std::endl;
	exit(1);
}

static std::vector<std::string> splitList(const std::string& strList) {
	std::stringstream strStream(strList);
	std::string segment;
	std::vector<std::string> results;

	while(std::getline(strStream, segment, ',')) {
		results.push_back(segment);
	}
	return results;
}


inline void runCommandWithArg(const std::string& cmd, const std::string& arg) {
	for(const CommandSet* cmdSet : knownCommandSets) {
		auto foundWithArg = cmdSet->commandsWithArg.find(cmd);
		if(foundWithArg != cmdSet->commandsWithArg.end()) {
			std::cout << "running " << cmd.c_str() << "(" << arg.c_str() << ")" << std::endl;
			TimeTracker timer;
			(*foundWithArg).second(splitList(arg));
			return;
		}
	}
	std::cout << "Command not found!" << std::endl;
	exit(1);
}

inline void runCommand(const std::string& cmdWithArg) {
	size_t argSepFound = cmdWithArg.find(':');

	if(argSepFound == std::string::npos) { // no arg
		runCommandNoArg(cmdWithArg);
	} else { // arg
		std::string cmd = cmdWithArg.substr(0, argSepFound);
		std::string arg = cmdWithArg.substr(argSepFound+1);
		
		runCommandWithArg(cmd, arg);
	}
}
inline void runCommands(const ParsedArgs& args) {
	for(size_t i = 0; i < args.argCount(); i++) {
		const std::string& cmd = args[i];

		runCommand(cmd);
	}
}

int main(int argc, const char** argv) {
	std::cout << "Detected " << std::thread::hardware_concurrency() << " threads" << std::endl;

	ParsedArgs parsed(argc, argv);

	configure(parsed);

	if(parsed.argCount() > 0) {
		runCommands(parsed);
	} else {
		std::string givenCommand;
		std::cout << "Enter command> ";
		std::cin >> givenCommand;

		runCommand(givenCommand);
	}
	return 0;
}


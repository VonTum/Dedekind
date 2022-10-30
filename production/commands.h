#pragma once

#include <map>
#include <vector>
#include <string>

struct CommandSet {
	std::string name;
	std::map<std::string, void(*)()> commands;
	std::map<std::string, void(*)(const std::vector<std::string>&)> commandsWithArg;
};

// main command buffer
extern CommandSet miscCommands;
extern CommandSet flatCommands;
extern CommandSet dataGenCommands;
extern CommandSet testSetCommands;
extern CommandSet processingCommands;
extern CommandSet codeGenCommands;
extern CommandSet bottomBufferCommands;
extern CommandSet superCommands;
extern CommandSet verificationCommands;

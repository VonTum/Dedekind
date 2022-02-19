#pragma once

#include <map>
#include <string>

struct CommandSet {
	std::string name;
	std::map<std::string, void(*)()> commands;
	std::map<std::string, void(*)(const std::string&)> commandsWithArg;
};

// main command buffer
extern CommandSet miscCommands;
extern CommandSet flatCommands;
extern CommandSet dataGenCommands;
extern CommandSet testSetCommands;
extern CommandSet processingCommands;
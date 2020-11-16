#pragma once

#include <chrono>
#include <string>
#include <iostream>

class TimeTracker {
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
	std::string message;
public:
	TimeTracker(std::string message) : message(message), startTime(std::chrono::high_resolution_clock::now()) {}
	TimeTracker() : message("Time taken: "), startTime(std::chrono::high_resolution_clock::now()) {}
	~TimeTracker() {
		std::chrono::nanoseconds deltaTime = std::chrono::high_resolution_clock::now() - startTime;
		std::cout << message << deltaTime.count() * 1.0e-6 << "ms";
	}
};


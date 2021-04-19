#pragma once

#include <chrono>
#include <string>
#include <iostream>

class TimeTracker {
	std::string message;
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
public:
	TimeTracker(std::string message) : message(message), startTime(std::chrono::high_resolution_clock::now()) {}
	TimeTracker() : message("Time taken: "), startTime(std::chrono::high_resolution_clock::now()) {}

	std::chrono::nanoseconds getTime() const {
		return std::chrono::high_resolution_clock::now() - startTime;
	}

	~TimeTracker() {
		std::chrono::nanoseconds deltaTime = getTime();
		long long nanos = deltaTime.count();
		if(nanos < 1.0e10) {
			std::cout << message << nanos / 1.0e6 << "ms";
		} else if(nanos < 1.0e13) {
			std::cout << message << nanos / 1.0e9 << "s";
		} else {
			std::cout << message << nanos / 60.0e-9 << "min";
		}
		std::cout << "\n";
	}


};


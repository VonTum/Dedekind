#include "testsMain.h"

#include <vector>
#include <iostream>
#include <cstring>
#include <stdio.h>

#include <string>
#include <fstream>
#include "stdarg.h"

#include <chrono>

#include "terminalColor.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
static const char sepChar = '\\';
#else
static const char sepChar = '/';
#endif


using namespace std;
using namespace chrono;

static const TerminalColor SUCCESS_COLOR = TerminalColor::GREEN;
static const TerminalColor FAILURE_COLOR = TerminalColor::RED;
static const TerminalColor ERROR_COLOR = TerminalColor::MAGENTA;
static const TerminalColor SKIP_COLOR = TerminalColor::LIGHT_GRAY;

thread_local TestInterface __testInterface;
stringstream logStream;
const bool reffableTrue = true;

static void resetLog() {
	logStream.str("");
	logStream.clear();
}

char logBuffer[1<<16];

void logf(const char* format, ...) {
	
	va_list args;
	va_start(args, format);
	int length = std::vsnprintf(logBuffer, 1<<16, format, args);
	va_end(args);

	logStream << logBuffer << '\n';
}

static void printDeltaTime(time_point<system_clock> startTime, TerminalColor c) {
	duration<double> delta = system_clock::now() - startTime;
	setColor(c);
	cout << " (" << fixed << delta.count() << "s)\n";
}

static ifstream getFileStream(const char* fileName) {
	ifstream s(fileName);
	if(s.good()) return s;

	string path = string("tests") + sepChar + fileName;
	s = ifstream(path);
	if(s.good()) return s;

	path = string("..") + sepChar + path;
	s = ifstream(path);
	if(s.good()) return s;

	return ifstream(string("..") + sepChar + path);
}

#define SHOW_LINES_BEFORE 5
#define SHOW_LINES_AFTER 2

static void printFileSlice(const char* fileName, int line) {
	ifstream inFile = getFileStream(fileName);

	int t = 0;

	if(inFile.good()) {
		// skip lines until right before first line to show
		for(int i = 0; i < line - SHOW_LINES_BEFORE - 1; i++) {
			string l;
			getline(inFile, l);
		}

		setColor(TerminalColor::WHITE);

		string s;
		for(int i = -SHOW_LINES_BEFORE; i <= SHOW_LINES_AFTER; i++) {
			if(!getline(inFile, s)) break;
			if(i == 0) {
				setColor(TerminalColor::YELLOW);
			}
			printf("%d: %s", line + i, s.c_str());
			if(i == 0) {
				cout << "  <<<<";
				setColor(TerminalColor::WHITE);
			}
			cout << endl;
		}
	} else {
		setColor(TerminalColor::WHITE);
		printf("Could not open File %s for debugging :(\n", fileName);
	}
}

static void dumpLog() {
	setColor(TerminalColor::GREEN);
	cout << logStream.str().c_str();
}
enum class TestResult {
	SUCCESS = 0,
	FAILURE = 1,
	ERROR = 2,
	SKIP = 3
};

struct TestFlags {
	bool coverageEnabled;
	bool allowSkip;
	bool catchErrors;
};

class Test {
public:
	const char* filePath;
	const char* fileName;
	const char* funcName;
	TestType type;
	void(*testFunc)();

	Test() : 
		filePath(nullptr), funcName(nullptr), fileName(nullptr), testFunc(nullptr), type(TestType::NORMAL) {};
	Test(const char* filePath, const char* funcName, void(*testFunc)(), TestType type) : 
		filePath(filePath), 
		funcName(funcName), 
		testFunc(testFunc), 
		type(type),
		fileName(std::strrchr(this->filePath, sepChar) ? std::strrchr(this->filePath, sepChar) + 1 : this->filePath) {}
private:
	TestResult runNoErrorChecking(TestFlags flags, time_point<system_clock>& startTime) {
		try {
			startTime = system_clock::now();

			__testInterface.reset();
			testFunc();

			std::size_t assertCount = __testInterface.getAssertCount();
			setColor(assertCount > 0 ? TerminalColor::GRAY : TerminalColor::RED);
			cout << " [" << assertCount << "]";

			printDeltaTime(startTime, SUCCESS_COLOR);

			return TestResult::SUCCESS;
		} catch(AssertionError& e) {

			printDeltaTime(startTime, FAILURE_COLOR);
			dumpLog();

			setColor(TerminalColor::RED);
			printf("An assertion was incorrect at line %d:\n", e.line);

			printFileSlice(filePath, e.line);

			setColor(TerminalColor::YELLOW);
			cout << e.what() << endl;
			setColor(TerminalColor::WHITE);

			return TestResult::FAILURE;
		}
	}
public:
	TestResult run(TestFlags flags) {
		resetLog();

		setColor(TerminalColor::CYAN);
		cout << fileName << ":" << funcName;

		if(flags.allowSkip && type == TestType::SLOW) {
			setColor(SKIP_COLOR);
			cout << " (skip)\n";
			return TestResult::SKIP;
		}

		time_point<system_clock> startTime;

		if(!flags.catchErrors) {
			return runNoErrorChecking(flags, startTime);
		} else {
			try {
				return runNoErrorChecking(flags, startTime);
			} catch(exception& e) {
				printDeltaTime(startTime, ERROR_COLOR);
				dumpLog();
				setColor(TerminalColor::RED); printf("An general error was thrown: %s\n", e.what());
				return TestResult::ERROR;
			} catch(string& e) {
				printDeltaTime(startTime, ERROR_COLOR);
				dumpLog();
				setColor(TerminalColor::RED); printf("An string exception was thrown: %s\n", e.c_str());
				return TestResult::ERROR;
			} catch(const char* ex) {
				printDeltaTime(startTime, ERROR_COLOR);
				dumpLog();
				setColor(TerminalColor::RED); printf("A char* exception was thrown: %s\n", ex);
				return TestResult::ERROR;
			} catch(...) {
				printDeltaTime(startTime, ERROR_COLOR);
				dumpLog();
				setColor(TerminalColor::RED); printf("An unknown exception was thrown\n");
				return TestResult::ERROR;
			}
		}
	}
};

AssertionError::AssertionError(int line, const char* info) : line(line), info(info) {}
const char* AssertionError::what() const noexcept { return info; }

// For some reason having this in static memory breaks it, a pointer seems to work
vector<Test>* tests = nullptr;

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
HANDLE console;

static void initConsole() {
	console = GetStdHandle(STD_OUTPUT_HANDLE);
	HWND consoleWindow = GetConsoleWindow();

	RECT r;
	GetWindowRect(consoleWindow, &r);

	MoveWindow(consoleWindow, r.left, r.top, 800, 900, TRUE);
}
#else
static void initConsole() {}
#endif

static bool isCoveredBy(Test& test, const std::vector<std::string>& filters) {
	if(filters.size() == 0) {
		return true;
	}
	for(const std::string& filter : filters){
		if(filter == test.fileName || filter + ".cpp" == test.fileName || filter == test.funcName || filter == test.fileName + std::string(":") + test.funcName) {
			return true;
		}
	}
	return false;
}

static void runTests(const std::vector<std::string>& filter, TestFlags flags) {
	if(tests == nullptr) tests = new vector<Test>();
	setColor(TerminalColor::WHITE); cout << "Starting tests: ";
	setColor(SUCCESS_COLOR); cout << "[SUCCESS] ";
	setColor(FAILURE_COLOR); cout << "[FAILURE] ";
	setColor(ERROR_COLOR); cout << "[ERROR] ";
	setColor(SKIP_COLOR); cout << "[SKIP]\n";
	setColor(TerminalColor::WHITE); cout << "Number of tests: " << tests->size() << endl;

	int totalTestsRan = 0;
	int resultCounts[4]{0,0,0,0};
	for(Test& t : *tests) {
		if(isCoveredBy(t, filter)){
			TestResult result = t.run(flags);
			if(result != TestResult::SKIP) totalTestsRan++;
			resultCounts[static_cast<int>(result)]++;
		}
	}
	
	setColor(TerminalColor::WHITE);
	cout << "Tests finished! Ran " << totalTestsRan << "/" << tests->size() << " tests\n";

	setColor(SUCCESS_COLOR); cout << resultCounts[0] << " SUCCESS\n";
	setColor(FAILURE_COLOR); cout << resultCounts[1] << " FAILURE\n";
	setColor(ERROR_COLOR); cout << resultCounts[2] << " ERROR\n";
	setColor(SKIP_COLOR); cout << resultCounts[3] << " SKIP" << endl;
}

struct ParsedInput {
	std::vector<std::string> inputArgs;
	TestFlags flags;
};

ParsedInput parseInput(int argc, char* argv[]) {
	ParsedInput result;
	
	std::vector<std::string> inputFlags;
	for(int i = 1; i < argc; i++) {
		if(argv[i][0] == '-' && argv[i][1] == '-') {
			inputFlags.push_back(argv[i]);
		} else {
			result.inputArgs.push_back(argv[i]);
		}
	}
	result.flags.coverageEnabled = false;
	result.flags.catchErrors = true;
	result.flags.allowSkip = result.inputArgs.size() == 0;

	for(const std::string& flag : inputFlags) {
		if(flag == "--coverage") 
			result.flags.coverageEnabled = true;
		if(flag == "--noskip") 
			result.flags.allowSkip = false;
		if(flag == "--nocatch")
			result.flags.catchErrors = false;
	}
	return result;
}

#ifdef RUN_TESTS
int main(int argc, char* argv[]) {
	initConsole();

	ParsedInput parameters = parseInput(argc, argv);

	runTests(parameters.inputArgs, parameters.flags);

	if(parameters.flags.coverageEnabled) return 0;

	while(true) {
		string input;

		setColor(TerminalColor::WHITE);
		cout << "> ";
		cin >> input;

		if(input == "") {
			continue;
		} else if(input == "exit") {
			break;
		} else {
			TestFlags flags = parameters.flags;
			flags.allowSkip = false;
			runTests(std::vector<std::string>{input}, flags);
		}
	}
}
#endif

static void logAssertError(string text) {
	setColor(TerminalColor::RED);
	cout << text.c_str();
}

TestAdder::TestAdder(const char* file, const char* name, void(*f)(), TestType isSlow) {
	if (tests == nullptr) tests = new vector<Test>();

	tests->push_back(Test(file, name, f, isSlow));
}

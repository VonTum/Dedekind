#pragma once

#include <sstream>
#include <array>

class TestInterface {
	long long assertCount = 0;
public:
	inline void markAssert() {
		assertCount++;
	}
	inline long long getAssertCount() const { return assertCount; }
	inline void reset() {
		this->assertCount = 0;
	}
};

enum class TestType {
	NORMAL,
	SLOW,
	PROPERTY
};

struct TestAdder {
	TestAdder(const char* filePath, const char* nameName, void(*testFunc)(), TestType isSlow);
};

class AssertionError {
	const char* info;
public:
	int line;
	AssertionError(int line, const char* info);
	const char* what() const noexcept;
};

extern thread_local TestInterface __testInterface;
extern thread_local std::stringstream logStream;
void logf(const char* format, ...);

#define printVar(Var) do { std::cout << #Var << " : " << (Var) << "\n"; } while(false)

// Testing utils:

template<typename T, std::size_t Size>
std::ostream& operator<<(std::ostream& ostream, const std::array<T, Size>& arr) {
	ostream << '{' << arr[0];
	for(std::size_t i = 1; i < Size; i++) {
		ostream << ", " << arr[i];
	}
	ostream << '}';
	return ostream;
}

template<typename R, typename P>
const char* errMsg(const R& first, const P& second, const char* sep) {
	std::stringstream s;
	s << first;
	s << ' ';
	s << sep;
	s << '\n';
	s << second;

	std::string msg = s.str();

	const char* data = msg.c_str();
	char* dataBuf = new char[msg.size() + 1];
	for (int i = 0; i < msg.size() + 1; i++)
		dataBuf[i] = data[i];

	return dataBuf;
}

template<typename R>
const char* errMsg(const R& first) {
	std::stringstream s;
	s << "(";
	s << first;
	s << ")";
	std::string msg = s.str();

	const char* data = msg.c_str();
	char* dataBuf = new char[msg.size() + 1];
	for (int i = 0; i < msg.size() + 1; i++)
		dataBuf[i] = data[i];

	return dataBuf;
}

// a referenceable boolean, for use in AssertComparer and TolerantAssertComparer
// using just a literal true would cause bugs as the literal falls out of scope after the return, leading to unpredictable results
extern const bool reffableTrue;

template<typename T>
class AssertComparer {
public:
	const int line;
	const T& arg;

	AssertComparer(int line, const T& arg) : line(line), arg(arg) {}

	template<typename P> AssertComparer<bool> operator<(const P& other) const { if (!(arg < other)) { throw AssertionError(line, errMsg(arg, other, "<")); }; return AssertComparer<bool>(this->line, reffableTrue); }
	template<typename P> AssertComparer<bool> operator>(const P& other) const { if (!(arg > other)) { throw AssertionError(line, errMsg(arg, other, ">")); }; return AssertComparer<bool>(this->line, reffableTrue); }
	template<typename P> AssertComparer<bool> operator<=(const P& other) const { if (!(arg <= other)) { throw AssertionError(line, errMsg(arg, other, "<=")); }; return AssertComparer<bool>(this->line, reffableTrue); }
	template<typename P> AssertComparer<bool> operator>=(const P& other) const { if (!(arg >= other)) { throw AssertionError(line, errMsg(arg, other, ">=")); }; return AssertComparer<bool>(this->line, reffableTrue); }
	template<typename P> AssertComparer<bool> operator==(const P& other) const { if (!(arg == other)) { throw AssertionError(line, errMsg(arg, other, "==")); }; return AssertComparer<bool>(this->line, reffableTrue); }
	template<typename P> AssertComparer<bool> operator!=(const P& other) const { if (!(arg != other)) { throw AssertionError(line, errMsg(arg, other, "!=")); }; return AssertComparer<bool>(this->line, reffableTrue); }
};

struct AssertBuilder {
	int line;
	AssertBuilder(int line) : line(line) {};
	template<typename T>
	AssertComparer<T> operator<(const T& other) const { return AssertComparer<T>(line, other); }
};

#define __JOIN2(a,b) a##b
#define __JOIN(a,b) __JOIN2(a,b)

#define TEST_CASE(func) void func(); static TestAdder __JOIN(tAdder, __LINE__)(__FILE__, #func, func, TestType::NORMAL); void func()
#define TEST_CASE_SLOW(func) void func(); static TestAdder __JOIN(tAdder, __LINE__)(__FILE__, #func, func, TestType::SLOW); void func()

#define ASSERT(condition) do {if(!(AssertBuilder(__LINE__) < condition).arg) {throw AssertionError(__LINE__, "false");}__testInterface.markAssert(); }while(false)
#define ASSERT_TRUE(condition) do {if(!(condition)) throw AssertionError(__LINE__, "false");__testInterface.markAssert(); }while(false)
#define ASSERT_FALSE(condition) do {if(condition) throw AssertionError(__LINE__, "true");__testInterface.markAssert(); }while(false)

#define PREV_VAL_NAME __JOIN(____previousValue, __LINE__)
#define ISFILLED_NAME __JOIN(____isFilled, __LINE__)
#define REMAINS_CONSTANT(value) do{\
	static bool ISFILLED_NAME = false;\
	static auto PREV_VAL_NAME = value;\
	if(ISFILLED_NAME) ASSERT(PREV_VAL_NAME == (value));\
	ISFILLED_NAME = true;\
}while(false)

#define TEST_PROPERTY(func) void func(); static TestAdder __JOIN(tAdder, __LINE__)(__FILE__, #func, func, TestType::PROPERTY); void func()

template<typename GenFunc>
auto ____test_generate(const char* name, const GenFunc& generator) {
	auto generated = generator();
	logStream << name << " = " << generated << "\n";
	return generated;
}
#define GEN(name, generator) auto name = ____test_generate(#name, (generator))

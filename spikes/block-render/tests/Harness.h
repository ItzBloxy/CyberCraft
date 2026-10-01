#pragma once

// A tiny test harness: TEST(name) registers a test, CHECK* record failures, TestMain.cpp runs them all.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace th
{
struct Test
{
	const char* name;
	void (*fn)();
};

inline std::vector<Test>& Registry()
{
	static std::vector<Test> tests;
	return tests;
}

inline int& Failures()
{
	static int failures = 0;
	return failures;
}

struct Registrar
{
	Registrar(const char* aName, void (*aFn)()) { Registry().push_back({ aName, aFn }); }
};

inline void Fail(const char* aFile, int aLine, const std::string& aWhat)
{
	std::printf("  FAIL %s:%d: %s\n", aFile, aLine, aWhat.c_str());
	++Failures();
}
}

#define TH_CAT2(a, b) a##b
#define TH_CAT(a, b) TH_CAT2(a, b)
#define TEST(name)                                                   \
	static void name();                                              \
	static th::Registrar TH_CAT(registrar_, name)(#name, &name);     \
	static void name()

#define CHECK(cond)                                   \
	do                                                \
	{                                                 \
		if (!(cond))                                  \
		{                                             \
			th::Fail(__FILE__, __LINE__, #cond);      \
		}                                             \
	} while (0)

#define CHECK_EQ(a, b)                                                                                            \
	do                                                                                                            \
	{                                                                                                             \
		const auto thA = (a);                                                                                     \
		const auto thB = (b);                                                                                     \
		if (!(thA == thB))                                                                                        \
		{                                                                                                         \
			th::Fail(__FILE__, __LINE__, std::string(#a " == " #b " (") + std::to_string(thA) + " vs " +          \
			                                 std::to_string(thB) + ")");                                          \
		}                                                                                                         \
	} while (0)

#define CHECK_NEAR(a, b, eps)                                                                                     \
	do                                                                                                            \
	{                                                                                                             \
		const double thA = double(a);                                                                             \
		const double thB = double(b);                                                                             \
		if (!(std::fabs(thA - thB) <= (eps)))                                                                     \
		{                                                                                                         \
			th::Fail(__FILE__, __LINE__, std::string(#a " ~= " #b " (") + std::to_string(thA) + " vs " +          \
			                                 std::to_string(thB) + ")");                                          \
		}                                                                                                         \
	} while (0)

#define CHECK_STR(a, b)                                                                                           \
	do                                                                                                            \
	{                                                                                                             \
		const std::string thA = (a);                                                                              \
		const std::string thB = (b);                                                                              \
		if (thA != thB)                                                                                           \
		{                                                                                                         \
			th::Fail(__FILE__, __LINE__, std::string(#a " == " #b "\n--- got:\n") + thA + "\n--- expected:\n" + thB); \
		}                                                                                                         \
	} while (0)

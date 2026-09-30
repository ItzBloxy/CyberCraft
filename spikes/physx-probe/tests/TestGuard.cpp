#include "Harness.h"

#include "physx/PhysxSession.h"

#include <stdexcept>

TEST(GuardedRunsTheFunction)
{
	int value = 0;
	CHECK(cpmc::px::Guarded(value, [](int& aValue) { aValue = 42; }));
	CHECK_EQ(value, 42);
}

TEST(GuardedSurvivesAnAccessViolation)
{
	// The kind of fault a mismatched PhysX virtual table would cause: a write through a bad pointer.
	int* nowhere = nullptr;
	CHECK(!cpmc::px::Guarded(nowhere, [](int*& aPointer) { *static_cast<volatile int*>(aPointer) = 1; }));
}

TEST(GuardedSurvivesAThrownException)
{
	int value = 0;
	CHECK(!cpmc::px::Guarded(value, [](int&) { throw std::runtime_error("inside the guard"); }));
}

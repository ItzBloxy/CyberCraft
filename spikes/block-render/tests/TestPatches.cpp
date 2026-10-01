#include "Harness.h"

#include "hooks/ImportPatch.h"
#include "hooks/VtablePatch.h"

#include <Windows.h>

namespace
{
DWORD WINAPI FakeProcessId()
{
	return 4242;
}

// Not inlined, so each call reads the import table afresh.
__declspec(noinline) DWORD ProcessIdThroughImport()
{
	return GetCurrentProcessId();
}

// A stand-in COM object: a pointer to a table of three functions.
int First(void*)
{
	return 1;
}

int Second(void*)
{
	return 2;
}

int Third(void*)
{
	return 3;
}

int Replacement(void*)
{
	return 20;
}

using Method = int (*)(void*);
Method g_table[] = { &First, &Second, &Third };

struct FakeObject
{
	Method* vtable = g_table;
};

int CallSlot(FakeObject& aObject, size_t aIndex)
{
	return aObject.vtable[aIndex](&aObject);
}
}

TEST(ImportPatchRedirectsAndRestoresAnImport)
{
	const DWORD real = ProcessIdThroughImport();
	auto        patch = cpmc::FindImport(GetModuleHandleW(nullptr), "kernel32.dll", "GetCurrentProcessId");
	CHECK(patch.has_value());
	CHECK(patch->original == reinterpret_cast<void*>(&GetCurrentProcessId));
	CHECK_EQ(ProcessIdThroughImport(), real);  // finding changes nothing
	CHECK(cpmc::ApplyImport(*patch, reinterpret_cast<void*>(&FakeProcessId)));
	CHECK_EQ(ProcessIdThroughImport(), DWORD(4242));
	CHECK(cpmc::RestoreImport(*patch));
	CHECK_EQ(ProcessIdThroughImport(), real);
	CHECK(!cpmc::RestoreImport(*patch));  // already restored
}

TEST(ImportPatchFindsNothingThatIsntImported)
{
	CHECK(!cpmc::FindImport(GetModuleHandleW(nullptr), "kernel32.dll", "NoSuchFunction"));
	CHECK(!cpmc::FindImport(GetModuleHandleW(nullptr), "nosuch.dll", "GetCurrentProcessId"));
}

TEST(VtablePatchSwapsOneEntryAndRestoresIt)
{
	FakeObject          object;
	cpmc::VtablePatcher patcher;
	void* original = patcher.Patch(&object, 1, reinterpret_cast<void*>(&Replacement));
	CHECK(original == reinterpret_cast<void*>(&Second));
	CHECK_EQ(CallSlot(object, 0), 1);
	CHECK_EQ(CallSlot(object, 1), 20);
	CHECK_EQ(CallSlot(object, 2), 3);

	// Patching the same entry again changes nothing and still hands back the true original.
	CHECK(patcher.Patch(&object, 1, reinterpret_cast<void*>(&Replacement)) == reinterpret_cast<void*>(&Second));
	CHECK_EQ(patcher.Count(), size_t(1));

	CHECK_EQ(patcher.RestoreAll(), size_t(1));
	CHECK_EQ(CallSlot(object, 1), 2);
	CHECK_EQ(patcher.Count(), size_t(0));
}

#include "Harness.h"

#include "physx/PhysxSession.h"

#include <Windows.h>

TEST(FileVersionsReadsBothFieldsOfARuntimeDll)
{
	// The C++ runtime this program loads: the string starts with the binary field's major.minor. (Not kernel32.dll:
	// Windows reports system DLLs as 6.2 in the binary field to programs without a Windows 10 manifest.)
	const cpmc::px::FileVersions versions = cpmc::px::ReadFileVersions(GetModuleHandleW(L"vcruntime140.dll"));
	CHECK(!versions.binary.empty());
	CHECK(!versions.text.empty());
	const std::string majorMinor = versions.binary.substr(0, versions.binary.find('.', versions.binary.find('.') + 1) + 1);
	if (!versions.text.starts_with(majorMinor))
	{
		th::Fail(__FILE__, __LINE__, "text \"" + versions.text + "\" doesn't start with \"" + majorMinor + "\" (binary \"" + versions.binary + "\")");
	}
}

TEST(FileVersionsOfAModuleWithoutAResourceAreEmpty)
{
	// The test program itself has no version resource.
	const cpmc::px::FileVersions versions = cpmc::px::ReadFileVersions(GetModuleHandleW(nullptr));
	CHECK(versions.binary.empty());
	CHECK(versions.text.empty());
}

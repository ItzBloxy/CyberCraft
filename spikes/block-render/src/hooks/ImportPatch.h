#pragma once

#include <Windows.h>

#include <optional>

namespace cpmc
{
// One import-table entry of a loaded module. Once applied, calls the module makes to that import reach the
// replacement; `original` is where they went before.
struct ImportPatch
{
	void** slot = nullptr;
	void*  original = nullptr;
	void*  replacement = nullptr;
};

// Finds aModule's import of aFunction (imported by name) from aDll, which is compared without regard to case, and
// what the entry holds, without changing it. Nothing if the module doesn't import it that way.
std::optional<ImportPatch> FindImport(HMODULE aModule, const char* aDll, const char* aFunction);

// Points the entry at aReplacement, with one atomic pointer write. Store aPatch.original where the replacement will
// look for it first, so no call can reach the replacement before it can call on.
bool ApplyImport(ImportPatch& aPatch, void* aReplacement);

// Puts the original back, if the entry still holds the replacement. False otherwise.
bool RestoreImport(const ImportPatch& aPatch);
}

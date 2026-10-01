#include "hooks/ImportPatch.h"

#include <cstring>

namespace cpmc
{
namespace
{
// Writes one pointer-sized entry, making its page writable just for the write.
bool WriteEntry(void** aSlot, void* aValue)
{
	DWORD oldProtect = 0;
	if (!VirtualProtect(aSlot, sizeof(void*), PAGE_READWRITE, &oldProtect))
	{
		return false;
	}
	InterlockedExchangePointer(aSlot, aValue);
	DWORD unused = 0;
	VirtualProtect(aSlot, sizeof(void*), oldProtect, &unused);
	return true;
}
}

std::optional<ImportPatch> FindImport(HMODULE aModule, const char* aDll, const char* aFunction)
{
	if (!aModule || !aDll || !aFunction)
	{
		return std::nullopt;
	}
	auto*       base = reinterpret_cast<uint8_t*>(aModule);
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return std::nullopt;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		return std::nullopt;
	}
	const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (directory.VirtualAddress == 0)
	{
		return std::nullopt;
	}
	for (auto* import = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); import->Name != 0;
	     ++import)
	{
		if (_stricmp(reinterpret_cast<const char*>(base + import->Name), aDll) != 0 || import->OriginalFirstThunk == 0)
		{
			continue;
		}
		const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + import->OriginalFirstThunk);
		auto*       entries = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + import->FirstThunk);
		for (size_t i = 0; names[i].u1.AddressOfData != 0; ++i)
		{
			if (IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal))
			{
				continue;
			}
			const auto* byName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData);
			if (std::strcmp(reinterpret_cast<const char*>(byName->Name), aFunction) != 0)
			{
				continue;
			}
			ImportPatch patch;
			patch.slot = reinterpret_cast<void**>(&entries[i].u1.Function);
			patch.original = *patch.slot;
			return patch;
		}
	}
	return std::nullopt;
}

bool ApplyImport(ImportPatch& aPatch, void* aReplacement)
{
	if (!aPatch.slot || !aReplacement || !WriteEntry(aPatch.slot, aReplacement))
	{
		return false;
	}
	aPatch.replacement = aReplacement;
	return true;
}

bool RestoreImport(const ImportPatch& aPatch)
{
	if (!aPatch.slot || *aPatch.slot != aPatch.replacement)
	{
		return false;
	}
	return WriteEntry(aPatch.slot, aPatch.original);
}
}

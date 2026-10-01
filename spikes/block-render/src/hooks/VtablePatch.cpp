#include "hooks/VtablePatch.h"

#include <Windows.h>

namespace cpmc
{
namespace
{
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

void* VtablePatcher::Patch(void* aObject, size_t aIndex, void* aReplacement)
{
	if (!aObject || !aReplacement)
	{
		return nullptr;
	}
	std::lock_guard lock(mutex_);
	void** vtable = *static_cast<void***>(aObject);
	void** slot = vtable + aIndex;
	for (const Entry& entry : entries_)
	{
		if (entry.slot == slot && *slot == aReplacement)
		{
			return entry.original;
		}
	}
	void* original = *slot;
	if (!WriteEntry(slot, aReplacement))
	{
		return nullptr;
	}
	entries_.push_back({ slot, original, aReplacement });
	return original;
}

size_t VtablePatcher::RestoreAll()
{
	std::lock_guard lock(mutex_);
	ReadableMemory  memory;
	size_t          restored = 0;
	for (const Entry& entry : entries_)
	{
		if (memory.Range(entry.slot, sizeof(void*)) && *entry.slot == entry.replacement && WriteEntry(entry.slot, entry.original))
		{
			++restored;
		}
	}
	entries_.clear();
	return restored;
}

size_t VtablePatcher::Count() const
{
	std::lock_guard lock(mutex_);
	return entries_.size();
}
}

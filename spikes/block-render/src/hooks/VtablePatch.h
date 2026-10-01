#pragma once

#include "Readable.h"

#include <cstddef>
#include <mutex>
#include <vector>

namespace cpmc
{
// Points entries of COM objects' vtables at replacements. Every object sharing the vtable then calls the
// replacement, which must call the original itself. Swapping one entry is a single atomic pointer write, so it's safe
// while other threads are calling through the vtable.
class VtablePatcher
{
public:
	// Patches entry aIndex of aObject's vtable. Returns the function the entry held before, which the replacement
	// must call: the original from the first patch if this entry already holds aReplacement (patching twice changes
	// nothing). Null if the entry couldn't be written.
	void* Patch(void* aObject, size_t aIndex, void* aReplacement);

	// Puts every original back where the entry still holds the replacement and its memory is still there. Returns how
	// many entries it restored.
	size_t RestoreAll();

	size_t Count() const;

private:
	struct Entry
	{
		void** slot;
		void*  original;
		void*  replacement;
	};

	mutable std::mutex mutex_;
	std::vector<Entry> entries_;
};
}

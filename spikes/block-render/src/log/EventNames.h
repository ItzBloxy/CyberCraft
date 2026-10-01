#pragma once

#include <cstdint>
#include <string>

namespace cpmc
{
// The text of a PIX event or marker (ID3D12GraphicsCommandList::BeginEvent and SetMarker): metadata 0 is a UTF-16
// string, 1 an ANSI string, and anything else (PIX 3's packed blobs) is searched for runs of printable text. Non-ASCII
// characters become '?'. At most 64 characters; "<event>" when there's no text to find.
std::string EventName(uint32_t aMetadata, const void* aData, uint32_t aSize);
}

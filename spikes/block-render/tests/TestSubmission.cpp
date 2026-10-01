#include "Harness.h"

#include "hooks/Submission.h"

namespace
{
// Lists are only compared here, never called, so any address will do.
ID3D12CommandList* Fake(uintptr_t aAddress)
{
	return reinterpret_cast<ID3D12CommandList*>(aAddress);
}
}

TEST(SubmissionsGoThroughUntouchedWithNothingToPutIn)
{
	ID3D12CommandList* lists[] = { Fake(0x10), Fake(0x20), Fake(0x30) };
	CHECK(cpmc::WithListsBefore(3, lists, [](ID3D12CommandList*) -> ID3D12CommandList* { return nullptr; }).empty());
	CHECK(cpmc::WithListsBefore(0, nullptr, [](ID3D12CommandList*) { return Fake(0x99); }).empty());
}

TEST(SubmissionsGetAListJustBeforeTheOneItGoesBefore)
{
	ID3D12CommandList* lists[] = { Fake(0x10), Fake(0x20), Fake(0x30), Fake(0x40) };
	// Ours goes before the third, as step 3's goes before the DLSS evaluate's list.
	const auto withOurs = cpmc::WithListsBefore(4, lists, [](ID3D12CommandList* aList) {
		return aList == Fake(0x30) ? Fake(0x99) : nullptr;
	});
	CHECK(withOurs == (std::vector<ID3D12CommandList*>{ Fake(0x10), Fake(0x20), Fake(0x99), Fake(0x30), Fake(0x40) }));
	// Before the first, and before more than one.
	const auto twice = cpmc::WithListsBefore(4, lists, [](ID3D12CommandList* aList) {
		return aList == Fake(0x10) || aList == Fake(0x40) ? Fake(0x99) : nullptr;
	});
	CHECK(twice == (std::vector<ID3D12CommandList*>{ Fake(0x99), Fake(0x10), Fake(0x20), Fake(0x30), Fake(0x99), Fake(0x40) }));
}

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

// Spike 7's snapshot ring (the spec's "The snapshot ring"): which of the renderer's upload buffers each frame's entities
// are copied into, and when one is free again. The main thread writes the newest frame into a buffer no list of ours may
// still read; a G-buffer list takes the newest, which is then held until the GPU passes the fence signalled after that
// list: after it, not before it, as Retired learnt from Task 52. In step B the frame's forward list takes it too, and it's
// held until the fence after that one. The newest is never written over, so a list can always take it. Pure; the
// renderer locks around it.
namespace cpmc
{
class Snapshots
{
public:
	explicit Snapshots(uint32_t aCount);

	// A buffer the main thread may write: not the newest, not taken by a list on its way to the GPU, and with every list
	// that took it done, aCompleted (the fence's completed value) at or past its fence. Nothing if all are held.
	std::optional<uint32_t> Writable(uint64_t aCompleted) const;
	void                    Written(uint32_t aIndex);  // it holds the newest frame now
	std::optional<uint32_t> Newest() const { return newest_; }

	// A list of ours takes the newest to read: it's held from now on. Nothing if none has been written yet.
	std::optional<uint32_t> TakeNewest();
	// Step B: a second list takes aIndex, a frame's G-buffer list's, for that frame's forward list, held as TakeNewest
	// holds it. Holding it only to the G-buffer list's fence would let the main thread rewrite it before the forward
	// list reads it. False if there's no such snapshot.
	bool                    Take(uint32_t aIndex);
	void                    Submitted(uint32_t aIndex, uint64_t aFence);  // that list went to the GPU, aFence after it
	void                    Abandon(uint32_t aIndex);                     // that list wasn't recorded after all

	uint32_t Count() const { return uint32_t(holds_.size()); }

private:
	struct Hold
	{
		uint32_t onTheirWay = 0;  // lists that took it, recorded and not yet submitted
		uint64_t fence = 0;       // the latest fence after a submitted list that took it
	};
	std::vector<Hold>       holds_;
	std::optional<uint32_t> newest_;
};
}

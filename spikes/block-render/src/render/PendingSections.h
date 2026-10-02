#pragma once

#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <map>
#include <utility>
#include <vector>

// The renderer's waiting uploads (the spec's "The renderer and the atlas"): the latest mesh for each section. Until
// drawing is first switched on nothing is uploaded, and Minecraft may send a section many times before the next upload,
// so each section waits once, with its latest mesh. Not thread-safe: the renderer holds its lock.
namespace cpmc
{
class PendingSections
{
public:
	void Put(const SectionPos& aSection, std::vector<Vertex> aVertices);  // replaces whatever waits for it; none: remove it
	std::vector<std::pair<SectionPos, std::vector<Vertex>>> Take();        // everything waiting, in section order
	void   Clear();
	size_t Size() const { return waiting_.size(); }

private:
	std::map<SectionPos, std::vector<Vertex>> waiting_;
};
}

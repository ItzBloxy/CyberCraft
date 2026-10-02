#include "render/PendingSections.h"

namespace cpmc
{
void PendingSections::Put(const SectionPos& aSection, std::vector<Vertex> aVertices)
{
	waiting_[aSection] = std::move(aVertices);
}

std::vector<std::pair<SectionPos, std::vector<Vertex>>> PendingSections::Take()
{
	std::vector<std::pair<SectionPos, std::vector<Vertex>>> taken;
	taken.reserve(waiting_.size());
	for (auto& [section, vertices] : waiting_)
	{
		taken.emplace_back(section, std::move(vertices));
	}
	waiting_.clear();
	return taken;
}

void PendingSections::Clear()
{
	waiting_.clear();
}
}

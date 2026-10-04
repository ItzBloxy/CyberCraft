#include "physx/BodyPlan.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace cpmc::px
{
namespace
{
constexpr double kFarthest = 1.0e6;  // blocks: a coordinate farther out is garbage

// How many sections apart two are, along the axis they're farthest apart on.
int32_t Apart(const SectionPos& aA, const SectionPos& aB)
{
	return std::max({ std::abs(aA.x - aB.x), std::abs(aA.y - aB.y), std::abs(aA.z - aB.z) });
}
}

std::optional<SectionPos> SectionAt(const Vec3d& aMc)
{
	const double at[3] = { aMc.x, aMc.y, aMc.z };
	if (!std::all_of(std::begin(at), std::end(at), [](double aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; }))
	{
		return std::nullopt;
	}
	return SectionPos{ int32_t(std::floor(aMc.x / 16.0)), int32_t(std::floor(aMc.y / 16.0)), int32_t(std::floor(aMc.z / 16.0)) };
}

Vec3d SectionCentre(const SectionPos& aSection)
{
	return WorldFromMc({ aSection.x * 16.0 + 8.0, aSection.y * 16.0 + 8.0, aSection.z * 16.0 + 8.0 });
}

LocalBox LocalBoxOf(const BlockBox& aBox)
{
	// Minecraft's x is the world's x, its y the world's z, and its z the world's -y.
	const Vec3d centre{ (aBox.x0 + aBox.x1) / 2.0 - 8.0, (aBox.y0 + aBox.y1) / 2.0 - 8.0, (aBox.z0 + aBox.z1) / 2.0 - 8.0 };
	return { WorldFromMc(centre), { (aBox.x1 - aBox.x0) / 2.0, (aBox.z1 - aBox.z0) / 2.0, (aBox.y1 - aBox.y0) / 2.0 } };
}

bool PosedAt(const Vec3d& aPhysx, const Vec3d& aWorld, const Vec3d& aOrigin)
{
	return std::hypot(aPhysx.x + aOrigin.x - aWorld.x, aPhysx.y + aOrigin.y - aWorld.y, aPhysx.z + aOrigin.z - aWorld.z) < 0.01;
}

std::vector<BodyPlan::Change> BodyPlan::Changes(const SectionPos& aV, bool aOn, const link::Solids& aSolids) const
{
	std::vector<Change> changes;
	for (const auto& [section, entry] : entries_)
	{
		const link::Solids::Section* solids = aSolids.Find(section);
		if (!aOn || !solids || Apart(section, aV) > 2)
		{
			changes.push_back({ false, section });
		}
		else if (solids->version != entry.version)
		{
			changes.push_back({ false, section });
			changes.push_back({ true, section, solids->version });
		}
	}
	if (!aOn)
	{
		return changes;
	}
	for (int32_t dx = -1; dx <= 1; ++dx)
	{
		for (int32_t dy = -1; dy <= 1; ++dy)
		{
			for (int32_t dz = -1; dz <= 1; ++dz)
			{
				const SectionPos section{ aV.x + dx, aV.y + dy, aV.z + dz };
				if (const link::Solids::Section* solids = aSolids.Find(section); solids && !entries_.contains(section))
				{
					changes.push_back({ true, section, solids->version });
				}
			}
		}
	}
	return changes;
}

void BodyPlan::Made(const SectionPos& aSection, uint64_t aVersion)
{
	entries_[aSection] = { aVersion, true };
}

void BodyPlan::Failed(const SectionPos& aSection, uint64_t aVersion)
{
	entries_[aSection] = { aVersion, false };
}

void BodyPlan::Removed(const SectionPos& aSection)
{
	entries_.erase(aSection);
}

void BodyPlan::Clear()
{
	entries_.clear();
}
}

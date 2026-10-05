#include "CombatInbox.h"

#include <utility>

namespace cpmc
{
void CombatInbox::SetTaking(bool aTaking)
{
	taking_.store(aTaking, std::memory_order_release);
}

bool CombatInbox::Taking() const
{
	return taking_.load(std::memory_order_acquire);
}

bool CombatInbox::Offer(const link::GameHurt& aHurt)
{
	const std::lock_guard lock(mutex_);
	if (Taking() && aHurt.nonLethal)
	{
		++counts_.nonLethal;
		return false;
	}
	if (!Taking() || hurts_.size() >= kMaxWaiting)
	{
		++counts_.refused;
		return false;
	}
	hurts_.push_back(aHurt);
	++counts_.taken;
	return true;
}

void CombatInbox::Land(const Landed& aLanded)
{
	const std::lock_guard lock(mutex_);
	if (landed_.size() >= kMaxWaiting)
	{
		++counts_.dropped;
		return;
	}
	landed_.push_back(aLanded);
	++counts_.landed;
}

void CombatInbox::End(const Ended& aEnded)
{
	const std::lock_guard lock(mutex_);
	if (ended_.size() >= kMaxWaiting)
	{
		++counts_.dropped;
		return;
	}
	ended_.push_back(aEnded);
	++counts_.ended;
}

std::vector<link::GameHurt> CombatInbox::TakeHurts()
{
	const std::lock_guard lock(mutex_);
	return std::exchange(hurts_, {});
}

std::vector<CombatInbox::Landed> CombatInbox::TakeLanded()
{
	const std::lock_guard lock(mutex_);
	return std::exchange(landed_, {});
}

std::vector<CombatInbox::Ended> CombatInbox::TakeEnded()
{
	const std::lock_guard lock(mutex_);
	return std::exchange(ended_, {});
}

CombatInbox::Counts CombatInbox::Count() const
{
	const std::lock_guard lock(mutex_);
	return counts_;
}
}

#include "Harness.h"

#include "CombatInbox.h"

#include <thread>

using namespace cpmc;

TEST(CombatInboxTakesHurtsOnlyWhileTaking)
{
	CombatInbox inbox;
	// Not taking: the hit is left to V.
	CHECK(!inbox.Offer({ 45.0, 10, 0x51 }));
	CHECK(inbox.TakeHurts().empty());
	inbox.SetTaking(true);
	CHECK(inbox.Taking());
	CHECK(inbox.Offer({ 45.0, 10, 0x51 }));
	CHECK(inbox.Offer({ 12.0, 9, 0 }));
	const std::vector<link::GameHurt> hurts = inbox.TakeHurts();
	CHECK_EQ(hurts.size(), size_t(2));
	CHECK(hurts.size() == 2 && hurts[0].amount == 45.0 && hurts[0].attackType == 10 && hurts[0].instigator == 0x51);
	CHECK(hurts.size() == 2 && hurts[1].amount == 12.0);
	CHECK(inbox.TakeHurts().empty());
	// With kMaxWaiting waiting, the next is left to V rather than lost.
	for (size_t i = 0; i < CombatInbox::kMaxWaiting; ++i)
	{
		CHECK(inbox.Offer({ 1.0, 9, 0 }));
	}
	CHECK(!inbox.Offer({ 1.0, 9, 0 }));
	inbox.SetTaking(false);
	CHECK(!inbox.Offer({ 1.0, 9, 0 }));
	const CombatInbox::Counts counts = inbox.Count();
	CHECK_EQ(counts.taken, uint64_t(2 + CombatInbox::kMaxWaiting));
	CHECK_EQ(counts.refused, uint64_t(3));
	CHECK_EQ(inbox.TakeHurts().size(), CombatInbox::kMaxWaiting);
}

TEST(CombatInboxKeepsLandingsUntilTaken)
{
	CombatInbox inbox;
	inbox.Land({ 7, 714.0, 1514.0, 800.0 });
	inbox.Land({ 8, 102.0, 48.0, 0.0 });
	const std::vector<CombatInbox::Landed> landed = inbox.TakeLanded();
	CHECK_EQ(landed.size(), size_t(2));
	CHECK(landed.size() == 2 && landed[0].target == 7 && landed[0].amount == 714.0 && landed[0].before == 1514.0 &&
	      landed[0].after == 800.0);
	CHECK(landed.size() == 2 && landed[1].target == 8);
	CHECK(inbox.TakeLanded().empty());
	for (size_t i = 0; i <= CombatInbox::kMaxWaiting; ++i)
	{
		inbox.Land({ 9, 1.0, 2.0, 1.0 });
	}
	CHECK_EQ(inbox.TakeLanded().size(), CombatInbox::kMaxWaiting);
	CHECK_EQ(inbox.Count().landed, uint64_t(2 + CombatInbox::kMaxWaiting));
	CHECK_EQ(inbox.Count().dropped, uint64_t(1));
}

// As the game does: the pipeline on several threads at once, while the main thread takes what they report.
TEST(CombatInboxTakesReportsFromManyThreadsAtOnce)
{
	constexpr int            kThreads = 8;
	constexpr int            kEach = 2000;
	CombatInbox              inbox;
	std::vector<std::thread> threads;
	std::atomic<int>         taken{ 0 };
	inbox.SetTaking(true);
	for (int t = 0; t < kThreads; ++t)
	{
		threads.emplace_back([&inbox, &taken, t] {
			for (int i = 0; i < kEach; ++i)
			{
				if (inbox.Offer({ 1.0, 9, uint64_t(t + 1) }))
				{
					++taken;
				}
				inbox.Land({ uint64_t(t + 1), 1.0, 2.0, 1.0 });
			}
		});
	}
	size_t hurts = 0;
	size_t landed = 0;
	for (int read = 0; read < 2000; ++read)
	{
		hurts += inbox.TakeHurts().size();
		landed += inbox.TakeLanded().size();
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	hurts += inbox.TakeHurts().size();
	landed += inbox.TakeLanded().size();
	const CombatInbox::Counts counts = inbox.Count();
	CHECK_EQ(hurts, size_t(taken.load()));
	CHECK_EQ(counts.taken + counts.refused, uint64_t(kThreads * kEach));
	CHECK_EQ(counts.taken, uint64_t(taken.load()));
	CHECK_EQ(uint64_t(landed), counts.landed);
	CHECK_EQ(counts.landed + counts.dropped, uint64_t(kThreads * kEach));
}

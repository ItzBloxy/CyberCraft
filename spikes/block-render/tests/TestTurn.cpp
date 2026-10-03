#include "Harness.h"

#include "link/Turn.h"

#include <cmath>

using namespace cpmc::link;

namespace
{
// One turn: aCounts every aEvery frames for aFrames frames, and V's yaw following by aDegreesACount a frame late, as the
// game may take them, then the mouse still until the turn ends. aYaw carries V's yaw from turn to turn; the game's stays
// between -180 and 180 degrees.
void Turn(TurnRatio& aRatio, double& aYaw, int64_t aCounts, int aFrames, double aDegreesACount, int aEvery = 1)
{
	int64_t late = 0;
	for (int i = 0; i < aFrames + TurnRatio::kStill; ++i)
	{
		const int64_t counts = i < aFrames && i % aEvery == 0 ? aCounts : 0;
		aYaw += double(late) * aDegreesACount;
		aRatio.Note(counts, std::remainder(aYaw, 360.0));
		late = counts;
	}
}
}

TEST(TurnRatioIsKnownAfterFiveTurns)
{
	TurnRatio ratio;
	double    yaw = 10.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 4; ++turn)
	{
		Turn(ratio, yaw, 30, 10, -0.02);  // 300 counts: 6 degrees
	}
	CHECK(!ratio.Ratio().has_value());
	CHECK_EQ(ratio.Turns(), uint64_t(4));
	CHECK_NEAR(ratio.Turn(100), 0.0, 1e-12);  // nothing is added until it's known
	Turn(ratio, yaw, 30, 10, -0.02);
	CHECK(ratio.Ratio().has_value());
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
	CHECK_NEAR(ratio.Turn(100), -2.0, 1e-12);
}

TEST(TurnRatioSkipsMenusAndSmallTurns)
{
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 10; ++turn)
	{
		Turn(ratio, yaw, 50, 10, 0.0);  // a menu: the mouse moves its cursor, and V doesn't turn
	}
	Turn(ratio, yaw, 5, 10, -0.05);  // 50 counts: too few, though V turned
	CHECK_EQ(ratio.Turns(), uint64_t(0));
	for (int turn = 0; turn < 5; ++turn)
	{
		Turn(ratio, yaw, 30, 10, -0.02);
	}
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioTakesTheMedianTurn)
{
	// A turn the game added to, a takedown's say, doesn't move it.
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	Turn(ratio, yaw, 30, 10, -0.02);
	Turn(ratio, yaw, 30, 10, -0.5);
	Turn(ratio, yaw, 30, 10, -0.021);
	Turn(ratio, yaw, 30, 10, -0.02);
	Turn(ratio, yaw, 30, 10, -0.019);
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioWrapsTheYaw)
{
	// Turning back and forth across 180 degrees, where the game's yaw jumps to -180: every turn crosses it.
	TurnRatio ratio;
	double    yaw = 177.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 5; ++turn)
	{
		Turn(ratio, yaw, turn % 2 == 0 ? 30 : -30, 10, 0.02);
	}
	CHECK_NEAR(ratio.Ratio().value_or(0.0), 0.02, 1e-12);
}

TEST(TurnRatioKeepsASlowTurnTogether)
{
	// The mouse moves every third frame: the frames between don't end the turn.
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 5; ++turn)
	{
		Turn(ratio, yaw, 40, 30, -0.02, 3);
	}
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioEndsALongTurnAnyway)
{
	// Turning without a pause for five turns' worth of frames: each is cut at kLongest, and only the first misses a
	// frame of its turn, to the game taking the counts late.
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	Turn(ratio, yaw, 10, TurnRatio::kLongest * 5, -0.02);
	CHECK_EQ(ratio.Turns(), uint64_t(5));
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioDropsATurnBrokenOff)
{
	// V gets into a car mid-turn, and the car turns V: that turn is dropped, and the yaw starts afresh after.
	TurnRatio ratio;
	ratio.Note(0, 0.0);
	ratio.Note(300, 0.0);
	ratio.Note(0, 90.0);
	ratio.Break();
	for (int i = 0; i < TurnRatio::kStill + 1; ++i)
	{
		ratio.Note(i == 0 ? 300 : 0, -45.0);
	}
	CHECK_EQ(ratio.Turns(), uint64_t(0));
}

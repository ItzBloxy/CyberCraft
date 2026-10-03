#include "Harness.h"

#include "log/DriveLog.h"

using namespace cpmc;

namespace
{
DriveRow Row(double aTime, const char* aState)
{
	DriveRow row;
	row.time = aTime;
	row.frameMs = 16.667;
	row.state = aState;
	row.camera = Vec3d{ -119.5, -2007.5, 7.74 };
	row.target = Vec3d{ -119.5, -2007.5, 7.76 };
	row.lastDistance = 0.02;
	row.delayMs = 17.716;
	row.teleportMs = 0.0123;
	row.mouseX = -12;
	row.yaw = 90.0;
	row.turn = 0.24;
	return row;
}
}

TEST(DriveLogWritesAHeaderAndARowAFrame)
{
	DriveLog log;
	CHECK(!log.Recording());
	CHECK(log.Start(1.0));
	CHECK(log.Recording());
	log.Add(Row(100.0, "driving"));
	DriveRow waiting = Row(100.016667, "waiting");
	waiting.camera.reset();
	waiting.target.reset();
	waiting.lastDistance.reset();
	waiting.yaw.reset();
	waiting.waited = true;
	log.Add(waiting);
	CHECK(!log.TakeFinished().has_value());  // still recording
	log.Add(Row(101.0, "driving"));          // a second from the first row: it's done, without this one
	const std::optional<DriveLog::Finished> finished = log.TakeFinished();
	CHECK(finished.has_value());
	CHECK(!log.Recording());
	CHECK(!log.TakeFinished().has_value());  // once
	if (!finished)
	{
		return;
	}
	CHECK_EQ(finished->rows, size_t(2));
	CHECK_STR(finished->csv,
	          "t,frame_ms,state,camera_x,camera_y,camera_z,target_x,target_y,target_z,last_distance,delay_ms,waited,"
	          "teleport_ms,mouse_x,yaw,turn\n"
	          "0.0000,16.667,driving,-119.5000,-2007.5000,7.7400,-119.5000,-2007.5000,7.7600,0.0200,17.716,0,0.0123,-12,"
	          "90.000,0.240\n"
	          "0.0167,16.667,waiting,,,,,,,,17.716,1,0.0123,-12,,0.240\n");
}

TEST(DriveLogStopsWhenFull)
{
	DriveLog log;
	CHECK(log.Start(60.0));
	CHECK(!log.Start(5.0));  // one at a time
	for (size_t i = 0; i < DriveLog::kMostRows; ++i)
	{
		log.Add(Row(double(i) * 0.001, "off"));
	}
	const std::optional<DriveLog::Finished> finished = log.TakeFinished();
	CHECK(finished.has_value());
	CHECK_EQ(finished ? finished->rows : 0, DriveLog::kMostRows);
	CHECK(!log.Recording());
	CHECK(log.Start(5.0));  // and another can start
}

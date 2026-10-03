#include "log/DriveLog.h"

#include <cmath>
#include <format>
#include <iterator>
#include <utility>

namespace cpmc
{
namespace
{
constexpr const char* kHeader = "t,frame_ms,state,camera_x,camera_y,camera_z,target_x,target_y,target_z,last_distance,"
                                "delay_ms,waited,teleport_ms,mouse_x,yaw,turn\n";

void AppendPoint(std::string& aOut, const std::optional<Vec3d>& aPoint)
{
	if (aPoint)
	{
		std::format_to(std::back_inserter(aOut), ",{:.4f},{:.4f},{:.4f}", aPoint->x, aPoint->y, aPoint->z);
	}
	else
	{
		aOut += ",,,";
	}
}
}

bool DriveLog::Start(double aSeconds)
{
	if (Recording() || !(aSeconds > 0.0))
	{
		return false;
	}
	seconds_ = aSeconds;
	start_.reset();
	csv_ = kHeader;
	rows_ = 0;
	return true;
}

void DriveLog::Add(const DriveRow& aRow)
{
	if (!Recording())
	{
		return;
	}
	if (!start_)
	{
		start_ = aRow.time;
	}
	const double t = aRow.time - *start_;
	if (t >= seconds_)
	{
		return Finish();
	}
	std::format_to(std::back_inserter(csv_), "{:.4f},{:.3f},{}", t, aRow.frameMs, aRow.state);
	AppendPoint(csv_, aRow.camera);
	AppendPoint(csv_, aRow.target);
	csv_ += aRow.lastDistance ? std::format(",{:.4f}", *aRow.lastDistance) : ",";
	std::format_to(std::back_inserter(csv_), ",{:.3f},{},{:.4f},{}", aRow.delayMs, aRow.waited ? 1 : 0, aRow.teleportMs, aRow.mouseX);
	csv_ += aRow.yaw ? std::format(",{:.3f}", *aRow.yaw) : ",";
	std::format_to(std::back_inserter(csv_), ",{:.3f}\n", aRow.turn);
	if (++rows_ >= kMostRows)
	{
		Finish();
	}
}

std::optional<DriveLog::Finished> DriveLog::TakeFinished()
{
	return std::exchange(finished_, std::nullopt);
}

void DriveLog::Finish()
{
	finished_ = Finished{ std::move(csv_), rows_ };
	csv_.clear();
	seconds_ = 0.0;
}
}

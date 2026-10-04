#include "link/Drive.h"

#include <cmath>

namespace cpmc::link
{
namespace
{
constexpr double kLevel = 0.005;     // metres the feet may move up or down between frames for a sample to count
constexpr double kEyeLowest = 0.5;   // a camera's height above the feet outside these isn't V's eye
constexpr double kEyeHighest = 2.5;
constexpr double kEyeWeight = 0.1;   // each sample's share of the average

bool Finite(const Vec3d& aPoint)
{
	return std::isfinite(aPoint.x) && std::isfinite(aPoint.y) && std::isfinite(aPoint.z);
}

double Distance(const Vec3d& aA, const Vec3d& aB)
{
	return std::sqrt((aA.x - aB.x) * (aA.x - aB.x) + (aA.y - aB.y) * (aA.y - aB.y) + (aA.z - aB.z) * (aA.z - aB.z));
}
}

const char* DriveStateName(DriveState aState)
{
	switch (aState)
	{
	case DriveState::Waiting: return "waiting";
	case DriveState::Driving: return "driving";
	default: return "off";
	}
}

void EyeHeight::Note(const Vec3d& aFeet, const Vec3d& aCamera)
{
	if (lastFeetZ_ && std::abs(aFeet.z - *lastFeetZ_) < kLevel)
	{
		const double height = aCamera.z - aFeet.z;
		if (height > kEyeLowest && height < kEyeHighest)
		{
			value_ = measured_ ? value_ + (height - value_) * kEyeWeight : height;
			measured_ = true;
		}
	}
	lastFeetZ_ = aFeet.z;
}

void EyeHeight::Break()
{
	lastFeetZ_.reset();
}

Vec3d FeetUnderEye(const TickSample& aMinecraft, double aEyeHeight)
{
	const Vec3d eye = WorldFromMc({ aMinecraft.feet.x, aMinecraft.feet.y + aMinecraft.eyeHeight, aMinecraft.feet.z });
	return { eye.x, eye.y, eye.z - aEyeHeight };
}

void Drive::Enter(const Vec3d& aFeet, double aNow)
{
	state_ = DriveState::Waiting;
	since_ = aNow;
	hold_ = aFeet;
	badSince_.reset();
	eye_.reset();
}

void Drive::Leave()
{
	state_ = DriveState::Off;
	badSince_.reset();
	eye_.reset();
}

Drive::Step Drive::Update(double aNow, bool aAcked, const std::optional<TickSample>& aMinecraft, const Vec3d& aFeet, double aEyeHeight)
{
	Step step;
	if (state_ == DriveState::Off)
	{
		return step;
	}
	if (state_ == DriveState::Driving && !aAcked)
	{
		// Minecraft teleported its player by itself, and holds it: V stays where it is until it lets go again.
		state_ = DriveState::Waiting;
		since_ = aNow;
		hold_ = aFeet;
		eye_.reset();
	}
	if (state_ == DriveState::Waiting)
	{
		if (!aAcked)
		{
			step.place = hold_;
			return step;
		}
		state_ = DriveState::Driving;
		since_ = aNow;
		badSince_.reset();
	}
	const char* bad = nullptr;
	if (!aMinecraft || !Finite(aMinecraft->feet) || !std::isfinite(aMinecraft->eyeHeight))
	{
		bad = "Minecraft's player had no usable position for a second";
	}
	else
	{
		const Vec3d target = FeetUnderEye(*aMinecraft, aEyeHeight);
		eye_ = Vec3d{ target.x, target.y, target.z + aEyeHeight };
		if (Distance(target, aFeet) > kFarthest)
		{
			bad = "Minecraft's player stayed over 20 m from V for a second";
		}
		else
		{
			badSince_.reset();
			step.place = target;
			return step;
		}
	}
	++skipped_;
	if (!badSince_)
	{
		badSince_ = aNow;
	}
	if (aNow - *badSince_ >= kBadFor)
	{
		step.end = bad;
	}
	return step;
}
}

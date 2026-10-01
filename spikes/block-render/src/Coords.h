#pragma once

// Game world (REDengine: metres, Z up, Y north) <-> Minecraft axes (Y up, Z south).
// 1 world unit = 1 block = 1 metre. The mapping is a proper rotation, so triangle winding is kept.
namespace cpmc
{
struct Vec3d
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct Vec3f
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

constexpr Vec3d McFromWorld(const Vec3d& aWorld)
{
	return { aWorld.x, aWorld.z, -aWorld.y };
}

constexpr Vec3d WorldFromMc(const Vec3d& aMc)
{
	return { aMc.x, -aMc.z, aMc.y };
}
}

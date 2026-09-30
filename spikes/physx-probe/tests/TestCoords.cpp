#include "Harness.h"

#include "Coords.h"

using cpmc::McFromWorld;
using cpmc::Vec3d;
using cpmc::WorldFromMc;

TEST(McFromWorldMapsAxes)
{
	// World east (+X) stays east; world up (+Z) becomes Minecraft up (+Y); world north (+Y) becomes Minecraft -Z.
	const Vec3d mc = McFromWorld({ 1.0, 2.0, 3.0 });
	CHECK_NEAR(mc.x, 1.0, 0.0);
	CHECK_NEAR(mc.y, 3.0, 0.0);
	CHECK_NEAR(mc.z, -2.0, 0.0);
}

TEST(WorldFromMcUndoesMcFromWorld)
{
	const Vec3d world{ -123.5, 4567.25, 89.125 };
	const Vec3d back = WorldFromMc(McFromWorld(world));
	CHECK_NEAR(back.x, world.x, 0.0);
	CHECK_NEAR(back.y, world.y, 0.0);
	CHECK_NEAR(back.z, world.z, 0.0);
}

TEST(McFromWorldIsAProperRotation)
{
	// Mapped X cross mapped Y must equal mapped Z (a determinant of +1), so winding is preserved.
	const Vec3d x = McFromWorld({ 1, 0, 0 });
	const Vec3d y = McFromWorld({ 0, 1, 0 });
	const Vec3d z = McFromWorld({ 0, 0, 1 });
	CHECK_NEAR(x.y * y.z - x.z * y.y, z.x, 0.0);
	CHECK_NEAR(x.z * y.x - x.x * y.z, z.y, 0.0);
	CHECK_NEAR(x.x * y.y - x.y * y.x, z.z, 0.0);
}

#include "Harness.h"

#include "McLinkScript.h"

#include <fstream>
#include <sstream>

using namespace cpmc::mclink;
using Kind = Action::Kind;

TEST(ScriptsInTheRepoParse)
{
	// The tests run in spikes/block-render, where the scripts are.
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt", "npcs.txt", "hurts.txt" })
	{
		std::ifstream     in(std::string("tools/mclink/") + name, std::ios::binary);
		std::stringstream text;
		text << in.rdbuf();
		std::string error;
		const auto  actions = ParseScript(text.str(), error);
		CHECK(in.good() || in.eof());
		CHECK(actions.has_value() && !actions->empty());
		if (!actions)
		{
			std::printf("  %s: %s\n", name, error.c_str());
		}
	}
}

TEST(ScriptParsesEveryAction)
{
	const char* script = R"(# step 1
status
wait-world 180

link on
build off
hud on
at -119.5 6.14 2007.5
look 180 60
wait 0.5
type creative
key 30
click right
floor
save-atlas atlas.dds
save-hud hud.dds
hold-check 10
quit
)";
	std::string error;
	const auto  actions = ParseScript(script, error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(16));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Status);
	CHECK_EQ(a[0].line, 2);
	CHECK(a[1].kind == Kind::WaitWorld);
	CHECK_EQ(a[1].numbers[0], 180.0);
	CHECK(a[2].kind == Kind::Link && a[2].on);
	CHECK_EQ(a[2].line, 5);
	CHECK(a[3].kind == Kind::Build && !a[3].on);
	CHECK(a[4].kind == Kind::Hud && a[4].on);
	CHECK(a[5].kind == Kind::At);
	CHECK_EQ(a[5].numbers[0], -119.5);
	CHECK_EQ(a[5].numbers[1], 6.14);
	CHECK_EQ(a[5].numbers[2], 2007.5);
	CHECK(a[6].kind == Kind::Look);
	CHECK_EQ(a[6].numbers[1], 60.0);
	CHECK(a[7].kind == Kind::Wait);
	CHECK_EQ(a[7].numbers[0], 0.5);
	CHECK(a[8].kind == Kind::Type);
	CHECK_STR(a[8].text, "creative");
	CHECK(a[9].kind == Kind::Key);
	CHECK_EQ(a[9].numbers[0], 30.0);
	CHECK(a[10].kind == Kind::Click);
	CHECK_EQ(a[10].numbers[0], 3.0);
	CHECK(a[11].kind == Kind::Floor);
	CHECK(a[12].kind == Kind::SaveAtlas);
	CHECK_STR(a[12].text, "atlas.dds");
	CHECK(a[13].kind == Kind::SaveHud);
	CHECK(a[14].kind == Kind::HoldCheck);
	CHECK_EQ(a[14].numbers[0], 10.0);
	CHECK(a[15].kind == Kind::Quit);
}

TEST(ScriptParsesHeldKeysAndTracking)
{
	std::string error;
	const auto  actions = ParseScript("down 26\nwait 2\nup 26\ntrack 5\n", error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(4));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Down);
	CHECK_EQ(a[0].numbers[0], 26.0);
	CHECK(a[2].kind == Kind::Up);
	CHECK_EQ(a[2].numbers[0], 26.0);
	CHECK(a[3].kind == Kind::Track);
	CHECK_EQ(a[3].numbers[0], 5.0);
	CHECK(!ParseScript("down 0", error).has_value());
	CHECK(!ParseScript("up 600", error).has_value());
	CHECK(!ParseScript("down", error).has_value());
	CHECK(!ParseScript("track 0", error).has_value());
	CHECK_STR(error, "line 1: track takes a number of seconds: \"track 0\"");
}

TEST(ScriptTypeKeepsTheWholeCommand)
{
	std::string error;
	const auto  actions = ParseScript("  type /setblock ~2 ~ ~ minecraft:stone  \r\n", error);
	CHECK(actions.has_value());
	CHECK_STR(actions->at(0).text, "/setblock ~2 ~ ~ minecraft:stone");
}

TEST(ScriptWaitWorldDefaultsToTwoMinutes)
{
	std::string error;
	CHECK_EQ(ParseScript("wait-world", error)->at(0).numbers[0], 120.0);
}

TEST(ScriptRejectsBadLines)
{
	std::string error;
	CHECK(!ParseScript("status\nfly away\n", error).has_value());
	CHECK_STR(error, "line 2: unknown action: \"fly away\"");
	CHECK(!ParseScript("wait soon", error).has_value());
	CHECK(!ParseScript("look 0 95", error).has_value());
	CHECK(!ParseScript("key 0", error).has_value());
	CHECK(!ParseScript("key 2.5", error).has_value());
	CHECK(!ParseScript("click sideways", error).has_value());
	CHECK(!ParseScript("at 1 2", error).has_value());
	CHECK(!ParseScript("link maybe", error).has_value());
	CHECK(!ParseScript("type", error).has_value());
	CHECK(!ParseScript("status now", error).has_value());
}

TEST(ScriptParsesHurts)
{
	std::string error;
	const auto  actions = ParseScript("hurt melee 0.25\nhurt projectile 0.05 3 power\nhurt other 1 0\nhurt magic 0.1 power\n", error);
	CHECK(actions.has_value());
	CHECK_EQ(actions ? actions->size() : 0, size_t(4));
	if (actions && actions->size() == 4)
	{
		const auto& a = *actions;
		CHECK(a[0].kind == Kind::Hurt);
		CHECK_EQ(a[0].numbers[0], 0.0);  // melee
		CHECK_EQ(a[0].numbers[1], 0.25);
		CHECK_EQ(a[0].numbers[2], 0.0);  // no attacker
		CHECK(!a[0].on);
		CHECK_EQ(a[1].numbers[0], 1.0);  // projectile
		CHECK_EQ(a[1].numbers[2], 3.0);
		CHECK(a[1].on);  // a power attack
		CHECK_EQ(a[2].numbers[0], 3.0);  // other
		CHECK_EQ(a[2].numbers[1], 1.0);
		CHECK_EQ(a[3].numbers[0], 2.0);  // magic
		CHECK(a[3].on);
	}
	CHECK(!ParseScript("hurt fire 0.1", error).has_value());
	CHECK(!ParseScript("hurt melee 0", error).has_value());
	CHECK(!ParseScript("hurt melee 1.5", error).has_value());
	CHECK(!ParseScript("hurt melee 0.1 1.5", error).has_value());
	CHECK(!ParseScript("hurt melee 0.1 3 hard", error).has_value());
	CHECK(!ParseScript("hurt melee", error).has_value());
	CHECK(!ParseScript("hurt melee 0.1 3 power extra", error).has_value());
}

TEST(ScriptParsesMadeUpNpcs)
{
	std::string error;
	const auto  actions = ParseScript("actor 1 520.5 6.14 2005.5\nactor 2 523.5 6.14 2007.5 90 0.64 1.72 Ganger  Two \n"
	                                  "actor-remove 2\nactor-remove all\n",
	                                  error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(4));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Actor);
	CHECK_EQ(a[0].numbers[0], 1.0);
	CHECK_EQ(a[0].numbers[3], 2005.5);
	CHECK_EQ(a[0].numbers[4], 0.0);  // facing south
	CHECK_EQ(a[0].numbers[5], 0.6);  // person-sized
	CHECK_EQ(a[0].numbers[6], 1.8);
	CHECK_STR(a[0].text, "");
	CHECK_EQ(a[1].numbers[4], 90.0);
	CHECK_EQ(a[1].numbers[5], 0.64);
	CHECK_EQ(a[1].numbers[6], 1.72);
	CHECK_STR(a[1].text, "Ganger  Two");  // as written, but for the line's ends
	CHECK(a[2].kind == Kind::ActorRemove);
	CHECK_EQ(a[2].numbers[0], 2.0);
	CHECK(a[3].kind == Kind::ActorRemove);
	CHECK_EQ(a[3].numbers[0], 0.0);  // all
	CHECK(!ParseScript("actor 0 1 2 3", error).has_value());
	CHECK(!ParseScript("actor 4294967296 1 2 3", error).has_value());
	CHECK(!ParseScript("actor 1.5 1 2 3", error).has_value());
	CHECK(!ParseScript("actor 1 1 2", error).has_value());
	CHECK(!ParseScript("actor 1 1 2 3 90", error).has_value());        // a yaw needs a size with it
	CHECK(!ParseScript("actor 1 1 2 3 90 0 1.8", error).has_value());  // no width
	CHECK(!ParseScript("actor-remove", error).has_value());
	CHECK(!ParseScript("actor-remove some", error).has_value());
	CHECK_STR(error, "line 1: actor-remove takes a number from 1 to 4294967295, or all: \"actor-remove some\"");
}

TEST(ScriptParsesTheGround)
{
	std::string error;
	const auto  actions =
	    ParseScript("ground city\ndump E:\\dumps\\2026-10-01_14-37-07\\collision.obj\nwait-ground\nwait-ground 60\nground floor\n", error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(5));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Ground);
	CHECK_STR(a[0].text, "city");
	CHECK(a[1].kind == Kind::Dump);
	CHECK_STR(a[1].text, "E:\\dumps\\2026-10-01_14-37-07\\collision.obj");
	CHECK(a[2].kind == Kind::WaitGround);
	CHECK_EQ(a[2].numbers[0], 30.0);
	CHECK_EQ(a[3].numbers[0], 60.0);
	CHECK_STR(a[4].text, "floor");
	CHECK(!ParseScript("ground stone", error).has_value());
	CHECK_STR(error, "line 1: ground takes floor or city: \"ground stone\"");
	CHECK(!ParseScript("dump", error).has_value());
	CHECK(!ParseScript("wait-ground 0", error).has_value());
}

#include "Harness.h"

#include "McLinkScript.h"

#include <fstream>
#include <sstream>

using namespace cpmc::mclink;
using Kind = Action::Kind;

TEST(ScriptsInTheRepoParse)
{
	// The tests run in spikes/block-render, where the scripts are.
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt" })
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

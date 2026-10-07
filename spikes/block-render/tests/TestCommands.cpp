#include "Harness.h"

#include "link/Commands.h"

using namespace cpmc::link;

namespace
{
const CommandTyper::Minecraft kReady{ true, false };
const CommandTyper::Minecraft kChatOpen{ true, true };

bool IsKey(const proto::InputEvent& aEvent, uint16_t aScancode, int32_t aDown)
{
	return aEvent.type == proto::kInKey && aEvent.code == aScancode && aEvent.a == aDown;
}
}

TEST(CommandsNamedSetsAreValidCommands)
{
	for (const char* name : { "creative", "tnt-kit", "tnt-ahead", "wall", "tnt-cube", "field", "wipe" })
	{
		const auto commands = NamedCommands(name);
		CHECK(commands.has_value());
		for (const auto& command : *commands)
		{
			std::string error;
			CHECK(ValidCommand(command, error));
			// "Ahead" is level, whatever the camera's pitch.
			CHECK(!command.starts_with("/execute") || command.starts_with("/execute rotated ~ 0 positioned ^ ^ ^"));
		}
	}
	CHECK_EQ(NamedCommands("tnt-cube")->size(), size_t(3));
	CHECK_STR(NamedCommands("tnt-cube")->front(), "/gamemode creative");
	CHECK_EQ(NamedCommands("tnt-ahead")->size(), size_t(4));
	// TNT needs a block under it: obsidian, which its blast can't break, so nothing drops; a marker lets Clear remove it.
	CHECK_STR(NamedCommands("tnt-ahead")->at(1), "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ obsidian");
	CHECK_STR(NamedCommands("tnt-ahead")->at(2), "/execute rotated ~ 0 positioned ^ ^ ^4 run summon marker ~ ~-1 ~ {Tags:[\"sp7b\"]}");
	CHECK(!NamedCommands("explode").has_value());
}

TEST(CommandsValidation)
{
	std::string error;
	CHECK(ValidCommand("/gamemode creative", error));
	CHECK(!ValidCommand("", error));
	CHECK(!ValidCommand("gamemode creative", error));
	CHECK(!ValidCommand("/" + std::string(256, 'a'), error));
	CHECK(error.find("257") != std::string::npos);
	CHECK(!ValidCommand("/say caf\xc3\xa9", error));
	CHECK(!ValidCommand("/say a\nb", error));
}

TEST(CommandsTypesACommand)
{
	CommandTyper typer;
	std::string  error;
	CHECK(typer.Queue("/gamemode creative", error));
	auto step = typer.Advance(0.0, kReady);
	CHECK_EQ(step.events.size(), size_t(2));
	CHECK(IsKey(step.events[0], kSdlT, 1));
	CHECK(IsKey(step.events[1], kSdlT, 0));
	CHECK(typer.Advance(0.1, kReady).events.empty());  // chat opens on Minecraft's next tick
	step = typer.Advance(0.2, kChatOpen);
	CHECK(step.entered);
	CHECK_EQ(step.events.size(), size_t(18 + 2));
	CHECK(step.events[0].type == proto::kInText && step.events[0].a == '/');
	CHECK(step.events[17].type == proto::kInText && step.events[17].a == 'e');
	CHECK(IsKey(step.events[18], kSdlReturn, 1));
	CHECK(IsKey(step.events[19], kSdlReturn, 0));
	CHECK(typer.Advance(0.3, kChatOpen).events.empty());
	step = typer.Advance(0.4, kReady);
	CHECK_EQ(step.log.size(), size_t(1));
	CHECK_STR(step.log[0], "typed: /gamemode creative");
	CHECK_EQ(typer.Waiting(), size_t(0));
}

TEST(CommandsSetsAreTypedInOrder)
{
	CommandTyper typer;
	std::string  error;
	CHECK(typer.Queue("tnt-kit", error));
	CHECK_EQ(typer.Waiting(), size_t(2));
	std::vector<std::string> log;
	for (int round = 0; round < 2; ++round)
	{
		typer.Advance(round * 1.0, kReady);
		typer.Advance(round * 1.0 + 0.1, kChatOpen);
		for (auto& line : typer.Advance(round * 1.0 + 0.2, kReady).log)
		{
			log.push_back(line);
		}
	}
	CHECK_EQ(log.size(), size_t(2));
	CHECK_STR(log[0], "typed: /give @s tnt 64");
	CHECK_STR(log[1], "typed: /give @s flint_and_steel");
}

TEST(CommandsAreDroppedWhenMinecraftIsntReady)
{
	CommandTyper typer;
	std::string  error;
	typer.Queue("creative", error);
	auto step = typer.Advance(0.0, { false, false });
	CHECK(step.events.empty());
	CHECK_STR(step.log[0], "not typed: Minecraft isn't in its world: /gamemode creative");
	typer.Queue("creative", error);
	step = typer.Advance(0.0, kChatOpen);
	CHECK(step.events.empty());
	CHECK_STR(step.log[0], "not typed: a Minecraft screen is open: /gamemode creative");
	CHECK_EQ(typer.Waiting(), size_t(0));
}

TEST(CommandsTimeOut)
{
	CommandTyper typer;
	std::string  error;
	typer.Queue("creative", error);
	typer.Advance(0.0, kReady);
	auto step = typer.Advance(2.5, kReady);
	CHECK_STR(step.log[0], "not typed: chat didn't open within 2 s: /gamemode creative");
	typer.Queue("creative", error);
	typer.Advance(10.0, kReady);
	typer.Advance(10.1, kChatOpen);
	step = typer.Advance(12.5, kChatOpen);
	CHECK_STR(step.log[0], "typed, but chat didn't close within 2 s: /gamemode creative");
	CHECK_EQ(typer.Waiting(), size_t(0));
}

TEST(CommandsClearDropsEverything)
{
	CommandTyper typer;
	std::string  error;
	typer.Queue("tnt-cube", error);
	typer.Advance(0.0, kReady);
	typer.Clear();
	CHECK_EQ(typer.Waiting(), size_t(0));
	CHECK(typer.Advance(0.1, kChatOpen).events.empty());
}

TEST(CommandsQueueRefusesNonsense)
{
	CommandTyper typer;
	std::string  error;
	CHECK(!typer.Queue("explode", error));
	CHECK(error.find("neither a command nor a set") != std::string::npos);
	CHECK(!typer.Queue("/" + std::string(300, 'x'), error));
	CHECK(error.find("at most 256") != std::string::npos);
	CHECK_EQ(typer.Waiting(), size_t(0));
}

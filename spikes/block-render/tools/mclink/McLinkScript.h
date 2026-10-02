#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// mclink's scripts: one action a line, run in order at 60 Hz. Blank lines and lines starting with # are skipped.
//   wait <seconds>                 pause
//   wait-world [seconds]           until Minecraft is in its world (120 s unless given), or give up
//   link on|off                    the in-game flag
//   build on|off                   build mode: off sets SkyState's menu flag (on by default)
//   hud on|off                     take HUD images (on by default)
//   at <x> <y> <z>                 Minecraft's player's feet, in Minecraft blocks
//   look <yaw> <pitch>             Minecraft degrees
//   type <command or set>          type it, and wait until it's typed (10 s at most)
//   key <scancode>                 press and release an SDL scancode
//   click left|middle|right        press and release a mouse button
//   floor                          send a fresh floor
//   save-atlas <file.dds>          wait for Minecraft's atlas (30 s at most), and save it
//   save-hud <file.dds>            wait for the next HUD image (5 s at most), save it top row first, and check its alpha
//   hold-check <seconds>           how far Minecraft's feet stray from the last teleport target meanwhile
//   status                         print the link's state
//   quit                           stop here
namespace cpmc::mclink
{
struct Action
{
	enum class Kind
	{
		Wait,
		WaitWorld,
		Link,
		Build,
		Hud,
		At,
		Look,
		Type,
		Key,
		Click,
		Floor,
		SaveAtlas,
		SaveHud,
		HoldCheck,
		Status,
		Quit,
	};

	Kind        kind = Kind::Quit;
	double      numbers[3]{};  // seconds; x, y, z; yaw and pitch; the scancode; the button (1 left, 2 middle, 3 right)
	bool        on = false;    // link, build and hud
	std::string text;          // the command, or the file
	int         line = 0;
};

// The script's actions, or nothing with aError naming the first bad line.
std::optional<std::vector<Action>> ParseScript(std::string_view aText, std::string& aError);
}

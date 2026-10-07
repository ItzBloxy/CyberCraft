#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// mclink's scripts: one action a line, run in order at 60 Hz. Blank lines and lines starting with # are skipped.
//   wait <seconds>                 pause
//   wait-world [seconds]           until Minecraft is in its world (120 s unless given), or give up
//   link on|off                    the in-game flag
//   build on|off                   build mode (off at first): on, Minecraft drives and the stand-in V follows; off, the
//                                  menu flag is set and Minecraft lets go of everything
//   hud on|off                     take HUD images (on by default)
//   at <x> <y> <z>                 the stand-in V's feet, in Minecraft blocks
//   look <yaw> <pitch>             Minecraft degrees
//   type <command or set>          type it, and wait until it's typed (10 s at most)
//   key <scancode>                 press and release an SDL scancode at once
//   down <scancode>                press an SDL scancode, and hold it
//   up <scancode>                  let it go
//   click left|middle|right        press and release a mouse button
//   floor                          send a fresh floor
//   save-atlas <file.dds>          wait for Minecraft's atlas (30 s at most), and save it
//   save-hud <file.dds>            wait for the next HUD image (5 s at most), save it top row first, and check its alpha
//   hold-check <seconds>           how far Minecraft's feet stray from the last teleport target meanwhile
//   track <seconds>                print Minecraft's player and the drive four times a second meanwhile
//   ground floor|city              what Minecraft gets as the game's shape (floor at first)
//   dump <collision.obj>           spike 1's saved collision as the city: its static shapes, around the stand-in V's feet
//   wait-ground [seconds]          until the city around the centre is all read and in the ring (30 s unless given)
//   actor <id> <x> <y> <z> [<yaw> <width> <height> [<name>]]
//                                  a made-up NPC in SkyCraft's actor table (spike 6), or one moved: its number, from 1
//                                  to 4294967295; its feet, in Minecraft blocks; its yaw, in Minecraft degrees, and its
//                                  size, in blocks above 0 (0, 0.6 and 1.8 unless given); then a name to the line's end
//   actor-remove <id>|all          one made-up NPC out of the table, or all of them
//   hurt <kind> <share> [<attacker>] [power]
//                                  V hurt, as the plugin sends it (spike 6's Part 2): melee, projectile, magic or other;
//                                  its share of V's health, over 0 and up to 1; the made-up NPC that did it, or 0; and
//                                  power for a power attack. It goes through the plugin's sums: at once, or in a sum
//   health <share>                 Minecraft's player's health set to that share of its max, over 0 and up to 1, as the
//                                  plugin sends V's as build mode starts (spike 6's follow-up): CyberCraft Tweaks 1.3.0 sets it
//   scene                          print the newest scene, batch by batch (spike 7's step 0), and the world-entity table
//   save-textures <folder>         save every entity texture received so far as <folder>\texture-<id>.dds
//   cursor <x> <y>                 Minecraft's cursor to that pixel of its HUD image, its window's (spike 8's step 0)
//   text <text>                    each character to the line's end typed into Minecraft's open screen, as kInText
//   pause                          open Minecraft's pause menu (kInOpenMenu)
//   wait-screen open|closed [seconds]
//                                  until Minecraft says a screen is open, or that none is (5 s unless given)
//   hud-box <x0> <y0> <x1> <y1>    wait for the next HUD image (5 s at most), and print the box's mean colour, the far
//                                  edges left out
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
		Down,
		Up,
		Click,
		Floor,
		SaveAtlas,
		SaveHud,
		HoldCheck,
		Track,
		Ground,
		Dump,
		WaitGround,
		Actor,
		ActorRemove,
		Hurt,
		Health,
		Scene,
		SaveTextures,
		Cursor,
		Text,
		Pause,
		WaitScreen,
		HudBox,
		Status,
		Quit,
	};

	Kind   kind = Kind::Quit;
	double numbers[7]{};  // seconds; x, y, z; yaw and pitch; the scancode; the button (1 left, 2 middle, 3 right); an
	                      // actor's number, x, y, z, yaw, width and height; the actor to remove, or 0 for all; a hurt's
	                      // kind (proto::HurtKind), share and attacker; the health's share; the cursor's x and y; a
	                      // box's x0, y0, x1 and y1
	bool        on = false;  // link, build and hud; a hurt's power attack; wait-screen open
	std::string text;        // the command, the file, the folder, the ground, an actor's name, or the text to type
	int         line = 0;
};

// The script's actions, or nothing with aError naming the first bad line.
std::optional<std::vector<Action>> ParseScript(std::string_view aText, std::string& aError);

// UTF-8 text's code points, or nothing if it isn't well-formed UTF-8: no overlong forms, surrogates, or code points
// past U+10FFFF.
std::optional<std::vector<uint32_t>> CodePoints(std::string_view aUtf8);
}

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace cpmc
{
// One command from blockrender.request (see the spec's "Requests"):
//   log
//   dump depth|mvec|color|gbuffer|all
//   mode off|present|predlss|gbuffer
//   place cube|sampler|house
//   place field <n>            (n: 1 to 1,000,000 blocks)
//   clear
//   reload
//   set <name> <value>         (name: lower-case letters, digits, '.' and '_'; value: a finite number)
//   set ground floor|city      (spike 5: what Minecraft gets as the game's shape; the number is 0 or 1)
//   link on|off                (spike 4: the in-game flag)
//   build on|off               (build mode, as F8: on links first, if it isn't linked)
//   floor                      (a fresh floor around V)
//   mc <set>|<command>         (a set: creative, tnt-kit, tnt-ahead, wall, tnt-cube, field or wipe; a command starts with
//                               '/' and runs verbatim to the end of the line)
//   drivelog <seconds>         (spike 5: build mode's drive, a row a frame, for 1 to 60 seconds)
//   scenewatch on|off          (spike 5's step 3: who calls scene 0, and when)
//   shapes                     (spike 5: the game's shapes within 2 m of V, into the log)
//   box on|off                 (spike 5's step 4: a test box 3 m ahead of V, made as the bodies for blocks are)
//   npcs                       (spike 6: every NPC listed, into the log, and what the game's targeting finds)
//   hit <id> <damage> [strong] (spike 6: as if Minecraft hit NPC <id>, from 1, for <damage>, over 0, of its damage)
//   punch                      (spike 6: V's punch, what it's read from, and what hits are made with, into the log)
//   explode <metres> <radius>  (spike 6's Part 3: a blast that far ahead of V, from 0 to 100, as if it were Minecraft's,
//                               with its radius, over 0 and up to 16)
//   entities                   (spike 7: the newest scene batch by batch, the textures and the world entities, into the
//                               log)
struct Request
{
	enum class Kind
	{
		Log,
		Dump,
		Mode,
		Place,
		Clear,
		Reload,
		Set,
		Link,
		Floor,
		Mc,
		Build,
		DriveLog,
		SceneWatch,
		Shapes,
		Box,
		Npcs,
		Hit,
		Punch,
		Explode,
		Entities,
	};

	Kind        kind = Kind::Log;
	std::string what;          // the dump target, mode, structure, setting name, on or off, what to type, or a hit's
	                           // strong or quick
	double      number = 0.0;  // the field's block count, the setting's value, the drive log's seconds, a hit's damage, or
	                           // how far ahead of V a blast is
	uint32_t    id = 0;        // a hit's NPC: its number in SkyCraft's actor table
	double      radius = 0.0;  // a blast's, as Minecraft gives one
};

// The command in aText (one line; surrounding whitespace is ignored). Nothing, with aError saying why, if the text
// isn't exactly one valid command.
std::optional<Request> ParseRequest(std::string_view aText, std::string& aError);

// settings.txt, read once at load (see the spec's "Safety"): lines of "name = value", where "#" starts a comment. Two
// settings, both off unless set to 1. Other names and values are ignored.
struct PluginSettings
{
	bool dred = false;        // DRED's breadcrumbs and page faults
	bool debugLayer = false;  // the D3D12 debug layer
};

PluginSettings ParseSettings(std::string_view aText);
}

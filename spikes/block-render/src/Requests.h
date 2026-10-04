#pragma once

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
	};

	Kind        kind = Kind::Log;
	std::string what;          // the dump target, mode, structure, setting name, on or off, or what to type
	double      number = 0.0;  // the field's block count, the setting's value, or the drive log's seconds
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

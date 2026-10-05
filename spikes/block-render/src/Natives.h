#pragma once

#include "CombatInbox.h"
#include "LogQueue.h"
#include "NpcRegistry.h"

// BlockRender's native functions for its redscript module, which declares them (scripts/BlockRender.reds). The module's
// wraps of NPCPuppet's OnGameAttached and OnDetach report each NPC through them, on whichever thread the game calls
// those on, and its List reads the list back on the main thread (spike 6's "The module's list"). Its wraps of the damage
// pipeline offer hits on V, and report our hits landing or dealt nothing, through them, on whichever thread runs the
// pipeline (Part 2).
// They're added once the game has registered its own types, which is before it loads its scripts.
namespace cpmc
{
// From Main's Load: asks the game's type system to call back once its types are registered, to add the natives; false if
// there's no type system yet. aLog gets a line when they're added.
bool RegisterNatives(LogQueue& aLog);

// The NPCs the wraps report.
NpcRegistry& NpcsReported();
// The hits on V the pipeline's wraps offer, and our hits as they land or end dealt nothing.
CombatInbox& CombatReported();
}

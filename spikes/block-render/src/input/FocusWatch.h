#pragma once

// The focus watch's decision (the spec's "input/"): whether the game has just stopped being in front. Its thread
// (input/RawInputHook.h) checks ten times a second, so that a button held while the user switches windows is let go of
// in Minecraft even if the game stopped updating.
namespace cpmc::input
{
class FocusWatch
{
public:
	// True once when the game stops being in front, and not again until it has been back.
	bool LostFront(bool aInFront);

private:
	bool inFront_ = true;
};
}

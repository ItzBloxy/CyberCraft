#include "input/FocusWatch.h"

namespace cpmc::input
{
bool FocusWatch::LostFront(bool aInFront)
{
	const bool lost = inFront_ && !aInFront;
	inFront_ = aInFront;
	return lost;
}
}

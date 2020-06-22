// Part of SimCoupe - A SAM Coupe emulator
//
// SSX.h: SAM main screen data saving in raw formats

#pragma once
#include "FrameBuffer.h"

namespace SSX
{
	bool Save(const FrameBuffer& fb, int main_x, int main_y);
}

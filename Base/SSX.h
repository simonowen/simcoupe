// Part of SimCoupe - A SAM Coupe emulator
//
// SSX.h: SAM main screen data saving in raw formats

#pragma once
#include "Screen.h"

namespace SSX
{
	bool Save(const Screen& screen, int main_x, int main_y);
}

#pragma once


#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

struct FMouseMove {
    FLOAT x{};
    FLOAT y{};
};

struct FMouse {
    FMouseMove current;
    FMouseMove previous;
    FMouseMove delta;
};

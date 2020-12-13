#pragma once

#include "camera.h"
#include "input.h"


// Returns true, if camera is moved, and therefore input is captured.
bool updateCamera(render_camera& camera, const user_input& input, uint32 viewportWidth, uint32 viewportHeight, float dt);

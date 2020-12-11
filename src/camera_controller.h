#pragma once

#include "camera.h"
#include "input.h"


void updateCamera(render_camera& camera, const user_input& input, uint32 viewportWidth, uint32 viewportHeight, float dt);

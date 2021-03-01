#pragma once

#include "camera.h"
#include "input.h"


struct camera_controller
{
	void initialize(render_camera* camera) { this->camera = camera; }

	// Returns true, if camera is moved, and therefore input is captured.
	bool centerCameraOnObject(const bounding_box& aabb);
	bool update(const user_input& input, uint32 viewportWidth, uint32 viewportHeight, float dt);

	render_camera* camera;

private:
	float orbitRadius = 10.f;


	float centeringTime = -1.f;

	vec3 centeringPositionStart;
	quat centeringRotationStart;

	vec3 centeringPositionTarget;
	quat centeringRotationTarget;
};


#include "pch.h"
#include "gizmo.h"

void gizmo::initialize(uint32 meshFlags)
{
	cpu_mesh mesh(meshFlags);
	float shaftLength = 2.f;
	float headLength = 0.4f;
	float radius = 0.06f;
	float headRadius = 0.13f;
	translationSubmesh = mesh.pushArrow(6, radius, headRadius, shaftLength, headLength);
	rotationSubmesh = mesh.pushTorus(6, 64, shaftLength, radius);
	scaleSubmesh = mesh.pushMace(6, radius, headRadius, shaftLength, headLength);
	this->mesh = mesh.createDXMesh();
}

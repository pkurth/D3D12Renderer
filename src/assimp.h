#pragma once

#include "math.h"
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

static mat4 readAssimpMatrix(const aiMatrix4x4& m)
{
	mat4 result;
	result.m00 = m.a1; result.m10 = m.b1; result.m20 = m.c1; result.m30 = m.d1;
	result.m01 = m.a2; result.m11 = m.b2; result.m21 = m.c2; result.m31 = m.d2;
	result.m02 = m.a3; result.m12 = m.b3; result.m22 = m.c3; result.m32 = m.d3;
	result.m03 = m.a4; result.m13 = m.b4; result.m23 = m.c4; result.m33 = m.d4;
	return result;
}

static vec3 readAssimpVector(const aiVector3D& v)
{
	vec3 result;
	result.x = v.x;
	result.y = v.y;
	result.z = v.z;
	return result;
}

static quat readAssimpQuaternion(const aiQuaternion& q)
{
	quat result;
	result.x = q.x;
	result.y = q.y;
	result.z = q.z;
	result.w = q.w;
	return result;
}

const aiScene* loadAssimpSceneFile(const char* filepathRaw, Assimp::Importer& importer);
ref<struct pbr_material> loadAssimpMaterial(const aiMaterial* material);

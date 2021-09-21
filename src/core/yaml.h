#pragma once

#include <yaml-cpp/yaml.h>
#include <fstream>

#include "math.h"



static YAML::Emitter& operator<<(YAML::Emitter& out, const vec2& v)
{
	out << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const vec3& v)
{
	out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const vec4& v)
{
	out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const quat& v)
{
	out << v.v4;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const mat2& m)
{
	out << YAML::Flow << YAML::BeginSeq << m.m00 << m.m01 << m.m10 << m.m11 << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const mat3& m)
{
	out << YAML::Flow << YAML::BeginSeq << m.m00 << m.m01 << m.m02 << m.m10 << m.m11 << m.m12 << m.m20 << m.m21 << m.m22 << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const mat4& m)
{
	out << YAML::Flow << YAML::BeginSeq << m.m00 << m.m01 << m.m02 << m.m03 << m.m10 << m.m11 << m.m12 << m.m13 << m.m20 << m.m21 << m.m22 << m.m23 << m.m30 << m.m31 << m.m32 << m.m33 << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const fs::path& p)
{
	out << p.string(); // TODO: This should really output wstrings.
	return out;
}

namespace YAML
{
	template<>
	struct convert<vec2>
	{
		static bool decode(const Node& n, vec2& v) { if (!n.IsSequence() || n.size() != 2) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); return true; }
	};

	template<>
	struct convert<vec3>
	{
		static bool decode(const Node& n, vec3& v) { if (!n.IsSequence() || n.size() != 3) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); return true; }
	};

	template<>
	struct convert<vec4>
	{
		static bool decode(const Node& n, vec4& v) { if (!n.IsSequence() || n.size() != 4) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); v.w = n[3].as<float>(); return true; }
	};

	template<>
	struct convert<quat>
	{
		static bool decode(const Node& n, quat& v) { return convert<vec4>::decode(n, v.v4); }
	};

	template<>
	struct convert<mat2>
	{
		static bool decode(const Node& n, mat2& m)
		{ 
			if (!n.IsSequence() || n.size() != 4) return false; 
			m.m00 = n[0].as<float>(); 
			m.m01 = n[1].as<float>();
			m.m10 = n[2].as<float>();
			m.m11 = n[3].as<float>();
			return true; }
	};

	template<>
	struct convert<mat3>
	{
		static bool decode(const Node& n, mat3& m)
		{
			if (!n.IsSequence() || n.size() != 9) return false;
			m.m00 = n[0].as<float>();
			m.m01 = n[1].as<float>();
			m.m02 = n[2].as<float>();
			m.m10 = n[3].as<float>();
			m.m11 = n[4].as<float>();
			m.m12 = n[5].as<float>();
			m.m20 = n[6].as<float>();
			m.m21 = n[7].as<float>();
			m.m22 = n[8].as<float>();
			return true;
		}
	};

	template<>
	struct convert<mat4>
	{
		static bool decode(const Node& n, mat4& m)
		{
			if (!n.IsSequence() || n.size() != 16) return false;
			m.m00 = n[0].as<float>();
			m.m01 = n[1].as<float>();
			m.m02 = n[2].as<float>();
			m.m03 = n[3].as<float>();
			m.m10 = n[4].as<float>();
			m.m11 = n[5].as<float>();
			m.m12 = n[6].as<float>();
			m.m13 = n[7].as<float>();
			m.m20 = n[8].as<float>();
			m.m21 = n[9].as<float>();
			m.m22 = n[10].as<float>();
			m.m23 = n[11].as<float>();
			m.m30 = n[12].as<float>();
			m.m31 = n[13].as<float>();
			m.m32 = n[14].as<float>();
			m.m33 = n[15].as<float>();
			return true;
		}
	};

	template<>
	struct convert<fs::path>
	{
		static bool decode(const Node& n, fs::path& v) { v = n.as<std::string>(); return true; }
	};
}



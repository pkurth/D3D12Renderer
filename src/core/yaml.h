#pragma once

#include <yaml-cpp/yaml.h>
#include <fstream>

#include "math.h"
#include "asset.h"


namespace YAML
{
	template<>
	struct convert<vec2>
	{
		static Node encode(const vec2& v)
		{
			Node n; n.SetStyle(EmitterStyle::Flow); n.push_back(v.x); n.push_back(v.y); return n;
		}

		static bool decode(const Node& n, vec2& v)
		{
			if (!n.IsSequence() || n.size() != 2) { return false; } v.x = n[0].as<float>(); v.y = n[1].as<float>(); return true;
		}
	};

	template<>
	struct convert<vec3>
	{
		static Node encode(const vec3& v)
		{
			Node n; n.SetStyle(EmitterStyle::Flow); n.push_back(v.x); n.push_back(v.y); n.push_back(v.z); return n;
		}

		static bool decode(const Node& n, vec3& v)
		{
			if (!n.IsSequence() || n.size() != 3) { return false; } v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); return true;
		}
	};

	template<>
	struct convert<vec4>
	{
		static Node encode(const vec4& v)
		{
			Node n; n.SetStyle(EmitterStyle::Flow); n.push_back(v.x); n.push_back(v.y); n.push_back(v.z); n.push_back(v.w);  return n;
		}

		static bool decode(const Node& n, vec4& v)
		{
			if (!n.IsSequence() || n.size() != 4) { return false; } v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); v.w = n[3].as<float>(); return true;
		}
	};

	template<>
	struct convert<quat>
	{
		static Node encode(const quat& v)
		{
			Node n; n.SetStyle(EmitterStyle::Flow); n.push_back(v.x); n.push_back(v.y); n.push_back(v.z); n.push_back(v.w);  return n;
		}

		static bool decode(const Node& n, quat& v)
		{
			if (!n.IsSequence() || n.size() != 4) { return false; } v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); v.w = n[3].as<float>(); return true;
		}
	};

	template<>
	struct convert<mat2>
	{
		static Node encode(const mat2& m)
		{
			Node n;
			n.SetStyle(EmitterStyle::Flow);
			n.push_back(m.m00);
			n.push_back(m.m01);
			n.push_back(m.m10);
			n.push_back(m.m11);
			return n;
		}

		static bool decode(const Node& n, mat2& m)
		{
			if (!n.IsSequence() || n.size() != 4) return false;
			m.m00 = n[0].as<float>();
			m.m01 = n[1].as<float>();
			m.m10 = n[2].as<float>();
			m.m11 = n[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<mat3>
	{
		static Node encode(const mat3& m)
		{
			Node n;
			n.SetStyle(EmitterStyle::Flow);
			n.push_back(m.m00);
			n.push_back(m.m01);
			n.push_back(m.m02);
			n.push_back(m.m10);
			n.push_back(m.m11);
			n.push_back(m.m12);
			n.push_back(m.m20);
			n.push_back(m.m21);
			n.push_back(m.m22);
			return n;
		}

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
		static Node encode(const mat4& m)
		{
			Node n;
			n.SetStyle(EmitterStyle::Flow);
			n.push_back(m.m00);
			n.push_back(m.m01);
			n.push_back(m.m02);
			n.push_back(m.m03);
			n.push_back(m.m10);
			n.push_back(m.m11);
			n.push_back(m.m12);
			n.push_back(m.m13);
			n.push_back(m.m20);
			n.push_back(m.m21);
			n.push_back(m.m22);
			n.push_back(m.m23);
			n.push_back(m.m30);
			n.push_back(m.m31);
			n.push_back(m.m32);
			n.push_back(m.m33);
			return n;
		}

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
		static Node encode(const fs::path& v) { Node n; n = v.string(); return n; }
		static bool decode(const Node& n, fs::path& v) { v = n.as<std::string>(); return true; }
	};

	template<>
	struct convert<asset_handle>
	{
		static Node encode(const asset_handle& v) { Node n; n = v.value; return n; }
		static bool decode(const Node& n, asset_handle& h) { h.value = n.as<uint64>(); return true; }
	};
}


#define YAML_LOAD(node, var, name) { auto nc = node[name]; if (nc) { var = nc.as<std::remove_reference_t<decltype(var)>>(); } }
#define YAML_LOAD_ENUM(node, var, name) { auto nc = node[name]; if (nc) { var = (std::remove_reference_t<decltype(var)>)(nc.as<int>()); } }



workspace "D3D12ProjectionMapping"
	architecture "x64"
	startproject "D3D12ProjectionMapping"

	configurations {
		"Debug",
		"Release"
	}

	flags {
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}_%{cfg.architecture}"

group "Dependencies"
	include "ext/assimp"
group ""

project "D3D12ProjectionMapping"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "On"

	targetdir ("./bin/" .. outputdir)
	objdir ("./bin_int/" .. outputdir ..  "/%{prj.name}")

	debugdir "."

	pchheader "pch.h"
	pchsource "src/pch.cpp"

	sysincludedirs {
		"ext",
		"ext/assimp/include",
		"ext/entt/single_include",
	}

	includedirs {
		"shaders/rs"
	}

	files {
		"src/*",
		"shaders/**.hlsl*",
	}

	vpaths {
		["Headers"] = { "src/*.h" },
		["Sources"] = { "src/*.cpp" },
		["Shaders"] = { "shaders/*.hlsl" },
	}

	links {
		"d3d12",
		"D3Dcompiler",
		"DXGI",
		"dxguid",
		"dxcompiler",
		"assimp"
	}

	filter "system:windows"
		systemversion "latest"

		defines {
			"_UNICODE",
			"UNICODE"
		}

	filter "configurations:Debug"
        runtime "Debug"
		symbols "On"
		
	filter "configurations:Release"
        runtime "Release"
		optimize "On"


	os.mkdir("shaders/bin")

	-- HLSL files that don't end with 'Extensions' will be ignored as they will be used as includes
	filter "files:**.hlsl*"
		shadermodel "6.0"
		flags "ExcludeFromBuild"
		shaderobjectfileoutput("shaders/bin/%{file.basename}" .. ".cso")
		shaderincludedirs {
			"shaders/rs",
			"shaders/common"
		}
		shaderdefines {
			"mat4=float4x4",
			"vec2=float2",
			"vec3=float3",
			"vec4=float4"
		}
 
	filter("files:**_vs.hlsl*")
		removeflags("ExcludeFromBuild")
		shadertype("Vertex")
 
	filter("files:**_gs.hlsl")
		removeflags("ExcludeFromBuild")
		shadertype("Geometry")
 
	filter("files:**_hs.hlsl")
		removeflags("ExcludeFromBuild")
		shadertype("Hull")
 
	filter("files:**_ds.hlsl")
		removeflags("ExcludeFromBuild")
		shadertype("Domain")

	filter("files:**_ps.hlsl")
		removeflags("ExcludeFromBuild")
		shadertype("Pixel")
 
	filter("files:**_cs.hlsl")
		removeflags("ExcludeFromBuild")
		shadertype("Compute")
		

   shaderoptions({"/WX"})


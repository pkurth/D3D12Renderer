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
		"ext/assimp/include"
	}

	files {
		"src/**.h",
		"src/**.cpp"
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


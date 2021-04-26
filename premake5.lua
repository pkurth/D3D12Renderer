
newoption {
    trigger     = "no-turing",
    description = "Use this if your GPU is older than Turing. This disables certain features, such as mesh shaders."
}

local turing_or_higher = false
local sdk_exists = false
local dxc_exists = false


-------------------------
-- CHECK GPU
-------------------------

if not _OPTIONS["no-turing"] then
	local handle = io.popen("wmic path win32_VideoController get name")
	local gpu_string = handle:read("*a")
	handle:close()

	for str in string.gmatch(gpu_string, "NVIDIA (.-)\n") do
		for model in string.gmatch(str, "%d%d%d%d") do
			local m = tonumber(model)
			if m >= 1650 then -- Turing is everything higher than GTX 1650.
				turing_or_higher = true
			end
		end
	end
end


-------------------------
-- CHECK WINDOWS SDK
-------------------------

local sdk_directory = os.getenv("programfiles(x86)") .. "/Windows Kits/10/bin/"
local sdk_directory_handle = io.popen('dir "'..sdk_directory..'" /b')
for filename in sdk_directory_handle:lines() do
    for version in string.gmatch(filename, "10.0.(%d+).0") do
		local dxc_exe = sdk_directory .. filename .. "/x64/dxc.exe"
		local f = io.open(dxc_exe, "r")
		if f~=nil then 
			io.close(f)
			dxc_exists = true
			
			local v = tonumber(version)
			if v >= 19041 then
				sdk_exists = true
			end
		end
	end
end
sdk_directory_handle:close()


if turing_or_higher then
	print("Found NVIDIA Turing GPU or newer.")
else
	print("No NVIDIA Turing or newer GPU found.")
end

if dxc_exists then
	print("Found DXC compiler.")
else
	print("No DXC compiler found.")
end

if sdk_exists then
	print("Found Windows SDK version >= 19041.")
else
	print("No Windows SDK version >= 19041 found.")
end

local all_exist = turing_or_higher and dxc_exists and sdk_exists

if not all_exist then
	print("Disabling mesh shader compilation, since not all requirements are met.")
end



-------------------------
-- GENERATING SHADERS
-------------------------

print("Generating custom shaders..")

local generated_directory = "shaders/generated/"
os.mkdir(generated_directory)

local generated_directory_handle = io.popen('dir "'..generated_directory..'" /b')
for filename in generated_directory_handle:lines() do
	os.remove(generated_directory..filename)
end


local local_particle_system_directory = "particle_systems/"

local local_emit_path = "particles/particle_emit.hlsli"
local local_sim_path = "particles/particle_sim.hlsli"
local local_vs_path = "particles/particle_vs.hlsli"
local local_ps_path = "particles/particle_ps.hlsli"

local particle_system_directory = "shaders/" .. local_particle_system_directory
local shader_directory_handle = io.popen('dir "'..particle_system_directory..'" /b')

for filename in shader_directory_handle:lines() do
	if filename:sub(-string.len(".hlsli")) == ".hlsli" then
		local stem = filename:match("(.+)%..+")
		local compute_header = '#define PARTICLE_SIMULATION\n#include "random.hlsli"\n#include "../' .. local_particle_system_directory .. filename .. '"\n'
		local render_header = '#define PARTICLE_RENDERING\n#include "random.hlsli"\n#include "../' .. local_particle_system_directory .. filename .. '"\n'


		local new_emit_content = compute_header .. '#include "../' .. local_emit_path .. '"\n'
		local new_sim_content = compute_header .. '#include "../' .. local_sim_path .. '"\n'
		local new_vs_content = render_header .. '#include "../' .. local_vs_path .. '"\n'
		local new_ps_content = render_header .. '#include "../' .. local_ps_path .. '"\n'
	
		local emit_file = assert(io.open(generated_directory .. stem .. "_emit_cs.hlsl", "w"))
		emit_file:write(new_emit_content)
		emit_file:close()
	
		local sim_file = assert(io.open(generated_directory .. stem .. "_sim_cs.hlsl", "w"))
		sim_file:write(new_sim_content)
		sim_file:close()
		
		local vs_file = assert(io.open(generated_directory .. stem .. "_vs.hlsl", "w"))
		vs_file:write(new_vs_content)
		vs_file:close()
		
		local ps_file = assert(io.open(generated_directory .. stem .. "_ps.hlsl", "w"))
		ps_file:write(new_ps_content)
		ps_file:close()

		print("- Generated particle system '" .. stem .. "'.")
	end
end



-----------------------------------------
-- GENERATE MAIN SOLUTION
-----------------------------------------


workspace "D3D12Renderer"
	architecture "x64"
	startproject "D3D12Renderer"

	configurations {
		"Debug",
		"Release"
	}

	flags {
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}_%{cfg.architecture}"
shaderoutputdir = "shaders/bin/%{cfg.buildcfg}/"

group "Dependencies"
	include "ext/assimp"
	include "ext/yaml-cpp"


	externalproject "DirectXTex_Desktop_2019_Win10"
		location "ext/directxtex/DirectXTex"
		kind "StaticLib"
		language "C++"

group ""

project "D3D12Renderer"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	targetdir ("./bin/" .. outputdir)
	objdir ("./bin_int/" .. outputdir ..  "/%{prj.name}")

	debugdir "."
	debugenvs {
		"PATH=ext/bin;%PATH%;"
	}

	pchheader "pch.h"
	pchsource "src/pch.cpp"

	sysincludedirs {
		"ext/assimp/include",
		"ext/yaml-cpp/include",
		"ext/entt/src",
		"ext/directxtex",
		"ext",
	}

	includedirs {
		"shaders/rs",
		"shaders/common",
		"shaders/particle_systems",
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
		"assimp",
		"yaml-cpp",
		"DirectXTex_Desktop_2019_Win10",
	}

	dependson {
		"assimp",
		"yaml-cpp",
		"DirectXTex_Desktop_2019_Win10",
	}

	filter "system:windows"
		systemversion "latest"

		defines {
			"_UNICODE",
			"UNICODE",
			"_CRT_SECURE_NO_WARNINGS"
		}
		
		if turing_or_higher then
			defines { "TURING_GPU_OR_NEWER_AVAILABLE" }
		end

		if sdk_exists then
			defines { "WINDOWS_SDK_19041_OR_NEWER_AVAILABLE" }
		end

		defines { "SHADER_BIN_DIR=L\"" .. shaderoutputdir .. "\"" }

	filter "configurations:Debug"
        runtime "Debug"
		symbols "On"
		
	filter "configurations:Release"
        runtime "Release"
		optimize "On"



	filter "files:**.hlsl"
	
		if turing_or_higher and dxc_exists then
			shadermodel "6.5"
		else
			shadermodel "5.1"
		end

		

		flags "ExcludeFromBuild"
		shaderobjectfileoutput(shaderoutputdir .. "%{file.basename}.cso")
		shaderincludedirs {
			"shaders/rs",
			"shaders/common"
		}
		shaderdefines {
			"HLSL",
			"mat4=float4x4",
			"vec2=float2",
			"vec3=float3",
			"vec4=float4",
			"uint32=uint"
		}

		shaderoptions {
			"/WX",
			"/all_resources_bound",
			--"/Qembed_debug",
		}
 
	filter("files:**_vs.hlsl")
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
		
	if all_exist then
		filter("files:**_ms.hlsl")
			removeflags("ExcludeFromBuild")
			shadertype("Mesh")
			shadermodel "6.5" -- Required for mesh shaders.

		filter("files:**_as.hlsl")
			removeflags("ExcludeFromBuild")
			shadertype("Amplification")
			shadermodel "6.5" -- Required for amplification shaders.
	end



-----------------------------------------
-- GENERATE PHYSICS ONLY SOLUTION
-----------------------------------------

workspace "Physics-Lib"
	architecture "x64"
	startproject "Physics-Lib"

	configurations {
		"Debug",
		"Release"
	}

	flags {
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}_%{cfg.architecture}"

project "Physics-Lib"
	kind "SharedLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	targetdir ("./bin/" .. outputdir)
	objdir ("./bin_int/" .. outputdir ..  "/%{prj.name}")

	debugdir "."
	debugenvs {
		"PATH=ext/bin;%PATH%;"
	}

	pchheader "pch.h"
	pchsource "src/pch.cpp"

	sysincludedirs {
		"ext/entt/src",
		"ext",
	}

	files {
		"src/bounding_volumes.*",
		"src/collision_broad.*",
		"src/collision_epa.*",
		"src/collision_gjk.*",
		"src/collision_narrow.*",
		"src/collision_sat.*",
		"src/constraints.*",
		"src/math.*",
		"src/pch.*",
		"src/physics.*",
		"src/ragdoll.*",
		"src/scene.*",
	}

	vpaths {
		["Headers"] = { "src/*.h" },
		["Sources"] = { "src/*.cpp" },
	}

	filter "system:windows"
		systemversion "latest"

		defines {
			"PHYSICS_ONLY",
			"_UNICODE",
			"UNICODE",
			"_CRT_SECURE_NO_WARNINGS"
		}

	filter "configurations:Debug"
        runtime "Debug"
		symbols "On"
		
	filter "configurations:Release"
        runtime "Release"
		optimize "On"


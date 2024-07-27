local gpu_name = ""
local gpu_model_number = 0
local sdk_version = 0


-------------------------
-- CHECK GPU
-------------------------

local gpu_handle = io.popen("wmic path win32_VideoController get name")
local gpu_string = gpu_handle:read("*a")
gpu_handle:close()

for str in string.gmatch(gpu_string, "NVIDIA (.-)\n") do
	for model in string.gmatch(str, "%d%d%d%d?") do
		gpu_model_number = tonumber(model)
		gpu_name = str
	end
end


-------------------------
-- CHECK WINDOWS SDK
-------------------------

local sdk_directory = os.getenv("programfiles(x86)") .. "/Windows Kits/10/bin/"
local sdk_directory_handle = io.popen('dir "'..sdk_directory..'" /b')
for filename in sdk_directory_handle:lines() do
    for version in string.gmatch(filename, "10.0.(%d+).0") do
		local v = tonumber(version)
		if v > sdk_version then
			sdk_version = v
		end
	end
end
sdk_directory_handle:close()


print("Windows SDK version: ", sdk_version)
print("Installed GPU: ", gpu_name)

local turing_or_higher = gpu_model_number >= 1650

local new_sdk_version = 19041
local new_sdk_available = sdk_version >= new_sdk_version


local force_old_sdk = false

local sdk_version_string = "latest"
if force_old_sdk then
	sdk_version_string = "10.0.18362.0"
	new_sdk_available = false
end



local mesh_shaders_supported = turing_or_higher and new_sdk_available

if not mesh_shaders_supported then
	term.pushColor(term.infoColor)
	print("Disabling mesh shader compilation, since not all requirements are met.")
	term.popColor()
end


print("\n")


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


print("\n")


-- Premake extension to include files at solution-scope. From https://github.com/premake/premake-core/issues/1061#issuecomment-441417853

require('vstudio')
premake.api.register {
	name = "workspacefiles",
	scope = "workspace",
	kind = "list:string",
}

premake.override(premake.vstudio.sln2005, "projects", function(base, wks)
	if wks.workspacefiles and #wks.workspacefiles > 0 then
		premake.push('Project("{2150E333-8FDC-42A3-9474-1A3956D46DE8}") = "Solution Items", "Solution Items", "{' .. os.uuid("Solution Items:"..wks.name) .. '}"')
		premake.push("ProjectSection(SolutionItems) = preProject")
		for _, file in ipairs(wks.workspacefiles) do
			file = path.rebase(file, ".", wks.location)
			premake.w(file.." = "..file)
		end
		premake.pop("EndProjectSection")
		premake.pop("EndProject")
	end
	base(wks)
end)


-----------------------------------------
-- GENERATE SOLUTION
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
	
	workspacefiles {
        "premake5.lua",
		"README.md",
		"LICENSE",
    }


outputdir = "%{cfg.buildcfg}_%{cfg.architecture}"
shaderoutputdir = "shaders/bin/%{cfg.buildcfg}/"


group "Dependencies"
	include "ext/directxtex"
	include "ext/yaml-cpp"
group ""





-----------------------------------------
-- GENERATE MAIN PROGRAM
-----------------------------------------


project "D3D12Renderer"
	--location "bin/D3D12Renderer"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	targetdir ("./bin/" .. outputdir)
	objdir ("./bin_int/" .. outputdir ..  "/%{prj.name}")

	debugenvs {
		"PATH=ext/bin;%PATH%;"
	}
	debugdir "."

	pchheader "pch.h"
	pchsource "src/pch.cpp"

	files {
		"src/**.h",
		"src/**.cpp",
		"shaders/**.hlsl*",
	}

	vpaths {
		["Headers/*"] = { "src/**.h" },
		["Sources/*"] = { "src/**.cpp" },
		["Shaders/*"] = { "shaders/**.hlsl" },
	}

	libdirs {
		"ext/lib",
	}

	links {
		"d3d12",
		"D3Dcompiler",
		"DXGI",
		"dxguid",
		"dxcompiler",
		"XAudio2",
		"uxtheme",
		"directxtex",
		"yaml-cpp",
	}

	dependson {
		"directxtex",
		"yaml-cpp",
	}

	includedirs {
		"src",
		"shaders/rs",
		"shaders/common",
		"shaders/particle_systems",
	}

	sysincludedirs {
		"ext/yaml-cpp/include",
		"ext/entt/src",
		"ext/directxtex",
		"ext",
	}

	prebuildcommands {
		"ECHO Compiling shaders...",
	}

	vectorextensions "AVX2"
	floatingpoint "Fast"

	filter "configurations:Debug"
        runtime "Debug"
		symbols "On"
		
	filter "configurations:Release"
        runtime "Release"
		optimize "On"
		inlining "Auto"

	filter "system:windows"
		systemversion (sdk_version_string)

		defines {
			"_UNICODE",
			"UNICODE",
			"_CRT_SECURE_NO_WARNINGS",
			"ENABLE_CPU_PROFILING=1",
			"ENABLE_DX_PROFILING=1",
			"ENABLE_MESSAGE_LOG=1",
		}

		defines { "SHADER_BIN_DIR=L\"" .. shaderoutputdir .. "\"" }

		if mesh_shaders_supported then
			defines { "MESH_SHADER_SUPPORTED "}
		end


	filter "files:**.hlsl"
		if turing_or_higher and new_sdk_available then
			shadermodel "6.5"
			shaderdefines {
				"SHADERMODEL=65"
			}
		elseif turing_or_higher then
			shadermodel "6.1"
			shaderdefines {
				"SHADERMODEL=61"
			}
		else
			shadermodel "5.1"
			shaderdefines {
				"SHADERMODEL=51"
			}
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
			"mat4x3=float4x3",
			"mat3x4=float3x4",
			"vec2=float2",
			"vec3=float3",
			"vec4=float4",
			"int32=int",
			"uint32=uint",
			"uint64=uint64_t",
		}
	
		shaderoptions {
			"/WX",
			"/all_resources_bound",
		}
	
	if turing_or_higher then
		filter { "configurations:Debug", "files:**.hlsl" }
			shaderoptions {
				"/Qembed_debug",
			}
	end
		
 	
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
		
	if mesh_shaders_supported then
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
-- GENERATE PHYSICS ONLY DLL
-----------------------------------------

project "Physics-Lib"
	--location "bin/Physics-Lib"
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

	includedirs {
		"src",
	}

	sysincludedirs {
		"ext/entt/src",
		"ext",
	}

	vectorextensions "AVX2"
	floatingpoint "Fast"

	files {
		"src/physics/bounding_volumes.*",
		"src/physics/collision_broad.*",
		"src/physics/collision_epa.*",
		"src/physics/collision_gjk.*",
		"src/physics/collision_narrow.*",
		"src/physics/collision_sat.*",
		"src/physics/constraints.*",
		"src/physics/physics.*",
		"src/physics/cloth.*",
		"src/physics/rigid_body.*",
		"src/physics/ragdoll.*",
		"src/physics/heightmap_collision.*",
		"src/learning/**",
		"src/core/math.*",
		"src/core/memory.*",
		"src/core/threading.*",
		"src/scene/scene.*",
		"src/terrain/heightmap_collider.*",
		"src/pch.*",
	}

	vpaths {
		["Headers/*"] = { "src/**.h" },
		["Sources/*"] = { "src/**.cpp" },
	}

	filter "system:windows"
		systemversion "latest"

		defines {
			"PHYSICS_ONLY",
			"_UNICODE",
			"UNICODE",
			"_CRT_SECURE_NO_WARNINGS",
			"ENABLE_CPU_PROFILING=0",
			"ENABLE_DX_PROFILING=0",
		}

	filter "configurations:Debug"
        runtime "Debug"
		symbols "On"
		
	filter "configurations:Release"
        runtime "Release"
		optimize "On"
		inlining "Auto"


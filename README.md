# D3D12Renderer

This project implements a custom rendering engine build from the ground up in C++ using Direct3D 12. 
It supports some "new" features like raytracing, mesh shaders etc. 

It also features a custom written physics engine written completely from scratch.

## Table of Contents
- [Graphics features](#graphics-features)
- [Physics](#physics)
- [Others](#others)
- [System Requirements](#system-requirements)
- [Build Instructions](#build-instructions)

## Graphics features

<img style="float:right" src="assets/samples/raster.png" width="300"/>
<img style="float:right" width="100%" />

<img style="float:right" src="assets/samples/raster2.png" width="300"/>
<img style="float:right" width="100%" />

<img style="float:right" src="assets/samples/raster3.png" width="300"/>

<p style="float:left">

- Forward+ rendering
- Physically based rendering
- Dynamic lights and dynamic shadows
	- Sun (with cascaded shadow maps)
	- Point lights
	- Spot lights
- Decals
- Post processing stack
	- Temporal anti-aliasing
	- Bloom
	- Filmic tone-mapping
	- Sharpening
- Tiled light and decal culling
- Screen space reflections
- Real-time raytracing (DXR)
- Integrated path tracer
- Skeletal animation
- Mesh shaders
- Hot-reloading of shaders

</p>

<img width="100%" />

It has an integrated (albeit pretty simple) path tracer (using hardware-accelerated raytracing), which in the future will be integrated into the real-time pipeline in some form to compute global illumination effects.

<img src="assets/samples/path_trace.png" width="512"/><br>


## Physics


<a style="float:right; text-align:right; position: relative; z-index: 10;" href="https://www.youtube.com/watch?v=FqwCIoI-c_A">
	<img src="https://img.youtube.com/vi/FqwCIoI-c_A/mqdefault.jpg" style="width:400px" />
</a>

<p style="float:left">

- Rigid body dynamics
- Cloth simulation
- Various constraints between rigid bodies (many with limits and motors)
  - Distance
  - Ball joints
  - Hinge joints
  - Cone twist
  - Slider
- Various collider types
  - Spheres
  - Capsules
  - Cylinders
  - AABBs and OBBs
  - Convex hulls
- SIMD support for constraint resolution
- Ragdolls
- Vehicle physics
- Machine learning for ragdoll locomotion. Based on [Machine Learning Summit: Ragdoll Motion Matching](https://www.youtube.com/watch?v=JZKaqQKcAnw) and [DReCon: Data-Driven Responsive Control of Physics-Based Characters](https://static-wordpress.akamaized.net/montreal.ubisoft.com/wp-content/uploads/2019/11/13214229/DReCon.pdf) by Ubisoft

</p>

## Others
- Editor tools
- Integrated CPU and GPU profiler (with multi-threading support)

## System Requirements

Since this project uses Direct3D 12 as the only rendering backend, the only supported platform is Windows 10. 
The project is only tested with Visual Studio 2019, and only on NVIDIA GPUs.

For mesh shaders you will need the Windows 10 SDK version 10.0.19041.0 or higher.
This can be downloaded using the Visual Studio Installer.
If you only have an older version of the SDK installed, the build system will automatically disable mesh shaders. 
To run you will need the Windows 10 May 2020 Update (20H1) or newer.
If these requirements are not met, you should still be able to build and run the program, but without mesh shader support.

If you want to use raytracing or mesh shaders, you need a compatible NVIDIA GPU. 
For raytracing these are the GPUs with the Pascal architecture or newer.
For mesh shaders you will need a Turing GPU or newer.

All other dependencies (external libraries) either come directly with the source code or in the form of submodules.


## Build Instructions

I have tried to keep the build process as simple as possible.
Therefore you will not need any build tools installed on your machine.
The project uses Premake, but all you need comes with the source.

- Clone the repository and make sure to clone with submodules. 
- Double-click the _generate.bat_ file in the root directory to generate a Visual Studio 2019 solution.
The build process will automatically enable and disable certain features based on your installed GPU and the available Windows 10 SDK.
- Open the solution and build. 
This _should_ work directly. 
In a release build Visual Studio sometimes reports an "Unknown error" when building. 
In that case simply restart Visual Studio and you are good to go.
- If you add new source files (or shaders), re-run the _generate.bat_ file.

The assets seen in the screenshots above are not included with the source code. 





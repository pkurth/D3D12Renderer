#ifndef LIGHT_CULLING_H
#define LIGHT_CULLING_H

#define LIGHT_CULLING_TILE_SIZE 16

struct light_culling_frustum_plane
{
    vec3 N;
    float d;
};

struct light_culling_view_frustum
{
    light_culling_frustum_plane planes[4]; // Left, right, top, bottom frustum planes.
};

struct frusta_cb
{
    uint32 numThreadsX;
    uint32 numThreadsY;
};

struct light_culling_cb
{
    uint32 numThreadGroupsX;
    uint32 numPointLights;
    uint32 numSpotLights;
};

struct point_light_bounding_volume
{
    vec3 position;
    float radius;
};

struct spot_light_bounding_volume
{
    vec3 tip;
    float height;  
    vec3 direction;  
    float radius;  
};

#define WORLD_SPACE_TILED_FRUSTA_RS \
    "RootFlags(0), " \
    "CBV(b0), " \
    "RootConstants(b1, num32BitConstants = 2), " \
    "UAV(u0)"


#define LIGHT_CULLING_RS \
    "RootFlags(0), " \
    "CBV(b0), " \
    "RootConstants(b1, num32BitConstants = 3), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "DescriptorTable( SRV(t1, numDescriptors = 3, flags = DESCRIPTORS_VOLATILE), UAV(u0, numDescriptors = 3, flags = DESCRIPTORS_VOLATILE) )"

#define MAX_NUM_LIGHTS_PER_TILE 1024

#define WORLD_SPACE_TILED_FRUSTA_RS_CAMERA      0
#define WORLD_SPACE_TILED_FRUSTA_RS_CB          1
#define WORLD_SPACE_TILED_FRUSTA_RS_FRUSTA_UAV  2

#define LIGHT_CULLING_RS_CAMERA                 0
#define LIGHT_CULLING_RS_CB                     1
#define LIGHT_CULLING_RS_DEPTH_BUFFER           2
#define LIGHT_CULLING_RS_SRV_UAV                3

#endif

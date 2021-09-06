#ifndef SKINNING_RS_HLSLI
#define SKINNING_RS_HLSLI


struct skinning_cb
{
    uint32 firstJoint;
    uint32 numJoints;
    uint32 firstVertex;
    uint32 numVertices;
    uint32 writeOffset;
};

#define SKINNING_RS \
    "RootConstants(b0, num32BitConstants = 5), " \
    "SRV(t0), " \
    "SRV(t1), " \
    "SRV(t2), " \
    "UAV(u0), " \
    "UAV(u1)"

#define SKINNING_RS_CB			                0
#define SKINNING_RS_INPUT_VERTEX_BUFFER0        1
#define SKINNING_RS_INPUT_VERTEX_BUFFER1        2
#define SKINNING_RS_MATRICES                    3
#define SKINNING_RS_OUTPUT0                     4
#define SKINNING_RS_OUTPUT1                     5




struct cloth_skinning_cb
{
    uint32 gridSizeX;
    uint32 gridSizeY;
    uint32 writeOffset;
};

#define CLOTH_SKINNING_RS \
    "RootConstants(b0, num32BitConstants = 3), " \
    "SRV(t0), " \
    "UAV(u0), " \
    "UAV(u1)"
    
#define CLOTH_SKINNING_RS_CB                    0
#define CLOTH_SKINNING_RS_INPUT                 1
#define CLOTH_SKINNING_RS_OUTPUT0               2
#define CLOTH_SKINNING_RS_OUTPUT1               3


struct mesh_position
{
    vec3 position;
};

struct mesh_others
{
    vec2 uv;
    vec3 normal;
    vec3 tangent;
};

struct skinned_mesh_position
{
    vec3 position;
};

struct skinned_mesh_others
{
    vec2 uv;
    vec3 normal;
    vec3 tangent;
    uint32 skinIndices;
    uint32 skinWeights;
};


#endif


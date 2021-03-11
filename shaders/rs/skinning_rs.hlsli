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

#define SKINNING_RS_CB			            0
#define SKINNING_RS_INPUT_VERTEX_BUFFER0    1
#define SKINNING_RS_INPUT_VERTEX_BUFFER1    2
#define SKINNING_RS_MATRICES                3
#define SKINNING_RS_OUTPUT0                 4
#define SKINNING_RS_OUTPUT1                 5

#endif


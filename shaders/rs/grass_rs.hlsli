#ifndef GRASS_RS_HLSLI
#define GRASS_RS_HLSLI


struct grass_cb
{
    mat4 mvp;

    uint32 numVertices;
    float halfWidth;
    float height;
    float lod; // [0, 1]

    vec3 windDirection;
    float time;
};

#define GRASS_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "RootConstants(b0, num32BitConstants=24, visibility=SHADER_VISIBILITY_VERTEX)"

#define GRASS_RS_CB     0


#endif

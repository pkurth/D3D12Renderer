#ifndef TREE_RS_HLSLI
#define TREE_RS_HLSLI

#include "transform.hlsli"

#define TREE_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define TREE_RS_MVP	                0

#endif

#include "tree_rs.hlsli"

#define RS TREE_RS
#define ALPHA_CUTOUT // TODO: Remove this when we have a depth prepass for trees.

#include "../geometry/default_pbr_ps.hlsl"

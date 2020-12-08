#ifndef MATERIAL_H
#define MATERIAL_H

#define USE_ALBEDO_TEXTURE		(1 << 0)
#define USE_NORMAL_TEXTURE		(1 << 1)
#define USE_ROUGHNESS_TEXTURE	(1 << 2)
#define USE_METALLIC_TEXTURE	(1 << 3)
#define USE_AO_TEXTURE			(1 << 4)

struct pbr_material_cb
{
#ifdef HLSL
    vec4 albedoTint;
#else
    float albedoTint[4]; // We do this, so that this struct gets packed to 24 bytes.
#endif
    uint32 roughnessOverride_metallicOverride;
    uint32 flags;
};

static float getRoughnessOverride(pbr_material_cb mat)
{
    return (mat.roughnessOverride_metallicOverride >> 16) / (float)0xFFFF;
}

static float getMetallicOverride(pbr_material_cb mat)
{
    return (mat.roughnessOverride_metallicOverride & 0xFFFF) / (float)0xFFFF;
}

static uint32 packRoughnessAndMetallic(float roughness, float metallic)
{
    uint32 r = (uint32)(roughness * 0xFFFF);
    uint32 m = (uint32)(metallic * 0xFFFF);
    return (r << 16) | m;
}

#endif

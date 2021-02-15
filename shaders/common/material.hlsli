#ifndef MATERIAL_H
#define MATERIAL_H

#define USE_ALBEDO_TEXTURE		(1 << 0)
#define USE_NORMAL_TEXTURE		(1 << 1)
#define USE_ROUGHNESS_TEXTURE	(1 << 2)
#define USE_METALLIC_TEXTURE	(1 << 3)
#define USE_AO_TEXTURE			(1 << 4)

struct pbr_material_cb // 24 bytes.
{
    vec3 emission;      // Since emission can be HDR, we require use full precision floats here.
    uint32 albedoTint;  // RGBA packed into one uint32. Can only be between 0 and 1, which is why we are only using 8 bit per channel.
    uint32 roughnessOverride_metallicOverride;
    uint32 flags;       // See defines above.
};

static float getRoughnessOverride(uint32 roughnessOverride_metallicOverride)
{
    return (roughnessOverride_metallicOverride >> 16) / (float)0xFFFF;
}

static float getMetallicOverride(uint32 roughnessOverride_metallicOverride)
{
    return (roughnessOverride_metallicOverride & 0xFFFF) / (float)0xFFFF;
}

static float getRoughnessOverride(pbr_material_cb mat)
{
    return getRoughnessOverride(mat.roughnessOverride_metallicOverride);
}

static float getMetallicOverride(pbr_material_cb mat)
{
    return getMetallicOverride(mat.roughnessOverride_metallicOverride);
}

static uint32 packRoughnessAndMetallic(float roughness, float metallic)
{
    uint32 r = (uint32)(roughness * 0xFFFF);
    uint32 m = (uint32)(metallic * 0xFFFF);
    return (r << 16) | m;
}

static vec4 unpackColor(uint32 c)
{
    const float mul = 1.f / 255.f;
    float r = (c & 0xFF) * mul;
    float g = ((c >> 8) & 0xFF) * mul;
    float b = ((c >> 16) & 0xFF) * mul;
    float a = ((c >> 24) & 0xFF) * mul;
    return vec4(r, g, b, a);
}


#endif

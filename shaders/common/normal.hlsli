#ifndef NORMAL_H
#define NORMAL_H


#if 0

// Spheremap Transform
// https://aras-p.info/texts/CompactNormalStorage.html#method04spheremap
static float2 packNormal(float3 n)
{
    float p = sqrt(n.z * 8 + 8);
    return float2(n.xy / p + 0.5);
}

static float3 unpackNormal(float2 enc)
{
    float2 fenc = enc * 4 - 2;
    float f = dot(fenc, fenc);
    if (f != f)
    {
        return float3(0.f, 0.f, -1.f);
    }

    float g = sqrt(1 - f / 4);
    float3 n;
    n.xy = fenc * g;
    n.z = 1 - f / 2;
    return n;
}

#else

// Stereographic Transform
// https://aras-p.info/texts/CompactNormalStorage.html#method07stereo
static float2 packNormal(float3 n)
{
    float2 enc = n.xy / (n.z + 1.f);

    //const float scale = 1.f / 1.7777f;
    //enc *= scale;
    //enc = enc * 0.5f + 0.5f;

    return enc;
}

static float3 unpackNormal(float2 enc)
{
    // enc = enc * 2 - vec2(1, 1);
    //const float scale = 1.7777f;
    //enc = enc * scale;

    float3 nn = float3(enc, 1.f);
    float g = 2.f / dot(nn, nn);
    if (g != g)
    {
        return float3(0.f, 0.f, -1.f);
    }

    float3 n;
    n.xy = g * nn.xy;
    n.z = g - 1.f;
    return n;
}

#endif

#endif

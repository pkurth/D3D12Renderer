#ifndef NORMAL_H
#define NORMAL_H

// https://aras-p.info/texts/CompactNormalStorage.html

// This is the Method #4: Spheremap Transform

static float2 packNormal(float3 n)
{
    float p = sqrt(n.z * 8 + 8);
    return float2(n.xy / p + 0.5);
}

static float3 unpackNormal(float2 enc)
{
    float2 fenc = enc * 4 - 2;
    float f = dot(fenc, fenc);
    float g = sqrt(1 - f / 4);
    float3 n;
    n.xy = fenc * g;
    n.z = 1 - f / 2;
    return n;
}

#endif

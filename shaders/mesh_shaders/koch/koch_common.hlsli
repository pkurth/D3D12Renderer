

static const uint DEFAULT_SHIFT = 7;
static const uint SHIFT = DEFAULT_SHIFT;

#define GRID_SIZE (1 << SHIFT)
#define STEP_SIZE (1.f / float(GRID_SIZE))

// From here: https://www.shadertoy.com/view/sdtcR2

float2 N(float angle) 
{
    return float2(sin(angle), cos(angle));
}

float2 koch(float2 uv)
{
    uv.x = abs(uv.x);

    float3 col = 0.f;
    float d;

    float angle = 0.f;
    float2 n = N((5.f / 6.f) * 3.1415f);

    uv.y += tan((5.f / 6.f) * 3.1415f) * 0.5f;
    d = dot(uv - float2(0.5f, 0.f), n);
    uv -= max(0.f, d) * n * 2.f;

    float scale = 1.;

    n = N((2.f / 3.f) * 3.1415f);
    uv.x += 0.5f;
    for (int i = 0; i < 8; ++i)
    {
        uv *= 3.f;
        scale *= 3.f;
        uv.x -= 1.5f;

        uv.x = abs(uv.x);
        uv.x -= 0.5f;
        d = dot(uv, n);
        uv -= min(0.f, d) * n * 2.f;
    }
    uv /= scale;

    return uv;
}

float field(float3 pos)
{
    pos = pos * 2.f - 1.f;


    float2 xz = koch(float2(length(pos.xz), pos.y));
    float2 yz = koch(float2(length(pos.yz), pos.x));
    float2 xy = koch(float2(length(pos.xy), pos.z));
    float d = max(xy.x, max(yz.x, xz.x));

    d = lerp(d, length(pos) - 0.5f, 0.5f);
    return -d;
}

#ifndef RANDOM_H
#define RANDOM_H

static float halton(uint index, uint base)
{
	float fraction = 1.f;
	float result = 0.f;
	while (index > 0)
	{
		fraction /= (float)base;
		result += fraction * (index % base);
		index = ~~(index / base);
	}
	return result;
}

static float2 halton23(uint index)
{
	return float2(halton(index, 2), halton(index, 3));
}

#endif

#ifndef PARTICLES_HLSLI
#define PARTICLES_HLSLI


static float getRelLife(float life, float maxLife)
{
	float relLife = saturate((maxLife - life) / maxLife);
	return relLife;
}


#endif

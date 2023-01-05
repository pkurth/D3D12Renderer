#include "pch.h"
#include "particle_systems.h"

#include "fire_particle_system.h"
#include "smoke_particle_system.h"
#include "boid_particle_system.h"

void loadAllParticleSystemPipelines()
{
	fire_particle_system::initializePipeline();
	smoke_particle_system::initializePipeline();
	boid_particle_system::initializePipeline();
}

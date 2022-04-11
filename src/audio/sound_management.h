#pragma once

#include "physics/physics.h"



extern bool soundEditorWindowOpen;

void loadSoundRegistry();
void drawSoundEditor();


const std::string& getCollisionSoundName(physics_material_type typeA, physics_material_type typeB);

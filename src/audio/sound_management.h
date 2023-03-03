#pragma once

#include "sound.h"


void loadSoundRegistry();
void drawSoundEditor(bool& open);

const sound_spec& getSoundSpec(const sound_id& id);


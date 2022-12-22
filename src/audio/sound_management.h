#pragma once

#include "sound.h"


extern bool soundEditorWindowOpen;

void loadSoundRegistry();
void drawSoundEditor();

const sound_spec& getSoundSpec(const sound_id& id);


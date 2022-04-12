#pragma once

#include "sound.h"


extern bool soundEditorWindowOpen;

void loadSoundRegistry();
void drawSoundEditor();

sound_spec getSoundSpec(const sound_id& id);


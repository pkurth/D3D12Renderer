#pragma once

#include "core/asset.h"
#include "audio_file.h"
#include "audio_generator.h"

#include <xaudio2.h>


enum audio_clip_type
{
	audio_clip_buffer,
	audio_clip_streaming_file,
	audio_clip_streaming_generator,
};

struct audio_clip
{
	~audio_clip();

	audio_clip_type type;
	audio_file file;
	bool loop;

	// Only for non-streaming.
	BYTE* dataBuffer = 0;

	// Only for streaming generator.
	audio_generator* generator = 0;
};


ref<audio_clip> createAudioClipFromFile(const fs::path& filename, bool stream, bool loop = false);
ref<audio_clip> createAudioClipFromBuffer(float* data, uint32 totalNumSamples, uint32 numChannels, uint32 sampleHz, bool loop = false);
ref<audio_clip> createAudioClipFromGenerator(audio_generator* generator, bool stream, bool loop = false);

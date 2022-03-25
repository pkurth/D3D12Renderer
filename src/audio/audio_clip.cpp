#include "pch.h"
#include "audio_clip.h"
#include "audio_file.h"

#include "core/file_registry.h"



ref<audio_clip> createAudioClipFromFile(const fs::path& filename, bool stream, bool loop)
{
	asset_handle handle = getAssetHandleFromPath(filename.lexically_normal());
	if (!handle)
	{
		return 0;
	}

	audio_file file = openAudioFile(filename);
	if (!file.valid())
	{
		return 0;
	}

	ref<audio_clip> result = make_ref<audio_clip>();

	if (!stream)
	{
		// Fill out the audio data buffer with the contents of the fourccDATA chunk.
		BYTE* dataBuffer = new BYTE[file.dataChunkSize];
		if (!readChunkData(file, dataBuffer, file.dataChunkSize, file.dataChunkPosition))
		{
			CloseHandle(file.fileHandle);
			delete[] dataBuffer;
			return 0;
		}

		result->dataBuffer = dataBuffer;
		result->type = audio_clip_buffer;

		closeAudioFile(file);
	}
	else
	{
		result->type = audio_clip_streaming_file;
	}

	result->loop = loop;
	result->file = file;

	return result;
}

ref<audio_clip> createAudioClipFromBuffer(float* data, uint32 totalNumSamples, uint32 numChannels, uint32 sampleHz, bool loop)
{
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	wfx.nChannels = numChannels;
	wfx.nSamplesPerSec = sampleHz;
	wfx.wBitsPerSample = numChannels * (uint32)sizeof(float) * 8;
	wfx.nBlockAlign = numChannels * (uint32)sizeof(float);
	wfx.nAvgBytesPerSec = sampleHz * numChannels * (uint32)sizeof(float);
	wfx.cbSize = 0; // Set to zero for PCM or IEEE float.


	ref<audio_clip> result = make_ref<audio_clip>();

	result->type = audio_clip_buffer;
	result->loop = loop;

	result->file.dataChunkSize = totalNumSamples * (uint32)sizeof(float);
	result->file.wfx.Format = wfx;

	result->dataBuffer = (BYTE*)data;

	return result;
}

ref<audio_clip> createAudioClipFromGenerator(audio_generator* generator, bool stream, bool loop)
{
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	wfx.nChannels = generator->numChannels;
	wfx.nSamplesPerSec = generator->sampleHz;
	wfx.wBitsPerSample = generator->numChannels * (uint32)sizeof(float) * 8;
	wfx.nBlockAlign = generator->numChannels * (uint32)sizeof(float);
	wfx.nAvgBytesPerSec = generator->sampleHz * generator->numChannels * (uint32)sizeof(float);
	wfx.cbSize = 0; // Set to zero for PCM or IEEE float.


	ref<audio_clip> result = make_ref<audio_clip>();

	result->dataBuffer = 0;

	if (!stream)
	{
		result->type = audio_clip_buffer;

		float* dataBuffer = new float[generator->totalNumSamples];
		generator->getNextSamples(dataBuffer, 0, generator->totalNumSamples);

		result->dataBuffer = (BYTE*)dataBuffer;
	}
	else
	{
		result->type = audio_clip_streaming_generator;
		result->generator = generator;
	}

	result->file.dataChunkSize = generator->totalNumSamples * (uint32)sizeof(float);
	result->file.wfx.Format = wfx;

	result->loop = loop;

	return result;
}

audio_clip::~audio_clip()
{
	if (dataBuffer)
	{
		delete[] dataBuffer;
		dataBuffer = 0;
	}
	if (file.valid())
	{
		closeAudioFile(file);
	}
}

#include "pch.h"
#include "audio.h"
#include "audio_file.h"
#include "core/log.h"
#include "core/cpu_profiling.h"

#include <xaudio2.h>
#include <x3daudio.h>

#include <unordered_map>



static IXAudio2MasteringVoice* masterVoice;
static com<IXAudio2> xaudio;
static X3DAUDIO_HANDLE xaudio3dInstance;

float masterAudioVolume = 0.1f;
static float oldMasterAudioVolume;

static X3DAUDIO_LISTENER listener;
static XAUDIO2_VOICE_DETAILS masterVoiceDetails;


struct audio_voice
{
	IXAudio2SourceVoice* voice;
	uint32 numOutputChannels;
	uint32 key;

	bool positioned;

	vec3 position;
	quat rotation;
};

struct voice_slot
{
	audio_voice voice;
	ref<audio_clip> clip;

	uint32 prev;
	uint32 next;

	uint32 generation;
};

#define NUM_VOICE_SLOTS 128
static voice_slot voiceSlots[NUM_VOICE_SLOTS];
static uint32 firstFreeVoiceSlot = 0;
static uint32 firstRunningVoiceSlot = -1;


static uint32 getFormatTag(const WAVEFORMATEX& wfx) noexcept
{
	if (wfx.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		if (wfx.cbSize < (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)))
		{
			return 0;
		}

		static const GUID s_wfexBase = { 0x00000000, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

		const WAVEFORMATEXTENSIBLE& wfex = (const WAVEFORMATEXTENSIBLE&)wfx;

		if (memcmp(reinterpret_cast<const BYTE*>(&wfex.SubFormat) + sizeof(DWORD),
			reinterpret_cast<const BYTE*>(&s_wfexBase) + sizeof(DWORD), sizeof(GUID) - sizeof(DWORD)) != 0)
		{
			return 0;
		}

		return wfex.SubFormat.Data1;
	}
	else
	{
		return wfx.wFormatTag;
	}
}

static uint32 getDefaultChannelMask(int channels)
{
	switch (channels)
	{
		case 1: return SPEAKER_MONO;
		case 2: return SPEAKER_STEREO;
		case 3: return SPEAKER_2POINT1;
		case 4: return SPEAKER_QUAD;
		case 5: return SPEAKER_4POINT1;
		case 6: return SPEAKER_5POINT1;
		case 7: return SPEAKER_5POINT1 | SPEAKER_BACK_CENTER;
		case 8: return SPEAKER_7POINT1;
		default: return 0;
	}
}

static uint32 getFormatKey(const WAVEFORMATEX& x)
{
	union key_gen
	{
		struct
		{
			uint32 tag : 9;
			uint32 channels : 7;
			uint32 bitsPerSample : 8;
		} pcm;

		struct
		{
			uint32 tag : 9;
			uint32 channels : 7;
			uint32 samplesPerBlock : 16;
		} adpcm;

		uint32 key;
	} result;

	result.key = 0;

	static_assert(sizeof(key_gen) == sizeof(uint32), "KeyGen is invalid");

	if (x.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		WAVEFORMATEXTENSIBLE xx = *(const WAVEFORMATEXTENSIBLE*)&x;

		if (xx.Samples.wValidBitsPerSample != 0 && xx.Samples.wValidBitsPerSample != x.wBitsPerSample)
		{
			return 0;
		}

		if (xx.dwChannelMask != 0 && xx.dwChannelMask != getDefaultChannelMask(x.nChannels))
		{
			return 0;
		}
	}

	uint32 tag = getFormatTag(x);
	switch (tag)
	{
		case WAVE_FORMAT_PCM:
			static_assert(WAVE_FORMAT_PCM < 0x1ff, "KeyGen tag is too small");
			result.pcm.tag = WAVE_FORMAT_PCM;
			result.pcm.channels = x.nChannels;
			result.pcm.bitsPerSample = x.wBitsPerSample;
			break;

		case WAVE_FORMAT_IEEE_FLOAT:
			static_assert(WAVE_FORMAT_IEEE_FLOAT < 0x1ff, "KeyGen tag is too small");

			if (x.wBitsPerSample != 32)
				return 0;

			result.pcm.tag = WAVE_FORMAT_IEEE_FLOAT;
			result.pcm.channels = x.nChannels;
			result.pcm.bitsPerSample = 32;
			break;

		case WAVE_FORMAT_ADPCM:
			static_assert(WAVE_FORMAT_ADPCM < 0x1ff, "KeyGen tag is too small");
			result.adpcm.tag = WAVE_FORMAT_ADPCM;
			result.adpcm.channels = x.nChannels;

			{
				const ADPCMWAVEFORMAT& wfadpcm = (const ADPCMWAVEFORMAT&)x;
				result.adpcm.samplesPerBlock = wfadpcm.wSamplesPerBlock;
			}
			break;

		default:
			return 0;
	}

	return result.key;
}

static void setVolume(audio_voice voice, float volume)
{
	checkResult(voice.voice->SetVolume(volume));
}

static void setPitch(audio_voice voice, float pitch)
{
	checkResult(voice.voice->SetFrequencyRatio(pitch));
}

struct voice_callback : IXAudio2VoiceCallback
{
	HANDLE playEndEvent;

	voice_callback() : playEndEvent(CreateEventEx(0, 0, 0, EVENT_MODIFY_STATE | SYNCHRONIZE)) {}
	~voice_callback() { CloseHandle(playEndEvent); }

	virtual void __stdcall OnVoiceProcessingPassStart(uint32 bytesRequired) override {}
	virtual void __stdcall OnVoiceProcessingPassEnd() override {}
	virtual void __stdcall OnStreamEnd() override
	{
		SetEvent(playEndEvent); // This for some reason only gets called for the non-streaming clips (where only one buffer is submitted).
	}
	virtual void __stdcall OnVoiceError(void* bufferContext, HRESULT error) override {}
	virtual void __stdcall OnBufferStart(void* bufferContext) override {}
	virtual void __stdcall OnBufferEnd(void* bufferContext) override
	{
		if (bufferContext)
		{
			SetEvent((HANDLE)bufferContext);
		}
	}
	virtual void __stdcall OnLoopEnd(void* bufferContext) override {}
};

static voice_callback voiceCallback;
static std::unordered_multimap<uint32, audio_voice> freeVoices;
static std::mutex mutex;

// Only call from mutexed code.
static void addToList(uint32 slot, uint32& first)
{
	if (first != -1)
	{
		assert(voiceSlots[first].prev == -1);
		voiceSlots[first].prev = slot;
	}
	voiceSlots[slot].next = first;
	voiceSlots[slot].prev = -1;
	first = slot;
}

static void removeFromList(uint32 slot, uint32& first)
{
	if (voiceSlots[slot].prev != -1)
	{
		voiceSlots[voiceSlots[slot].prev].next = voiceSlots[slot].next;
	}
	else
	{
		assert(first == slot);
		first = voiceSlots[slot].next;
	}

	if (voiceSlots[slot].next != -1)
	{
		voiceSlots[voiceSlots[slot].next].prev = voiceSlots[slot].prev;
	}
}

static std::pair<uint32, uint32> getFreeVoiceSlot()
{
	mutex.lock();

	uint32 result = firstFreeVoiceSlot;
	uint32 generation = 0;
	if (firstFreeVoiceSlot != -1)
	{
		removeFromList(result, firstFreeVoiceSlot);
		addToList(result, firstRunningVoiceSlot);

		generation = voiceSlots[result].generation;
	}

	mutex.unlock();

	if (result == -1)
	{
		LOG_WARNING("No free voice slot found");
	}

	return { result, generation };
}

static void retireVoiceSlotNoMutex(uint32 slot)
{
	voiceSlots[slot].clip.reset();
	++voiceSlots[slot].generation;

	removeFromList(slot, firstRunningVoiceSlot);
	addToList(slot, firstFreeVoiceSlot);
}

static void retireVoiceSlot(uint32 slot)
{
	const std::lock_guard<std::mutex> lock(mutex);
	retireVoiceSlotNoMutex(slot);
}

static void resetOutputMatrix(audio_voice& voice)
{
	assert(masterVoiceDetails.InputChannels == 2);

	float matrix[8];
	if (voice.numOutputChannels == 1)
	{
		matrix[0] = 0.5f;
		matrix[1] = 0.5f;
	}
	else if (voice.numOutputChannels == 2)
	{
		matrix[0] = 1.f;
		matrix[1] = 0.f;
		matrix[2] = 0.f;
		matrix[3] = 1.f;
	}
	else
	{
		assert(false);
	}

	voice.voice->SetOutputMatrix(masterVoice, voice.numOutputChannels, masterVoiceDetails.InputChannels, matrix);
}

static audio_voice getFreeVoice(const WAVEFORMATEX& wfx)
{
	audio_voice result;
	uint32 key = getFormatKey(wfx);

	mutex.lock();
	auto it = freeVoices.find(key);
	if (it == freeVoices.end())
	{
		checkResult(xaudio->CreateSourceVoice(&result.voice, (WAVEFORMATEX*)&wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCallback));
		result.key = key;
		result.numOutputChannels = wfx.nChannels;

		mutex.unlock();

	}
	else
	{
		result = it->second;
		freeVoices.erase(it);

		mutex.unlock();

		result.voice->SetSourceSampleRate(wfx.nSamplesPerSec);
	}

	resetOutputMatrix(result);
	result.positioned = false;

	assert(result.voice);
	checkResult(result.voice->Start());

	return result;
}

struct stream_context
{
	ref<audio_clip> clip;
	uint32 slotIndex;
	float volume;
	float pitch;

	bool positioned;
	vec3 position;
	quat rotation;
};

static void positionSound(audio_voice& voice, vec3 position, quat rotation = quat::identity)
{
	voice.positioned = true;
	voice.position = position;
	voice.rotation = rotation;

	vec3 forward = rotation * vec3(0.f, 0.f, -1.f);
	vec3 up = rotation * vec3(0.f, 1.f, 0.f);


	X3DAUDIO_EMITTER emitter = { 0 };
	emitter.ChannelCount = 1;
	emitter.CurveDistanceScaler = 1.f;
	emitter.OrientFront = { forward.x, forward.y, -forward.z };
	emitter.OrientTop = { up.x, up.y, -up.z };
	emitter.Position = { position.x, position.y, -position.z };

	float matrix[8] = {};


	X3DAUDIO_DSP_SETTINGS dspSettings = { 0 };
	dspSettings.SrcChannelCount = voice.numOutputChannels;
	dspSettings.DstChannelCount = masterVoiceDetails.InputChannels;
	dspSettings.pMatrixCoefficients = matrix;

	X3DAudioCalculate(xaudio3dInstance, &listener, &emitter,
		X3DAUDIO_CALCULATE_MATRIX/* | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_REVERB*/,
		&dspSettings);

	checkResult(voice.voice->SetOutputMatrix(masterVoice, voice.numOutputChannels, masterVoiceDetails.InputChannels, matrix));
}

static DWORD WINAPI streamFileAudio(void* parameter)
{
	stream_context* context = (stream_context*)parameter;

	audio_file file = context->clip->file;
	if (file.valid())
	{
		voice_slot& slot = voiceSlots[context->slotIndex];
		slot.voice = getFreeVoice((const WAVEFORMATEX&)file.wfx);



		constexpr uint32 MAX_BUFFER_COUNT = 3;
		constexpr uint32 STREAMING_BUFFER_SIZE = (1024 * 8 * 6);

		BYTE buffers[MAX_BUFFER_COUNT][STREAMING_BUFFER_SIZE];

		uint32 currentBufferIndex = 0;


		HANDLE bufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		setVolume(slot.voice, context->volume);
		setPitch(slot.voice, context->pitch);

		if (context->positioned)
		{
			positionSound(slot.voice, context->position, context->rotation);
		}

		bool error = false;

		while (!error)
		{
			uint32 currentPosition = 0;

			if (SetFilePointer(file.fileHandle, file.dataChunkPosition, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
			{
				LOG_ERROR("Could not set file pointer");
				break;
			}

			while (currentPosition < file.dataChunkSize)
			{
				DWORD size = STREAMING_BUFFER_SIZE;
				if (ReadFile(file.fileHandle, buffers[currentBufferIndex], size, &size, 0) == 0)
				{
					error = true;
					break;
				}

				currentPosition += size;

				XAUDIO2_BUFFER buffer = { 0 };
				buffer.AudioBytes = size;
				buffer.pAudioData = buffers[currentBufferIndex];
				buffer.pContext = bufferEndEvent;

				checkResult(slot.voice.voice->SubmitSourceBuffer(&buffer));

				XAUDIO2_VOICE_STATE state;
				slot.voice.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
				while (state.BuffersQueued > MAX_BUFFER_COUNT - 1)
				{
					WaitForSingleObject(bufferEndEvent, INFINITE);
					slot.voice.voice->GetState(&state);
				}
				currentBufferIndex++;
				currentBufferIndex %= MAX_BUFFER_COUNT;
			}

			if (!context->clip->loop)
			{
				break;
			}
		}

		XAUDIO2_VOICE_STATE state;
		while (slot.voice.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED), state.BuffersQueued > 0)
		{
			WaitForSingleObject(bufferEndEvent, INFINITE);
		}

		CloseHandle(bufferEndEvent);

		SetEvent(voiceCallback.playEndEvent); // Call play end ourselves. See comment in voice_callback.
		//CloseHandle(file.fileHandle);
	}
	else
	{
		retireVoiceSlot(context->slotIndex);
	}

	delete context;

	return 0;
}

static DWORD WINAPI streamProceduralAudio(void* parameter)
{
	stream_context* context = (stream_context*)parameter;

	constexpr uint32 MAX_BUFFER_COUNT = 3;
	constexpr uint32 STREAMING_BUFFER_SIZE = (1024 * 8 * 6);

	float buffers[MAX_BUFFER_COUNT][STREAMING_BUFFER_SIZE];

	uint32 currentBufferIndex = 0;


	HANDLE bufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	voice_slot& slot = voiceSlots[context->slotIndex];
	slot.voice = getFreeVoice((const WAVEFORMATEX&)context->clip->file.wfx);

	setVolume(slot.voice, context->volume);
	setPitch(slot.voice, context->pitch);

	if (context->positioned)
	{
		positionSound(slot.voice, context->position, context->rotation);
	}

	bool error = false;

	while (!error)
	{
		uint32 currentPosition = 0;

		uint32 offset = 0;

		while (true)
		{
			uint32 size = STREAMING_BUFFER_SIZE;
			size = context->clip->generator->getNextSamples(buffers[currentBufferIndex], offset, size);

			if (size == 0)
			{
				break;
			}

			offset += size;

			currentPosition += size;

			XAUDIO2_BUFFER buffer = { 0 };
			buffer.AudioBytes = size * sizeof(float);
			buffer.pAudioData = (BYTE*)buffers[currentBufferIndex];
			buffer.pContext = bufferEndEvent;

			checkResult(slot.voice.voice->SubmitSourceBuffer(&buffer));

			XAUDIO2_VOICE_STATE state;
			slot.voice.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			while (state.BuffersQueued > MAX_BUFFER_COUNT - 1)
			{
				WaitForSingleObject(bufferEndEvent, INFINITE);
				slot.voice.voice->GetState(&state);
			}
			currentBufferIndex++;
			currentBufferIndex %= MAX_BUFFER_COUNT;
		}

		if (!context->clip->loop)
		{
			break;
		}
	}

	XAUDIO2_VOICE_STATE state;
	while (slot.voice.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED), state.BuffersQueued > 0)
	{
		WaitForSingleObject(bufferEndEvent, INFINITE);
	}

	CloseHandle(bufferEndEvent);

	SetEvent(voiceCallback.playEndEvent); // Call play end ourselves. See comment in voice_callback.

	delete context;

	return 0;
}

enum audio_command_type
{
	audio_command_pause,
	audio_command_resume,
	audio_command_stop,
	audio_command_change_volume,
};

struct audio_command
{
	audio_command_type type;
	audio_handle handle;

	union
	{
		struct
		{
			float volume;
		} changeVolume;
	};
};

#define MAX_NUM_AUDIO_COMMANDS 512

static audio_command audioCommands[2][MAX_NUM_AUDIO_COMMANDS];
static std::atomic<uint32> audioCommandIndex;
static std::atomic<uint32> audioCommandsCompletelyWritten[2];

#define _AUDIO_COMMAND_GET_ARRAY_INDEX(v) ((v) >> 31)
#define _AUDIO_COMMAND_GET_EVENT_INDEX(v) ((v) & 0xFFFF)

static std::pair<uint32, audio_command*> startAudioCommand(audio_command_type type, audio_handle handle)
{
	uint32 arrayAndEventIndex = audioCommandIndex++;
	uint32 eventIndex = _AUDIO_COMMAND_GET_EVENT_INDEX(arrayAndEventIndex);
	uint32 arrayIndex = _AUDIO_COMMAND_GET_ARRAY_INDEX(arrayAndEventIndex);
	assert(eventIndex < MAX_NUM_AUDIO_COMMANDS);
	audio_command* c = audioCommands[arrayIndex] + eventIndex;
	c->type = type;
	c->handle = handle;
	return { arrayIndex, c };
}

static void endAudioCommand(uint32 arrayIndex)
{
	audioCommandsCompletelyWritten[arrayIndex].fetch_add(1, std::memory_order_release); // Mark this event as written. Release means that the compiler may not reorder the previous writes after this.
}

static DWORD WINAPI audioThread(void* parameter)
{
	while (true)
	{
		Sleep(10);

		uint32 arrayIndex = _AUDIO_COMMAND_GET_ARRAY_INDEX(audioCommandIndex); // We are only interested in the most significant bit here, so don't worry about thread safety.
		uint32 currentIndex = audioCommandIndex.exchange((1 - arrayIndex) << 31); // Swap array and get current event count.

		audio_command* commands = audioCommands[arrayIndex];
		uint32 numCommands = _AUDIO_COMMAND_GET_EVENT_INDEX(currentIndex);

		while (numCommands > audioCommandsCompletelyWritten[arrayIndex]) {} // Wait until all events and stats have been written completely.
		audioCommandsCompletelyWritten[arrayIndex] = 0;


		for (uint32 i = 0; i < numCommands; ++i)
		{
			audio_command* c = commands + i;

			if (c->handle.valid())
			{
				IXAudio2SourceVoice* voice = voiceSlots[c->handle.slotIndex].voice.voice;

				switch (c->type)
				{
					case audio_command_pause:
					{
						voice->Stop();
					} break;

					case audio_command_resume:
					{
						voice->Start();
					} break;

					case audio_command_stop:
					{
						voice->Stop();
						voice->FlushSourceBuffers();
					} break;
					
					case audio_command_change_volume:
					{
						voice->SetVolume(c->changeVolume.volume);
					} break;
				}
			}
			else
			{
				LOG_MESSAGE("Audio handle %u (generation %u) is not valid", c->handle.slotIndex, c->handle.generation);
			}
		}


		//if (WaitForSingleObject(voiceCallback.playEndEvent, INFINITE) == WAIT_OBJECT_0)
		{
			mutex.lock();

			for (uint32 i = firstRunningVoiceSlot; i != -1;)
			{
				voice_slot& slot = voiceSlots[i];

				uint32 next = slot.next;

				if (!slot.voice.voice)
				{
					i = next;
					continue;
				}

				XAUDIO2_VOICE_STATE state;
				slot.voice.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);


				if (!state.BuffersQueued)
				{
					slot.voice.voice->Stop();

					LOG_MESSAGE("Retiring voice slot %u", i);

					if (slot.voice.key != 0)
					{
						freeVoices.insert({ slot.voice.key, slot.voice });
					}
					else
					{
						slot.voice.voice->DestroyVoice();
					}

					retireVoiceSlotNoMutex(i);
				}

				i = next;
			}

			mutex.unlock();
		}
	}

	return 0;
}

bool initializeAudio()
{
	oldMasterAudioVolume = masterAudioVolume;

	uint32 flags = 0;
#ifdef _DEBUG
	flags |= XAUDIO2_DEBUG_ENGINE;
#endif
	checkResult(XAudio2Create(xaudio.GetAddressOf(), flags));
	checkResult(xaudio->CreateMasteringVoice(&masterVoice));

	masterVoice->SetVolume(masterAudioVolume);

	DWORD channelMask;
	masterVoice->GetChannelMask(&channelMask);
	masterVoice->GetVoiceDetails(&masterVoiceDetails);

	X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, xaudio3dInstance);

	for (uint32 i = 0; i < NUM_VOICE_SLOTS; ++i)
	{
		voiceSlots[i].prev = (i == 0) ? -1 : (i - 1);
		voiceSlots[i].next = (i == NUM_VOICE_SLOTS - 1) ? -1 : (i + 1);
	}

	listener.OrientFront = { 0.f, 0.f, 1.f };
	listener.OrientTop = { 0.f, 1.f, 0.f };

	HANDLE audioThreadHandle = CreateThread(0, 0, audioThread, 0, 0, 0);
	SetThreadPriority(audioThreadHandle, THREAD_PRIORITY_HIGHEST);
	SetThreadDescription(audioThreadHandle, L"Audio thread");

	return true;
}

void shutdownAudio()
{
	xaudio->StopEngine();
}

static audio_handle playAudio(const ref<audio_clip>& clip, float volume, float pitch, bool positioned, vec3 position = vec3(0.f, 0.f, 0.f), quat rotation = quat::identity)
{
	if (!clip)
	{
		return {};
	}

	auto [slotIndex, slotGeneration] = getFreeVoiceSlot();
	if (slotIndex == -1)
	{
		return {};
	}

	voice_slot& slot = voiceSlots[slotIndex];
	slot.clip = clip;
	slot.voice.voice = 0;

	if (clip->type == audio_clip_buffer)
	{
		slot.voice = getFreeVoice((const WAVEFORMATEX&)clip->file.wfx);

		XAUDIO2_BUFFER buffer = { 0 };
		buffer.AudioBytes = clip->file.dataChunkSize;
		buffer.pAudioData = clip->dataBuffer;
		if (clip->loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}
		else
		{
			buffer.Flags = XAUDIO2_END_OF_STREAM;
		}

		setVolume(slot.voice, volume);
		setPitch(slot.voice, pitch);

		if (positioned)
		{
			positionSound(slot.voice, position, rotation);
		}

		checkResult(slot.voice.voice->SubmitSourceBuffer(&buffer));
	}
	else
	{
		auto func = (clip->type == audio_clip_streaming_file) ? streamFileAudio : streamProceduralAudio;

		HANDLE threadHandle = CreateThread(0, 0, func, new stream_context{ clip, slotIndex, volume, pitch, positioned, position, rotation }, 0, 0);
		if (!threadHandle)
		{
			retireVoiceSlot(slotIndex);
			return {};
		}

		CloseHandle(threadHandle);
	}

	return { slotIndex, slotGeneration };
}

audio_handle play2DAudio(const ref<audio_clip>& clip, float volume, float pitch)
{
	return playAudio(clip, volume, pitch, false);
}

audio_handle play3DAudio(const ref<audio_clip>& clip, float volume, float pitch, vec3 position)
{
	return playAudio(clip, volume, pitch, true, position);
}

void setAudioListener(vec3 position, quat rotation, vec3 velocity)
{
	vec3 forward = rotation * vec3(0.f, 0.f, -1.f);
	vec3 up = rotation * vec3(0.f, 1.f, 0.f);

	// XAudio2 uses a lefthanded coordinate system, which is why we need to flip the z-axis.
	listener.OrientFront = { forward.x, forward.y, -forward.z };
	listener.OrientTop = { up.x, up.y, -up.z };
	listener.Position = { position.x, position.y, -position.z };
	listener.Velocity = { velocity.x, velocity.y, -velocity.z };
	listener.pCone = 0;
}

void updateAudio(game_scene& scene)
{
	CPU_PROFILE_BLOCK("Update audio");

	if (masterAudioVolume != oldMasterAudioVolume)
	{
		masterVoice->SetVolume(masterAudioVolume);
		oldMasterAudioVolume = masterAudioVolume;
	}

	mutex.lock();

	for (uint32 i = firstRunningVoiceSlot; i != -1;)
	{
		voice_slot& slot = voiceSlots[i];

		uint32 next = slot.next;

		if (!slot.voice.voice)
		{
			i = next;
			continue;
		}

		if (slot.voice.positioned)
		{
			positionSound(slot.voice, slot.voice.position, slot.voice.rotation);
		}

		i = next;
	}

	mutex.unlock();
}

bool audio_handle::valid()
{
	return slotIndex != -1 && voiceSlots[slotIndex].generation == generation;
}

void audio_handle::pause()
{
	auto [index, command] = startAudioCommand(audio_command_pause, *this);
	endAudioCommand(index);
}

void audio_handle::resume()
{
	auto [index, command] = startAudioCommand(audio_command_resume, *this);
	endAudioCommand(index);
}

void audio_handle::stop()
{
	auto [index, command] = startAudioCommand(audio_command_stop, *this);
	endAudioCommand(index);
}

void audio_handle::changeVolume(float volume)
{
	auto [index, command] = startAudioCommand(audio_command_change_volume, *this);
	command->changeVolume.volume = volume;
	endAudioCommand(index);
}

#include "pch.h"
#include "dx_pipeline.h"
#include "threading.h"

#include <unordered_map>
#include <set>
#include <deque>
#include <filesystem>
#include <iostream>
#include <ppl.h>

namespace fs = std::filesystem;

static const wchar* shaderDir = L"shaders/bin/";
static DWORD checkForFileChanges(void*);

struct shader_file
{
	dx_blob blob;
	std::set<struct reloadable_pipeline_state*> usedByPipelines;
};


static std::wstring stringToWideString(const std::string& s)
{
	return std::wstring(s.begin(), s.end());
}

struct reloadable_pipeline_state
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	graphics_pipeline_files files;

	dx_pipeline_state pipeline;
	dx_root_signature rootSignature;
	bool userRootSignature;

	D3D12_INPUT_ELEMENT_DESC inputLayout[16];

	reloadable_pipeline_state(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, 
		const graphics_pipeline_files& files,
		dx_root_signature userRootSignature) :
		desc(desc),
		files(files),
		rootSignature(userRootSignature),
		userRootSignature(userRootSignature != nullptr)
	{
		assert(desc.InputLayout.NumElements <= arraysize(inputLayout));

		memcpy(inputLayout, desc.InputLayout.pInputElementDescs, sizeof(D3D12_INPUT_ELEMENT_DESC) * desc.InputLayout.NumElements);
		this->desc.InputLayout.pInputElementDescs = inputLayout;

		if (desc.InputLayout.NumElements == 0)
		{
			this->desc.InputLayout.pInputElementDescs = nullptr;
		}
	}
};


static std::unordered_map<std::string, shader_file> shaderBlobs;
static std::deque<reloadable_pipeline_state> pipelines;
static std::vector<reloadable_pipeline_state*> dirtyPipelines;
static thread_mutex mutex;


static void pushBlob(const std::string& filename, reloadable_pipeline_state* pipelineIndex)
{
	if (filename.size() > 0)
	{
		auto it = shaderBlobs.find(filename);
		if (it == shaderBlobs.end())
		{
			std::wstring filepath = shaderDir + stringToWideString(filename) + L".cso";

			dx_blob blob;
			checkResult(D3DReadFileToBlob(filepath.c_str(), &blob));

			shaderBlobs[filename] = { blob, { pipelineIndex } };
		}
		else
		{
			it->second.usedByPipelines.insert(pipelineIndex);
		}
	}
}

dx_pipeline createReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const graphics_pipeline_files& files,
	dx_root_signature userRootSignature)
{
	assert((files.rs.size() > 0) ^ (userRootSignature != nullptr));

	pipelines.emplace_back(desc, files, userRootSignature);
	auto& state = pipelines.back();

	pushBlob(files.rs, &state);
	pushBlob(files.vs, &state);
	pushBlob(files.ps, &state);
	pushBlob(files.gs, &state);
	pushBlob(files.ds, &state);
	pushBlob(files.hs, &state);

	// What. The. Fuck.
	// The root signature looses its internal pointer, if I take the pointer to the ComPtr.
	// Pipeline doesn't.
	auto p = state.pipeline.Get();
	auto r = state.rootSignature.Get();

	dx_pipeline result = { &state.pipeline, &state.rootSignature };

	state.pipeline = p;
	state.rootSignature = r;

	return result;
}

static void loadPipeline(reloadable_pipeline_state& p)
{
	if (p.files.rs.size() > 0) { dx_blob rs = shaderBlobs[p.files.rs].blob; dxContext.retireObject(p.rootSignature); p.rootSignature = createRootSignature(&dxContext, rs); }
	if (p.files.vs.size() > 0) { dx_blob shader = shaderBlobs[p.files.vs].blob; p.desc.VS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
	if (p.files.ps.size() > 0) { dx_blob shader = shaderBlobs[p.files.ps].blob; p.desc.PS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
	if (p.files.gs.size() > 0) { dx_blob shader = shaderBlobs[p.files.gs].blob; p.desc.GS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
	if (p.files.ds.size() > 0) { dx_blob shader = shaderBlobs[p.files.ds].blob; p.desc.DS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
	if (p.files.hs.size() > 0) { dx_blob shader = shaderBlobs[p.files.hs].blob; p.desc.HS = CD3DX12_SHADER_BYTECODE(shader.Get()); }

	p.desc.pRootSignature = p.rootSignature.Get();
	dxContext.retireObject(p.pipeline);
	checkResult(dxContext.device->CreateGraphicsPipelineState(&p.desc, IID_PPV_ARGS(&p.pipeline)));
}

void createAllReloadablePipelines()
{
	concurrency::parallel_for(0, (int)pipelines.size(), [&](int i)
	{
		loadPipeline(pipelines[i]);
	});

	mutex = createMutex();
	CreateThread(0, 0, checkForFileChanges, 0, 0, 0);
}

void checkForChangedPipelines()
{
	lock(mutex);
	concurrency::parallel_for(0, (int)dirtyPipelines.size(), [&](int i)
	{
		loadPipeline(*dirtyPipelines[i]);
	});
	dirtyPipelines.clear();
	unlock(mutex);
}

static bool fileIsLocked(const wchar* filename)
{
	HANDLE fileHandle = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		return true;
	}
	CloseHandle(fileHandle);
	return false;
}

static DWORD checkForFileChanges(void*)
{
	HANDLE directoryHandle;
	OVERLAPPED overlapped;

	uint8 buffer[1024] = {};

	directoryHandle = CreateFileW(
		shaderDir,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL);

	if (directoryHandle == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Monitor directory failed.\n");
		return 1;
	}

	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	ResetEvent(overlapped.hEvent);

	DWORD eventName = FILE_NOTIFY_CHANGE_LAST_WRITE;

	DWORD error = ReadDirectoryChangesW(directoryHandle,
		buffer, sizeof(buffer), TRUE,
		eventName,
		NULL, &overlapped, NULL);

	fs::path lastChangedPath = "";
	fs::file_time_type lastChangedPathTimeStamp;

	while (true)
	{
		DWORD result = WaitForSingleObject(overlapped.hEvent, INFINITE);

		DWORD dw;
		if (!GetOverlappedResult(directoryHandle, &overlapped, &dw, FALSE) || dw == 0)
		{
			fprintf(stderr, "Get overlapped result failed.\n");
			return 1;
		}

		FILE_NOTIFY_INFORMATION* filenotify;

		DWORD offset = 0;

		do
		{
			filenotify = (FILE_NOTIFY_INFORMATION*)(&buffer[offset]);

			if (filenotify->Action == FILE_ACTION_MODIFIED)
			{
				char filename[MAX_PATH];
				int ret = WideCharToMultiByte(CP_ACP, 0, filenotify->FileName,
					filenotify->FileNameLength / sizeof(WCHAR),
					filename, MAX_PATH, NULL, NULL);

				filename[filenotify->FileNameLength / sizeof(WCHAR)] = 0;

				fs::path changedPath = (shaderDir / fs::path(filename)).lexically_normal();
				auto changedPathWriteTime = fs::last_write_time(changedPath);

				// The filesystem usually sends multiple notifications for changed files, since the file is first written, then metadata is changed etc.
				// This check prevents these notifications if they are too close together in time.
				// This is a pretty crude fix. In this setup files should not change at the same time, since we only ever track one file.
				if (changedPath == lastChangedPath
					&& std::chrono::duration_cast<std::chrono::milliseconds>(changedPathWriteTime - lastChangedPathTimeStamp).count() < 200)
				{
					lastChangedPath = changedPath;
					lastChangedPathTimeStamp = changedPathWriteTime;
					break;
				}

				bool isFile = !fs::is_directory(changedPath);

				if (isFile)
				{
					auto it = shaderBlobs.find(changedPath.stem().string());
					if (it != shaderBlobs.end())
					{
						while (fileIsLocked(changedPath.wstring().c_str()))
						{
							//printf("File is locked.\n");
						}

						std::cout << "Reloading shader blob " << changedPath << std::endl;
						dx_blob blob;
						checkResult(D3DReadFileToBlob(changedPath.wstring().c_str(), &blob));

						lock(mutex);
						it->second.blob = blob;
						dirtyPipelines.insert(dirtyPipelines.end(), it->second.usedByPipelines.begin(), it->second.usedByPipelines.end());
						unlock(mutex);
					}

					lastChangedPath = changedPath;
					lastChangedPathTimeStamp = changedPathWriteTime;
				}

			}

			offset += filenotify->NextEntryOffset;

		} while (filenotify->NextEntryOffset != 0);


		if (!ResetEvent(overlapped.hEvent))
		{
			fprintf(stderr, "Reset event failed.\n");
		}

		DWORD error = ReadDirectoryChangesW(directoryHandle,
			buffer, sizeof(buffer), TRUE,
			eventName,
			NULL, &overlapped, NULL);

		if (error == 0)
		{
			fprintf(stderr, "Read directory failed.\n");
		}

	}

	return 0;
}



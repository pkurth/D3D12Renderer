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

	struct reloadable_root_signature* rootSignature;
};


static std::wstring stringToWideString(const std::string& s)
{
	return std::wstring(s.begin(), s.end());
}

enum pipeline_type
{
	pipeline_type_graphics,
	pipeline_type_compute,
};

struct reloadable_pipeline_state
{
	pipeline_type type;

	union
	{
		struct
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc;
			graphics_pipeline_files graphicsFiles;
		};

		struct
		{
			D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc;
			const char* computeFile;
		};
	};

	dx_pipeline_state pipeline;
	dx_root_signature* rootSignature;

	D3D12_INPUT_ELEMENT_DESC inputLayout[16];

	reloadable_pipeline_state() {}

	void initialize(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const graphics_pipeline_files& files, dx_root_signature* rootSignature)
	{
		this->type = pipeline_type_graphics;
		this->graphicsDesc = desc;
		this->graphicsFiles = files;
		this->rootSignature = rootSignature;

		assert(desc.InputLayout.NumElements <= arraysize(inputLayout));

		memcpy(inputLayout, desc.InputLayout.pInputElementDescs, sizeof(D3D12_INPUT_ELEMENT_DESC) * desc.InputLayout.NumElements);
		this->graphicsDesc.InputLayout.pInputElementDescs = inputLayout;

		if (desc.InputLayout.NumElements == 0)
		{
			this->graphicsDesc.InputLayout.pInputElementDescs = nullptr;
		}
	}

	void initialize(const char* file, dx_root_signature* rootSignature)
	{
		this->type = pipeline_type_compute;
		this->computeDesc = {};
		this->computeFile = file;
		this->rootSignature = rootSignature;
	}
};

struct reloadable_root_signature
{
	const char* file;
	dx_root_signature rootSignature;
};


static std::unordered_map<std::string, shader_file> shaderBlobs;
static std::deque<reloadable_pipeline_state> pipelines;
static std::deque<reloadable_root_signature> rootSignaturesFromFiles;
static std::deque<dx_root_signature> userRootSignatures;


static std::vector<reloadable_pipeline_state*> dirtyPipelines;
static std::vector<reloadable_root_signature*> dirtyRootSignatures;
static thread_mutex mutex;


static reloadable_root_signature* pushBlob(const char* filename, reloadable_pipeline_state* pipelineIndex, bool isRootSignature = false)
{
	reloadable_root_signature* result = 0;

	if (filename)
	{
		auto it = shaderBlobs.find(filename);
		if (it == shaderBlobs.end())
		{
			// New file.

			std::wstring filepath = shaderDir + stringToWideString(filename) + L".cso";

			dx_blob blob;
			checkResult(D3DReadFileToBlob(filepath.c_str(), &blob));

			if (isRootSignature)
			{
				rootSignaturesFromFiles.push_back({ filename, nullptr });
				result = &rootSignaturesFromFiles.back();
			}

			shaderBlobs[filename] = { blob, { pipelineIndex }, result };
		}
		else
		{
			// Already used.

			it->second.usedByPipelines.insert(pipelineIndex);

			if (isRootSignature)
			{
				if (!it->second.rootSignature)
				{
					rootSignaturesFromFiles.push_back({ filename, nullptr });
					it->second.rootSignature = &rootSignaturesFromFiles.back();
				}

				result = it->second.rootSignature;
			}
		}
	}

	return result;
}

dx_pipeline createReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const graphics_pipeline_files& files,
	dx_root_signature userRootSignature)
{
	pipelines.emplace_back();
	auto& state = pipelines.back();

	pushBlob(files.vs, &state);
	pushBlob(files.ps, &state);
	pushBlob(files.gs, &state);
	pushBlob(files.ds, &state);
	pushBlob(files.hs, &state);

	userRootSignatures.push_back(userRootSignature);
	dx_root_signature* rootSignature = &userRootSignatures.back();
	userRootSignatures.back() = userRootSignature; // Fuck. You.

	state.initialize(desc, files, rootSignature);

	dx_pipeline result = { &state.pipeline, rootSignature };
	return result;
}

dx_pipeline createReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const graphics_pipeline_files& files, const char* rootSignatureFile)
{
	if (!rootSignatureFile)
	{
		rootSignatureFile = files.ps;
	}

	pipelines.emplace_back();
	auto& state = pipelines.back();

	reloadable_root_signature* reloadableRS = pushBlob(rootSignatureFile, &state, true);
	pushBlob(files.vs, &state);
	pushBlob(files.ps, &state);
	pushBlob(files.gs, &state);
	pushBlob(files.ds, &state);
	pushBlob(files.hs, &state);

	dx_root_signature* rootSignature = &reloadableRS->rootSignature;

	state.initialize(desc, files, rootSignature);

	dx_pipeline result = { &state.pipeline, rootSignature };
	return result;
}

dx_pipeline createReloadablePipeline(const char* csFile, dx_root_signature userRootSignature)
{
	pipelines.emplace_back();
	auto& state = pipelines.back();

	pushBlob(csFile, &state);

	userRootSignatures.push_back(userRootSignature);
	dx_root_signature* rootSignature = &userRootSignatures.back();
	userRootSignatures.back() = userRootSignature; // Fuck. You.

	state.initialize(csFile, rootSignature);

	dx_pipeline result = { &state.pipeline, rootSignature };
	return result;
}

dx_pipeline createReloadablePipeline(const char* csFile, const char* rootSignatureFile)
{
	if (!rootSignatureFile)
	{
		rootSignatureFile = csFile;
	}

	pipelines.emplace_back();
	auto& state = pipelines.back();

	reloadable_root_signature* reloadableRS = pushBlob(rootSignatureFile, &state, true);
	pushBlob(csFile, &state);

	dx_root_signature* rootSignature = &reloadableRS->rootSignature;

	state.initialize(csFile, rootSignature);

	dx_pipeline result = { &state.pipeline, rootSignature };
	return result;
}

static void loadRootSignature(reloadable_root_signature& r)
{
	dx_blob rs = shaderBlobs[r.file].blob;

	dxContext.retireObject(r.rootSignature); 
	r.rootSignature = createRootSignature(rs);
}

static void loadPipeline(reloadable_pipeline_state& p)
{
	dxContext.retireObject(p.pipeline);

	if (p.type == pipeline_type_graphics)
	{
		if (p.graphicsFiles.vs) { dx_blob shader = shaderBlobs[p.graphicsFiles.vs].blob; p.graphicsDesc.VS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
		if (p.graphicsFiles.ps) { dx_blob shader = shaderBlobs[p.graphicsFiles.ps].blob; p.graphicsDesc.PS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
		if (p.graphicsFiles.gs) { dx_blob shader = shaderBlobs[p.graphicsFiles.gs].blob; p.graphicsDesc.GS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
		if (p.graphicsFiles.ds) { dx_blob shader = shaderBlobs[p.graphicsFiles.ds].blob; p.graphicsDesc.DS = CD3DX12_SHADER_BYTECODE(shader.Get()); }
		if (p.graphicsFiles.hs) { dx_blob shader = shaderBlobs[p.graphicsFiles.hs].blob; p.graphicsDesc.HS = CD3DX12_SHADER_BYTECODE(shader.Get()); }

		p.graphicsDesc.pRootSignature = p.rootSignature->Get();
		checkResult(dxContext.device->CreateGraphicsPipelineState(&p.graphicsDesc, IID_PPV_ARGS(&p.pipeline)));
	}
	else
	{
		dx_blob shader = shaderBlobs[p.computeFile].blob; p.computeDesc.CS = CD3DX12_SHADER_BYTECODE(shader.Get());

		p.computeDesc.pRootSignature = p.rootSignature->Get();
		checkResult(dxContext.device->CreateComputePipelineState(&p.computeDesc, IID_PPV_ARGS(&p.pipeline)));
	}
}

void createAllReloadablePipelines()
{
	concurrency::parallel_for(0, (int)rootSignaturesFromFiles.size(), [&](int i)
	{
		loadRootSignature(rootSignaturesFromFiles[i]);
	});

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
	concurrency::parallel_for(0, (int)dirtyRootSignatures.size(), [&](int i)
	{
		loadRootSignature(*dirtyRootSignatures[i]);
	});

	concurrency::parallel_for(0, (int)dirtyPipelines.size(), [&](int i)
	{
		loadPipeline(*dirtyPipelines[i]);
	});
	dirtyRootSignatures.clear();
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
						if (it->second.rootSignature)
						{
							dirtyRootSignatures.push_back(it->second.rootSignature);
						}
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



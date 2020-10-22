#include "pch.h"

#include <imgui/imgui_draw.cpp>
#include <imgui/imgui.cpp>
#include <imgui/imgui_widgets.cpp>
#include <imgui/imgui_demo.cpp>

#include "imgui.h"
#include "window.h"
#include "dx_context.h"
#include "dx_render_primitives.h"
#include "dx_command_list.h"

#include <iostream>

static dx_texture fontTexture;
static dx_cbv_srv_uav_descriptor_heap fontTextureDescriptorHeap;
static dx_descriptor_handle fontTextureDescriptorHandle;

static dx_pipeline_state pipeline;
static dx_root_signature rootSignature;

static ImGuiContext* imguiContext;
static bool initializedImGui;


static const char* vertexShader =
R"(
cbuffer vertexBuffer : register(b0)
{
    float4x4 proj;
};

struct vs_input
{
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

struct vs_output
{
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
    float4 pos : SV_POSITION;
};

vs_output main(vs_input IN)
{
    vs_output OUT;
    OUT.pos = mul(proj, float4(IN.pos.xy, 0.f, 1.f));
    OUT.col = IN.col;
    OUT.uv  = IN.uv;
    return OUT;
}
)";

static const char* pixelShader =
R"(
struct ps_input
{
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

SamplerState sampler0 : register(s0);
Texture2D<float> texture0 : register(t0);

float4 main(ps_input IN) : SV_TARGET
{
    float4 out_col = IN.col * texture0.Sample(sampler0, IN.uv); 
    return out_col; 
}
)";


ImGuiContext* initializeImGui(D3D12_RT_FORMAT_ARRAY screenRTFormats)
{
	IMGUI_CHECKVERSION();
	imguiContext = ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	io.KeyMap[ImGuiKey_Tab] = button_tab;
	io.KeyMap[ImGuiKey_LeftArrow] = button_left;
	io.KeyMap[ImGuiKey_RightArrow] = button_right;
	io.KeyMap[ImGuiKey_UpArrow] = button_up;
	io.KeyMap[ImGuiKey_DownArrow] = button_down;
	io.KeyMap[ImGuiKey_Delete] = button_delete;
	io.KeyMap[ImGuiKey_Backspace] = button_backspace;
	io.KeyMap[ImGuiKey_Enter] = button_enter;
	io.KeyMap[ImGuiKey_Escape] = button_esc;
	io.KeyMap[ImGuiKey_A] = button_a;
	io.KeyMap[ImGuiKey_C] = button_c;
	io.KeyMap[ImGuiKey_V] = button_v;
	io.KeyMap[ImGuiKey_X] = button_x;
	io.KeyMap[ImGuiKey_Y] = button_y;
	io.KeyMap[ImGuiKey_Z] = button_z;

	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

	fontTexture = createTexture(&dxContext, pixels, width, height, DXGI_FORMAT_R8_UNORM);
	io.Fonts->TexID = (void*)&fontTexture;

	fontTextureDescriptorHeap = createDescriptorHeap(&dxContext, 1);
	fontTextureDescriptorHandle = fontTextureDescriptorHeap.push2DTextureSRV(fontTexture);




	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
	;

	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // Projection matrix.
	rootParameters[1].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	rootSignature = createRootSignature(&dxContext, rootParameters, arraysize(rootParameters), &sampler, 1, rootSignatureFlags);

	dx_blob vertexShaderBlob;
	dx_blob pixelShaderBlob;

	checkResult(D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &vertexShaderBlob, NULL));
	checkResult(D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &pixelShaderBlob, NULL));

	struct pipeline_state_stream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blendDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizerDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
	} pipelineStateStream;


	pipelineStateStream.rootSignature = rootSignature.Get();
	pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.rtvFormats = screenRTFormats;

	CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
	depthDesc.DepthEnable = false;
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	pipelineStateStream.depthStencilDesc = depthDesc;

	CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pipelineStateStream.blendDesc = blendDesc;

	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pipelineStateStream.rasterizerDesc = rasterizerDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(dxContext.device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipeline)));

	SET_NAME(rootSignature, "ImGui Root Signature");
	SET_NAME(pipeline, "ImGui Pipeline");

	return imguiContext;
}

void newImGuiFrame(const user_input& input, float dt)
{
	if (win32_window::mainWindow && imguiContext)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2((float)win32_window::mainWindow->clientWidth, (float)win32_window::mainWindow->clientHeight);
		io.DeltaTime = dt;
		io.MousePos = ImVec2((float)input.mouse.x, (float)input.mouse.y);
		io.MouseDown[0] = input.mouse.left.isDown;
		io.MouseDown[1] = input.mouse.right.isDown;
		io.MouseWheel = input.mouse.scroll;
		io.KeyAlt = isDown(input, button_alt);
		io.KeyShift = isDown(input, button_shift);
		io.KeyCtrl = isDown(input, button_ctrl);
		for (uint32 i = 0; i < button_count; ++i)
		{
			io.KeysDown[i] = input.keyboard[i].isDown;
		}

		ImGui::NewFrame();
	}
	else
	{
		std::cerr << "ImGui is not initialized." << std::endl;
	}
}

void renderImGui(dx_command_list* cl)
{
	ImGui::Render();

	ImDrawData* drawData = ImGui::GetDrawData();

	if (drawData->DisplaySize.x <= 0.f || drawData->DisplaySize.y <= 0.f)
	{
		return;
	}


	uint32 vertexBufferSizeInBytes = sizeof(ImDrawVert) * drawData->TotalVtxCount;
	dx_allocation vertexBufferAllocation = cl->allocateDynamicBuffer(vertexBufferSizeInBytes);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = vertexBufferAllocation.gpuPtr;
	vertexBufferView.SizeInBytes = vertexBufferSizeInBytes;
	vertexBufferView.StrideInBytes = sizeof(ImDrawVert);

	uint32 indexBufferSizeInBytes = sizeof(ImDrawIdx) * drawData->TotalIdxCount;
	dx_allocation indexBufferAllocation = cl->allocateDynamicBuffer(indexBufferSizeInBytes);

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	indexBufferView.BufferLocation = indexBufferAllocation.gpuPtr;
	indexBufferView.SizeInBytes = indexBufferSizeInBytes;
	indexBufferView.Format = getIndexBufferFormat(sizeof(ImDrawIdx));


	ImDrawVert* vertexDestination = (ImDrawVert*)vertexBufferAllocation.cpuPtr;
	ImDrawIdx* indexDestination = (ImDrawIdx*)indexBufferAllocation.cpuPtr;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* drawList = drawData->CmdLists[n];
		memcpy(vertexDestination, drawList->VtxBuffer.Data, drawList->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(indexDestination, drawList->IdxBuffer.Data, drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
		vertexDestination += drawList->VtxBuffer.Size;
		indexDestination += drawList->IdxBuffer.Size;
	}


	struct imgui_cb
	{
		float mvp[4][4];
	} cb;

	float L = drawData->DisplayPos.x;
	float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
	float T = drawData->DisplayPos.y;
	float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
	float mvp[4][4] =
	{
		{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
		{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
		{ 0.0f,         0.0f,           0.5f,       0.0f },
		{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
	};
	memcpy(&cb.mvp[0][0], mvp, sizeof(mvp));


	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, drawData->DisplaySize.x, drawData->DisplaySize.y);




	cl->setViewport(viewport);

	cl->setVertexBuffer(0, vertexBufferView);
	cl->setIndexBuffer(indexBufferView);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->setPipelineState(pipeline);
	cl->setGraphicsRootSignature(rootSignature);
	cl->setGraphics32BitConstants(0, cb);

	cl->setDescriptorHeap(fontTextureDescriptorHeap);
	cl->setGraphicsDescriptorTable(1, fontTextureDescriptorHandle);

	// Setup blend factor
	const float blendFactor[] = { 0.f, 0.f, 0.f, 0.f };
	cl->setBlendFactor(blendFactor);


	int globalVertexOffset = 0;
	int globalIndexOffset = 0;
	ImVec2 clip_off = drawData->DisplayPos;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
				if (r.right > r.left && r.bottom > r.top)
				{
					//ctx->SetGraphicsRootDescriptorTable(1, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
					cl->setScissor(r);
					cl->drawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + globalIndexOffset, pcmd->VtxOffset + globalVertexOffset, 0);
				}
			}
		}
		globalIndexOffset += cmd_list->IdxBuffer.Size;
		globalVertexOffset += cmd_list->VtxBuffer.Size;
	}
}

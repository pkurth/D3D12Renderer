#include "pch.h"
#include "debug_visualization.h"


void flat_simple_material::setupPipeline(dx_command_list* cl, const common_material_info& info)
{
	cl->setPipelineState(*flatSimplePipeline.pipeline);
	cl->setGraphicsRootSignature(*flatSimplePipeline.rootSignature);

	cl->setGraphicsDynamicConstantBuffer(2, info.cameraCBV);
}

void flat_simple_material::prepareForRendering(dx_command_list* cl)
{
	cl->setGraphics32BitConstants(1, color);
}

void flat_unlit_triangle_material::setupPipeline(dx_command_list* cl, const common_material_info& info)
{
	cl->setPipelineState(*flatUnlitTrianglePipeline.pipeline);
	cl->setGraphicsRootSignature(*flatUnlitTrianglePipeline.rootSignature);
}

void flat_unlit_line_material::setupPipeline(dx_command_list* cl, const common_material_info& info)
{
	cl->setPipelineState(*flatUnlitLinePipeline.pipeline);
	cl->setGraphicsRootSignature(*flatUnlitLinePipeline.rootSignature);
}

void flat_unlit_material::prepareForRendering(dx_command_list* cl)
{
	cl->setGraphics32BitConstants(1, color);
}


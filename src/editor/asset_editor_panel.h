#pragma once

#include "dx/dx_texture.h"
#include "core/camera.h"
#include "rendering/main_renderer.h"


struct asset_editor_panel
{
	void beginFrame();
	virtual void endFrame() {}

	bool isOpen() const { return windowOpen; }
	void open();
	void close();

protected:
	const char* title;
	const char* dragDropTarget;

private:
	virtual void edit(uint32 renderWidth, uint32 renderHeight) = 0;
	virtual ref<dx_texture> getRendering() = 0;
	virtual void setDragDropData(void* data, uint32 size) {}
	
	bool windowOpen = false;
	bool windowOpenInternal = false;
};

struct mesh_editor_panel : asset_editor_panel
{
	mesh_editor_panel();

	virtual void endFrame() override;

private:
	virtual void edit(uint32 renderWidth, uint32 renderHeight) override;
	virtual ref<dx_texture> getRendering() override;
	virtual void setDragDropData(void* data, uint32 size) override;

	render_camera camera;
	ref<multi_mesh> mesh;
	main_renderer renderer;

	opaque_render_pass renderPass;
};


struct editor_panels
{
	mesh_editor_panel meshEditor;
};



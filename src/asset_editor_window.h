#pragma once

#include "dx_texture.h"

struct asset_editor_window
{
	virtual void setAsset(const fs::path& path) = 0;

	void draw();

protected:
	const char* title;
	bool open = false;

private:
	virtual void edit(uint32 renderWidth, uint32 renderHeight) = 0;
	virtual ref<dx_texture> getRendering() = 0;

};


struct mesh_editor_window : asset_editor_window
{
	mesh_editor_window();

	virtual void setAsset(const fs::path& path) override;

private:
	virtual void edit(uint32 renderWidth, uint32 renderHeight) override;
	virtual ref<dx_texture> getRendering() override;
};


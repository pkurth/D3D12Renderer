#pragma once

#include "asset.h"

asset_handle getAssetHandleFromPath(const fs::path& path);
fs::path getPathFromAssetHandle(asset_handle handle);


void initializeFileRegistry();

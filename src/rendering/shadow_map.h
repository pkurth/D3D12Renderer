#pragma once

#include "scene/scene.h"
#include "light_source.h"
#include "render_pass.h"

void renderSunShadowMap(directional_light& sun, sun_shadow_render_pass* renderPass, scene& appScene, bool invalidateCache);
spot_shadow_info renderSpotShadowMap(const spot_light_cb& spotLight, uint32 lightID, spot_shadow_render_pass* renderPass, scene& appScene, bool invalidateCache, uint32 resolution);
point_shadow_info renderPointShadowMap(const point_light_cb& pointLight, uint32 lightID, point_shadow_render_pass* renderPass, scene& appScene, bool invalidateCache, uint32 resolution);

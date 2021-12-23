#include "pch.h"
#include "asset.h"
#include "random.h"

static random_number_generator rng = time(0);

asset_handle asset_handle::generate()
{
	return asset_handle(rng.randomUint64());
}

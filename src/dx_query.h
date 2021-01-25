#pragma once

#include "dx.h"

struct dx_timestamp_query_heap
{
	void initialize(uint32 maxCount);

	dx_query_heap heap;
};


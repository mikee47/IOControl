/**
 * Debug.h
 *
 * Copyright 2022 mikee47 <mike@sillyhouse.net>
 *
 * This file is part of the IOControl Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#pragma once

#include <debug_progmem.h>

#ifdef ENABLE_HEAP_PRINTING
#ifdef ENABLE_MALLOC_COUNT
#include <malloc_count.h>

#define PRINT_HEAP()                                                                                                   \
	debug_i("%s %s #%u : Free Heap = %u, Used = %u, Peak = %u", __FILE__, __FUNCTION__, __LINE__,                      \
			system_get_free_heap_size(), MallocCount::getCurrent(), MallocCount::getPeak())
#else
#define PRINT_HEAP()                                                                                                   \
	debug_i("%s %s #%u : Free Heap = %u", __FILE__, __FUNCTION__, __LINE__, system_get_free_heap_size())
#endif
#else
#define PRINT_HEAP()
#endif

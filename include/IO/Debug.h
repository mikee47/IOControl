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

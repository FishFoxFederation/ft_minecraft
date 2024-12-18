#pragma once

#include <memory>
#include "Tracy.hpp"

extern const char* const str_update_thread;
extern const char* const str_block_update_thread;
extern const char* const str_network_thread;
extern const char* const str_main_thread;
extern const char* const str_thread_pool;
extern const char* const str_render_thread;
constexpr const char* str_chunk_alloc = "ChunkAlloc";

extern const char* const str_worker_thread[20];

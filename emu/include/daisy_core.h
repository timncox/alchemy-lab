/**
 * @file daisy_core.h (host-build stub)
 *
 * Minimal libDaisy core stub for host-native builds.
 * Provides DMA_BUFFER_MEM_SECTION and dsy_dma_invalidate_cache_for_buffer,
 * both no-ops outside of the embedded target.
 */

#pragma once

#include <cstdint>
#include <cstddef>

#define DMA_BUFFER_MEM_SECTION

extern "C" inline void dsy_dma_invalidate_cache_for_buffer(uint8_t* /*buf*/,
                                                            size_t  /*len*/) {}

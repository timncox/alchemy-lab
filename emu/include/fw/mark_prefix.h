/* Mark on the host: its pool is sized to the M7 build (62 MiB, 99.6 % used
 * by design -- the bump allocator's capacity is fixed). With 8-byte pointers
 * the engine's bookkeeping is larger and the first rung no longer fits, so
 * the emulator gives the same engine more room. The firmware's own
 * #include "versio_alloc.h" is then a no-op (include guard). */
#pragma once
#include "versio_alloc.h"
#undef VERSIO_POOL_BYTES
#define VERSIO_POOL_BYTES (72u * 1024u * 1024u)

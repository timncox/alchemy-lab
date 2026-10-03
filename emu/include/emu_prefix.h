/* Force-included into the firmware's own translation unit only.
 * ELF section placement (".belt_pool" in D2 SRAM, ".sram1_bss", ...) has no
 * meaning on the host, and Mach-O rejects ELF-style section names, so a
 * section attribute becomes a harmless `used`. */
#pragma once
#define section(x) used

/* The emulator's audio side (emu_main.cpp). */
#pragma once
#include <cstddef>
#include <cstdint>

namespace emu {
void  Render(float* interleaved_out, size_t frames);   /* runs the firmware callback */
bool  StartSoundCard(bool want_mic);
bool  LoadWav(const char* path);
bool  OpenRecord(const char* path);
void  CloseRecord();
float InLevel();
float OutLevel();
float TakeOutRms();
float ToneLevel(float hz);
void  LrRms(float* l, float* r);   /* last 8192 out samples */
void  OutputHealth(uint32_t* bad, uint32_t* pinned, uint64_t* frames);   /* amplitude at hz in the last 8192 out samples */
void  SetSynthHz(int hz);   /* >0: a sung buzz at hz replaces the input */
} // namespace emu

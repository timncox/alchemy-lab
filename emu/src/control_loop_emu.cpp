/*
 * The SDK's real ControlLoop, with one addition: every Tick() hands its
 * `this` to the emulator, so the panel can name each pot after the firmware's
 * own VirtualKnob on the page that is showing (B2 page cycles, B3 shift
 * layers, any Pager layout) -- for every firmware, with no per-firmware list.
 *
 * Same trick as presets_emu.cpp: the headers are included first so their
 * declarations are untouched; the macro only rewrites the call in
 * control_loop.cpp's Tick(). A comma expression, so it fits wherever the call
 * stands. If an SDK version stops calling it, nothing is noted and the panel
 * falls back to the fw/<fw>.mk labels.
 */
#include <atomic>
#include <cstring>

#include "alchemy/surface/control_loop.h"
#include "alchemy/surface/knob_storage.h"
#include "alchemy/surface/page.h"
#include "alchemy/surface/virtual_knob.h"
#include "alchemy/hw/alchemy_lab.h"
#include "alchemy/surface/button_bank.h"
#include "alchemy/surface/settings.h"
#include "daisy_seed.h"

namespace emu {

namespace {
std::atomic<alchemy::ControlLoop*> g_loop{nullptr};
}

void NoteControlLoop(alchemy::ControlLoop* loop) { g_loop.store(loop, std::memory_order_relaxed); }

/* The name of the knob on `pot` of the page the firmware shows now, or null
 * when there is none to name (no loop yet, no Pager, an unnamed knob).
 * `alt` is set when that page is not the first attached one (drawn purple,
 * like the B3 labels). Read from the panel thread: the pages and names are
 * fixed after boot and ActivePage() is one byte, so the race is benign. */
const char* LiveKnobName(int pot, bool* alt)
{
    alchemy::ControlLoop* loop = g_loop.load(std::memory_order_relaxed);
    if (!loop) return nullptr;
    alchemy::KnobStorage* storage = loop->AttachedStorage();
    if (!storage || loop->NumAttachedPages() == 0) return nullptr;
    const uint8_t active = storage->ActivePage();
    for (uint8_t i = 0; i < loop->NumAttachedPages(); i++)
    {
        const alchemy::Page* page = loop->AttachedPage(i);
        if (!page || page->Index() != active) continue;
        for (uint8_t k = 0; k < page->Count(); k++)
        {
            const alchemy::VirtualKnob* knob = page->At(k);
            if (!knob || knob->Pot() != pot) continue;
            const char* name = knob->Name();
            if (!name || !*name) return nullptr;
            if (alt) *alt = loop->AttachedPage(0) && page->Index() != loop->AttachedPage(0)->Index();
            return name;
        }
        return nullptr;
    }
    return nullptr;
}

}  // namespace emu

#define ProcessAllControls() ProcessAllControls(), ::emu::NoteControlLoop(this)
#include "framework/src/surface/control_loop.cpp"   /* -I$(ALCHEMY_DIR) */

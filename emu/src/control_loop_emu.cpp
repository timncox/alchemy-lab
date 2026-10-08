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
#include <cstdio>
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
/* While Settings is open: the heading ("SETTINGS 3/4 - CV TO"). */
const char* LiveHeading()
{
    static char buf[64];
    alchemy::ControlLoop* loop = g_loop.load(std::memory_order_relaxed);
    alchemy::Settings*    set  = loop ? loop->AttachedSettings() : nullptr;
    if (!set || !set->IsActive()) return nullptr;
    const uint8_t pg   = set->CurrentPage();
    const char*   name = set->PageNameAt(pg);
    if (name && *name)
        std::snprintf(buf, sizeof buf, "SETTINGS %u/%u - %s", pg + 1u, (unsigned)set->NumPages(), name);
    else
        std::snprintf(buf, sizeof buf, "SETTINGS %u/%u", pg + 1u, (unsigned)set->NumPages());
    return buf;
}

const char* LiveKnobName(int pot, bool* alt)
{
    alchemy::ControlLoop* loop = g_loop.load(std::memory_order_relaxed);
    if (!loop) return nullptr;

    /* Settings open: its own names for this page's pots (the display name,
     * else the ident); a pot Settings doesn't use shows nothing. */
    if (alchemy::Settings* set = loop->AttachedSettings(); set && set->IsActive())
    {
        const uint8_t pg = set->CurrentPage();
        const char*   n  = set->DisplayNameAt(pg, (uint8_t)pot);
        if (!n || !*n) n = set->IdentAt(pg, (uint8_t)pot);
        if (alt) *alt = true;
        return (n && *n) ? n : "";
    }

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

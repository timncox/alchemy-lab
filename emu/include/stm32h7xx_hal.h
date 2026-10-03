/**
 * @file stm32h7xx_hal.h (host-build stub)
 *
 * Minimal STM32H7 HAL type stubs covering what ws2812.cpp uses.
 * All function bodies are no-ops; register structs have the fields accessed
 * by the driver so pointer dereferences type-check correctly.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

/* ── Typedefs ─────────────────────────────────────────────────────────── */

typedef uint32_t HAL_StatusTypeDef;
typedef uint32_t HAL_TIM_StateTypeDef;
typedef uint32_t IRQn_Type;

/* ── HAL status / timer state constants ──────────────────────────────── */

static constexpr HAL_TIM_StateTypeDef HAL_TIM_STATE_BUSY  = 2u;
static constexpr HAL_StatusTypeDef    HAL_OK               = 0u;

/* ── TIM register struct (fields accessed by ws2812.cpp) ─────────────── */

struct TIM_TypeDef {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
};

/* ── RCC register struct ─────────────────────────────────────────────── */

struct RCC_TypeDef {
    volatile uint32_t D2CFGR;
};

/* ── DMA register struct ─────────────────────────────────────────────── */

struct DMA_Stream_TypeDef {
    volatile uint32_t CR;
    volatile uint32_t NDTR;
    volatile uint32_t PAR;
    volatile uint32_t M0AR;
};

/* ── Global peripheral instances (host-side stubs, one copy per TU) ─── */

static TIM_TypeDef        s_stub_tim3    = {};
static RCC_TypeDef        s_stub_rcc     = {};
static DMA_Stream_TypeDef s_stub_dma1_s7 = {};

#define TIM3         (&s_stub_tim3)
#define RCC          (&s_stub_rcc)
#define DMA1_Stream7 (&s_stub_dma1_s7)

/* ── GPIO ────────────────────────────────────────────────────────────── */

struct GPIO_TypeDef { volatile uint32_t dummy; };
static GPIO_TypeDef s_stub_gpioc = {};
#define GPIOC (&s_stub_gpioc)

struct GPIO_InitTypeDef {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
    uint32_t Alternate;
};

static constexpr uint32_t GPIO_PIN_9           = (1u << 9);
static constexpr uint32_t GPIO_MODE_AF_PP      = 0x02u;
static constexpr uint32_t GPIO_PULLDOWN        = 0x02u;
static constexpr uint32_t GPIO_SPEED_FREQ_HIGH = 0x02u;
static constexpr uint32_t GPIO_AF2_TIM3        = 0x02u;

inline void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*) {}

/* ── TIM init structs ─────────────────────────────────────────────────── */

struct TIM_Base_InitTypeDef {
    uint32_t Prescaler;
    uint32_t CounterMode;
    uint32_t Period;
    uint32_t ClockDivision;
    uint32_t RepetitionCounter;
    uint32_t AutoReloadPreload;
};

struct TIM_HandleTypeDef {
    TIM_TypeDef*       Instance;
    TIM_Base_InitTypeDef Init;
    HAL_TIM_StateTypeDef State;
};

struct TIM_OC_InitTypeDef {
    uint32_t OCMode;
    uint32_t Pulse;
    uint32_t OCPolarity;
    uint32_t OCFastMode;
    uint32_t OCIdleState;
    uint32_t OCNIdleState;
};

static constexpr uint32_t TIM_COUNTERMODE_UP           = 0u;
static constexpr uint32_t TIM_CLOCKDIVISION_DIV1       = 0u;
static constexpr uint32_t TIM_AUTORELOAD_PRELOAD_DISABLE = 0u;
static constexpr uint32_t TIM_OCMODE_PWM1              = 0x0060u;
static constexpr uint32_t TIM_OCPOLARITY_HIGH          = 0u;
static constexpr uint32_t TIM_OCFAST_DISABLE           = 0u;
static constexpr uint32_t TIM_OCIDLESTATE_RESET        = 0u;
static constexpr uint32_t TIM_OCNIDLESTATE_RESET       = 0u;
static constexpr uint32_t TIM_CHANNEL_4                = 0x000Cu;
static constexpr uint32_t TIM_DMA_UPDATE               = 0x0100u;
static constexpr uint32_t TIM_CCx_ENABLE               = 1u;
static constexpr uint32_t TIM_CCMR2_OC4PE             = (1u << 11);
static constexpr uint32_t RCC_D2CFGR_D2PPRE1          = (7u << 4);
static constexpr uint32_t RCC_D2CFGR_D2PPRE1_Pos      = 4u;

inline HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef*)                   { return HAL_OK; }
inline HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef*,
                                                    TIM_OC_InitTypeDef*,
                                                    uint32_t)                    { return HAL_OK; }
inline void TIM_CCxChannelCmd(TIM_TypeDef*, uint32_t, uint32_t)                 {}

/* ── DMA ─────────────────────────────────────────────────────────────── */

struct DMA_InitTypeDef {
    uint32_t Request;
    uint32_t Direction;
    uint32_t PeriphInc;
    uint32_t MemInc;
    uint32_t PeriphDataAlignment;
    uint32_t MemDataAlignment;
    uint32_t Mode;
    uint32_t Priority;
    uint32_t FIFOMode;
};

struct DMA_HandleTypeDef {
    DMA_Stream_TypeDef* Instance;
    DMA_InitTypeDef     Init;
    void (*XferCpltCallback)(DMA_HandleTypeDef*);
    void (*XferHalfCpltCallback)(DMA_HandleTypeDef*);
    void (*XferErrorCallback)(DMA_HandleTypeDef*);
};

static constexpr uint32_t DMA_REQUEST_TIM3_UP      = 25u;
static constexpr uint32_t DMA_MEMORY_TO_PERIPH     = 1u;
static constexpr uint32_t DMA_PINC_DISABLE         = 0u;
static constexpr uint32_t DMA_MINC_ENABLE          = 0x0400u;
static constexpr uint32_t DMA_PDATAALIGN_HALFWORD  = 0x0800u;
static constexpr uint32_t DMA_MDATAALIGN_HALFWORD  = 0x2000u;
static constexpr uint32_t DMA_NORMAL               = 0u;
static constexpr uint32_t DMA_PRIORITY_LOW         = 0u;
static constexpr uint32_t DMA_FIFOMODE_DISABLE     = 0u;
static constexpr IRQn_Type DMA1_Stream7_IRQn       = static_cast<IRQn_Type>(68);

inline HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef*)                       { return HAL_OK; }
inline HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef*,
                                           uintptr_t, uintptr_t, uint32_t)      { return HAL_OK; }
inline void HAL_DMA_IRQHandler(DMA_HandleTypeDef*)                              {}

/* ── NVIC ────────────────────────────────────────────────────────────── */

inline void HAL_NVIC_SetPriority(IRQn_Type, uint32_t, uint32_t) {}
inline void HAL_NVIC_EnableIRQ(IRQn_Type)                        {}

/* ── RCC ─────────────────────────────────────────────────────────────── */

inline uint32_t HAL_RCC_GetPCLK1Freq() { return 200000000u; }

/* ── Clock-enable macros ─────────────────────────────────────────────── */

#define __HAL_RCC_DMA1_CLK_ENABLE()   do {} while(0)
#define __HAL_RCC_TIM3_CLK_ENABLE()   do {} while(0)
#define __HAL_RCC_GPIOC_CLK_ENABLE()  do {} while(0)

/* ── DMA/TIM control macros ──────────────────────────────────────────── */

#define __HAL_DMA_DISABLE(__HANDLE__)              \
    do { (__HANDLE__)->Instance->CR &= ~1u; } while(0)

#define __HAL_TIM_ENABLE(__HANDLE__)               \
    do { (__HANDLE__)->Instance->CR1 |= 1u; } while(0)

#define __HAL_TIM_ENABLE_DMA(__HANDLE__, __DMA__)  \
    do { (__HANDLE__)->Instance->DIER |= (__DMA__); } while(0)

#define __HAL_TIM_DISABLE_DMA(__HANDLE__, __DMA__) \
    do { (__HANDLE__)->Instance->DIER &= ~(__DMA__); } while(0)

/* ---- emulator additions: reset + the chip's unique ID ---- */
namespace emu { void RebootRequested(const char* why); }
inline void     HAL_NVIC_SystemReset(void) { emu::RebootRequested("system reset"); }
inline uint32_t HAL_GetUIDw0(void) { return 0x454D5530u; }   /* "EMU0" */
inline uint32_t HAL_GetUIDw1(void) { return 0x42454C54u; }   /* "BELT" */
inline uint32_t HAL_GetUIDw2(void) { return 0x00000001u; }
/* The unique-ID words as memory (some firmwares read *(uint32_t*)UID_BASE). */
inline const uint32_t emu_uid_words[3] = {0x454D5530u, 0x42454C54u, 0x00000001u};
#define UID_BASE ((uintptr_t)emu_uid_words)

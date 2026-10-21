#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"

/* В офіційному API FreeRTOS функція має префікс pv (pointer to void).
   Макрос нижче дозволяє використовувати назву з методички без помилки LNK2019 */
#define xTaskGetThreadLocalStoragePointer pvTaskGetThreadLocalStoragePointer

   /* ============================================================================
    * ЛАБОРАТОРНА РОБОТА №2: FreeRTOS TLS
    * Студент: Сеник Ярослав
    * Варіант: 18 (Ntasks=3, Mode=delay, Burst=8, V1=0, V2=1, Base=90ms)
    * ============================================================================ */

#define VARIANT_IDX     18
#define NTASKS          3
#define BURST_B         8
#define TLS_V1_IDX      0   /* Слот для task_context_t */
#define TLS_V2_IDX      1   /* Слот для profile_t */
#define BASE_DELAY_MS   90
#define STEP_DELAY_MS   7
#define TOTAL_ITERS     (10 * NTASKS + VARIANT_IDX) /* 48 ітерацій */

typedef enum {
    MODE_DELAY = 0,
    MODE_BUSY = 1
} run_mode_t;

typedef struct {
    run_mode_t mode;
    uint8_t    burstMode;
    uint32_t   baseDelayMs;
    uint32_t   baseCycles;
    uint8_t    step;
} profile_t;

typedef struct {
    char     name[16];
    uint32_t iter;
    uint32_t checksum;
    uint32_t delayTicksOrCycles;
    uint32_t seed;
    char     lines[BURST_B][96];
    uint8_t  bcnt;
} task_context_t;

/* CRC-8 SAE J1850 */
static uint8_t crc8_sae_j1850(const uint8_t* data, size_t length)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x1D;
            else
                crc <<= 1;
        }
    }
    return crc ^ 0xFF;
}

/* Пакетний вивід буфера */
static void burst_flush(task_context_t* ctx)
{
    if (ctx != NULL && ctx->bcnt > 0)
    {
        for (uint8_t j = 0; j < ctx->bcnt; j++)
        {
            printf("%s", ctx->lines[j]);
        }
        fflush(stdout);
        ctx->bcnt = 0;
    }
}

static void vWorker(void* pvParameters)
{
    int i = (int)(uintptr_t)pvParameters;

    task_context_t* ctx = (task_context_t*)pvPortMalloc(sizeof(task_context_t));
    profile_t* prof = (profile_t*)pvPortMalloc(sizeof(profile_t));

    configASSERT(ctx != NULL);
    configASSERT(prof != NULL);

    snprintf(ctx->name, sizeof(ctx->name), "T%02d_%d", VARIANT_IDX, i);
    ctx->iter = 0;
    ctx->checksum = 0;
    ctx->seed = 0xA5A50000U | (uint32_t)i;
    ctx->bcnt = 0;
    ctx->delayTicksOrCycles = BASE_DELAY_MS + (STEP_DELAY_MS * i);

    prof->mode = MODE_DELAY;
    prof->burstMode = 1;
    prof->baseDelayMs = BASE_DELAY_MS;
    prof->baseCycles = 0;
    prof->step = STEP_DELAY_MS;

    /* Записуємо структури в слоти TLS V1=0 та V2=1 */
    vTaskSetThreadLocalStoragePointer(NULL, TLS_V1_IDX, (void*)ctx);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_V2_IDX, (void*)prof);

    for (uint32_t it = 0; it < TOTAL_ITERS; it++)
    {
        task_context_t* c = (task_context_t*)xTaskGetThreadLocalStoragePointer(NULL, TLS_V1_IDX);
        profile_t* p = (profile_t*)xTaskGetThreadLocalStoragePointer(NULL, TLS_V2_IDX);

        c->iter++;

        uint32_t data_to_hash[2] = { c->iter, c->seed };
        c->checksum = (uint32_t)crc8_sae_j1850((const uint8_t*)data_to_hash, sizeof(data_to_hash));

        TickType_t curTick = xTaskGetTickCount();

        snprintf(c->lines[c->bcnt], sizeof(c->lines[c->bcnt]),
            "[Task %s] Tick: %5lu Iter: %2lu Sum: 0x%08X\n",
            c->name,
            (unsigned long)curTick,
            (unsigned long)c->iter,
            (unsigned int)c->checksum);
        c->bcnt++;

        if (c->bcnt >= BURST_B)
        {
            burst_flush(c);
        }

        if (p->mode == MODE_DELAY)
        {
            vTaskDelay(pdMS_TO_TICKS(c->delayTicksOrCycles));
        }
    }

    task_context_t* final_ctx = (task_context_t*)xTaskGetThreadLocalStoragePointer(NULL, TLS_V1_IDX);
    profile_t* final_prof = (profile_t*)xTaskGetThreadLocalStoragePointer(NULL, TLS_V2_IDX);

    burst_flush(final_ctx);

    printf("Task finished %s\n", final_ctx->name);
    fflush(stdout);

    vPortFree(final_ctx);
    vPortFree(final_prof);

    vTaskSetThreadLocalStoragePointer(NULL, TLS_V1_IDX, NULL);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_V2_IDX, NULL);

    vTaskDelete(NULL);
}

void main_blinky(void)
{
    printf("=== FreeRTOS Lab 2: Senyk Yaroslav (Variant 18) - TLS Started ===\n");
    fflush(stdout);

    for (int i = 0; i < NTASKS; ++i)
    {
        xTaskCreate(vWorker, "Task", 1024, (void*)(uintptr_t)i, 1, NULL);
    }

    vTaskStartScheduler();

    for (;;);
}

void vBlinkyKeyboardInterruptHandler(int xKeyPress)
{
    (void)xKeyPress;
}
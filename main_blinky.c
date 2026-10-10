#include <stdio.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"

/*
 * Параметри варіанта 18 (Сеник Ярослав):
 * Префікс: SEYA_
 * T1 = 17 ms, C1 = 2 ms, Пріоритет: 3 (Найвищий)
 * T2 = 24 ms, C2 = 3 ms, Пріоритет: 2 (Середній)
 * T3 = 34 ms, C3 = 4 ms, Пріоритет: 1 (Низький)
 */

#define T1_PERIOD_MS    17
#define T2_PERIOD_MS    24
#define T3_PERIOD_MS    34

#define C1_COMPUTE_MS   2
#define C2_COMPUTE_MS   3
#define C3_COMPUTE_MS   4

 /* Функція імітації навантаження (busy-loop) тривалістю Ci */
static void simulate_busy_work(uint32_t work_ms)
{
    TickType_t xStartTick = xTaskGetTickCount();
    TickType_t xTicksToWait = pdMS_TO_TICKS(work_ms);

    if (xTicksToWait == 0)
    {
        xTicksToWait = 1;
    }

    /* Активне навантаження процесора */
    while ((xTaskGetTickCount() - xStartTick) < xTicksToWait)
    {
        /* Busy wait */
    }
}

/* Періодична задача Tau 1 */
static void prvSEYA_Task1(void* pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(T1_PERIOD_MS);

    for (;; )
    {
        TickType_t xStartTime = xTaskGetTickCount();
        simulate_busy_work(C1_COMPUTE_MS);
        TickType_t xFinishTime = xTaskGetTickCount();

        printf("[SEYA_T1] Start tick: %5lu | Finish tick: %5lu | Duration: %lums\n",
            (unsigned long)xStartTime,
            (unsigned long)xFinishTime,
            (unsigned long)C1_COMPUTE_MS);
        fflush(stdout);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* Періодична задача Tau 2 */
static void prvSEYA_Task2(void* pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(T2_PERIOD_MS);

    for (;; )
    {
        TickType_t xStartTime = xTaskGetTickCount();
        simulate_busy_work(C2_COMPUTE_MS);
        TickType_t xFinishTime = xTaskGetTickCount();

        printf("[SEYA_T2] Start tick: %5lu | Finish tick: %5lu | Duration: %lums\n",
            (unsigned long)xStartTime,
            (unsigned long)xFinishTime,
            (unsigned long)C2_COMPUTE_MS);
        fflush(stdout);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* Періодична задача Tau 3 */
static void prvSEYA_Task3(void* pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(T3_PERIOD_MS);

    for (;; )
    {
        TickType_t xStartTime = xTaskGetTickCount();
        simulate_busy_work(C3_COMPUTE_MS);
        TickType_t xFinishTime = xTaskGetTickCount();

        printf("[SEYA_T3] Start tick: %5lu | Finish tick: %5lu | Duration: %lums\n",
            (unsigned long)xStartTime,
            (unsigned long)xFinishTime,
            (unsigned long)C3_COMPUTE_MS);
        fflush(stdout);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* Точка входу в режим blinky демо */
void main_blinky(void)
{
    printf("=== FreeRTOS Lab 1: Senyk Yaroslav (Variant 18) Started ===\n");
    fflush(stdout);

    /* Створення трьох періодичних задач згідно з пріоритетами: T1 > T2 > T3 */
    xTaskCreate(prvSEYA_Task1, "SEYA_T1", configMINIMAL_STACK_SIZE + 128, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(prvSEYA_Task2, "SEYA_T2", configMINIMAL_STACK_SIZE + 128, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(prvSEYA_Task3, "SEYA_T3", configMINIMAL_STACK_SIZE + 128, NULL, tskIDLE_PRIORITY + 1, NULL);

    /* Запуск диспетчера FreeRTOS */
    vTaskStartScheduler();

    /* Сюди код ніколи не дійде, якщо планувальник запустився успішно */
    for (;; );
}

/* Обробник клавіатури для симулятора Windows */
void vBlinkyKeyboardInterruptHandler(int xKeyPress)
{
    (void)xKeyPress;
}
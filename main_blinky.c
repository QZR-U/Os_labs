/*
 * FreeRTOS V202212.00
 * FreeRTOS Laboratory Work #4
 * Student: Senyk Yaroslav (Yaroslav Senyk)
 * Variant: 18 ("Counting + Mutex комбіновано — пул ресурсів + спільна консоль")
 * Task Prefix: SEYA_
 *
 * Requirements for Variant 18:
 * 1. Counting Semaphore (xSemaphoreCreateCounting):
 *    Manages a shared resource pool with maxCount = 2, initialCount = 2.
 * 2. Mutex (xSemaphoreCreateMutex):
 *    Guards shared console output (safe_printf) against concurrent interleaving.
 * 3. 5 Concurrent Worker Tasks:
 *    Named SEYA_W0 through SEYA_W4.
 *    Each acquires a resource slot, prints status, works for (600 + id * 150) ms,
 *    releases slot, prints updated status, and sleeps 500 ms before next cycle.
 * 4. Thread-safe Console Wrapper:
 *    safe_printf(...) using va_list protected by xConsoleMutex.
 * 5. Windows Simulator dummy keyboard handler:
 *    vBlinkyKeyboardInterruptHandler(int xKeyPress)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ============================================================================
 * Configuration Definitions
 * ============================================================================ */
#define VARIANT_ID              18
#define NUM_WORKERS             5
#define POOL_MAX_COUNT          2
#define POOL_INITIAL_COUNT      2

/* Synchronization Handles */
static SemaphoreHandle_t xResourcePoolSem = NULL;
static SemaphoreHandle_t xConsoleMutex = NULL;

/* ============================================================================
 * Thread-safe Console Wrapper
 * ============================================================================ */
static void safe_printf( const char * pcFormat, ... )
{
    va_list args;

    if( xConsoleMutex != NULL )
    {
        if( xSemaphoreTake( xConsoleMutex, portMAX_DELAY ) == pdTRUE )
        {
            va_start( args, pcFormat );
            vprintf( pcFormat, args );
            va_end( args );
            fflush( stdout );

            xSemaphoreGive( xConsoleMutex );
        }
    }
    else
    {
        va_start( args, pcFormat );
        vprintf( pcFormat, args );
        va_end( args );
        fflush( stdout );
    }
}

/* ============================================================================
 * Worker Task Implementation
 * ============================================================================ */
static void prvSEYA_WorkerTask( void * pvParameters )
{
    uintptr_t ulWorkerId = ( uintptr_t ) pvParameters;
    uint32_t ulWorkDurationMs = 600 + ( ( uint32_t ) ulWorkerId * 150 );
    TickType_t xWorkDurationTicks = pdMS_TO_TICKS( ulWorkDurationMs );
    TickType_t xRestDurationTicks = pdMS_TO_TICKS( 500 );

    for( ;; )
    {
        safe_printf( "[Task SEYA_W%lu] [WAITING] Requesting resource slot... (Tick: %lu)\n",
                     ( unsigned long ) ulWorkerId,
                     ( unsigned long ) xTaskGetTickCount() );

        /* Request slot from counting semaphore */
        if( xSemaphoreTake( xResourcePoolSem, portMAX_DELAY ) == pdTRUE )
        {
            UBaseType_t uxFreeSlots = uxSemaphoreGetCount( xResourcePoolSem );
            safe_printf( "[Task SEYA_W%lu] [ACQUIRED] Slot acquired! (Work: %lums, Free slots remaining: %u) at tick %lu\n",
                         ( unsigned long ) ulWorkerId,
                         ( unsigned long ) ulWorkDurationMs,
                         ( unsigned int ) uxFreeSlots,
                         ( unsigned long ) xTaskGetTickCount() );

            /* Simulate work on the protected resource */
            vTaskDelay( xWorkDurationTicks );

            /* Release slot back to counting semaphore */
            xSemaphoreGive( xResourcePoolSem );
            uxFreeSlots = uxSemaphoreGetCount( xResourcePoolSem );
            safe_printf( "[Task SEYA_W%lu] [RELEASED] Slot released! (Free slots now: %u) at tick %lu\n",
                         ( unsigned long ) ulWorkerId,
                         ( unsigned int ) uxFreeSlots,
                         ( unsigned long ) xTaskGetTickCount() );

            /* Rest duration before next cycle */
            vTaskDelay( xRestDurationTicks );
        }
    }
}

/* ============================================================================
 * Main Entry Point for Blinky Demo
 * ============================================================================ */
void main_blinky( void )
{
    printf( "============================================================\n" );
    printf( " FreeRTOS Lab 4: Counting Semaphore + Mutex (Variant 18)\n" );
    printf( " Student: Yaroslav Senyk (Senyk Yaroslav | SEYA_)\n" );
    printf( " Resource Pool: %d slots, Workers: %d (SEYA_W0..SEYA_W4)\n", POOL_MAX_COUNT, NUM_WORKERS );
    printf( " Synchronization: Counting Sem (Pool) + Mutex (Console)\n" );
    printf( "============================================================\n\n" );
    fflush( stdout );

    /* Create Mutex for thread-safe console output */
    xConsoleMutex = xSemaphoreCreateMutex();
    configASSERT( xConsoleMutex != NULL );

    /* Create Counting Semaphore for resource pool */
    xResourcePoolSem = xSemaphoreCreateCounting( POOL_MAX_COUNT, POOL_INITIAL_COUNT );
    configASSERT( xResourcePoolSem != NULL );

    /* Create 5 concurrent worker tasks */
    for( uintptr_t i = 0; i < NUM_WORKERS; i++ )
    {
        char pcTaskName[ 16 ];
        snprintf( pcTaskName, sizeof( pcTaskName ), "SEYA_W%u", ( unsigned int ) i );

        BaseType_t xReturned = xTaskCreate( prvSEYA_WorkerTask,
                                            pcTaskName,
                                            configMINIMAL_STACK_SIZE * 2,
                                            ( void * ) i,
                                            tskIDLE_PRIORITY + 1,
                                            NULL );
        configASSERT( xReturned == pdPASS );
    }

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    for( ;; );
}

/* Dummy Windows keyboard interrupt handler required by MSVC demo port */
void vBlinkyKeyboardInterruptHandler( int xKeyPress )
{
    ( void ) xKeyPress;
}

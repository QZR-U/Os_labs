/*
 * FreeRTOS V202212.00
 * FreeRTOS Laboratory Work #3
 * Student: Senyk Yaroslav (Yaroslav Senyk)
 * Variant: 18 -> Mapped to Variant 8 (18 mod 10 = 8)
 * Task Prefix: SEYA_
 *
 * Requirements for Variant 8:
 * - 2 tasks: SEYA_Producer and SEYA_Consumer
 * - Queue Item Data Type: char[5] (fixed 5-byte character buffer, e.g. "M001")
 * - Queue Length: 5 elements
 * - Send timeout: 100 ms (pdMS_TO_TICKS(100))
 * - Receive timeout: 200 ms (pdMS_TO_TICKS(200))
 * - Variable Producer delay: cyclically choosing {80ms, 120ms, 180ms, 260ms, 320ms}
 *   to demonstrate synchronized delivery and consumer timeouts (>200ms).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ============================================================================
 * Configuration Definitions
 * ============================================================================ */
#define VARIANT_BASE            18
#define VARIANT_MAPPED          8
#define QUEUE_LENGTH            5
#define ITEM_SIZE               5   /* char[5], e.g. "M001\0" */

#define SEND_TIMEOUT_MS         100
#define RECEIVE_TIMEOUT_MS      200

typedef char QueueItem_t[ITEM_SIZE];

/* Queue Handle */
static QueueHandle_t xSEYA_Queue = NULL;

/* Variable delay cycle for Producer (ms) */
static const uint32_t ulProducerDelays[] = { 80, 120, 180, 260, 320 };
#define NUM_DELAYS ( sizeof( ulProducerDelays ) / sizeof( ulProducerDelays[0] ) )

/* ============================================================================
 * Task Implementations
 * ============================================================================ */

/* Producer Task: generates messages M001, M002, ... and sends them to the queue */
static void prvSEYA_ProducerTask( void * pvParameters )
{
    ( void ) pvParameters;
    uint32_t ulMsgIndex = 1;
    size_t xDelayIdx = 0;
    QueueItem_t cMessageBuffer;

    for( ;; )
    {
        /* Format fixed 5-byte message: "M%03u" with null-terminator fits in char[5] */
        snprintf( cMessageBuffer, sizeof( cMessageBuffer ), "M%03u", ( unsigned int )( ulMsgIndex % 1000 ) );

        uint32_t ulDelayMs = ulProducerDelays[ xDelayIdx ];
        xDelayIdx = ( xDelayIdx + 1 ) % NUM_DELAYS;

        TickType_t xSendTimeout = pdMS_TO_TICKS( SEND_TIMEOUT_MS );
        BaseType_t xStatus = xQueueSend( xSEYA_Queue, cMessageBuffer, xSendTimeout );

        TickType_t xCurrentTick = xTaskGetTickCount();

        if( xStatus == pdPASS )
        {
            printf( "[Producer | SEYA] Sent: '%s' at tick %lu (next delay: %lums)\n",
                    cMessageBuffer,
                    ( unsigned long ) xCurrentTick,
                    ( unsigned long ) ulDelayMs );
            fflush( stdout );
            ulMsgIndex++;
        }
        else
        {
            printf( "[Producer | SEYA] Queue FULL! Timeout at tick %lu\n",
                    ( unsigned long ) xCurrentTick );
            fflush( stdout );
        }

        vTaskDelay( pdMS_TO_TICKS( ulDelayMs ) );
    }
}

/* Consumer Task: receives messages from the queue with 200ms timeout */
static void prvSEYA_ConsumerTask( void * pvParameters )
{
    ( void ) pvParameters;
    QueueItem_t cReceivedBuffer;

    for( ;; )
    {
        TickType_t xStartTick = xTaskGetTickCount();
        TickType_t xRecvTimeout = pdMS_TO_TICKS( RECEIVE_TIMEOUT_MS );

        BaseType_t xStatus = xQueueReceive( xSEYA_Queue, cReceivedBuffer, xRecvTimeout );
        TickType_t xEndTick = xTaskGetTickCount();
        TickType_t xWaitTicks = xEndTick - xStartTick;

        if( xStatus == pdPASS )
        {
            cReceivedBuffer[ ITEM_SIZE - 1 ] = '\0';
            printf( "[Consumer | SEYA] Got: '%s' at tick %lu (waiting: %lu)\n",
                    cReceivedBuffer,
                    ( unsigned long ) xEndTick,
                    ( unsigned long ) xWaitTicks );
            fflush( stdout );
        }
        else
        {
            printf( "[Consumer | SEYA] Timeout waiting for data at tick %lu\n",
                    ( unsigned long ) xEndTick );
            fflush( stdout );
        }
    }
}

/* ============================================================================
 * Main Entry Point for Blinky Demo
 * ============================================================================ */
void main_blinky( void )
{
    printf( "============================================================\n" );
    printf( " FreeRTOS Lab 3: Queues & Timeouts (Variant 18 -> 8)\n" );
    printf( " Student: Yaroslav Senyk (Senyk Yaroslav | SEYA_)\n" );
    printf( " Queue length: %d, Item size: %d bytes (char[5])\n", QUEUE_LENGTH, ITEM_SIZE );
    printf( " Send timeout: %d ms, Receive timeout: %d ms\n", SEND_TIMEOUT_MS, RECEIVE_TIMEOUT_MS );
    printf( "============================================================\n\n" );
    fflush( stdout );

    xSEYA_Queue = xQueueCreate( QUEUE_LENGTH, sizeof( QueueItem_t ) );

    if( xSEYA_Queue != NULL )
    {
        xTaskCreate( prvSEYA_ProducerTask,
                     "SEYA_Producer",
                     configMINIMAL_STACK_SIZE * 2,
                     NULL,
                     tskIDLE_PRIORITY + 1,
                     NULL );

        xTaskCreate( prvSEYA_ConsumerTask,
                     "SEYA_Consumer",
                     configMINIMAL_STACK_SIZE * 2,
                     NULL,
                     tskIDLE_PRIORITY + 1,
                     NULL );

        vTaskStartScheduler();
    }
    else
    {
        printf( "Error: Failed to create queue!\n" );
        fflush( stdout );
    }

    for( ;; );
}

/* Dummy Windows keyboard interrupt handler required by MSVC demo port */
void vBlinkyKeyboardInterruptHandler( int xKeyPress )
{
    ( void ) xKeyPress;
}

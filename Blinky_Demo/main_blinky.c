/*
 * FreeRTOS V202212.00
 * FreeRTOS Laboratory Work #6
 * Student: Senyk Yaroslav (Yaroslav Senyk)
 * Variant: 10 ("Простий чат між двома вузлами через gateway")
 * Task Prefix: SEYA_
 *
 * Requirements for Variant 10:
 * 1. Stream Buffers, Message Buffers, and Event Groups in FreeRTOS.
 * 2. Scenario 10: Two-node communication chat through a central gateway.
 * 3. Tasks:
 *    - NodeATask (SEYA_NodeA): Sends raw byte characters / strings ("Hello from Node A\n")
 *      into a shared Stream Buffer.
 *    - NodeBTask (SEYA_NodeB): Sends raw byte characters / strings ("Ack from Node B\n")
 *      into the shared Stream Buffer.
 *    - GatewayTask (SEYA_Gateway): Reads byte streams from the Stream Buffer, delimits
 *      whole messages by newline '\n', formats full structured messages, and sends them
 *      into a Message Buffer addressed to MonitorTask and the respective nodes.
 *    - MonitorTask (SEYA_Monitor): Monitors overall traffic and status via Event Group bits.
 * 4. Primitives:
 *    - Stream Buffer (xStreamBufferCreate): Shared byte stream receiving raw character input from Node A and Node B.
 *    - Message Buffer (xMessageBufferCreate): Passes structured packet records between Gateway and recipients.
 *    - Event Group (xEventGroupCreate):
 *        * BIT_A_ONLINE     (1 << 0): Signals Node A is active.
 *        * BIT_B_ONLINE     (1 << 1): Signals Node B is active.
 *        * BIT_MSG_PENDING  (1 << 2): Signals a new processed message is ready.
 *        * BIT_ERROR_FORMAT (1 << 3): Set if a malformed/empty packet is caught.
 *        * MonitorTask uses xEventGroupWaitBits() to verify both nodes are concurrently online.
 * 5. Windows Simulator dummy keyboard handler:
 *    vBlinkyKeyboardInterruptHandler(int xKeyPress)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "message_buffer.h"
#include "event_groups.h"

/* ============================================================================
 * Configuration & Constants
 * ============================================================================ */
#define SEYA_VARIANT_ID                 10

/* Event Group Bits */
#define SEYA_BIT_A_ONLINE               ( 1 << 0 )
#define SEYA_BIT_B_ONLINE               ( 1 << 1 )
#define SEYA_BIT_MSG_PENDING            ( 1 << 2 )
#define SEYA_BIT_ERROR_FORMAT           ( 1 << 3 )

/* Buffer Sizes */
#define SEYA_STREAM_BUFFER_SIZE         512
#define SEYA_STREAM_TRIGGER_LEVEL       1
#define SEYA_MESSAGE_BUFFER_SIZE        1024
#define SEYA_MAX_PAYLOAD_LEN            128

/* Structured Message Packet definition */
typedef struct
{
    uint32_t ulPacketId;
    char cSender;            /* 'A', 'B', etc. */
    char cRecipient;         /* 'B', 'A', 'M' (Monitor) */
    uint32_t ulPayloadLength;
    char cPayload[ SEYA_MAX_PAYLOAD_LEN ];
} SEYA_ChatPacket_t;

/* Synchronization & IPC Handles */
static StreamBufferHandle_t  xSEYA_StreamBuffer  = NULL;
static MessageBufferHandle_t xSEYA_MessageBuffer = NULL;
static EventGroupHandle_t    xSEYA_EventGroup    = NULL;
static SemaphoreHandle_t     xSEYA_StreamMutex   = NULL;
static SemaphoreHandle_t     xConsoleMutex       = NULL;

/* Task Handles */
static TaskHandle_t xSEYA_NodeAHandle   = NULL;
static TaskHandle_t xSEYA_NodeBHandle   = NULL;
static TaskHandle_t xSEYA_GatewayHandle = NULL;
static TaskHandle_t xSEYA_MonitorHandle = NULL;

/* ============================================================================
 * Thread-Safe Console Output
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
 * Helper to safely write to shared Stream Buffer
 * ============================================================================ */
static void vSendToStreamBuffer( const char * pcData )
{
    size_t xLength = strlen( pcData );
    if( xLength == 0 ) return;

    if( xSEYA_StreamMutex != NULL )
    {
        if( xSemaphoreTake( xSEYA_StreamMutex, portMAX_DELAY ) == pdTRUE )
        {
            xStreamBufferSend( xSEYA_StreamBuffer, ( const void * ) pcData, xLength, portMAX_DELAY );
            xSemaphoreGive( xSEYA_StreamMutex );
        }
    }
    else
    {
        xStreamBufferSend( xSEYA_StreamBuffer, ( const void * ) pcData, xLength, portMAX_DELAY );
    }
}

/* ============================================================================
 * Task Implementations
 * ============================================================================ */

/* 1. NodeATask (SEYA_NodeA)
 * Sends raw byte characters / strings ("Hello from Node A\n") into a shared Stream Buffer.
 */
static void prvNodeATask( void * pvParameters )
{
    ( void ) pvParameters;
    uint32_t ulSeq = 0;
    char cMsgBuf[ 128 ];

    /* Signal Node A is active in the Event Group */
    xEventGroupSetBits( xSEYA_EventGroup, SEYA_BIT_A_ONLINE );
    safe_printf( "[SEYA_CHAT] Node A  : Online signal dispatched (BIT_A_ONLINE set).\r\n" );

    /* Small startup pause to ensure Monitor verifies both nodes concurrently */
    vTaskDelay( pdMS_TO_TICKS( 400 ) );

    for( ;; )
    {
        ulSeq++;

        /* Format raw byte string with newline delimiter */
        snprintf( cMsgBuf, sizeof( cMsgBuf ), "Hello from Node A [seq=%lu]\n", ulSeq );

        safe_printf( "[SEYA_CHAT] Node A  : Streaming raw bytes -> \"%s", cMsgBuf ); /* cMsgBuf already has \n */
        vSendToStreamBuffer( cMsgBuf );

        /* Keep online status active */
        xEventGroupSetBits( xSEYA_EventGroup, SEYA_BIT_A_ONLINE );

        /* Delay between transmissions */
        vTaskDelay( pdMS_TO_TICKS( 1200 ) );
    }
}

/* 2. NodeBTask (SEYA_NodeB)
 * Sends raw byte characters / strings ("Ack from Node B\n") into the shared Stream Buffer.
 */
static void prvNodeBTask( void * pvParameters )
{
    ( void ) pvParameters;
    uint32_t ulSeq = 0;
    char cMsgBuf[ 128 ];

    /* Signal Node B is active in the Event Group */
    xEventGroupSetBits( xSEYA_EventGroup, SEYA_BIT_B_ONLINE );
    safe_printf( "[SEYA_CHAT] Node B  : Online signal dispatched (BIT_B_ONLINE set).\r\n" );

    /* Initial offset delay so Node A and Node B alternate cleanly */
    vTaskDelay( pdMS_TO_TICKS( 900 ) );

    for( ;; )
    {
        ulSeq++;

        /* Occasionally send an empty/malformed line to demonstrate error bit handling */
        if( ulSeq == 3 )
        {
            safe_printf( "[SEYA_CHAT] Node B  : Injecting test malformed/empty packet to test BIT_ERROR_FORMAT...\r\n" );
            vSendToStreamBuffer( "\n" );
            vTaskDelay( pdMS_TO_TICKS( 600 ) );
        }

        /* Format standard raw byte string with newline delimiter */
        snprintf( cMsgBuf, sizeof( cMsgBuf ), "Ack from Node B [seq=%lu]\n", ulSeq );

        safe_printf( "[SEYA_CHAT] Node B  : Streaming raw bytes -> \"%s", cMsgBuf );
        vSendToStreamBuffer( cMsgBuf );

        /* Keep online status active */
        xEventGroupSetBits( xSEYA_EventGroup, SEYA_BIT_B_ONLINE );

        /* Delay between transmissions */
        vTaskDelay( pdMS_TO_TICKS( 1400 ) );
    }
}

/* 3. GatewayTask (SEYA_Gateway)
 * Reads byte streams from the Stream Buffer, delimits whole messages by newline '\n',
 * formats full structured messages, and sends them into a Message Buffer
 * addressed to MonitorTask and the respective nodes.
 */
static void prvGatewayTask( void * pvParameters )
{
    ( void ) pvParameters;
    static char cLineAccumulator[ SEYA_MAX_PAYLOAD_LEN ];
    size_t xAccumLength = 0;
    uint32_t ulPacketCounter = 0;
    char cChar;

    safe_printf( "[SEYA_CHAT] Gateway : GatewayTask active. Listening to raw stream buffer...\r\n" );

    for( ;; )
    {
        /* Read raw byte character by character from the shared stream buffer */
        size_t xReceived = xStreamBufferReceive( xSEYA_StreamBuffer, &cChar, 1, portMAX_DELAY );

        if( xReceived == 1 )
        {
            if( cChar == '\n' )
            {
                /* Newline delimiter reached! */
                cLineAccumulator[ xAccumLength ] = '\0';

                /* Strip optional carriage return if present */
                if( xAccumLength > 0 && cLineAccumulator[ xAccumLength - 1 ] == '\r' )
                {
                    cLineAccumulator[ xAccumLength - 1 ] = '\0';
                    xAccumLength--;
                }

                /* Check for empty/malformed message */
                if( xAccumLength == 0 )
                {
                    safe_printf( "[SEYA_CHAT] Gateway : [!] Empty/malformed line detected! Setting BIT_ERROR_FORMAT.\r\n" );
                    xEventGroupSetBits( xSEYA_EventGroup, SEYA_BIT_ERROR_FORMAT );
                }
                else
                {
                    ulPacketCounter++;
                    SEYA_ChatPacket_t xPacket;
                    memset( &xPacket, 0, sizeof( xPacket ) );
                    xPacket.ulPacketId = ulPacketCounter;

                    /* Inspect sender and assign recipient */
                    if( strstr( cLineAccumulator, "Node A" ) != NULL )
                    {
                        xPacket.cSender = 'A';
                        xPacket.cRecipient = 'B';
                    }
                    else if( strstr( cLineAccumulator, "Node B" ) != NULL )
                    {
                        xPacket.cSender = 'B';
                        xPacket.cRecipient = 'A';
                    }
                    else
                    {
                        xPacket.cSender = '?';
                        xPacket.cRecipient = 'M';
                    }

                    strncpy( xPacket.cPayload, cLineAccumulator, sizeof( xPacket.cPayload ) - 1 );
                    xPacket.ulPayloadLength = ( uint32_t ) strlen( xPacket.cPayload );

                    safe_printf( "[SEYA_CHAT] Gateway : Formatted Packet #%lu (Sender=%c -> Recipient=%c | \"%s\"). Forwarding to Message Buffer.\r\n",
                                 xPacket.ulPacketId, xPacket.cSender, xPacket.cRecipient, xPacket.cPayload );

                    /* Send structured packet into Message Buffer */
                    size_t xSent = xMessageBufferSend(
                        xSEYA_MessageBuffer,
                        ( const void * ) &xPacket,
                        sizeof( xPacket ),
                        portMAX_DELAY
                    );

                    if( xSent == sizeof( xPacket ) )
                    {
                        /* Signal Event Group that a new structured message is ready */
                        xEventGroupSetBits( xSEYA_EventGroup, SEYA_BIT_MSG_PENDING );
                    }
                    else
                    {
                        safe_printf( "[SEYA_CHAT] Gateway : Failed to enqueue into Message Buffer!\r\n" );
                    }
                }

                /* Reset accumulator */
                xAccumLength = 0;
            }
            else
            {
                /* Accumulate character */
                if( xAccumLength < ( sizeof( cLineAccumulator ) - 1 ) )
                {
                    cLineAccumulator[ xAccumLength++ ] = cChar;
                }
                else
                {
                    /* Buffer overrun without newline: truncate and flush */
                    cLineAccumulator[ xAccumLength ] = '\0';
                    xAccumLength = 0;
                    xEventGroupSetBits( xSEYA_EventGroup, SEYA_BIT_ERROR_FORMAT );
                }
            }
        }
    }
}

/* 4. MonitorTask (SEYA_Monitor)
 * Monitors overall traffic and status via Event Group bits.
 * First uses xEventGroupWaitBits() to verify both nodes are concurrently online.
 */
static void prvMonitorTask( void * pvParameters )
{
    ( void ) pvParameters;
    uint32_t ulTotalInspected = 0;
    uint32_t ulErrorsCounted = 0;

    safe_printf( "[SEYA_CHAT] Monitor : Waiting for Node A and Node B to register online via Event Group...\r\n" );

    /* Verify both nodes are concurrently online: wait for BIT_A_ONLINE and BIT_B_ONLINE */
    EventBits_t xOnlineBits = xEventGroupWaitBits(
        xSEYA_EventGroup,
        ( SEYA_BIT_A_ONLINE | SEYA_BIT_B_ONLINE ),
        pdFALSE,         /* Do not clear bits on exit */
        pdTRUE,          /* Wait for ALL bits (both nodes) */
        portMAX_DELAY
    );

    safe_printf( "[SEYA_CHAT] Monitor : >>> BOTH NODES ONLINE! (EventBits=0x%02lX). Network channel operational.\r\n\r\n",
                 ( unsigned long ) xOnlineBits );

    for( ;; )
    {
        /* Wait for either a new message or a format error */
        EventBits_t xEvent = xEventGroupWaitBits(
            xSEYA_EventGroup,
            ( SEYA_BIT_MSG_PENDING | SEYA_BIT_ERROR_FORMAT ),
            pdTRUE,          /* Clear wait bits on exit */
            pdFALSE,         /* Wait for ANY bit */
            portMAX_DELAY
        );

        /* Handle format error bit */
        if( ( xEvent & SEYA_BIT_ERROR_FORMAT ) != 0 )
        {
            ulErrorsCounted++;
            safe_printf( "[SEYA_CHAT] Monitor : [ALERT] Event BIT_ERROR_FORMAT caught! Malformed packet logged. (Total Errors=%lu)\r\n",
                         ulErrorsCounted );
        }

        /* Handle message pending bit */
        if( ( xEvent & SEYA_BIT_MSG_PENDING ) != 0 )
        {
            SEYA_ChatPacket_t xReceivedPacket;
            size_t xReceivedBytes = xMessageBufferReceive(
                xSEYA_MessageBuffer,
                &xReceivedPacket,
                sizeof( xReceivedPacket ),
                pdMS_TO_TICKS( 100 )
            );

            if( xReceivedBytes == sizeof( xReceivedPacket ) )
            {
                ulTotalInspected++;
                safe_printf( "[SEYA_CHAT] Monitor : [INSPECT PACKET #%lu] %c -> %c | Length: %lu bytes | Text: \"%s\" | Lifetime Packets: %lu\r\n",
                             xReceivedPacket.ulPacketId,
                             xReceivedPacket.cSender,
                             xReceivedPacket.cRecipient,
                             xReceivedPacket.ulPayloadLength,
                             xReceivedPacket.cPayload,
                             ulTotalInspected );
            }
        }
    }
}

/* ============================================================================
 * Windows Simulator Keyboard Handler
 * ============================================================================ */
void vBlinkyKeyboardInterruptHandler( int xKeyPress )
{
    ( void ) xKeyPress;
}

/* ============================================================================
 * Main Blinky Entry Point
 * ============================================================================ */
void main_blinky( void )
{
    safe_printf( "\r\n===================================================================\r\n" );
    safe_printf( " FreeRTOS Laboratory Work #6\r\n" );
    safe_printf( " Student: Senyk Yaroslav (SEYA_)\r\n" );
    safe_printf( " Variant: %d (\"Простий чат між двома вузлами через gateway\")\r\n", SEYA_VARIANT_ID );
    safe_printf( " Primitives: Stream Buffer + Message Buffer + Event Group\r\n" );
    safe_printf( "===================================================================\r\n\r\n" );

    /* Create console mutex */
    xConsoleMutex = xSemaphoreCreateMutex();
    configASSERT( xConsoleMutex != NULL );

    /* Create stream write mutex to protect concurrent multi-byte writes */
    xSEYA_StreamMutex = xSemaphoreCreateMutex();
    configASSERT( xSEYA_StreamMutex != NULL );

    /* 1. Create Stream Buffer */
    xSEYA_StreamBuffer = xStreamBufferCreate(
        SEYA_STREAM_BUFFER_SIZE,
        SEYA_STREAM_TRIGGER_LEVEL
    );
    configASSERT( xSEYA_StreamBuffer != NULL );

    /* 2. Create Message Buffer */
    xSEYA_MessageBuffer = xMessageBufferCreate(
        SEYA_MESSAGE_BUFFER_SIZE
    );
    configASSERT( xSEYA_MessageBuffer != NULL );

    /* 3. Create Event Group */
    xSEYA_EventGroup = xEventGroupCreate();
    configASSERT( xSEYA_EventGroup != NULL );

    /* 4. Create Tasks */
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        prvMonitorTask,
        "SEYA_Monitor",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 3,
        &xSEYA_MonitorHandle
    );
    configASSERT( xStatus == pdPASS );

    xStatus = xTaskCreate(
        prvGatewayTask,
        "SEYA_Gateway",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 3,
        &xSEYA_GatewayHandle
    );
    configASSERT( xStatus == pdPASS );

    xStatus = xTaskCreate(
        prvNodeATask,
        "SEYA_NodeA",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 2,
        &xSEYA_NodeAHandle
    );
    configASSERT( xStatus == pdPASS );

    xStatus = xTaskCreate(
        prvNodeBTask,
        "SEYA_NodeB",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 2,
        &xSEYA_NodeBHandle
    );
    configASSERT( xStatus == pdPASS );

    /* Start the scheduler */
    safe_printf( "[SEYA_CHAT] Starting FreeRTOS Scheduler...\r\n" );
    vTaskStartScheduler();

    /* Should never reach here */
    for( ;; );
}

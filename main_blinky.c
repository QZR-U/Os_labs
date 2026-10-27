/*
 * FreeRTOS V202212.00
 * FreeRTOS Laboratory Work #5
 * Student: Senyk Yaroslav (Yaroslav Senyk)
 * Variant: 12 ("Планувальник задач з дедлайнами" / EDF Deadline Task Scheduler)
 * Task Prefix: SEYA_
 *
 * Requirements for Variant 12:
 * 1. Direct-to-task notifications and Software Timers in FreeRTOS.
 * 2. Scenario 12: Console simulation of an EDF-like deadline task scheduler.
 * 3. Tasks:
 *    - GeneratorTask (SEYA_GenTask): Periodically generates new jobs with an incremental
 *      Job ID and a relative deadline timestamp, notifying SchedulerTask via xTaskNotify(..., value, eSetValueWithOverwrite).
 *    - SchedulerTask (SEYA_SchedTask): Receives new job notifications. It tracks finished and
 *      missed deadlines using ulTaskNotifyTake() as a counting semaphore. It wakes WorkerTask
 *      by notifying it with the current Job ID.
 *    - WorkerTask (SEYA_WorkerTask): Simulates execution of the assigned job via vTaskDelay().
 *      Upon completion, sends a notification back to SchedulerTask / LoggerTask.
 *    - LoggerTask (SEYA_LogTask): Outputs execution statistics and deadlines status with prefix [SEYA_EDF].
 * 4. Software Timers:
 *    - Periodic Timer (period 1200 ms): Periodically triggers GeneratorTask to create the next workload job.
 *    - One-Shot Deadline Timer: Started for each active job. If it expires before WorkerTask
 *      reports completion, the timer callback sends a direct notification to LoggerTask signaling "MISSED DEADLINE".
 * 5. Direct Notification usage:
 *    - Value carrier: Job ID and deadline timestamp passed via xTaskNotify() value parameter.
 *    - Counting semaphore: Completed/missed job counters tallied via ulTaskNotifyTake().
 * 6. Windows Simulator dummy keyboard handler:
 *    vBlinkyKeyboardInterruptHandler(int xKeyPress)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* ============================================================================
 * Configuration & Constants
 * ============================================================================ */
#define SEYA_VARIANT_ID                 12
#define SEYA_PERIODIC_TIMER_PERIOD_MS   1200

/* Direct Notification Event Masks for LoggerTask */
#define SEYA_NOTIFY_EVENT_MASK          0xFFFF0000UL
#define SEYA_NOTIFY_JOB_ID_MASK         0x0000FFFFUL

#define SEYA_EVENT_JOB_MET              0x00010000UL
#define SEYA_EVENT_DEADLINE_MISSED      0xDEAD0000UL
#define SEYA_EVENT_JOB_LATE             0x00020000UL

/* Job structure */
typedef struct
{
    uint32_t ulJobId;
    uint32_t ulExecTimeMs;
    uint32_t ulDeadlineMs;
    TickType_t xStartTimeTicks;
    BaseType_t bDeadlineExpired;
    BaseType_t bCompleted;
} SEYA_JobInfo_t;

/* Task Handles */
static TaskHandle_t xSEYA_GenTaskHandle    = NULL;
static TaskHandle_t xSEYA_SchedTaskHandle  = NULL;
static TaskHandle_t xSEYA_WorkerTaskHandle = NULL;
static TaskHandle_t xSEYA_LogTaskHandle    = NULL;

/* Timer Handles */
static TimerHandle_t xSEYA_PeriodicTimer    = NULL;
static TimerHandle_t xSEYA_DeadlineTimer    = NULL;

/* Mutex for thread-safe console output */
static SemaphoreHandle_t xConsoleMutex = NULL;

/* Current active job */
static SEYA_JobInfo_t xCurrentJob;

/* Workload table to demonstrate both met and missed deadlines */
typedef struct
{
    uint32_t ulExecMs;
    uint32_t ulDeadlineMs;
} SEYA_JobProfile_t;

static const SEYA_JobProfile_t xJobProfiles[] =
{
    { 400,  750 },   /* Job 1: Met (400 <= 750) */
    { 850,  500 },   /* Job 2: Missed (850 > 500) */
    { 300,  600 },   /* Job 3: Met (300 <= 600) */
    { 900,  450 },   /* Job 4: Missed (900 > 450) */
    { 500,  800 },   /* Job 5: Met (500 <= 800) */
    { 700,  700 }    /* Job 6: Met (700 <= 700) */
};
#define SEYA_NUM_PROFILES   ( sizeof( xJobProfiles ) / sizeof( xJobProfiles[ 0 ] ) )

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
 * Software Timer Callbacks
 * ============================================================================ */

/* Periodic Timer Callback: triggers GeneratorTask */
static void prvPeriodicTimerCallback( TimerHandle_t xTimer )
{
    ( void ) xTimer;
    if( xSEYA_GenTaskHandle != NULL )
    {
        /* Signal GeneratorTask that it's time to create the next workload job */
        xTaskNotifyGive( xSEYA_GenTaskHandle );
    }
}

/* One-Shot Deadline Timer Callback: triggers if WorkerTask hasn't completed */
static void prvDeadlineTimerCallback( TimerHandle_t xTimer )
{
    ( void ) xTimer;

    xCurrentJob.bDeadlineExpired = pdTRUE;

    /* Direct notification to LoggerTask signaling MISSED DEADLINE */
    if( xSEYA_LogTaskHandle != NULL )
    {
        uint32_t ulNotifyVal = SEYA_EVENT_DEADLINE_MISSED | ( xCurrentJob.ulJobId & SEYA_NOTIFY_JOB_ID_MASK );
        xTaskNotify( xSEYA_LogTaskHandle, ulNotifyVal, eSetValueWithOverwrite );
    }

    /* Signal SchedulerTask counting semaphore: accounts for deadline event */
    if( xSEYA_SchedTaskHandle != NULL )
    {
        xTaskNotifyGive( xSEYA_SchedTaskHandle );
    }
}

/* ============================================================================
 * Tasks Implementation
 * ============================================================================ */

/* 1. GeneratorTask (SEYA_GenTask)
 * Periodically generates new jobs with an incremental Job ID and a relative
 * deadline timestamp, notifying SchedulerTask via xTaskNotify(..., value, eSetValueWithOverwrite).
 */
static void prvGeneratorTask( void * pvParameters )
{
    ( void ) pvParameters;
    static uint32_t ulJobCounter = 0;
    size_t xProfileIdx = 0;

    safe_printf( "[SEYA_EDF] GeneratorTask initialized and waiting for timer triggers.\r\n" );

    for( ;; )
    {
        /* Wait for trigger from Periodic Timer */
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

        ulJobCounter++;
        const SEYA_JobProfile_t * pxProf = &xJobProfiles[ xProfileIdx ];
        xProfileIdx = ( xProfileIdx + 1 ) % SEYA_NUM_PROFILES;

        /* Fill in job details */
        xCurrentJob.ulJobId           = ulJobCounter;
        xCurrentJob.ulExecTimeMs      = pxProf->ulExecMs;
        xCurrentJob.ulDeadlineMs      = pxProf->ulDeadlineMs;
        xCurrentJob.xStartTimeTicks   = xTaskGetTickCount();
        xCurrentJob.bDeadlineExpired  = pdFALSE;
        xCurrentJob.bCompleted        = pdFALSE;

        /* Carrier value: High 16 bits = Job ID, Low 16 bits = Deadline (ms) */
        uint32_t ulCarrierValue = ( ( ulJobCounter & 0xFFFFUL ) << 16 ) | ( pxProf->ulDeadlineMs & 0xFFFFUL );

        safe_printf( "[SEYA_EDF] Generator : Created Job #%lu | Workload=%lu ms | Relative Deadline=%lu ms\r\n",
                     ulJobCounter, pxProf->ulExecMs, pxProf->ulDeadlineMs );

        /* Notify SchedulerTask with carrier value */
        xTaskNotify( xSEYA_SchedTaskHandle, ulCarrierValue, eSetValueWithOverwrite );
    }
}

/* 2. SchedulerTask (SEYA_SchedTask)
 * Receives new job notifications. It tracks finished and missed deadlines
 * using ulTaskNotifyTake() as a counting semaphore. It wakes WorkerTask
 * by notifying it with the current Job ID.
 */
static void prvSchedulerTask( void * pvParameters )
{
    ( void ) pvParameters;
    uint32_t ulReceivedCarrier = 0;
    uint32_t ulTotalEventsTracked = 0;

    safe_printf( "[SEYA_EDF] SchedulerTask ready to schedule jobs and track deadlines.\r\n" );

    for( ;; )
    {
        /* Wait for new job notification from GeneratorTask */
        xTaskNotifyWait( 0, ULONG_MAX, &ulReceivedCarrier, portMAX_DELAY );

        uint32_t ulJobId      = ( ulReceivedCarrier >> 16 ) & 0xFFFFUL;
        uint32_t ulDeadlineMs = ulReceivedCarrier & 0xFFFFUL;

        safe_printf( "[SEYA_EDF] Scheduler : Scheduled Job #%lu (Deadline: %lu ms). Arming one-shot timer.\r\n",
                     ulJobId, ulDeadlineMs );

        /* Start / restart one-shot deadline timer with relative deadline period */
        xTimerChangePeriod( xSEYA_DeadlineTimer, pdMS_TO_TICKS( ulDeadlineMs ), 0 );
        xTimerStart( xSEYA_DeadlineTimer, 0 );

        /* Wake WorkerTask with the current Job ID as the notification value */
        xTaskNotify( xSEYA_WorkerTaskHandle, ulJobId, eSetValueWithOverwrite );

        /* Track finished or missed deadline using ulTaskNotifyTake() as counting semaphore */
        uint32_t ulSemCount = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        ulTotalEventsTracked += ( ulSemCount > 0 ) ? ulSemCount : 1;

        safe_printf( "[SEYA_EDF] Scheduler : Accounting event processed via ulTaskNotifyTake(). Total tallied events=%lu\r\n",
                     ulTotalEventsTracked );
    }
}

/* 3. WorkerTask (SEYA_WorkerTask)
 * Simulates execution of the assigned job via vTaskDelay().
 * Upon completion, sends a notification back to SchedulerTask / LoggerTask.
 */
static void prvWorkerTask( void * pvParameters )
{
    ( void ) pvParameters;
    uint32_t ulJobId = 0;

    safe_printf( "[SEYA_EDF] WorkerTask ready to execute jobs.\r\n" );

    for( ;; )
    {
        /* Wait to be woken by SchedulerTask with Job ID */
        xTaskNotifyWait( 0, ULONG_MAX, &ulJobId, portMAX_DELAY );

        uint32_t ulExecMs = xCurrentJob.ulExecTimeMs;
        safe_printf( "[SEYA_EDF] Worker    : >>> Executing Job #%lu (Simulated workload: %lu ms)...\r\n",
                     ulJobId, ulExecMs );

        /* Simulate workload */
        vTaskDelay( pdMS_TO_TICKS( ulExecMs ) );

        /* Check if deadline timer is still running */
        if( xTimerIsTimerActive( xSEYA_DeadlineTimer ) != pdFALSE )
        {
            /* Worker finished BEFORE deadline! Stop the timer */
            xTimerStop( xSEYA_DeadlineTimer, 0 );
            xCurrentJob.bCompleted = pdTRUE;

            /* Notify LoggerTask of successful on-time completion */
            uint32_t ulNotifyVal = SEYA_EVENT_JOB_MET | ( ulJobId & SEYA_NOTIFY_JOB_ID_MASK );
            xTaskNotify( xSEYA_LogTaskHandle, ulNotifyVal, eSetValueWithOverwrite );

            /* Give counting semaphore token to SchedulerTask */
            xTaskNotifyGive( xSEYA_SchedTaskHandle );
        }
        else
        {
            /* Deadline timer already fired and signaled deadline miss */
            xCurrentJob.bCompleted = pdTRUE;

            /* Notify LoggerTask that late execution finished */
            uint32_t ulNotifyVal = SEYA_EVENT_JOB_LATE | ( ulJobId & SEYA_NOTIFY_JOB_ID_MASK );
            xTaskNotify( xSEYA_LogTaskHandle, ulNotifyVal, eSetValueWithOverwrite );
        }
    }
}

/* 4. LoggerTask (SEYA_LogTask)
 * Outputs execution statistics and deadlines status with prefix [SEYA_EDF].
 */
static void prvLoggerTask( void * pvParameters )
{
    ( void ) pvParameters;
    uint32_t ulNotification = 0;
    uint32_t ulJobsMet = 0;
    uint32_t ulJobsMissed = 0;

    safe_printf( "[SEYA_EDF] LoggerTask online and logging execution statistics.\r\n" );

    for( ;; )
    {
        /* Block waiting for status notifications from WorkerTask or DeadlineTimer */
        xTaskNotifyWait( 0, ULONG_MAX, &ulNotification, portMAX_DELAY );

        uint32_t ulEvent = ulNotification & SEYA_NOTIFY_EVENT_MASK;
        uint32_t ulJobId = ulNotification & SEYA_NOTIFY_JOB_ID_MASK;
        uint32_t ulTotal = ulJobsMet + ulJobsMissed;

        switch( ulEvent )
        {
            case SEYA_EVENT_JOB_MET:
                ulJobsMet++;
                ulTotal = ulJobsMet + ulJobsMissed;
                safe_printf( "[SEYA_EDF] Logger    : [MET] Job #%lu COMPLETED ON TIME! (Exec=%lu ms <= Deadline=%lu ms)\r\n",
                             ulJobId, xCurrentJob.ulExecTimeMs, xCurrentJob.ulDeadlineMs );
                safe_printf( "[SEYA_EDF] Logger    : [STATS] Met: %lu | Missed: %lu | Total: %lu | Success Rate: %.1f%%\r\n",
                             ulJobsMet, ulJobsMissed, ulTotal, ( ( double ) ulJobsMet * 100.0 ) / ( double ) ulTotal );
                break;

            case SEYA_EVENT_DEADLINE_MISSED:
                ulJobsMissed++;
                ulTotal = ulJobsMet + ulJobsMissed;
                safe_printf( "[SEYA_EDF] Logger    : [!] *** MISSED DEADLINE *** Job #%lu! (Deadline %lu ms expired)\r\n",
                             ulJobId, xCurrentJob.ulDeadlineMs );
                safe_printf( "[SEYA_EDF] Logger    : [STATS] Met: %lu | Missed: %lu | Total: %lu | Success Rate: %.1f%%\r\n",
                             ulJobsMet, ulJobsMissed, ulTotal, ( ( double ) ulJobsMet * 100.0 ) / ( double ) ulTotal );
                break;

            case SEYA_EVENT_JOB_LATE:
                safe_printf( "[SEYA_EDF] Logger    : [LATE] Job #%lu finished overdue workload (%lu ms) after deadline.\r\n",
                             ulJobId, xCurrentJob.ulExecTimeMs );
                break;

            default:
                safe_printf( "[SEYA_EDF] Logger    : Unknown notification event: 0x%08lX\r\n", ulNotification );
                break;
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
    safe_printf( " FreeRTOS Laboratory Work #5\r\n" );
    safe_printf( " Student: Senyk Yaroslav (SEYA_)\r\n" );
    safe_printf( " Variant: %d (\"Планувальник задач з дедлайнами\" / EDF)\r\n", SEYA_VARIANT_ID );
    safe_printf( "===================================================================\r\n\r\n" );

    /* Create console mutex */
    xConsoleMutex = xSemaphoreCreateMutex();
    configASSERT( xConsoleMutex != NULL );

    /* 1. Create Periodic Software Timer (1200 ms) */
    xSEYA_PeriodicTimer = xTimerCreate(
        "SEYA_PeriodicTimer",
        pdMS_TO_TICKS( SEYA_PERIODIC_TIMER_PERIOD_MS ),
        pdTRUE,                 /* Auto-reload */
        ( void * ) 0,
        prvPeriodicTimerCallback
    );
    configASSERT( xSEYA_PeriodicTimer != NULL );

    /* 2. Create One-Shot Deadline Timer (initial period dummy 1000 ms, updated dynamically) */
    xSEYA_DeadlineTimer = xTimerCreate(
        "SEYA_DeadlineTimer",
        pdMS_TO_TICKS( 1000 ),
        pdFALSE,                /* One-shot */
        ( void * ) 0,
        prvDeadlineTimerCallback
    );
    configASSERT( xSEYA_DeadlineTimer != NULL );

    /* 3. Create Tasks */
    BaseType_t xStatus;

    xStatus = xTaskCreate(
        prvLoggerTask,
        "SEYA_LogTask",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 3,
        &xSEYA_LogTaskHandle
    );
    configASSERT( xStatus == pdPASS );

    xStatus = xTaskCreate(
        prvSchedulerTask,
        "SEYA_SchedTask",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 3,
        &xSEYA_SchedTaskHandle
    );
    configASSERT( xStatus == pdPASS );

    xStatus = xTaskCreate(
        prvGeneratorTask,
        "SEYA_GenTask",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 2,
        &xSEYA_GenTaskHandle
    );
    configASSERT( xStatus == pdPASS );

    xStatus = xTaskCreate(
        prvWorkerTask,
        "SEYA_WorkerTask",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        &xSEYA_WorkerTaskHandle
    );
    configASSERT( xStatus == pdPASS );

    /* Start the periodic timer */
    xTimerStart( xSEYA_PeriodicTimer, 0 );

    /* Kick-start initial generator trigger */
    xTaskNotifyGive( xSEYA_GenTaskHandle );

    /* Start the scheduler */
    safe_printf( "[SEYA_EDF] Starting FreeRTOS Scheduler...\r\n" );
    vTaskStartScheduler();

    /* Should never reach here */
    for( ;; );
}

/*
 * FreeRTOS V202107.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/******************************************************************************
 * NOTE 1: The FreeRTOS demo threads will not be running continuously, so
 * do not expect to get real time behaviour from the FreeRTOS Linux port, or
 * this demo application.  Also, the timing information in the FreeRTOS+Trace
 * logs have no meaningful units.  See the documentation page for the Linux
 * port for further information:
 * https://freertos.org/FreeRTOS-simulator-for-Linux.html
 *
 * NOTE 2:  This project provides two demo applications.  A simple blinky style
 * project, and a more comprehensive test and demo application.  The
 * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting in main.c is used to select
 * between the two.  See the notes on using mainCREATE_SIMPLE_BLINKY_DEMO_ONLY
 * in main.c.  This file implements the simply blinky version.  Console output
 * is used in place of the normal LED toggling.
 *
 * NOTE 3:  This file only contains the source code that is specific to the
 * basic demo.  Generic functions, such FreeRTOS hook functions, are defined
 * in main.c.
 ******************************************************************************
 *
 * main_blinky() creates one queue, one software timer, and two tasks.  It then
 * starts the scheduler.
 *
 * The Queue Send Task:
 * The queue send task is implemented by the prvQueueSendTask() function in
 * this file.  It uses vTaskDelayUntil() to create a periodic task that sends
 * the value 100 to the queue every 200 milliseconds (please read the notes
 * above regarding the accuracy of timing under Linux).
 *
 * The Queue Send Software Timer:
 * The timer is an auto-reload timer with a period of two seconds.  The timer's
 * callback function writes the value 200 to the queue.  The callback function
 * is implemented by prvQueueSendTimerCallback() within this file.
 *
 * The Queue Receive Task:
 * The queue receive task is implemented by the prvQueueReceiveTask() function
 * in this file.  prvQueueReceiveTask() waits for data to arrive on the queue.
 * When data is received, the task checks the value of the data, then outputs a
 * message to indicate if the data came from the queue send task or the queue
 * send software timer.
 *
 * Expected Behaviour:
 * - The queue send task writes to the queue every 200ms, so every 200ms the
 *   queue receive task will output a message indicating that data was received
 *   on the queue from the queue send task.
 * - The queue send software timer has a period of two seconds, and is reset
 *   each time a key is pressed.  So if two seconds expire without a key being
 *   pressed then the queue receive task will output a message indicating that
 *   data was received on the queue from the queue send software timer.
 *
 * NOTE:  Console input and output relies on Linux system calls, which can
 * interfere with the execution of the FreeRTOS Linux port. This demo only
 * uses Linux system call occasionally. Heavier use of Linux system calls
 * may crash the port.
 */

#include <stdio.h>
#include <pthread.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Local includes. */
#include "console.h"

/* Priorities at which the tasks are created. */
#define Task_1_PRIORITY                    ( tskIDLE_PRIORITY + 3 )
#define Task_2_PRIORITY                    ( tskIDLE_PRIORITY + 2 )
#define Task_3_PRIORITY                    ( tskIDLE_PRIORITY + 1 )
#define Task_4_PRIORITY                    ( tskIDLE_PRIORITY  )

/* The rate at which data is sent to the queue.  The times are converted from
 * milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTIMER_SEND_FREQUENCY_MS        pdMS_TO_TICKS( 2000UL )
#define Task_1_FREQUENCY_MS                pdMS_TO_TICKS( 200UL )
#define Task_2_FREQUENCY_MS                pdMS_TO_TICKS( 300UL )
#define Task_3_FREQUENCY_MS                pdMS_TO_TICKS( 450UL )
#define Task_4_FREQUENCY_MS                pdMS_TO_TICKS( 600UL )
/* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH                   ( 4 )

/* The values sent to the queue receive task from the queue send task and the
 * queue send software timer respectively. */
#define mainVALUE_SENT_FROM_TIMER          ( 200UL )

/*-----------------------------------------------------------*/

/*
 * The tasks as described in the comments at the top of this file.
 */
static void Task_1(void * pvParameters );
static void Task_2(void * pvParameters );
static void Task_3(void * pvParameters );
static void Task_4(void * pvParameters );

/*
 * The callback function executed when the software timer expires.
 */
static void prvQueueSendTimerCallback( TimerHandle_t xTimerHandle );

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/*-----------------------------------------------------------*/

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void main_blinky( void )
{
    const TickType_t xTimerPeriod = mainTIMER_SEND_FREQUENCY_MS;

    /* Create the queue. */
    xQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( uint32_t ) );

    if( xQueue != NULL )
    {
        /* Start the two tasks as described in the comments at the top of this
         * file. */
        xTaskCreate( Task_1,                            /* The function that implements the task. */
                     "Task_1",                          /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                     configMINIMAL_STACK_SIZE,          /* The size of the stack to allocate to the task. */
                     NULL,                              /* The parameter passed to the task - not used in this simple case. */
                     Task_1_PRIORITY,                   /* The priority assigned to the task. */
                     NULL );                            /* The task handle is not required, so NULL is passed. */

        xTaskCreate( Task_2,                            /* The function that implements the task. */
                     "Task_2",                          /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                     configMINIMAL_STACK_SIZE,          /* The size of the stack to allocate to the task. */
                     NULL,                        /* The parameter passed to the task - not used in this simple case. */
                     Task_2_PRIORITY,                   /* The priority assigned to the task. */
                     NULL );                            /* The task handle is not required, so NULL is passed. */

        xTaskCreate( Task_3,                            /* The function that implements the task. */
                     "Task_3",                          /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                     configMINIMAL_STACK_SIZE,          /* The size of the stack to allocate to the task. */
                     NULL,                        /* The parameter passed to the task - not used in this simple case. */
                     Task_3_PRIORITY,                   /* The priority assigned to the task. */
                     NULL );                            /* The task handle is not required, so NULL is passed. */

        xTaskCreate( Task_4,                            /* The function that implements the task. */
                     "Task_4",                          /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                     configMINIMAL_STACK_SIZE,          /* The size of the stack to allocate to the task. */
                     NULL,                        /* The parameter passed to the task - not used in this simple case. */
                     Task_4_PRIORITY,                   /* The priority assigned to the task. */
                     NULL );                            /* The task handle is not required, so NULL is passed. */

        xTimer = xTimerCreate( "Timer",                     /* The text name assigned to the software timer - for debug only as it is not used by the kernel. */
                               xTimerPeriod,                /* The period of the software timer in ticks. */
                               pdTRUE,                      /* xAutoReload is set to pdTRUE. */
                               NULL,                        /* The timer's ID is not used. */
                               prvQueueSendTimerCallback ); /* The function executed when the timer expires. */

        if( xTimer != NULL )
        {
            xTimerStart( xTimer, 0 );
        }

        /* Start the tasks and timer running. */
        vTaskStartScheduler();
    }

    /* If all is well, the scheduler will now be running, and the following
     * line will never be reached.  If the following line does execute, then
     * there was insufficient FreeRTOS heap memory available for the idle and/or
     * timer tasks	to be created.  See the memory management section on the
     * FreeRTOS web site for more details. */
    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

static void prvQueueSendTimerCallback( TimerHandle_t xTimerHandle )
{
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_TIMER;

    /* This is the software timer callback function.  The software timer has a
     * period of two seconds and is reset each time a key is pressed.  This
     * callback function will execute if the timer expires, which will only happen
     * if a key is not pressed for two seconds. */

    /* Avoid compiler warnings resulting from the unused parameter. */
    ( void ) xTimerHandle;

    /* Send to the queue - causing the queue receive task to unblock and
     * write out a message.  This function is called from the timer/daemon task, so
     * must not block.  Hence the block time is set to 0. */
    xQueueSend( xQueue, &ulValueToSend, 0U );
}
/*-----------------------------------------------------------*/

static void Task_1( void * pvParameters ) {
    
    TickType_t xNextWakeTime;
    const TickType_t xBlockTime = Task_1_FREQUENCY_MS;
    
    /* Block for 100 ms */
    const TickType_t xDelay = pdMS_TO_TICKS( 100UL ) ;

    xNextWakeTime = xTaskGetTickCount();

    /* Prevent the compiler warning about the unused parameter. */
    ( void ) pvParameters;

    for( ; ; )
    {
        vTaskDelayUntil( &xNextWakeTime, xBlockTime );

        console_print("Working \n");
        vTaskDelay( xDelay );
    }
}
/*-----------------------------------------------------------*/

static void Task_2( void * pvParameters ) {
    
    TickType_t xNextWakeTime;
    const TickType_t xBlockTime = Task_2_FREQUENCY_MS;

    xNextWakeTime = xTaskGetTickCount();
    int Fahrenheit = 32;
    /* Avoid compiler warnings resulting from the unused parameter. */
    (void * ) pvParameters;

    for( ; ; )
    {
        vTaskDelayUntil( &xNextWakeTime, xBlockTime );
        double Celsius = (Fahrenheit - 32) * (5.0/9.0);
        console_print("%d°F => %f°C \n",Fahrenheit, Celsius);
    }
}
/*-----------------------------------------------------------*/

static void Task_3( void * pvParameters ) {
    
    TickType_t xNextWakeTime;
    const TickType_t xBlockTime = Task_3_FREQUENCY_MS;

    xNextWakeTime = xTaskGetTickCount();
    /* Avoid compiler warnings resulting from the unused parameter. */
    (void * ) pvParameters;
    long a = 2036854775807;
    long b = 2036854775807;
    for( ; ; )
    {
        vTaskDelayUntil( &xNextWakeTime, xBlockTime );
        long result = a * b;
        console_print("result : %ld \n",result);
    }
}
/*-----------------------------------------------------------*/

int binarySearch(int arr[], int low, int high, int target) {
    while (low <= high) {
        int mid = low + (high - low) / 2;

        // Check if the target is present at the middle
        if (arr[mid] == target)
            return mid;

        // If the target is greater, ignore the left half
        if (arr[mid] < target)
            low = mid + 1;

        // If the target is smaller, ignore the right half
        else
            high = mid - 1;
    }

    // Target not present in the array
    return -1;
}

static void Task_4( void * pvParameters ) {
    
    TickType_t xNextWakeTime;
    const TickType_t xBlockTime = Task_4_FREQUENCY_MS;

    xNextWakeTime = xTaskGetTickCount();
    /* Avoid compiler warnings resulting from the unused parameter. */
    (void * ) pvParameters;
    
    int arr[50] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20,
                   22, 24, 26, 28, 30, 32, 34, 36, 38, 40,
                   42, 44, 46, 48, 50, 52, 54, 56, 58, 60,
                   62, 64, 66, 68, 70, 72, 74, 76, 78, 80,
                   82, 84, 86, 88, 90, 92, 94, 96, 98, 100};
    
    int target = 42;  // Element to search

    int low = 0;
    int high = 100;


    for( ; ; )
    {
        vTaskDelayUntil( &xNextWakeTime, xBlockTime );
        
        int result = binarySearch(arr, 0, 49, target);

    if (result != -1)
        console_print("Element %d found at index %d\n", target, result);
    else
        console_print("Element %d not found in the array\n", target);
    }
}

#include "ActiveObject.h"
#include "MethodRequest.h"
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


ActiveObject::ActiveObject(string name, uint16_t stackSize , UBaseType_t priority):
    name(name)
{
    TaskHandle_t newTask;
    BaseType_t xReturned;

    mrQueue = xQueueCreate(20,sizeof(MRequest*));

    printf("xqueuecreate, task: %x pxMutexHolder: %x ", (int) xTaskGetCurrentTaskHandle(), (int) xQueueGetMutexHolder(mrQueue));

    if (mrQueue == NULL)
    {
        printf("MR Queue was not created \r\n ");
    }

    xReturned = xTaskCreatePinnedToCore( ActiveObjectTaskFunction,
            name.c_str(), 
            stackSize,
            mrQueue, 
            priority,
            &newTask,
            1);
    
    if (xReturned != pdPASS)
    {
        printf("Task %s was not created\r\n ", name.c_str());
    }

    taskHandles[this] = newTask;

}

TimerHandle_t ActiveObject::createOneTimeTimer
     (   const std::function<void()> &f, 
         const TickType_t period )
{
    TimerHandle_t returnTimer;
    auto mr = new MRequest( this, f);
    mr->setPersistent(false);

    returnTimer = xTimerCreate
        ( "tmr",
          period,
          0,
          mr,
          ActiveObjectTimerCallback );

    xTimerStart(returnTimer,0);

    return returnTimer;
}

void ActiveObject::stopTimer(TimerHandle_t tmr)
{
    xTimerStop( tmr, 0 );
}

TimerHandle_t ActiveObject::createTimer
     (   const std::function<void()> &f, 
         const TickType_t period )
{
    TimerHandle_t returnTimer;
    auto mr = new MRequest( this, f);
    mr->setPersistent(true);

    returnTimer = xTimerCreate
        ( "tmr",
          period,
          1,
          mr,
          ActiveObjectTimerCallback );

    xTimerStart(returnTimer,0);

    return returnTimer;
}

uint8_t ActiveObject::executeMethod(MRequest *mr)
{
    return xQueueSend(mrQueue, &mr, 0);
}

uint8_t ActiveObject::executeMethod(const std::function<void()> &f )
{
    auto mr = new MRequest(this, f);
    return xQueueSend(mrQueue, &mr, 0);
}

uint32_t ActiveObject::executeMethodSynchronously(const std::function<void()> &f)
{
    TaskHandle_t callerHandle = xTaskGetCurrentTaskHandle();
    auto mr = new MRequest(this, f, callerHandle);
    xQueueSend(mrQueue, &mr, 0);
    uint32_t returnValue;

    // block until notification from receiver task
    returnValue =  ulTaskNotifyTake( pdTRUE, portMAX_DELAY ); 

    return returnValue;
}


void ActiveObjectTimerCallback( TimerHandle_t xTimer )
{
    // At this point we are in timer task context. We need to pass on mr to
    // ActiveObject context via queue
    MRequest *mr = static_cast<MRequest*>(pvTimerGetTimerID(xTimer));
    ActiveObject *ao =  mr->getActiveObject();
    ao->executeMethod(mr);
}

void ActiveObjectTaskFunction( void *q)
{
    QueueHandle_t queue = q;
    MRequest *mr;

    for (;;) 
    {
        xQueueReceive( queue, &mr, portMAX_DELAY );

        (*(mr->func))();

        // If callerID is set this mean that calling task expects notification
        // after function call end
        if (mr->callerID != NULL)
        {
           xTaskNotifyGive( mr->callerID ); 
        }

        if (mr->isPersistent() == false )
        {
            delete mr;
            mr = NULL;
        }
    }
}

std::map<ActiveObject*, TaskHandle_t> ActiveObject::taskHandles;


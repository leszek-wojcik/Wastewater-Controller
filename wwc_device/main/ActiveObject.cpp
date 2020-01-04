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

AOTimer_t ActiveObject::createOneTimeTimer (   
        AOTimer_t *tmr,
        const std::function<void()> &f, 
        const TickType_t period )
{

    if (*tmr !=NULL) {
        printf("timer was not created \n");
        return NULL;
    }

    auto mr = new MRequest( this, f);
    mr->setPersistent(false);

    *tmr = xTimerCreate
        ( "tmr",
          period/portTICK_PERIOD_MS,
          0,
          mr,
          ActiveObjectTimerCallback );

    xTimerStart(*tmr,0);

    return *tmr;
}

void ActiveObject::stopTimer(AOTimer_t *tmr)
{
    if (*tmr == NULL)
    {
        printf("timer was not stopped\n");
        return;
    }
    xTimerStop( *tmr, 0 );
    *tmr = NULL;
}

AOTimer_t ActiveObject::createTimer (   
        AOTimer_t *tmr,
        const std::function<void()> &f, 
        const TickType_t period )
{

    if (*tmr !=NULL) {
        return NULL;
    }

    auto mr = new MRequest( this, f);
    mr->setPersistent(true);

    *tmr = xTimerCreate
        ( "tmr",
          period/portTICK_PERIOD_MS,
          1,
          mr,
          ActiveObjectTimerCallback );

    xTimerStart(*tmr,0);
    return *tmr;
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


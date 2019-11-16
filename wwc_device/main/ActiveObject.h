#ifndef ACTIVEOBJECT_INCLUDED
#define ACTIVEOBJECT_INCLUDED

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include <functional>
#include <string>
#include <map>

using namespace std;

class MethodRequestBase;
class MRequest;


class ActiveObject
{
    public:
        string name;
        QueueHandle_t mrQueue; 

        ActiveObject(string name="\0",uint16_t stackSize=configMINIMAL_STACK_SIZE, UBaseType_t priority=tskIDLE_PRIORITY);

        TimerHandle_t createOneTimeTimer(
                const std::function<void()> &f, 
                const TickType_t period );

        TimerHandle_t createTimer (
                const std::function<void()> &f, 
                const TickType_t period );

        void stopTimer(TimerHandle_t tmr);

        uint8_t executeMethod(const std::function<void()> &);

        // This method is used to dispatch function call on active object and
        // block until it executed on other thread context
        uint32_t executeMethodSynchronously(const std::function<void()> &);


        QueueHandle_t getQueue()
        {
            return mrQueue; 
        }


    protected:
        // Structure collects all running active objects in order to
        // synchronize tasks when needed.
        static std::map<ActiveObject*, TaskHandle_t> taskHandles;

    private:
        uint8_t executeMethod(MRequest *mr);


    friend void ActiveObjectTimerCallback( TimerHandle_t xTimer );
    friend void ActiveObjectTaskFunction( void * );

};

void ActiveObjectTimerCallback( TimerHandle_t xTimer );
void ActiveObjectTaskFunction( void * );


#endif

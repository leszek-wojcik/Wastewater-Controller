#include "ActiveObject.h"

class WWC : public ActiveObject
{
    private:
        TimerHandle_t aWWCtmr;
        TimerHandle_t aLEDtmr;
        bool areation;
        bool circulation;
        int wwcCounter;
        bool ledOn;
        
    public:
        WWC();

        inline void disableCirculation() { circulation = false; }
        inline void enableCirculation() { circulation = true; }
        inline void disableAreation() { areation = false; }
        inline void enableAreation() { areation = true; }

        void updateControlPins();

        void controlOnTmr();
        void ledOnTmr();
};


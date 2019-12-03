#include "ActiveObject.h"

class WWC : public ActiveObject
{
    private:
        TimerHandle_t aWWCtmr;
        bool areation;
        bool circulation;
        int wwcCounter;
        
    public:
        WWC();

        inline void disableCirculation() { circulation = false; }
        inline void enableCirculation() { circulation = true; }
        inline void disableAreation() { areation = false; }
        inline void enableAreation() { areation = true; }

        void updateControlPins();

        void controlOnTmr();
};


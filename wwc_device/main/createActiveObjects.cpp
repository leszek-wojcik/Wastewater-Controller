#include "ActiveObject.h"
#include "wwc.h"

extern "C"
{

    WWC *aWWC;

    void createActiveObjects()
    {
        aWWC = new WWC();
    }

}

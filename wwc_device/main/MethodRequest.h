#ifndef METHOD_REQUEST_INCLUDED
#define METHOD_REQUEST_INCLUDED

#include <tuple>
#include <functional>

class ActiveObject;

class MRequest
{
    public:
        MRequest(ActiveObject *a, std::function<void()> f, void *callerID=NULL, bool persistent=false):
            ao(a),
            callerID(callerID),
            persistent(persistent)
        {
            func = new std::function<void()>(f);
        }

        // Active Object. This is subject for method request. 
        ActiveObject *ao;

        // Calling task handle. Used to store an unique identifier for caller
        // process 
        void *callerID;

        // function call object (c++11) that binds arguments along with object
        // method pointer that will be called my MRequest execution
        std::function<void()> *func;


        // Persistance flag. This flag signals no need for releasing memory for
        // particular method request object
        bool persistent;

        void setPersistent(bool p)
        {
            persistent = p;
        }
    
        bool isPersistent()
        {
            if (persistent)
                return true;
            else
                return false;
        }

        ActiveObject *getActiveObject()
        {
            return ao;
        }

        ~MRequest()
        {
            delete func;
        }
        
};




#endif

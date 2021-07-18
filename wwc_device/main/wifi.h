class MRequest;
#include "nvs.h"

class WiFi: public ActiveObject
{
    private:
        bool alt;
        signed char wps;
        void connect_wifi(void);
        void connect_alt_wifi(void);
        void connectWPS(void);
        nvs_handle dbHandle;
        void initWiFiStorage();
        void initWPSButton();
        void initWiFiDriver();
        void initWiFiWPSDriver();
    public:
        WiFi();
        static WiFi *instance;
        static MRequest *interruptMr;

        void pushButton();


        inline void toggle(void)
        {
            alt = !alt;
        }
        inline void connect(void)
        {
            (alt) ? connect_alt_wifi() : connect_wifi();
        }

        void disconnect(void);
        
};

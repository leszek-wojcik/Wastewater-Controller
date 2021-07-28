class MRequest;
#include "nvs_handle.hpp"
using namespace nvs;

class WiFi: public ActiveObject
{
    private:
        bool alt;
        bool wpsProcedure;
        void connect_wifi(void);
        void connect_alt_wifi(void);
        std::unique_ptr<NVSHandle> dbHandle;
        void initWiFiStorage();
        void initWPSButton();
        void initWiFiDriver();
        static WiFi *instance;

    public:
        WiFi();

        static WiFi* getInstance()
        {
            return instance;
        }

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

        void connectAndToggle(void);

        void disconnect(void);
        void storeWPSAccess(uint8_t *ssid, uint8_t *pass, int len_ssid, int len_pass);
        void eraseWPS(void);
};

class WiFi
{
    private:
        bool alt;
        void connect_wifi(void);
        void connect_alt_wifi(void);
    public:
        WiFi();

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

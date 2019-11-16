class WWC : public ActiveObject
{
    private:
        TimerHandle_t aWWCtmr;
        
    public:
        WWC();
        void controlOnTmr();
};


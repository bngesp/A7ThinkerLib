#ifndef A7ThinkerLib_h

    #define A7ThinkerLib_h
    #include <Arduino.h>
    #include "SoftwareSerial.h"

    // #ifdef DEBUG
    #define log(msg) Serial.print(msg)
    #define logln(msg) Serial.println(msg)
    // #else
    //     #define log(msg)
    //     #define logln(msg)
    // #endif

    #define countof(a) (sizeof(a) / sizeof(a[0]))


    #define AT_END "\r\n"

    #define A7_CMD_TIMEOUT 2000
    #define TIMEOUT_HTTP_RESPONSE 3000
    // expected length of response from http request on Bytes
    #define EXPECTED_RESPONSE_LENGTH  1000


    struct SMSmessage {
        String number;
        String date;
        String message;
    };

    enum STATUS {
        OK = 0,
        NOTOK = 1,
        TIMEOUT = 2,
        FAILURE = 3,
    };

    enum ENDCHARACTER{
        CR ='\r',
        NL ='\n',
        TB ='\t', 
    };



class A7ThinkerLib{
    private:
        /* variable for GPRS */
        String _host;
        int _port; 
        String _path;
        String _apn;
        int _length;
        int _ResponseLength;
        float _lat;
        float _long;

        /* data */
        // String read();
        // byte command(const char *command, const char *resp1, const char *resp2, int timeout, int repetitions, String *response);
        // byte waitFor(const char *resp1, const char *resp2, int timeout, String *response);
        long detectRate();
        char setRate(long baudRate);
        byte waitFor(const char *resp1, const char *resp2, int timeout, String *response);
        int freeModem(long timeout);
        
    public:
        A7ThinkerLib(int, int );
        ~A7ThinkerLib();

        String read();
        byte command(
            const char *command, 
            const char *resp1, 
            const char *resp2, 
            int timeout, 
            int repetitions, 
            String *response
        );


        byte begin(long baudRate);
        byte blockUntilReady(long baudRate);
        // void powerCycle(int pin);
        // void powerOn(int pin);
        // void powerOff(int pin);
        // byte sendSMS(String number, String text);
        // SMSmessage readSMS(int index);
        // int freeModem(long timeout)
        String getGPSPosition();
        byte initGPS(int freq);
        float getLat(String receive_data);
        float getLong(String receive_data);

        bool connectGPRS(String apn);
        bool initHTTP(String host, String path);
        void header(String);
        void post(const char *);
        String get(String host, String path);
        String getResponseData(String);
        int getResponseLength();
        void closeHTTP();
        SoftwareSerial *A7;

};



#endif
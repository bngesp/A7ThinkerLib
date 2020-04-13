#include <Arduino.h>
#include <SoftwareSerial.h>
#include "A7ThinkerLib.h"

String dummy_string = "";
String body_rcvd_data = "";
byte var = STATUS::NOTOK;
String rcv_str = "+CIPRCV";


A7ThinkerLib::A7ThinkerLib(int rx_pin, int tx_pin) {
#ifdef ESP8266
    A7 = new SoftwareSerial(rx_pin, tx_pin, false, 1024);
#else
    A7 = new SoftwareSerial(rx_pin, tx_pin, false);

#endif
    A7->setTimeout(100);
    _lat = 0;
    _long = 0;
}

A7ThinkerLib::~A7ThinkerLib(){
    delete A7;
}

// Initialize the software serial connection and change the baud rate from the
// default (autodetected) to the desired speed.
byte A7ThinkerLib::begin(long baudRate) {

    A7->flush();

    if (STATUS::OK != setRate(baudRate)) {
        return STATUS::NOTOK;
    }

    // Factory reset.
    command("AT&F", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL);

    // Echo off.
    command("ATE0", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL);

    // Set caller ID on.
    command("AT+CLIP=1", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL);

    // Set SMS to text mode.
    command("AT+CMGF=1", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL);

    // Turn SMS indicators off.
    command("AT+CNMI=1,0", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL);
     // Turn GPS  off.
    command("AT+GSP=0", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL);
    command("AT+GPSRD=0", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL);

    // Set SMS storage to the GSM modem.
    if (STATUS::OK != command("AT+CPMS=ME,ME,ME", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL))
        // This may sometimes fail, in which case the modem needs to be
        // rebooted.
    {
        return STATUS::FAILURE;
    }

    // Set GPS
    if (STATUS::OK != command("AT+GPS=1", "OK", "yy", A7_CMD_TIMEOUT, 2, NULL))
        // This may sometimes fail, in which case the modem needs to be
        // rebooted.
    {
        return STATUS::FAILURE;
    }

    // Set SMS character set.
    //setSMScharset("UCS2");

    return STATUS::OK;
}


// Set the A6 baud rate.
char A7ThinkerLib::setRate(long baudRate) {
    int rate = 0;

    rate = detectRate();
    if (rate == STATUS::NOTOK) {
        return STATUS::NOTOK;
    }

    // The rate is already the desired rate, return.
    //if (rate == baudRate) return OK;

    logln("Setting baud rate on the module...");

    // Change the rate to the requested.
    char buffer[30];
    sprintf(buffer, "AT+IPR=%d", baudRate);
    command(buffer, "OK", "+IPR=", A7_CMD_TIMEOUT, 3, NULL);

    logln("Switching to the new rate...");
    // Begin the connection again at the requested rate.
    A7->begin(baudRate);
    logln("Rate set.");

    return STATUS::OK;
}

// Autodetect the connection rate.
long A7ThinkerLib::detectRate() {
    unsigned long rate = 0;
    unsigned long rates[] = {9600, 115200};

    // Try to autodetect the rate.
    logln("Autodetecting connection rate...");
    for (int i = 0; i < countof(rates); i++) {
        rate = rates[i];

        A7->begin(rate);
        log("Trying rate ");
        log(rate);
        logln("...");

        delay(100);
        if (command("\rAT", "OK", "+CME", 2000, 2, NULL) == STATUS::OK) {
            return rate;
        }
    }

    logln("Couldn't detect the rate.");

    return STATUS::NOTOK;
}

// Block until the module is ready.
byte A7ThinkerLib::blockUntilReady(long baudRate) {

    byte response = STATUS::NOTOK;
    while (STATUS::OK != response) {
        response = begin(baudRate);
        // This means the modem has failed to initialize and we need to reboot
        // it.
        if (STATUS::FAILURE == response) {
            return STATUS::FAILURE;
        }
        delay(1000);
        logln("Waiting for module to be ready...");
    }
    return STATUS::OK;
}

// Issue a command.
byte A7ThinkerLib::command(const char *command, const char *resp1, const char *resp2, int timeout, int repetitions, String *response) {
    byte returnValue = STATUS::NOTOK;
    byte count = 0;

    // Get rid of any buffered output.
    A7->flush();

    while (count < repetitions && returnValue != STATUS::OK) {
        log("Issuing command: ");
        logln(command);

        A7->write(command);
        A7->write(ENDCHARACTER::CR);

        if (waitFor(resp1, resp2, timeout, response) == STATUS::OK) {
            returnValue = STATUS::OK;
        } else {
            returnValue = STATUS::NOTOK;
        }
        count++;
    }
    return returnValue;
}

// Return the modem to a state where it can receive commands.
// The modem might get stuck in a half-way state after some commands,
// but we can send a blank line to make it return to the baseline state.
// based 
int A7ThinkerLib::freeModem(long timeout) {
    byte retval = STATUS::NOTOK;
    logln(F("Setting module free from past command..."));
    long ptime = millis();
    long now = 0;
    while (1) {
        now = millis();
        if (now - ptime > timeout || retval == 0) {
            A7->write(AT_END);
            delay(1000);
            byte retval = command("AT", "OK", "yy", 1000, 1, NULL);
            if (retval == STATUS::OK) {
                log("breaking out:\t");
            }
            log("\t\t");
            logln(retval);
            break;
        }
    }
    return STATUS::OK;
}
// Wait for responses.
byte A7ThinkerLib::waitFor(const char *resp1, const char *resp2, int timeout, String *response) {
    unsigned long entry = millis();
    int count = 0;
    String reply = "";
    byte retVal = 99;
    do {
        reply += read();
        yield();
    } while (((reply.indexOf(resp1) + reply.indexOf(resp2)) == -2) && ((millis() - entry) < timeout));

    if (reply != "") {
        log("Reply in ");
        log((millis() - entry));
        log(" ms: ");
        logln(reply);
    }

    if (response != NULL) {
        *response = reply;
    }

    if ((millis() - entry) >= timeout) {
        retVal = STATUS::TIMEOUT;
        logln("Timed out.");
    } else {
        if (reply.indexOf(resp1) + reply.indexOf(resp2) > -2) {
            logln("Reply OK.");
            retVal = STATUS::OK;
        } else {
            logln("Reply NOT OK.");
            retVal = STATUS::OK;
        }
    }
    return retVal;
}

// Read some data from the A6 in a non-blocking manner.
String A7ThinkerLib::read() {
    String reply = "";
    if (A7->available()) {
        reply = A7->readString();
    }

    // XXX: Replace NULs with \xff so we can match on them.
    for (int x = 0; x < reply.length(); x++) {
        if (reply.charAt(x) == 0) {
            reply.setCharAt(x, 255);
        }
    }
    return reply;
}

/////////////////////////////////////////////
// GPRS methods.
////////////////////////////////////////////

bool A7ThinkerLib::connectGPRS(String apn) {
    String dummy_string = "";
    String _APN = apn;

    byte retstatus = STATUS::NOTOK;

    while (retstatus != STATUS::NOTOK) {
        retstatus = command((const char *)"AT+CGATT?", "OK", "yy", 20000, 1, NULL);
        while (freeModem(3000) != 0);
    }
    retstatus = STATUS::NOTOK;;

    while (retstatus != STATUS::OK) {
        retstatus = command((const char *)"AT+CGATT=1", "OK", "yy", 20000, 2, NULL);
        while (freeModem(3000) != 0);
    }
    retstatus = STATUS::NOTOK;

    A7->write("AT+CSQ"); //Signal Quality
    logln("AT+CSQ");
    delay(200);
    while (freeModem(3000) != 0);

    while (retstatus != STATUS::OK) {
        dummy_string = "AT+CGDCONT=1,\"IP\",\"" + _APN + "\"";
        retstatus = command(dummy_string.c_str(), "OK", "yy", 20000, 2, NULL); //bring up wireless connection
        while (freeModem(3000) != 0);
    }
    retstatus = STATUS::NOTOK;

    while (retstatus != STATUS::OK) {
        retstatus = command((const char *)"AT+CGACT=1,1", "OK", "yy", 10000, 2, NULL);
        while (freeModem(3000) != 0);
        // delay(10000);
    }

    return retstatus;
}



bool A7ThinkerLib::initHTTP(String host, String path) {
    _host = host;
    _path = path;

    _port = 80;
    byte var = STATUS::NOTOK;

    dummy_string = "AT+CIPSTART=\"TCP\",\"" + _host + "\"," + _port;
    while (var != STATUS::OK) {
        //close prior connections if any.
        command("AT+CIPCLOSE", "OK", "yy", 2000, 2, NULL); //start up the connection

        var = command(dummy_string.c_str(), "CONNECT OK", "yy", 30000, 1, NULL); //start up the connection
        logln(var);
        while (freeModem(3000) != 0);
        command("AT+CSQ", "OK", "yy", 2000, 2, NULL); //start up the connection
        logln("AT+CSQ");
        delay(200);
        while (freeModem(3000) != 0);

    }

    logln(F("checking current staus : issued cipstart command"));
    command((const char *)"AT+CIPSTATUS", "OK", "yy", 10000, 2, NULL);
    logln(F("AT+CIPSTATUS"));
    command((const char *)"AT+CIPSEND", ">", "yy", 10000, 1, NULL); //begin send data to remote server
    logln(F("AT+CIPSEND"));
    delay(500);

    dummy_string = "POST " + _path;
    A7->write(dummy_string.c_str());
    log("POST " + _path);
    A7->write(" HTTP/1.1");
    log(F(" HTTP/1.1"));
    A7->write(AT_END);
    log(AT_END);

    A7->write("User-Agent: A6 Modem");
    log(F("User-Agent: A6 Modem"));
    A7->write(AT_END);
    log(AT_END);

    A7->write("HOST: ");
    log(F("HOST: "));
    A7->write(_host.c_str());
    log(_host);
    A7->write(AT_END);
    log(AT_END);

}

void A7ThinkerLib::header(String header) {
    A7->write(header.c_str());
    A7->write(AT_END);
    logln(header.c_str()) ;
}


void A7ThinkerLib::post(const char *postbody) {
    char end_c[2];
    end_c[0] = 0x1a;
    end_c[1] = '\0';

    int i = 0;
    int PostBodyLength = 0;
    while (postbody[i]) {
        PostBodyLength++;
        i++;
    }
    log("body content length is\t");
    logln(PostBodyLength);

    dummy_string = "Content-Type: application/json";
    A7->write(dummy_string.c_str());
    A7->write(AT_END);
    logln(dummy_string.c_str()) ;

    dummy_string = "Content-Length: ";
    dummy_string += String(PostBodyLength);
    A7->write(dummy_string.c_str());
    A7->write(AT_END);

    logln(dummy_string.c_str()) ;
    A7->write("Connection: close");
    log(F("Connection: close"));

    A7->write(AT_END);
    A7->write(AT_END);
    logln();
    logln();

    A7->write(postbody);
    log(postbody);
    A7->write(AT_END);
    log(AT_END);
    command(end_c, "OK", "yy", 30000, 1, NULL); //begin send data to remote server
    long ptime = millis();
    int response_bytes = 0;
    while (millis() - ptime < TIMEOUT_HTTP_RESPONSE && response_bytes < EXPECTED_RESPONSE_LENGTH) {
        if (A7->available()) {
            log((char)A7->read());
            response_bytes++;
        }

        //do further things here like storing data in some variable.
    }
    closeHTTP();

}

String A7ThinkerLib::get(String host, String path) {
//  body_rcvd_data.reserve(1000);
    _path = path;
    _host = host;
    _port = 80;
    char end_c[2];
    end_c[0] = 0x1a;
    end_c[1] = '\0';

    String rcv = "";
    rcv.reserve(50);

    char ch = '\0';
    int count_incoming_bytes = 0;
    String bytes_in_body = "";
    int bytes_read = 0;

    var = STATUS::NOTOK;

    dummy_string = "AT+CIPSTART=\"TCP\",\"" + _host + "\"," + _port;
    while (var != STATUS::OK) {
        //close prior connections if any.
        command("AT+CIPCLOSE", "OK", "yy", 2000, 2, NULL); //start up the connection

        var = command(dummy_string.c_str(), "CONNECT OK", "yy", 30000, 1, NULL); //start up the connection
        //logln(dummy_string.c_str());
        logln(var);
        while (freeModem(3000) != 0);
        command("AT+CSQ", "OK", "yy", 2000, 2, NULL); //start up the connection
        logln(F("AT+CSQ"));
        delay(200);
        while (freeModem(3000) != 0);

    }

    logln(F("checking current staus : issued cipstart command"));
    command((const char *)"AT+CIPSTATUS", "OK", "yy", 10000, 2, NULL);
    logln(F("AT+CIPSTATUS"));
    command((const char *)"AT+CIPSEND", ">", "yy", 10000, 1, NULL); //begin send data to remote server
    logln(F("AT+CIPSEND"));
    delay(500);


    dummy_string = "GET " + _path;
    A7->write(dummy_string.c_str());
    log("GET " + _path);
    A7->write(" HTTP/1.1");
    log(F(" HTTP/1.1"));
    A7->write(AT_END);
    log(F(AT_END));
    A7->write("HOST: ");
    log(F("HOST: "));
    A7->write(_host.c_str());
    log(_host);
    A7->write("\r\n\r\n");
    log(F(AT_END));


    command(end_c, "OK", "yy", 30000, 1, NULL); //begin send data to remote server
    A7->write(end_c);


    while (1) {
        if (A7->available()) {
            rcv = "";
            rcv = A7->readStringUntil('\r');
            int index = rcv.indexOf('+');
            if (index != -1) {
                int colon_index = rcv.indexOf(':');
                String cutfromrecvd = rcv.substring(index, colon_index);
                if (cutfromrecvd == rcv_str) {
                    logln(F("\nrecieving data"));
                }
                int comma_index = rcv.indexOf(',');
                if (comma_index != -1) {
                    bytes_in_body = rcv.substring(colon_index + 1, comma_index);
                    _ResponseLength = bytes_in_body.toInt();
                    int no_of_bytes = _ResponseLength;
                    long now = millis();
                    long ntime = 0;
                    while (1) {
                        long ntime = millis();
                        if (ntime - now > 10000 || (bytes_read == no_of_bytes)) {
                            break;
                        }
                        if (A7->available()) {
                            body_rcvd_data += (char) A7->read();
                            bytes_read += 1;
                        }
                    }
                    break;
                } else {
                    logln(F("Incorrect Data found"));
                }
            }
        }


    }
    logln(body_rcvd_data);
    closeHTTP();
    return body_rcvd_data;
}

int A7ThinkerLib::getResponseLength() {
    return _ResponseLength;
}

void A7ThinkerLib::closeHTTP() {
    logln(F("now going to close tcp connection"));
    command((const char *)"AT+CIPCLOSE", "OK", "yy", 10000, 2, NULL);
    delay(100);
    logln(F("closed"));
    command((const char *)"AT+CIPSTATUS", "OK", "yy", 10000, 1, NULL);
}


String A7ThinkerLib::getResponseData(String body_rcvd_data) {
    int body_content_start = 0;
    int body_content_start_content = 0;
    int body_content_end = 0;

    int bytes_read = strlen(body_rcvd_data.c_str());
    logln(bytes_read);
    body_content_start = body_rcvd_data.indexOf("\r\n\r\n");

    log(F("found double newline at  "));
    logln(body_content_start);
    // int body_content_end=0;
    if (body_content_start != -1) {
        for (int i = body_content_start; i < bytes_read; i++)
            if (isAlphaNumeric(body_rcvd_data[i])) {
                body_content_start_content = i;
            }
        body_content_end = bytes_read;
        log(F("body content start at "));
        logln(body_content_start_content + 1);
        log(F("body content end at  "));
        logln(body_content_end);
        String body_content = body_rcvd_data.substring(body_content_start + 4, body_content_end);
        logln(body_content);
    } else {
        logln("bad body content");
    }
    return "\0";
}

byte A7ThinkerLib::initGPS(int freq){

    dummy_string = "AT+GPSRD="+freq;
    while (var != STATUS::OK) {
    //close prior connections if any.
        //command("AT+CIPCLOSE", "OK", "yy", 2000, 2, NULL); //start up the connection

        var = command("AT+GPSRD=2", "OK", "yy", 3000, 1, NULL); //start up the connection
        logln(var);
       // while (freeModem(3000) != 0);
        // command("AT+CSQ", "OK", "yy", 2000, 2, NULL); //start up the connection
        // logln("AT+CSQ");
        delay(200);
       // while (freeModem(3000) != 0);
    }
    return STATUS::OK;
}

String A7ThinkerLib::getGPSPosition(){
    String recu = "";
    if (A7->available()){
        // recu += (char)A7->read();
        recu += A7->readStringUntil('\n');
    }

    //recu.
    return recu;
}

float A7ThinkerLib::getLat(String receive_data){
    return _lat;
}

float A7ThinkerLib::getLong(String receive_data){
    return _long;
}
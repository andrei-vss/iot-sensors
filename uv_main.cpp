#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#define MyNULL 255

String token = "";
int httpCode = 0;
char server[40];
char port[6] = "8080";
char freqMinutes[2] = "5";
bool shouldSaveConfig = false;
WiFiManager wifiManager;

void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

void getUrl()
{
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile)
        {
            Serial.println("opened config file");
            size_t size = configFile.size();
            // Allocate a buffer to store contents of the file.
            std::unique_ptr<char[]> buf(new char[size]);

            configFile.readBytes(buf.get(), size);
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(buf.get());
            json.printTo(Serial);
            if (json.success())
            {
                Serial.println("\nparsed json");
                strcpy(server, json["server"]);
                strcpy(port, json["port"]);
                strcpy(freqMinutes, json["freqMinutes"]);
            }
            else
            {
                Serial.println("failed to load json config");
            }
            configFile.close();
        }
    }
}

void getToken()
{
    int i = 0;
    byte rByte;
    while ((rByte = EEPROM.read(i++)) != MyNULL)
    {
        token.concat((char)rByte);
    }
    Serial.println("tokenData: " + token);
    if (token == "")
    {
        HTTPClient http;
        http.begin("http://" + String(server) + ":" + String(port) + "/api/getToken");
        http.addHeader("Content-Type", "text/plain"); //Specify content-type header
        http.addHeader("Content-Length", "0");
        httpCode = http.POST("");          //Send the request
        String payload = http.getString(); //Get the response payload
        Serial.println(httpCode);          //Print HTTP return code
        Serial.println(payload);           //Print request response payload
        token = payload;
        http.end(); //Close connection
        for (int i = 0; i < (int)payload.length(); ++i)
        {
            EEPROM.write(i, payload.charAt(i));
            Serial.print("Wrote: ");
            Serial.println(payload[i]);
        }
        EEPROM.commit();
        EEPROM.end();
        Serial.println("TOKEN BBY from http: " + token);
    }
}

void sendData()
{
    int UVindex = (analogRead(A0) * 3.3 / 102.4);
    String jsonResult = "{ \n \"uvValue\" : " + String(UVindex) + " \n }";
    Serial.println(jsonResult);
    HTTPClient http;
    http.begin("http://" + String(server) + ":" + String(port) + "/api/save-data");
    http.addHeader("Content-Type", "application/json"); //Specify content-type header
    http.addHeader("jwt", token);
    int httpCode1 = http.POST(jsonResult);
    String payload = http.getString(); //Get the response payload

    Serial.println(httpCode1); //Print HTTP return code
    Serial.println(payload);   //Print request response payload
    http.end();                //Close connection
}

void setupWifiManager()
{
    WiFiManagerParameter customIp("ip", "server ip", server, 40);
    WiFiManagerParameter customPort("port", "mqtt port", port, 6);
    WiFiManagerParameter customFreq("frequency", "frequency", freqMinutes, 2);
    // wifiManager.resetSettings();
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&customIp);
    wifiManager.addParameter(&customPort);
    wifiManager.addParameter(&customFreq);
    wifiManager.autoConnect("UvSensor", "12345678");

    strcpy(server, customIp.getValue());
    strcpy(port, customPort.getValue());
    strcpy(freqMinutes, customFreq.getValue());

    if (shouldSaveConfig)
    {
        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.createObject();
        json["server"] = server;
        json["port"] = port;
        json["freqMinutes"] = freqMinutes;

        File configFile = SPIFFS.open("/config.json", "w");

        json.printTo(Serial);
        json.printTo(configFile);
        configFile.close();
        //end save
    }
}

void setup()
{
    SPIFFS.begin();
    pinMode(A0, INPUT);
    pinMode(D3, INPUT_PULLUP);
    Serial.begin(9600);
    Serial.println("I'm awake.");
    setupWifiManager();
    EEPROM.begin(512);
    Serial.println("I have connection to wifi.");
    getUrl();
    getToken();
    Serial.println(token);
    sendData();
    int minute = atoi(freqMinutes);
    Serial.println("Going into deep sleep for " + String(minute) + " minutes");
    if (digitalRead(D3) == 0)
    {
        Serial.println("");
        Serial.println("Im high!");
        wifiManager.resetSettings();
    }
    ESP.deepSleep(minute * (60e6));
}

void loop()
{
}

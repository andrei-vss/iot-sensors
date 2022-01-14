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
int gpio_12_relay = 12;
char ip[40];
char port[6] = "8000";
bool shouldSaveConfig = false;
WiFiManager wifiManager;
int SONOFF_BUTTON = 0;


void setupWifiManager()
{
    WiFiManagerParameter customIp("ip", "ip", ip, 40);
    WiFiManagerParameter customPort("port", "port", port, 6);
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&customIp);
    wifiManager.addParameter(&customPort);
    wifiManager.autoConnect("CurentSensor", "12345678");

    strcpy(ip, customIp.getValue());
    strcpy(port, customPort.getValue());

    if (shouldSaveConfig)
    {
        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.createObject();
        json["ip"] = ip;
        json["port"] = port;

        File configFile = SPIFFS.open("/configData.json", "w");

        json.printTo(Serial);
        json.printTo(configFile);
        configFile.close();
        //end save
    }
}

void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

void getUrl()
{
    Serial.println("mounted file system");
    if (SPIFFS.exists("/configData.json"))
    {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/configData.json", "r");
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
                strcpy(ip, json["ip"]);
                strcpy(port, json["port"]);
            }
            else
            {
                Serial.println("failed to load json config");
            }
            configFile.close();
        }
    }
}

void startCurrent()
{
    Serial.println("start!");
    digitalWrite(gpio_12_relay, HIGH);
    sendData(true);
    delay(1000);
}

void stopCurrent()
{
    Serial.println("stop!");
    digitalWrite(gpio_12_relay, LOW);
    sendData(false);
    delay(1000);
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
        http.begin("http://" + String(ip) + ":" + String(port) + "/api/getToken");
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

void checkData()
{
    StaticJsonBuffer<200> jsonBuffer;
    HTTPClient http;
    http.begin("http://" + String(ip) + ":" + String(port) + "/api/get-data");
    http.addHeader("Content-Type", "application/json"); //Specify content-type header
    http.addHeader("jwt", token);
    http.addHeader("Content-Length", "0");
    int httpCode1 = http.POST("");
    String payload = http.getString(); //Get the response payload
    Serial.println(payload);
    Serial.println(httpCode1);
    JsonObject &root = jsonBuffer.parseObject(payload);
    http.end();
    if (!root.success())
    {
        Serial.println("root is empty!");
        return;
    }
    else
    {
        boolean started = root["started"];
        if (started)
        {
            startCurrent();
        }
        else
        {
            stopCurrent();
        }
    }
}

void resetData(){
  wifiManager.resetSettings();
  delay(5000);
  ESP.restart();
}

void setup()
{
    Serial.begin(9600);
    Serial.println("I'm awake.");
    pinMode(SONOFF_BUTTON, INPUT);
    attachInterrupt(SONOFF_BUTTON, resetData, HIGH);
    SPIFFS.begin();
    delay(3000);
    setupWifiManager();
    getUrl();
    EEPROM.begin(512);
    getToken();
    pinMode(gpio_12_relay, OUTPUT);
    stopCurrent();
}

void sendData(boolean active)
{
    String value = "true";
    if (!active)
    {
        value = "false";
    }
    String jsonResult = "{ \n \"active\" : " + value + " \n}";
    Serial.println(jsonResult);
    HTTPClient http;
    http.begin("http://" + String(ip) + ":" + String(port) + "/api/save-data");
    http.addHeader("Content-Type", "application/json"); //Specify content-type header
    http.addHeader("jwt", token);
    int httpCode1 = http.POST(jsonResult);
    String payload = http.getString(); //Get the response payload

    Serial.println(httpCode1); //Print HTTP return code
    Serial.println(payload);   //Print request response payload
    http.end();                //Close connection
}

void loop()
{
    checkData();
    delay(10000);
}

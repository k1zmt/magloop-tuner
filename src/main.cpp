#include <Arduino.h>


#include <SPI.h>
#include <TFT_eSPI.h>
#ifdef ESP32
#include <SPIFFS.h>
#endif
#include <WiFi.h>
#include <Wire.h>
#include <Button2.h>
#include <TMC2209.h>

#include <EEPROM.h>
#include <ESP32Encoder.h>
#include <ArduinoOTA.h>

#include <WebServer.h>
#include <WebSocketsServer.h>

#include <ArduinoJson.h>

/*******************************************************/
// To provide wifi password and SSID 
// see https://docs.platformio.org/en/latest/projectconf/interpolation.html



#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

/*******************************************************/

WebServer httpServer(80);
WebSocketsServer wsServer(8080);

ESP32Encoder encoder;
TMC2209 stepper_driver;

#define STEN_PIN 17

#define STEN_DELAY 50
#define STEPS 2



#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define ROTARY_PIN      25

#define BUTTON_0        0
#define BUTTON_1        35


const long SERIAL_BAUD_RATE = 500000;
const int32_t RUN_VELOCITY = 35000;
const int32_t STOP_VELOCITY = 0;
const int RUN_DURATION = 2000;
const int STOP_DURATION = 1000;
const uint8_t RUN_CURRENT_PERCENT = 100;

int32_t velocity = 35000;

#define EEPROM_SIZE 512

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

Button2 botBtn(BUTTON_0, INPUT_PULLUP);

Button2 topBtn(BUTTON_1);

Button2 rotaryBtn(ROTARY_PIN, INPUT_PULLUP, true);

#define CURRENT_FREQ_ADDR sizeof(float_t)
#define K_ADDR 0

int32_t current_frequency = 3500000;
int32_t desired_frequency = 3500000;
uint32_t position = 0;
char buf[32];


#define CAL_POINTS 2
int32_t c_points[CAL_POINTS] = {0, 0};

float_t k = 80.0;

HardwareSerial & serial2_stream = Serial2;

int16_t gs = 16;

unsigned long ts;
TMC2209::Status sDrvStatus;

// Memory map:
// 0..3: k
// 4..7: current_frequency

bool invert_direction = false;

String getMimeType(const String& p)
{
    if (p.endsWith(".js")) {
        return "application/javascript";
    }
    if (p.endsWith(".gz")) {
        return "application/x-gzip";
    }
    if (p.endsWith(".ico")) {
        return "image/x-icon";
    }
    if (p.endsWith(".htm") || p.endsWith(".html")) {
        return "text/html";
    }
    if (p.endsWith(".gif")) {
        return "image/gif";
    }
    if (p.endsWith(".jpg") || p.endsWith(".jpeg")) {
        return "image/jpeg";
    }
    if (p.endsWith(".css")) {
        return "text/css";
    }
    return "text/plain";
}

void handleNotFound()
{
    auto path = httpServer.uri();
    Serial.println(path);

    if (SPIFFS.exists(path)) {
        fs::File file = SPIFFS.open(path, "r");
        size_t sent = httpServer.streamFile(file, getMimeType(path));
        file.close();
        return;
    }
    httpServer.send(404, "text/plain", "404: File Not Found");
}

void draw_frequency() {

    if (current_frequency == desired_frequency) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        sprintf(buf, "Curr %10d MHz", current_frequency);
        tft.drawString(buf, tft.width() / 2, gs * 2);

    } else {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        sprintf(buf, "Des: %10d MHz", desired_frequency);
        tft.drawString(buf, tft.width() / 2, gs * 2);
    }

    sprintf(buf, "3.8:%10d 4.0:%10d", c_points[0], c_points[1]);
    tft.drawString(buf, tft.width() / 2, gs * 4);

    sprintf(buf, "K= %10.6f", k);
    tft.drawString(buf, tft.width() / 2, gs * 5);
    tft.drawString(WiFi.localIP().toString(),tft.width() / 2, gs * 6);
}


void step_up(int steps) {
    digitalWrite(STEN_PIN, HIGH);
    delay(STEN_DELAY);

    delay(STEN_DELAY);
    digitalWrite(STEN_PIN, LOW);
}


void moveStepper(int d) {
    if (stepper_driver.isSetupAndCommunicating()) {
        if (d > 0) {
            if (stepper_driver.getSettings().inverse_motor_direction_enabled) {
                stepper_driver.disableInverseMotorDirection();
            }
            stepper_driver.moveAtVelocity(velocity);
        } else if (d < 0) {
            if (!stepper_driver.getSettings().inverse_motor_direction_enabled) {
                stepper_driver.enableInverseMotorDirection();
            }
            stepper_driver.moveAtVelocity(velocity);
        }
    }
}

/**
 * Handles incoming webSocket messages
 * @param num
 * @param type
 * @param payload
 * @param length
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
{
    switch (type) {
        case WStype_DISCONNECTED:
        case WStype_ERROR:
            stepper_driver.moveAtVelocity(STOP_VELOCITY);
            break;
        case WStype_CONNECTED:
            //
            break;
        case WStype_TEXT:
            //
            const int capacity = JSON_OBJECT_SIZE(3);
            StaticJsonDocument<capacity> doc;
            DeserializationError err = deserializeJson(doc, payload);
            if (err) {
                Serial.println(err.f_str());
                return;
            }
            boolean pressed = doc["p"].as<boolean>();
            int direction = doc["d"].as<int>();
            velocity = doc["s"].as<int32_t>();
            if (pressed) {
                moveStepper(direction);
            } else {
                stepper_driver.moveAtVelocity(STOP_VELOCITY);
            }
            break;
    }
}


void topBtnPressed(Button2 btn) {
    moveStepper(1);
}

void botBtnPressed(Button2 btn) {
    moveStepper(-1);
}
void topBtnReleased(Button2 btn) {
    stepper_driver.moveAtVelocity(0);
}


void initStepperDriver() {
    if (not stepper_driver.isSetupAndCommunicating())  {
        Serial.println("Stepper driver is not setup and communicating!");
        delay(STOP_DURATION);
        stepper_driver.setup(serial2_stream, SERIAL_BAUD_RATE);
        stepper_driver.setRunCurrent(RUN_CURRENT_PERCENT);
        stepper_driver.enableAutomaticGradientAdaptation();
        stepper_driver.enableCoolStep();
        stepper_driver.enable();
        stepper_driver.setStandstillMode(TMC2209::FREEWHEELING);
        stepper_driver.moveAtVelocity(STOP_VELOCITY);
        return;
    }
}

void setup() {

    ts = millis();
    tft.init();
    tft.setRotation(1);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("Warming up...", tft.width() / 2, tft.height() / 2);

    pinMode(STEN_PIN, OUTPUT);
    digitalWrite(STEN_PIN, LOW);

    Serial.begin(115200);
    Serial.println("Start");
    Serial.println(
        EEPROM.begin(512)
    );


    delay(1);

    ESP32Encoder::useInternalWeakPullResistors=UP;
    encoder.attachSingleEdge(26, 27);
    encoder.setCount(0);

    Serial.printf("Connecting to %s ", WIFI_SSID);

    k = EEPROM.readFloat(K_ADDR);
    current_frequency = EEPROM.readInt(CURRENT_FREQ_ADDR);
    if (current_frequency < 3000000 || current_frequency > 30000000) {
        current_frequency = 3000000;
        desired_frequency = 3000000;
    }


    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }

    Serial.println(" CONNECTED");
    tft.fillScreen(TFT_BLACK);


    rotaryBtn.setClickHandler([](Button2 & b) {
        step_up(
                (((float_t)desired_frequency - (float_t)current_frequency) / k)
        );
        current_frequency = desired_frequency;
        EEPROM.writeInt(CURRENT_FREQ_ADDR, current_frequency);
        EEPROM.commit();
        draw_frequency();
    });

    draw_frequency();

    digitalWrite(STEN_PIN, LOW);


    ArduinoOTA.setHostname("magloop");

    ArduinoOTA
            .onStart([]() {
                String type;
                if (ArduinoOTA.getCommand() == U_FLASH)
                    type = "sketch";
                else // U_SPIFFS
                    type = "filesystem";

                // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                Serial.println("Start updating " + type);
            })
            .onEnd([]() {
                Serial.println("\nEnd");
            })
            .onProgress([](unsigned int progress, unsigned int total) {
                Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
            })
            .onError([](ota_error_t error) {
                Serial.printf("Error[%u]: ", error);
                if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                else if (error == OTA_END_ERROR) Serial.println("End Failed");
            });

    ArduinoOTA.begin();

    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    httpServer.on("/", HTTP_GET, []() {
        fs::File file = SPIFFS.open("/index.html", "r");
        httpServer.streamFile(file, "text/html");
        file.close();
    });

    httpServer.onNotFound(handleNotFound);

    Serial2.begin(SERIAL_BAUD_RATE);
    stepper_driver.setup(serial2_stream, SERIAL_BAUD_RATE);
    stepper_driver.setup(serial2_stream, SERIAL_BAUD_RATE, TMC2209::SERIAL_ADDRESS_2);

    httpServer.begin();
    wsServer.begin();
    wsServer.onEvent(webSocketEvent);


    initStepperDriver();

    topBtn.setPressedHandler(topBtnPressed);
    topBtn.setReleasedHandler(topBtnReleased);
    botBtn.setPressedHandler(botBtnPressed);
    botBtn.setReleasedHandler(topBtnReleased);


}


void loop() {
    ArduinoOTA.handle();

    wsServer.loop();

    topBtn.loop();
    botBtn.loop();

    httpServer.handleClient();

    initStepperDriver();

    ulong nts = millis();
    bool driverOk = stepper_driver.isSetupAndCommunicating();
    bool driverStatusHasChanged = false;
    if (driverOk) {
        TMC2209::Status newDriverStatus = stepper_driver.getStatus();
        driverStatusHasChanged = (memcmp(&sDrvStatus, &newDriverStatus, sizeof(TMC2209::Status)) != 0);
        sDrvStatus = newDriverStatus;
    }
    if (nts-ts > 1000 || driverStatusHasChanged) {
        StaticJsonDocument<384> doc;
        doc["drv"] = driverOk;
        doc["s"] = velocity;
        doc["d"] = stepper_driver.getSettings().inverse_motor_direction_enabled ? -1 : 1;
        if (doc["drv"]) {
            doc["otw"] = sDrvStatus.over_temperature_warning;
            doc["ots"] = sDrvStatus.over_temperature_shutdown;
            doc["stga"] = sDrvStatus.short_to_ground_a;
            doc["stgb"] = sDrvStatus.short_to_ground_b;
            doc["lssa"] = sDrvStatus.low_side_short_a;
            doc["lssb"] = sDrvStatus.low_side_short_b;
            doc["ola"] = sDrvStatus.open_load_a;
            doc["olb"] = sDrvStatus.open_load_b;
            doc["ot120c"] = sDrvStatus.over_temperature_120c;
            doc["ot143c"] = sDrvStatus.over_temperature_143c;
            doc["ot150c"] = sDrvStatus.over_temperature_150c;
            doc["ot157c"] = sDrvStatus.over_temperature_157c;
            doc["cs"] = sDrvStatus.current_scaling;
            doc["scm"] = sDrvStatus.stealth_chop_mode;
            doc["ss"] = sDrvStatus.standstill;
        }
        char b[256]={""};
        if (auto len = serializeJson(doc, b)) {
            wsServer.broadcastTXT(b, len);

        }
        ts = nts;
    }



}
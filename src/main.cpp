#include <Arduino.h>
#include <DHT.h> // Digital relative humidity & temperature sensor AM2302/DHT22
#include "arduino_secrets.h"

#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
//#include <WiFiClientSecure.h>
//#include <DNSServer.h>
//#include <ESP8266HTTPClient.h>
//#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "DefinePin8266.h"
#endif

#ifdef ARDUINO_ESP32_DEV
#include <WiFi.h>
#include <WiFiClient.h>
#include "DefinePin32.h"
#endif

#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <EspMQTTClient.h>
#include <Wire.h> // Library for I2C communication
#include <SH1106Wire.h>
#include <ArduinoJson.h>
#include <CircularBuffer.h>
#include "SlopeTracker.h"
#include "iconset_16x12.xbm"
#include <ezTime.h>
#include <Ticker.h>

#if (defined(OTA_HOST_ESP32) && defined(ARDUINO_ESP32_DEV))
#include <HTTPClient.h>
HTTPClient client_http;
#endif

SH1106Wire display(0x3c, SDA, SCL); // ADDRESS, SDA, SCL

WiFiManager wifiManager;

#define FIRMWARE_VER "1.2.2" // No Blynk
#define DHTTYPE DHT22        // DHT 22 (AM2302)

DHT dht(DHTPIN, DHTTYPE);

EspMQTTClient client_mqtt(
    NULL,
    NULL,
    MQTT_BROKER,    // MQTT Broker server ip
    MQTT_USER,      // Can be omitted if not needed
    MQTT_PASS,      // Can be omitted if not needed
    THIS_DEVICE_LOC, // Client name that uniquely identify your device
    1883            // The MQTT port, default to 1883. this line can be omitted
);

float temp_realtime, rh_realtime, rh_1s_mva, t_1s_mva,
    temp_outside = 0;

const unsigned long mqtt_timer_int = 60000L;
const uint16_t avg_sample_time = 250, disp_timer_int = 1000;

void checkFirmware();
void updateFirmware(uint8_t *data, size_t len);

void read_sensor();
void display_value();
void send_dht_mqtt();

Ticker timer_measure(read_sensor, avg_sample_time);
Ticker timer_display(display_value, disp_timer_int);
Ticker timer_mqtt(send_dht_mqtt, mqtt_timer_int);

SlopeTracker t_short_buffer(4, avg_sample_time / 60000.0);
SlopeTracker rh_short_buffer(4, avg_sample_time / 60000.0);
const uint8_t x_1col = 14, x_2col = 68, x_3col = 92, y_0row = 1, y_1row = 20, y_2row = 44, left_margin = 4;

int totalLength; // total size of firmware

Timezone myTZ;
// Provide official timezone names
// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones

// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.

void onConnectionEstablished()
{
  String topic = MQTT_TOPIC_PREFIX;
  topic += THIS_DEVICE_LOC;
  // topic += WiFi.getHostname();
  topic += "/misc";

  DynamicJsonDocument misc_json(96);
  misc_json["ip"] = WiFi.localIP().toString();
  misc_json["hostname"] = WiFi.getHostname();
  misc_json["location"] = THIS_DEVICE_LOC;
  misc_json["version"] = FIRMWARE_VER;
  char json_out[96];
  int b = serializeJson(misc_json, json_out);
  client_mqtt.publish(topic, json_out);

  String topic_outside = MQTT_TOPIC_PREFIX;
  topic_outside += "outside";
  client_mqtt.subscribe(topic_outside, [](const String &payload)
                        {
                     StaticJsonDocument<100> doc;
                     char json[100];
                     payload.toCharArray(json, payload.length() + 1);
                     deserializeJson(doc, json);
                     const int cursor_indent = 10;
                     // Serial.println(payload);
                     temp_outside = doc["temp"]; });
}

void setup()
{
  // Debug console
  Serial.begin(9600);

  display.init();
  display.flipScreenVertically();
  display.clear();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(left_margin, 6, "DHT MQTT Station");
  display.drawString(left_margin, 18, THIS_DEVICE_LOC);
  display.drawString(left_margin, 32, FIRMWARE_VER);
  display.drawString(left_margin, 46, "Starting...");
  display.display();

  // connect_to_wifi();
  wifiManager.setTimeout(180);
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
#ifdef ARDUINO_ESP32_DEV
  wifiManager.setConfigPortalBlocking(false);
#endif
  wifiManager.autoConnect("AutoConnectAP");
  delay(2000);

  dht.begin();

  // Optionnal functionnalities of EspMQTTClient :
  client_mqtt.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client_mqtt.enableHTTPWebUpdater();    // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").

  waitForSync();
  myTZ.setLocation(F("America/Vancouver"));
  timer_measure.start();
  timer_display.start();
  timer_mqtt.start();
#if (defined(OTA_HOST_ESP32) && defined(ARDUINO_ESP32_DEV))
  if (WiFi.status() == WL_CONNECTED)
  {
    checkFirmware();
  }
#endif
}

void loop()
{
  client_mqtt.loop();
  timer_measure.update();
  timer_display.update();
  timer_mqtt.update();
}

void draw_background()
{
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(x_1col, y_1row, "T");
  display.drawString(x_1col, y_2row, "RH");
  display.drawString(128, y_2row, "Outside");
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(x_2col, y_1row, "'C");
  display.drawString(x_2col, y_2row, "%");
}

void displayTime()
{
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(1, 0, myTZ.dateTime("l"));
  display.drawString(1, 9, myTZ.dateTime("m/d H:i"));
}

void read_sensor()
{
  rh_realtime = dht.readHumidity();
  temp_realtime = dht.readTemperature();
  rh_short_buffer.addPoint(rh_realtime);
  t_short_buffer.addPoint(temp_realtime);
  if (rh_short_buffer.ready())
  {
    rh_1s_mva = rh_short_buffer.getAvg();
  }
  else
  {
    rh_1s_mva = rh_realtime;
  }
  if (t_short_buffer.ready())
  {
    t_1s_mva = t_short_buffer.getAvg();
  }
  else
  {
    t_1s_mva = temp_realtime;
  }
}

void send_dht_mqtt()
{
  String tele_topic = MQTT_TOPIC_PREFIX;
  tele_topic += THIS_DEVICE_LOC;
  tele_topic += MQTT_TOPIC_SUFFIX;

  DynamicJsonDocument tele_json(64);
  tele_json["t"] = t_1s_mva;
  tele_json["h"] = rh_1s_mva;
  char json_out[64];
  int b = serializeJson(tele_json, json_out);

  client_mqtt.publish(tele_topic, json_out);
}

void display_value()
{
  display.clear();
  draw_background();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(x_2col - 2, y_1row - 2, String(t_1s_mva, 1));
  display.drawString(x_2col - 2, y_2row - 2, String(rh_1s_mva, 1));
  display.drawString(128, y_1row - 2, String(temp_outside, 1));
  displayTime();
  // display.drawLine(74, y_0row + 13, 128, y_0row + 13);

  if ((WiFi.status() == WL_CONNECTED))
  {
    display.setFont(ArialMT_Plain_10);
    // display.drawString(x_2col, y_3row, "WiFi");
    display.drawXbm(112, y_0row, Iot_Icon_width, Iot_Icon_height, wifi1_icon16x12);
  }
  if (client_mqtt.isConnected())
  {
    display.drawXbm(94, y_0row, Iot_Icon_width, Iot_Icon_height, mqtt_icon16x12);
  }
  display.display();
}

#include <Arduino.h>
#include <DHT.h> // Digital relative humidity & temperature sensor AM2302/DHT22
#include "arduino_secrets.h"

#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
//#include <WiFiClientSecure.h>
//#include <DNSServer.h>
//#include <ESP8266HTTPClient.h>
//#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "DefinePin8266.h"
#endif

#ifdef ARDUINO_ESP32_DEV
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
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

char auth[] = BLYNK_AUTH_TOKEN;

#define DHTTYPE DHT22 // DHT 22 (AM2302)

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

EspMQTTClient client_mqtt(
    NULL,
    NULL,
    MQTT_BROKER,    // MQTT Broker server ip
    MQTT_USER,      // Can be omitted if not needed
    MQTT_PASS,      // Can be omitted if not needed
    THIS_DEVICE_ID, // Client name that uniquely identify your device
    1883            // The MQTT port, default to 1883. this line can be omitted
);

float temp_realtime, rh_realtime, rh_1s_mva, t_1s_mva;

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
uint8_t x_1col = 28, x_2col = 96, y_0row = 1, y_1row = 20, y_2row = 44;

int totalLength; // total size of firmware

Timezone myTZ;
// Provide official timezone names
// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones

// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
void sendSensor()
{
  Blynk.virtualWrite(V5, rh_1s_mva);
  Blynk.virtualWrite(V6, t_1s_mva);
}

void onConnectionEstablished()
{
  String topic = "dt/misc/";
  topic += THIS_DEVICE_ID;
  topic += "/ip";
  client_mqtt.publish(topic, WiFi.localIP().toString());
}

void setup()
{
  // Debug console
  Serial.begin(9600);
  // connect_to_wifi();
  wifiManager.setTimeout(180);
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
#ifdef ARDUINO_ESP32_DEV
  wifiManager.setConfigPortalBlocking(false);
#endif
  wifiManager.autoConnect("AutoConnectAP");
  delay(2000);

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("connected...yeey :)");
    Blynk.config(auth, IPAddress(64, 225, 16, 22), 8080);
    // You can also specify server:
    // Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 80);
  }
  display.init();
  display.flipScreenVertically();
  display.clear();
  dht.begin();

  // Setup a function to be called every second
  timer.setInterval(mqtt_timer_int, sendSensor);

  // Optionnal functionnalities of EspMQTTClient :
  client_mqtt.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client_mqtt.enableHTTPWebUpdater();    // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
  client_mqtt.enableLastWillMessage("TestClient/lastwill", "I am going offline");

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
  Blynk.run();
  timer.run();
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
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(x_2col, y_1row, "'C");
  display.drawString(x_2col, y_2row, "%");
}

void displayTime()
{
  display.setFont(ArialMT_Plain_10);
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
  String telemetry_json = "{\"t\":";
  telemetry_json += String(t_1s_mva, 1);
  telemetry_json += ",\"h\":";
  telemetry_json += String(rh_1s_mva, 1);
  telemetry_json += "}";
  String tele_topic = MQTT_TOPIC_PREFIX;
  tele_topic += THIS_DEVICE_ID;
  tele_topic += MQTT_TOPIC_SUFFIX;
  client_mqtt.publish(tele_topic, telemetry_json);
}

void display_value()
{
  uint16_t xc = x_1col + 14;
  display.clear();
  draw_background();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(xc, y_1row - 2, String(t_1s_mva, 1));
  display.drawString(xc, y_2row - 2, String(rh_1s_mva, 1));
  displayTime();
  display.drawLine(74, y_0row + 13, 128, y_0row + 13);

  if ((WiFi.status() == WL_CONNECTED))
  {
    display.setFont(ArialMT_Plain_10);
    // display.drawString(x_2col, y_3row, "WiFi");
    display.drawXbm(112, y_0row, Iot_Icon_width, Iot_Icon_height, wifi1_icon16x12);
  }
  if (Blynk.CONNECTED)
  {
    display.drawXbm(94, y_0row, Iot_Icon_width, Iot_Icon_height, blynk_icon16x12);
  }
  if (client_mqtt.isConnected())
  {
    display.drawXbm(76, y_0row, Iot_Icon_width, Iot_Icon_height, mqtt_icon16x12);
  }
  display.display();
}

#if (defined(OTA_HOST_ESP32) && defined(ARDUINO_ESP32_DEV))
// https://github.com/kurimawxx00/webota-esp32/blob/main/WebOTA.ino
void checkFirmware()
{
  client_http.begin(OTA_HOST_ESP32);
  // Get file, just to check if each reachable
  int resp = client_http.GET();
  Serial.print("Response: ");
  Serial.println(resp);
  // If file is reachable, start downloading
  if (resp > 0 && resp != 403) // 403 error
  {
    // get length of document (is -1 when Server sends no Content-Length header)
    totalLength = client_http.getSize();
    // transfer to local variable
    int len = totalLength;
    // this is required to start firmware update process
    Update.begin(UPDATE_SIZE_UNKNOWN);
    Serial.printf("FW Size: %u\n", totalLength);
    // create buffer for read
    uint8_t buff[128] = {0};
    // get tcp stream
    WiFiClient *stream = client_http.getStreamPtr();
    // read all data from server
    Serial.println("Updating firmware...");
    while (client_http.connected() && (len > 0 || len == -1))
    {
      // get available data size
      size_t size = stream->available();
      if (size)
      {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        // pass to function
        updateFirmware(buff, c);
        if (len > 0)
        {
          len -= c;
        }
      }
      delay(1);
    }
  }
  else
  {
    Serial.println("Cannot download firmware file");
  }
  client_http.end();
}

// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
void updateFirmware(uint8_t *data, size_t len)
{
  int currentLength = 0;
  Update.write(data, len);
  currentLength += len;
  // Print dots while waiting for update to finish
  Serial.print('.');
  // if current length of written firmware is not equal to total firmware size, repeat
  if (currentLength != totalLength)
    return;
  Update.end(true);
  Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", currentLength);
  // Restart ESP32 to see changes
  ESP.restart();
}
#endif

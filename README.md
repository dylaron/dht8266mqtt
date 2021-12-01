## What is it?
- A Clock that gets accurate time from the internet
- T & RH Display
- IoT Sensor sends the T & RH measurement to Blynk, and/or any MQTT broker of your choice

![Picture](pic/t_rh_station.jpg)

## Hardware
- ESP8266
- DHT22 Temperature & Relative Humidity Sensor
- 128x64 OLED Display with SH1102 controller and I2C Interface
- A mini breadboard

## configuration
Create `arduino_secrets.h` as below
```
#define BLYNK_TEMPLATE_ID "....."
#define BLYNK_DEVICE_NAME "....."
#define BLYNK_AUTH "..............."

#define MQTT_BROKER "**.**.**.**"
#define MQTT_USER "..."
#define MQTT_PASS "..."
#define MQTT_TOPIC_PREFIX "dt/..../"
#define MQTT_TOPIC_SUFFIX "/..."
```

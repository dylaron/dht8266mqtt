## What is it?
- A Clock that gets accurate time from the internet
- T & RH Display
- IoT Sensor sends the T & RH measurement to an MQTT broker
- Displaying outside temperature getting from MQTT

![Picture](pic/t_rh_station.jpg)


## Hardware
- ESP32 or ESP8266 microcontroller development board
- DHT22 Temperature & Relative Humidity Sensor
- 128x64 OLED Display with SH1106 controller and I2C Interface
- A mini breadboard

![Wiring](pic/wiring.jpg)

(Wemos D1 Mini as in the example)
- D1 - SCL of display
- D2 - SDA of display
- D4 - DHT signal
- 5V & GND ... obviously

## configuration
Create `arduino_secrets.h` as below
```
#define MQTT_BROKER "**.**.**.**"
#define MQTT_USER "..."
#define MQTT_PASS "..."
#define MQTT_TOPIC_PREFIX "dt/..../"
#define MQTT_TOPIC_SUFFIX "/..."

#define THIS_DEVICE_ID "ThisCoolESP"
```

 MQTT topic for outside temperature (weather): 'dt/...../outside'
 i.e.,
 `{"temp": 23.0}`
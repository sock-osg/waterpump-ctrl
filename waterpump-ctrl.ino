#include <EEPROM.h>
#include <TM1637Display.h>
#include <ButtonHandler.h>
#include <ESP8266WiFi.h>
#include <Espalexa.h>

#include "Credentials.h"

#define CLK               4   // D2
#define DIO               5   // D1
#define RELAY_OUT_PIN     12  // D6
#define BUTTON_PIN        13  // D7
#define WIFI_INDICATOR    14  // D5

#define DELAY            20  // Delay per loop in ms

bool wifi_connected = false;
bool wifi_connecting = false;
bool manual_control = false;

const unsigned int ONE_SECOND = 1000; // 1 second
const unsigned int TEN_SECONDS = 10 * ONE_SECOND; // 10 seconds
const unsigned int ONE_MINUTE = 60 * ONE_SECOND; // 1 minute

int counter_addr = 0;
byte minutes_left = 0;
byte prev_minutes_left = 0;

// time controls
unsigned long init_time;

const uint8_t SEG_ALEXA[] = {
	SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
	SEG_D | SEG_E | SEG_F,                          // L
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,          // E
	SEG_B | SEG_C | SEG_E | SEG_F | SEG_G           // X ..... A
};

TM1637Display display(CLK, DIO);
ButtonHandler btn_control(BUTTON_PIN);
WiFiClient espClient;
Espalexa alexa;

void print_and_update(int number) {
  EEPROM.write(counter_addr, number);
  EEPROM.commit();

  display.showNumberDec(number);
}

void waterPumpControlCallback(uint8_t brigth) {
  manual_control = false;
  if (brigth) { // if value then trun on
    digitalWrite(RELAY_OUT_PIN, HIGH);
    display.setSegments(SEG_ALEXA);
    Serial.println("turning ON water-pump");
  } else {
    minutes_left = 0x00;
    print_and_update(minutes_left);
    digitalWrite(RELAY_OUT_PIN, LOW);
    Serial.println("turning OFF water-pump");
  }
}

void wifi_connect() {
  if (!wifi_connecting) {
    wifi_connecting = true;
    Serial.print(" ===> Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    Serial.print(" -> .");
  } else {
    Serial.print(".");
  }

  delay(500);

  wifi_connected = WiFi.status() == WL_CONNECTED;
  if (wifi_connected) {
    wifi_connecting = false;

    Serial.println("");
    Serial.print(" -> WiFi connected\n    + IP assigned: ");
    Serial.print(WiFi.localIP());
    Serial.println("");
  }

  digitalWrite(WIFI_INDICATOR, wifi_connected);
}

void setup() {
  Serial.begin(115200);
  display.setBrightness(0x0f);

  EEPROM.begin(512);

  btn_control.init();
  pinMode(RELAY_OUT_PIN, OUTPUT);
  pinMode(WIFI_INDICATOR, OUTPUT);

  minutes_left = EEPROM.read(counter_addr);
  display.showNumberDec(minutes_left);

  if (minutes_left > 0) {
    manual_control = true;
    digitalWrite(RELAY_OUT_PIN, HIGH);
  } else {
    digitalWrite(RELAY_OUT_PIN, LOW);
  }

  wifi_connect();

  alexa.addDevice("Bomba de agua", waterPumpControlCallback);
  alexa.begin();
}

void loop() {
  wifi_connected = WiFi.status() == WL_CONNECTED;
  if (!wifi_connected) {
    wifi_connect();
  } else {
    alexa.loop();
  }
  
  int event = btn_control.handle();

  switch(event) {
    case EV_LONGPRESS: // Reset timmer, in consecuence stops water bomb
      manual_control = false;

      minutes_left = 0x00;
      print_and_update(minutes_left);
      digitalWrite(RELAY_OUT_PIN, LOW);

      break;
    case EV_SHORTPRESS: // Add x minutes
      manual_control = true;

      minutes_left += 0x02;
      
      print_and_update(minutes_left);

      digitalWrite(RELAY_OUT_PIN, HIGH);
      init_time = millis();
      break;
  }

  if (manual_control) {
    if (minutes_left == 0x00) {
      digitalWrite(RELAY_OUT_PIN, LOW);
      manual_control = false;
    }

    if ((millis() - init_time) >= ONE_MINUTE) {
      if (minutes_left > 0x00) {
        minutes_left--;
        init_time = millis();

        print_and_update(minutes_left);

        digitalWrite(RELAY_OUT_PIN, HIGH);
      }
    }
  }

  delay(DELAY);
}

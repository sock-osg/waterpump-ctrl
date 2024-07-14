#include <EEPROM.h>
#include <TM1637Display.h>
#include <ButtonHandler.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "Credentials.h"

const char* mqtt_server = "MQTT_SERVER";
const char* STATUS_TOPIC = "STATUS_TOPIC";
const char* TIMMER_TOPIC = "TIMMER_TOPIC";
const char* DEVICE_ID = "DEVICE_ID";

#define CLK               4   // D2
#define DIO               5   // D1
#define RELAY_OUT_PIN     12  // D6
#define BUTTON_PIN        13  // D7
#define WIFI_INDICATOR    14  // D5
#define MQTT_INDICATOR    15  // D8

#define DELAY            20  // Delay per loop in ms

bool wifi_connected = false;
bool mqtt_connected = false;
bool wifi_connecting = false;

const unsigned int ONE_SECOND = 1000; // 1 second
const unsigned int TEN_SECONDS = 10 * ONE_SECOND; // 10 seconds
const unsigned int ONE_MINUTE = 60 * ONE_SECOND; // 1 minute

int counter_addr = 0;
byte minutes_left = 0;
byte prev_minutes_left = 0;

// time controls
unsigned long init_time;

TM1637Display display(CLK, DIO);
ButtonHandler btn_control(BUTTON_PIN);
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void print_save_and_publish(int number) {
  EEPROM.write(counter_addr, number);
  EEPROM.commit();

  if (mqtt_connected) {
    mqtt_client.publish(STATUS_TOPIC, String(number).c_str());
  }

  display.showNumberDec(number);
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Make payload a string by NULL terminating it.
  int newMinutes = atoi((char *) payload);

  minutes_left = newMinutes;
  if (minutes_left > 0) {
    init_time = millis();
    digitalWrite(RELAY_OUT_PIN, HIGH);
  }

  print_save_and_publish(minutes_left);
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
  digitalWrite(MQTT_INDICATOR, mqtt_connected);
}

void mqtt_connect() {
  if (mqtt_connected) {
    digitalWrite(MQTT_INDICATOR, mqtt_connected);
    return;
  } else {
    Serial.println(" -> Attempting MQTT connection");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    if (mqtt_client.connect(DEVICE_ID)) {
    // if (mqtt_client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("    + connected");
      mqtt_client.subscribe(TIMMER_TOPIC);
      mqtt_connected = true;
      print_save_and_publish(minutes_left);
    } else {
      Serial.print("    + failed, rc=");
      Serial.println(mqtt_client.state());
      mqtt_connected = false;
    }
  }

  digitalWrite(MQTT_INDICATOR, mqtt_connected);
}

void setup() {
  Serial.begin(115200);
  display.setBrightness(0x0f);

  EEPROM.begin(512);

  btn_control.init();
  pinMode(RELAY_OUT_PIN, OUTPUT);
  pinMode(WIFI_INDICATOR, OUTPUT);
  pinMode(MQTT_INDICATOR, OUTPUT);

  minutes_left = EEPROM.read(counter_addr);
  display.showNumberDec(minutes_left);

  if (minutes_left > 0) {
    digitalWrite(RELAY_OUT_PIN, HIGH);
  }

  wifi_connect();

  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);
}

void loop() {
  wifi_connected = WiFi.status() == WL_CONNECTED;
  if (!wifi_connected) {
    wifi_connect();
  }

  mqtt_connected = mqtt_client.connected();
  if (wifi_connected && !mqtt_connected) {
    mqtt_connect();
  }

  int event = btn_control.handle();

  switch(event) {
    case EV_LONGPRESS: // Reset timmer, in consecuence stops water bomb
      minutes_left = 0x00;
      
      print_save_and_publish(minutes_left);

      break;
    case EV_SHORTPRESS: // Add x minutes
      minutes_left += 0x02;
      
      print_save_and_publish(minutes_left);

      digitalWrite(RELAY_OUT_PIN, HIGH);
      init_time = millis();
      break;
  }

  if (minutes_left == 0x00) {
    digitalWrite(RELAY_OUT_PIN, LOW);
  }

  if ((millis() - init_time) >= ONE_MINUTE) {
    if (minutes_left > 0x00) {
      minutes_left--;
      init_time = millis();

      print_save_and_publish(minutes_left);

      digitalWrite(RELAY_OUT_PIN, HIGH);
    }
  }

  // This should be called regularly to allow the client to process incoming messages and maintain its connection to the server.
  // Important to execute it at the end.
  mqtt_client.loop();
  delay(DELAY);
}

#include <EEPROM.h>
#include <TM1637Display.h>
#include <ButtonHandler.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* ssid = "YOUR_SSID";
const char* password = "WIFI_PASSWORD";
const char* mqtt_server = "MQTT_SERVER";
const char* STATUS_TOPIC = "STATUS_TOPIC";
const char* TIMMER_TOPIC = "TIMMER_TOPIC";
const char* DEVICE_ID = "DEVICE_ID";

#define BUTTON_PIN        13  // D7 - Button
#define RELAY_OUT_PIN     12  // D6 - To relay
#define CLK               4   // D2
#define DIO               5   // D1

#define DELAY            20  // Delay per loop in ms

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
  mqtt_client.publish(STATUS_TOPIC, String(number).c_str());
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

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    if (mqtt_client.connect(DEVICE_ID)) {
    // if (mqtt_client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      mqtt_client.subscribe(TIMMER_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  display.setBrightness(0x0f);

  EEPROM.begin(512);

  btn_control.init();
  pinMode(RELAY_OUT_PIN, OUTPUT);

  minutes_left = EEPROM.read(counter_addr);
  display.showNumberDec(minutes_left);

  if (minutes_left > 0) {
    digitalWrite(RELAY_OUT_PIN, HIGH);
  }

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
 
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
 
  // Print the IP address
  Serial.print("IP assigned: ");
  Serial.print(WiFi.localIP());

  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);
}

void loop() {
  if (!mqtt_client.connected()) {
    reconnect();
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

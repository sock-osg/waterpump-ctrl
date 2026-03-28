#include <EEPROM.h>
#include <TM1637Display.h>
#include <ButtonHandler.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "Credentials.h"

#define CLK               4   // D2
#define DIO               5   // D1
#define RELAY_OUT_PIN     12  // D6
#define BUTTON_PIN        13  // D7
#define WIFI_INDICATOR    14  // D5

#define DELAY            20  // Delay per loop in ms

bool wifi_connected = false;
bool wifi_connecting = false;

const unsigned int ONE_SECOND = 1000; // 1 second
const unsigned int ONE_MINUTE = 60 * ONE_SECOND; // 1 minute

int counter_addr = 0;
byte minutes_left = 0;
byte prev_minutes_left = 0;

// time controls
unsigned long init_time;
unsigned long lastClientUpdateTime = 0;
const unsigned long CLIENT_UPDATE_INTERVAL = 5000; // Update client every 5 seconds

TM1637Display display(CLK, DIO);
ButtonHandler btn_control(BUTTON_PIN);
ESP8266WebServer server(80);

void print_save_and_publish(int number) {
  EEPROM.write(counter_addr, number);
  EEPROM.commit();
  display.showNumberDec(number);
}

// HTML page with input field
String getHtmlPage() {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Timer Control</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }";
  html += ".timer { font-size: 150px; margin: 20px; padding: 20px; background: #f0f0f0; border-radius: 10px; }";
  html += "input { font-size: 24px; padding: 10px; margin: 10px; width: 150px; text-align: center; }";
  html += "button { font-size: 24px; margin: 10px; padding: 10px 20px; cursor: pointer; }";
  html += ".set-button { background-color: #4CAF50; color: white; border: none; border-radius: 5px; }";
  html += ".reset-button { background-color: #f44336; color: white; border: none; border-radius: 5px; }";
  html += "button:hover { opacity: 0.8; }";
  html += ".status { margin: 20px; padding: 10px; border-radius: 5px; display: none; }";
  html += ".status.success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".status.error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<h1>Timer Controller</h1>";
  html += "<div class='timer'><span id='time'>" + String(minutes_left) + "</span></div><p>Minutes Remaining</p>";
  html += "<div>";
  html += "<input type='number' id='minutesInput' min='0' max='255' placeholder='Enter minutes'>";
  html += "<button class='set-button' onclick='setTime()'>Set Timer</button>";
  html += "</div>";
  html += "<div>";
  html += "<button class='reset-button' onclick='resetTime()'>Reset Timer</button>";
  html += "</div>";
  html += "<div id='status' class='status'></div>";
  html += "<script>";
  html += "function showStatus(message, isSuccess) {";
  html += "  var statusDiv = document.getElementById('status');";
  html += "  statusDiv.textContent = message;";
  html += "  statusDiv.className = 'status ' + (isSuccess ? 'success' : 'error');";
  html += "  statusDiv.style.display = 'block';";
  html += "  setTimeout(function() {";
  html += "    statusDiv.style.display = 'none';";
  html += "  }, 3000);";
  html += "}";
  html += "";
  html += "function setTime() {";
  html += "  var minutes = document.getElementById('minutesInput').value;";
  html += "  if (minutes === '') {";
  html += "    showStatus('Please enter a value', false);";
  html += "    return;";
  html += "  }";
  html += "  fetch('/setTime?minutes=' + minutes)";
  html += "    .then(response => {";
  html += "      if (!response.ok) throw new Error('Network response was not ok');";
  html += "      return response.text();";
  html += "    })";
  html += "    .then(data => {";
  html += "      document.getElementById('time').innerText = data;";
  html += "      showStatus('Timer set to ' + data + ' minutes', true);";
  html += "      document.getElementById('minutesInput').value = '';";
  html += "    })";
  html += "    .catch(error => {";
  html += "      showStatus('Error setting timer: ' + error.message, false);";
  html += "    });";
  html += "}";
  html += "";
  html += "function resetTime() {";
  html += "  fetch('/reset')";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      document.getElementById('time').innerText = data;";
  html += "      showStatus('Timer reset to 0 minutes', true);";
  html += "    })";
  html += "    .catch(error => {";
  html += "      showStatus('Error resetting timer: ' + error.message, false);";
  html += "    });";
  html += "}";
  html += "";
  html += "setInterval(function() {";
  html += "  fetch('/getTime')";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      document.getElementById('time').innerText = data;";
  html += "    })";
  html += "    .catch(error => console.log('Error fetching time:', error));";
  html += "}, 5000);";
  html += "</script>";
  html += "</body>";
  html += "</html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", getHtmlPage());
}

void handleSetTime() {
  if (server.hasArg("minutes")) {
    int newMinutes = server.arg("minutes").toInt();
    
    // Validate input
    if (newMinutes < 0) newMinutes = 0;
    if (newMinutes > 255) newMinutes = 255;
    
    minutes_left = (byte)newMinutes;
    
    if (minutes_left > 0) {
      init_time = millis();
      digitalWrite(RELAY_OUT_PIN, HIGH);
    } else {
      digitalWrite(RELAY_OUT_PIN, LOW);
    }
    
    print_save_and_publish(minutes_left);
    
    // Return the new value to update the screen immediately
    server.send(200, "text/plain", String(minutes_left));
  } else {
    server.send(400, "text/plain", "Missing minutes parameter");
  }
}

void handleReset() {
  minutes_left = 0;
  digitalWrite(RELAY_OUT_PIN, LOW);
  print_save_and_publish(minutes_left);
  server.send(200, "text/plain", String(minutes_left));
}

void handleGetTime() {
  server.send(200, "text/plain", String(minutes_left));
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
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
    Serial.print("    + Web server available at: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
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
    digitalWrite(RELAY_OUT_PIN, HIGH);
  }

  wifi_connect();

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/setTime", handleSetTime);
  server.on("/reset", handleReset);
  server.on("/getTime", handleGetTime);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  wifi_connected = WiFi.status() == WL_CONNECTED;
  if (!wifi_connected) {
    wifi_connect();
  }

  int event = btn_control.handle();

  switch(event) {
    case EV_LONGPRESS: // Reset timer, consequently stops water pump
      minutes_left = 0x00;
      digitalWrite(RELAY_OUT_PIN, LOW);
      print_save_and_publish(minutes_left);
      break;
    case EV_SHORTPRESS: // Add 2 minutes
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

  // Handle web server requests
  server.handleClient();
  
  delay(DELAY);
}

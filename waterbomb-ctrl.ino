#include <EEPROM.h>
#include <TM1637Display.h>

#define BUTTON_PIN        2  // Button
#define RELAY_OUT_PIN     3  // To relay
#define CLK               4
#define DIO               5
#define DELAY            20  // Delay per loop in ms

const unsigned int ONE_SECOND = 1000; // 1 second
const unsigned int TEN_SECONDS = 10 * ONE_SECOND; // 10 seconds
const unsigned int ONE_MINUTE = 60 * ONE_SECOND; // 1 minute

int counter_addr = 0;
byte minutes_left = 0;

// Variables
boolean button_was_pressed; // previous state

// time controls
unsigned long init_time;

TM1637Display display(CLK, DIO);

boolean handle_button() {
  boolean event;
  int button_now_pressed = !digitalRead(BUTTON_PIN); // pin low -> pressed

  event = button_now_pressed && !button_was_pressed;
  button_was_pressed = button_now_pressed;
  return event;
}

void print_to_display(int number) {
  display.showNumberDec(number);
}

void setup() {
  display.setBrightness(0x0f);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(RELAY_OUT_PIN, OUTPUT);

  button_was_pressed = false;

  minutes_left = EEPROM.read(counter_addr);
  print_to_display(minutes_left);
  
  if (minutes_left > 0) {
    digitalWrite(RELAY_OUT_PIN, HIGH);
  }
  digitalWrite(BUTTON_PIN, HIGH);
}

void loop() {
  // handle button
  boolean raising_edge = handle_button();

  if (raising_edge) {
    minutes_left += 0x05;
    
    EEPROM.write(counter_addr, minutes_left);

    print_to_display(minutes_left);
    digitalWrite(RELAY_OUT_PIN, HIGH);
    init_time = millis();
  }
  if (minutes_left == 0x00 && digitalRead(RELAY_OUT_PIN)) {
    digitalWrite(RELAY_OUT_PIN, LOW);
  }

  if ((millis() - init_time) >= ONE_MINUTE) {
    if (minutes_left > 0x00) {
      minutes_left--;
      init_time = millis();

      EEPROM.write(counter_addr, minutes_left);

      print_to_display(minutes_left);
    }
  }

  delay(DELAY);
}


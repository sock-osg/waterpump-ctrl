#define DEFAULT_LONGPRESS_LEN    25  // Min nr of loops for a long press
#define DELAY                    20  // Delay per loop in ms

//////////////////////////////////////////////////////////////////////////////

enum { EV_NONE = 0, EV_SHORTPRESS, EV_LONGPRESS };
enum { SETUP_24_HOURS_MODE = 1, SETUP_HOUR, SETUP_MINUTES, SETUP_ALARM, SETUP_AL_HOUR, SETUP_AL_MINUTES, SETUP_SET_ALARM };

unsigned char setupCounter = 0;

//////////////////////////////////////////////////////////////////////////////
// Class definition

class ButtonHandler {

  public:
    // Constructor
    ButtonHandler(int pin, int longpress_len = DEFAULT_LONGPRESS_LEN);

    // Initialization done after construction, to permit static instances
    void init();

    // Handler, to be called in the loop()
    int handle();

  protected:
    boolean was_pressed;     // previous state
    int pressed_counter;     // press running duration
    const int pin;           // pin to which button is connected
    const int longpress_len; // longpress duration
};

ButtonHandler::ButtonHandler(int p, int lp) : pin(p), longpress_len(lp) { }

void ButtonHandler::init() {
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH); // pull-up
  was_pressed = false;
  pressed_counter = 0;
}

int ButtonHandler::handle() {
  int event;
  boolean now_pressed = digitalRead(pin);

  if (!now_pressed && was_pressed) {
    // handle release event
    event = pressed_counter < longpress_len ? EV_SHORTPRESS : EV_LONGPRESS;
  } else {
    event = EV_NONE;
  }

  // update press running duration
  pressed_counter = now_pressed ? pressed_counter + 1 : 0;

  // remember state, and we're done
  was_pressed = now_pressed;
  return event;
}

//////////////////////////////////////////////////////////////////////////////

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

// time controls
unsigned long init_time;

TM1637Display display(CLK, DIO);
ButtonHandler btn_control(BUTTON_PIN);

void print_to_display(int number) {
  display.showNumberDec(number);
}

void setup() {
  display.setBrightness(0x0f);

  btn_control.init();
  pinMode(RELAY_OUT_PIN, OUTPUT);

  minutes_left = EEPROM.read(counter_addr);
  print_to_display(minutes_left);
  
  if (minutes_left > 0) {
    digitalWrite(RELAY_OUT_PIN, HIGH);
  }
}

void loop() {
  // handle button
  int event = btn_control.handle();

  switch(event) {
    case EV_LONGPRESS:
      minutes_left = 0x00;
      
      EEPROM.write(counter_addr, minutes_left);
  
      print_to_display(minutes_left);
      break;
    case EV_SHORTPRESS:
      minutes_left += 0x02;
      
      EEPROM.write(counter_addr, minutes_left);
  
      print_to_display(minutes_left);
      digitalWrite(RELAY_OUT_PIN, HIGH);
      init_time = millis();
      break;
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

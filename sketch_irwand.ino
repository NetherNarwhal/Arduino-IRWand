/*
 * Program to detect and handle IR events produced by IR wands.
 * This setup requires 2 pins to be used. 1 for the IR sensor (RECV_PIN) and 1 for the status LED (LED_PIN).
 */
#include <IRremote.h>

// For easily adding or removing debug information. Comment out the debug define and it all gets removed from compiled code.
#define DEBUG
#ifdef DEBUG
  #define LOG(x) Serial.println(x)
#else
  #define LOG(x)
#endif

#define RECV_PIN 11 // Should have a IR sensor on this pin to read input.
#define LED_PIN 3   // Should have a LED on this pin for status output.

// #define RED_PIN 11 // This must be a PWM pin so it supports analog writes (3, 5, 6, 9, 10, 11).
// #define GREEN_PIN 10 // This must be a PWM pin so it supports analog writes (3, 5, 6, 9, 10, 11).
// #define BLUE_PIN 9 // This must be a PWM pin so it supports analog writes (3, 5, 6, 9, 10, 11).
// Uncomment this line if using a Common Anode LED, which I am.
// #define COMMON_ANODE

IRrecv irrecv(RECV_PIN);
decode_results results;

/*
 * Standard procedure. Called once during initialization.
 */
void setup() {
  Serial.begin(9600);
  LOG("Starting...");
  irrecv.enableIRIn(); // This will set pin mode for RECV_PIN among other things.
  pinMode(LED_PIN, OUTPUT); // Setup the LED pin.

  // Blink it once to show that we are ready.
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);

  // pinMode(RED_PIN, OUTPUT);
  // pinMode(GREEN_PIN, OUTPUT);
  // pinMode(BLUE_PIN, OUTPUT);

  LOG("Setup complete.");
}

/*
 * Standard procedure. After setup, this is called repeatedly as long as the arduino is running.
 */
void loop() {
  // Loop until we recieve an IR event.
  if (irrecv.decode(&results)) {

    // Validate this is actually a wand and not some other IR signal.
    if (results.decode_type == MAGIQUEST) {
      // Make sure the wand event is strong enough it is not triggered when folks are just walking with them.
      if (results.magiquestMagnitude >= 33000) {
        #ifdef DEBUG
        Serial.print("Wand id '");
        Serial.print(results.value, HEX);
        Serial.println("' detected.");
        #endif
        wandDetected(results.value);
      } else {
        LOG("Wand detected, but ignored due to low magnitude.");
      }
    } else {
      LOG("Unknown IR event that is being ignored.");
    }
    #ifdef DEBUG
    dump(&results);
    #endif

    // Clear this event so we are ready for the next one.
    irrecv.resume(); // Receive the next value
  }
}

/*
 * Dumps out the decode_results structure.
 */
#ifdef DEBUG
void dump(decode_results *results) {
  if (results->decode_type == MAGIQUEST) {
    Serial.print("Wand detected - magnitude=");
    Serial.print(results->magiquestMagnitude, DEC);
    Serial.print(", wand_id=");
    Serial.println(results->value, HEX);

  } else {
    Serial.print("Unknown encoding: ");
    Serial.print(" value=");
    Serial.print(results->value, HEX);
    Serial.print(" (");
    Serial.print(results->bits, DEC);
    Serial.println(" bits)");
    Serial.print("Raw (");
    int count = results->rawlen;
    Serial.print(count, DEC);
    Serial.print("): ");

    for (int i = 0; i < count; i++) {
      if ((i % 2) == 1) {
        Serial.print(results->rawbuf[i] * USECPERTICK, DEC);
      } else {
        Serial.print(-(long)results->rawbuf[i] * USECPERTICK, DEC);
      }
      Serial.print(" ");
    }
  }

  Serial.println("");
}
#endif

/*
 * Called when a wand event is detected and validated.
 */
void wandDetected(int32_t wandId) {
  // Toggle the LED.
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  // setColor(80, 0, 80); // purple (or do hex like #4B0082 = 0x4B, 0x0, 0x82).
}

/*
void setColor(int red, int green, int blue) {
  #ifdef COMMON_ANODE
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
  #endif
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);  
}
*/

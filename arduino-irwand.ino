/*
 * Program to detect and handle IR events produced by IR wands.
 * There are two possible configuration supported here (although you are free to add whatever you wish):
 * 1) Support for a single color LED (ex. red). This requires 2 pins: 1 for the IR sensor (RECV_PIN) and 1 for the status LED (LED_PIN).
 * 2) Support for a tri-color LED (any RTB color). This requires 4 pins: 1 for the IR sensor (RECV_PIN) and 3 for the status LED.
 */
#include <IRremote.h>

// For easily adding or removing debug information. Comment out the debug define and it all gets removed from compiled code.
#define DEBUG
#ifdef DEBUG
  #define LOG(x) Serial.println(x)
#else
  #define LOG(x)
#endif

const int RECV_PIN = 6; // Should have a IR sensor on this pin to read input. Must be a PWM pin to support analog reads (3,5,6,9,10,11).
// If you are using a single color LED (2 pins) instead of a tri-color LED (4 pins) for the status LED.
const bool SINGLE_LED = false;
const int LED_PIN = 11; // For single color LED (2 pins). Does not need to be a PWM pin.
const int RED_PIN = 11; // For tri-color LED (4 pins). Must be a PWM pin to support analog writes (3,5,6,9,10,11).
const int GREEN_PIN = 10; // For tri-color LED (4 pins). Must be a PWM pin to support analog writes (3,5,6,9,10,11).
const int BLUE_PIN = 9; // For tri-color LED (4 pins). Must be a PWM pin to support analog writes (3,5,6,9,10,11).
// If using tri-color LED, it is very important to comment this define out if you are using a common *CATHODE* (not *ANODE*) LED.
// Note: Cathodes connect their long pin to GND; Anodes connect theirs to power (5V).
#define COMMON_ANODE

IRrecv irrecv(RECV_PIN);
decode_results results;
bool triLit = false;
int32_t lastWand = 0;

/*
 * Standard procedure. Called once during initialization.
 */
void setup() {
  Serial.begin(9600);
  LOG("Starting...");
  irrecv.enableIRIn(); // This will set pin mode for RECV_PIN among other things.

  if (SINGLE_LED) {
    pinMode(LED_PIN, OUTPUT); // Setup the LED pin.
    // Blink it once to show that we are ready.
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
  } else {
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    // Cycle the colors once to show that we are ready.
    setColor(255, 0, 0);
    delay(200);
    setColor(0, 255, 0);
    delay(200);
    setColor(0, 0, 255);
    delay(200);
    setColor(0, 0, 0); // Turn off
    triLit = false;
  }

  LOG("Setup complete.");
}

/*
 * Standard procedure. After setup, this is called repeatedly as long as the arduino is running.
 */
void loop() {
  // Loop until we recieve an IR event.
  if (irrecv.decode(&results)) {

//    dump(&results);

    // Validate this is actually a wand and not some other IR signal.
    if (results.decode_type == MAGIQUEST) {
      // Make sure the wand event is strong enough it is not triggered when folks are just walking with them or
      // due to the rebound of the bearing. May need to adjust this based on your wand as some are better than others.
      if (results.magiquestMagnitude >= 33000) {
        #ifdef DEBUG
          Serial.print("Wand id '");
          Serial.print(results.value);
          Serial.print("' detected with a magnitude of ");
          Serial.println(results.magiquestMagnitude);
        #endif
        wandDetected(results.value);
      } else {
        #ifdef DEBUG
          Serial.print("Wand id '");
          Serial.print(results.value);
          Serial.print("' detected, but ignored due to a low magnitude of ");
          Serial.println(results.magiquestMagnitude);
        #endif
      }
    } else {
      #ifdef DEBUG
        LOG("Unknown IR event that is being ignored.");
        dump(&results);
      #endif
    }

    // Clear this event so we are ready for the next one.
    irrecv.resume(); // Receive the next value
  }
}

/*
 * Dumps out the decode_results structure.
 */
#ifdef DEBUG
void dump(decode_results *results) {
  Serial.print("Unknown encoding: ");
  Serial.print(" value=");
  Serial.print(results->value);
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

  Serial.println("");
}
#endif

/*
 * Called when a wand event is detected and validated.
 */
void wandDetected(int32_t wandId) {
  if (SINGLE_LED) {
    // Toggle the LED.
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  } else {
    // If this is a different wand than last time then show their color.
    if (lastWand != wandId) {
      triLit = true;
      lastWand = wandId;
      showWandColor(wandId);

    // If same wand, then just toggle their color on and off.
    } else if (triLit) {
      triLit = false;
      setColor(0, 0, 0);
    } else {
      triLit = true;
      showWandColor(wandId);
    }
  }
}

/*
 * Shows a color for the specified wand. Unknown wands will show white. Update with your wand ids for specific colors.
 * For tri-LED, our informal game is the winner is the person who has their color shown the longest without turning the LED off.
 */
void showWandColor(int32_t wandId) {
  if (wandId == 601111169) { // Ice Wand
      setColor(0, 255, 255); // aqua
  } else if (wandId == 275545345) { // Dark Brown Wand (#1)
      setColor(0, 255, 0); // green
  } else if (wandId == 275529089) { // Dark Brown Wand (#2)
      setColor(255, 255, 0); // yellow
  } else if (wandId == 461818113) { // Red Dragon Wand
      setColor(255, 0, 0); // red
  } else if (wandId == 462244353) { // Ice Dragon Wand
      setColor(0, 0, 255); // blue
  } else if (wandId == 276431617) { // Pink Wand
      setColor(255, 0, 255); // pink
  } else {
      setColor(255, 255, 255); // white
  }
}

/*
 * Used to set the color of a tri-color LED. May be called with base 10 (255, 0, 255) or hex (0x4B, 0x0, 0x82), which is #4B0082.
 */
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

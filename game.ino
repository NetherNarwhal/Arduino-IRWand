/*
 * Dueling game made for IR wands. There are two modes:
 * 1) Freestyle - This is the starting mode. Anyone may use their wand to light the indicator. If already lit with their color then it is unlit.
 * 2) King of the Wand - This mode is activated by pressing the button. The wand indicator will be cleared and the status LED will blink 3
 *    times. It will then remain on until someone is able to have the indicator show their color for a total of 5 seconds. Rules are similar
 *    to Freestyle in that anyone waving their wand will have their color shown (with duration until being replaced added to their counter).
 *    However, if they wave while their color is still shown the light will go off and they will lose 1 sec of accumulated time (no less than 0).
 *    Once someone accumulates 5 seconds, the red LED will go off and their color will be shown on the indicator. It goes back into Freestyle.
 */
#include <IRremote.h>

// For easily adding or removing debug information. Comment out the debug define and it all gets removed from compiled code.
#define DEBUG
#ifdef DEBUG
  #define LOG(x) Serial.println(x)
#else
  #define LOG(x)
#endif

// *** Wand Input (IR Sensor) ***
const int RECV_PIN = 6; // Should have a IR sensor on this pin to read input. Must be a PWM pin to support analog reads (3,5,6,9,10,11).

// *** Wand Output (RGB or Tri-Color LED) ***
const int RED_PIN = 11; // For RGB LED (4 pins). Must be a PWM pin to support analog writes (3,5,6,9,10,11).
const int GREEN_PIN = 10; // For RGB LED (4 pins). Must be a PWM pin to support analog writes (3,5,6,9,10,11).
const int BLUE_PIN = 9; // For RGB LED (4 pins). Must be a PWM pin to support analog writes (3,5,6,9,10,11).
// It is very important to comment this define out if you are using a common *CATHODE* (not *ANODE*) LED.
// Note: Cathodes connect their long pin to GND; Anodes connect theirs to power (5V).
#define COMMON_ANODE

// *** Status Output ***
const int LED_PIN = 12;

// *** Button Input ***
const int BTN_PIN = 13;

// Wand structure.
typedef struct {
  int32_t id; // Wand id returned from the IR sensor.
  byte r;     // Red component of the color to show for the wand.
  byte g;     // Green component of the color to show for the wand.
  byte b;     // Blue component of the color to show for the wand.
  unsigned long duration; // For wand game, how much time they have had the LED lit with their color.
} Wand;
// These are my wands and the colors I associated with them. Replace the ids with those of your wands and choose the colors (avoid white and black).
Wand wands[] = {
  { 276431617, 255,   0, 255, 0 }, // Pink Wand (pink)
  { 275529089, 255, 255,   0, 0 }, // Dark Brown Wand #1 (yellow)
  { 275545345,   0, 255,   0, 0 }, // Dark Brown Wand #2 (green)
  { 461818113, 255,   0,   0, 0 }, // Red Dragon Wand (red)
  { 462244353,   0,   0, 255, 0 }, // Ice Dragon Wand (blue)
  { 601111169,   0, 255, 255, 0 }  // Ice Wand (aqua)
};
// Cheat and create a macro with size, since it is static, so we don't need to lookup each time. 
#define NUM_WANDS (sizeof(wands) / sizeof(Wand))
// Define the id of the unknown wand so we can exclude it from things like winning the King of the Wand game.
#define UNKNOWN_WAND_ID 0
// Constant to indicate an unknown wand. The signal from the wand can be messy, especially if more than one wand is being used at a time.
Wand UNKNOWN_WAND = { UNKNOWN_WAND_ID, 255, 255, 255, 0 }; // Use white for color to indicate unknown.

IRrecv irrecv(RECV_PIN);
decode_results results;
Wand *lastWand = NULL; // This is the last wand that lit up the wand light.
bool wandLit = false; // If the wand light is currently on.
bool gameInProgress = false; // If King of the Wand game is currently running.
unsigned long startTime = millis(); // Keep track of the time so we add to the wand accumulate duration for King of the Wand's game.

#define TIME_PENALTY 1000 // They lose 1 sec (1000 milliseconds) if they turn off the light.
#define TIME_WIN 5000     // Wizard wins if they have kept the light lit for this duration (accumulated, not all at once).

/*
 * Standard procedure. Called once during initialization.
 */
void setup() {
  Serial.begin(9600);
  LOG("Setup");
  irrecv.enableIRIn(); // This will set pin mode for RECV_PIN among other things.

  // Setup the wand LED.
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
  clearWandLight();

  // Setup the button. The pin mode of INPUT_PULLUP means that the pin is to be used as an input,
  // but that if nothing else is connected to the input it should be 'pulled up' to HIGH. In other words,
  // the default value for the input is HIGH, unless it is pulled LOW by the action of pressing the button.
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Setup the game status LED.
  pinMode(LED_PIN, OUTPUT); // Setup the LED pin.
  // Blink it once to show that we are ready.
  blinkGameStatusLED(200, false);

  LOG("Setup done");
}

/*
 * Standard procedure. After setup, this is called repeatedly as long as the arduino is running.
 */
void loop() {
  // Keep track of the time for each loop. This allows us to accumulate that time for the folks playing the game.
  unsigned long deltaTime = millis() - startTime;
  startTime = millis();

  if (!gameInProgress) {
    // See if the button is pressed to start the game.
    if (digitalRead(BTN_PIN) == LOW) startGame();
  } else if (gameInProgress && lastWand != NULL && lastWand->id != UNKNOWN_WAND_ID) {
    // Increment accumulated time for the last wand and see if they have won.
    lastWand->duration += deltaTime;
    #ifdef DEBUG
      Serial.print(lastWand->id);
      Serial.print(" : ");
      Serial.println(lastWand->duration);
    #endif
    // See if they won the game.
    if (lastWand->duration >= TIME_WIN) endGame();
  }

  // Loop until we receive an IR event.
  if (irrecv.decode(&results)) {
    // Validate this is actually a wand and not some other IR signal.
    if (results.decode_type == MAGIQUEST) {
      // Make sure the wand event is strong enough it is not triggered when folks are just walking with them or
      // due to the rebound of the bearing. May need to adjust this based on your wand as some are better than others.
      if (results.magiquestMagnitude >= 33000) {
        #ifdef DEBUG
          Serial.print("Wand ");
          Serial.print(results.value);
          Serial.print(" detected. Magnitude=");
          Serial.println(results.magiquestMagnitude);
//          dump(&results);
        #endif
        handleWand(results.value);
      } else {
        #ifdef DEBUG
          Serial.print("Wand ");
          Serial.print(results.value);
          Serial.print(" detected. Ignored low magnitude=");
          Serial.println(results.magiquestMagnitude);
        #endif
      }
    } else {
      #ifdef DEBUG
        LOG("Unknown IR event");
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
  Serial.print("Unknown encoding: value=");
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
 * Used to blink the game status LED. The wait time is applied both while the LED is on and after it is turned off before returning.
 */
void blinkGameStatusLED(int wait, bool after) {
  digitalWrite(LED_PIN, HIGH);
  delay(wait);
  digitalWrite(LED_PIN, LOW);
  if (after) delay(wait);
}

/*
 * Called when a game of King of the Wand is started.
 */
void startGame() {
  gameInProgress = true;
  LOG("Game starting");
  // Reset the scores.
  for (int i = 0; i < NUM_WANDS; i++) {
    wands[i].duration = 0;
  }
  // Turn off the wand light.
  clearWandLight();
  // Reset the last wand.
  lastWand = NULL;
  // Blink the game status LED to show that we are ready.
  blinkGameStatusLED(200, true);
  blinkGameStatusLED(200, true);
  blinkGameStatusLED(200, false);
  digitalWrite(LED_PIN, HIGH); // Turn it on for the game.
  // Set the game mode.
  LOG("Game started");
}

/*
 * Called when a game of King of the Wand is over.
 */
void endGame() {
  #ifdef DEBUG
    Serial.print("Winner is ");
    Serial.println(lastWand->id);
  #endif
  // Set the indicator to their color (should be that anyway, but just in case).
  wandLit = true;
  setColor(lastWand->r, lastWand->g, lastWand->b);
  // Blink the game status LED to show that we have a winner.
  blinkGameStatusLED(200, true);
  blinkGameStatusLED(200, true);
  blinkGameStatusLED(200, true);
  blinkGameStatusLED(200, true);
  blinkGameStatusLED(200, false);
  // Set the game mode.
  gameInProgress = false;
  LOG("Game over");
}

/*
 * Called when a wand event is detected and validated.
 */
void handleWand(int32_t wandId) {
  // Get the wand.
  Wand *wand = &UNKNOWN_WAND;
  for (int i = 0; i < NUM_WANDS; i++) {
    if (wands[i].id == wandId) {
      wand = &wands[i];
      break;
    }
  }

  // If this is a different wand than last time then show their color.
  if (lastWand == NULL || lastWand->id != wand->id) {
    lastWand = wand;
    wandLit = true;
    setColor(wand->r, wand->g, wand->b);

  // If same wand, then just toggle their color on and off.
  } else if (wandLit) {
    clearWandLight();
    // If we are playing the King of the Wand game, then give them a penalty for getting too trigger happy.
    if (gameInProgress && wand->id != UNKNOWN_WAND_ID) {
      if (wand->duration < TIME_PENALTY) {
        wand->duration = 0;
      } else {
        wand->duration -= TIME_PENALTY; // They lose some time as a penalty.
      }
      // Unset them as the last wand so they don't accumulate any more points until they light the LED again.
      lastWand = NULL;
    }

  } else {
    wandLit = true;
    setColor(wand->r, wand->g, wand->b);
  }
}

/*
 * Turns off the wand light indicator.
 */
void clearWandLight() {
  wandLit = false;
  setColor(0, 0, 0);
}

/*
 * Turns on the wand light indicator with the specified color.
 */
void setColor(byte red, byte green, byte blue) {
  #ifdef COMMON_ANODE
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
  #endif
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);  
}

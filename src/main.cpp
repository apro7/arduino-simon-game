/**
   Simon Game for Arduino with Score display

   Copyright (C) 2022, Uri Shaked

   Released under the MIT License.
*/

#include <Arduino.h>
#include <pitches.h>
#include "SevSeg.h"

/* Constants - define pin numbers for LEDs,
   buttons and speaker, and also the game tones: */
const uint8_t ledPins[] = {39, 41, 43, 45}; //yellow, blue, green, [red]
const uint8_t buttonPins[] = {31, 33, 35, 37}; //yellow, blue, green, [red]
#define SPEAKER_PIN 8

// // These are connected to 74HC595 shift register (used to show game score):
const int LATCH_PIN = A1;  // 74HC595 pin 12
const int DATA_PIN = A0;  // 74HC595pin 14
const int CLOCK_PIN = A2;  // 74HC595 pin 11

// SevSeg sevseg; //Initiate a seven segment controller object
// byte numDigits = 4;  
// byte digitPins[] = {2, 3, 4, 5};
// byte segmentPins[] = {6, 7, 30, 9, 10, 11, 12, 13}; //changed 8 to 30
// bool resistorsOnSegments = 0; 
// // variable above indicates that 4 resistors were placed on the digit pins.
// // set variable to 1 if you want to use 8 resistors on the segment pins.

#define MAX_GAME_LENGTH 100

const int gameTones[] = { NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G5};

/* Global variables - store the game state */
uint8_t gameSequence[MAX_GAME_LENGTH] = {0};
uint8_t gameIndex = 0;

// unsigned long previousMillis = 0;
// const long interval = 300; // interval at which to perform actions (milliseconds)
// int toneState = 0;
// int pitch = -10;
// int pitchDirection = 1;

enum GameState {
  GAME_START,
  NEW_LEVEL, // New state for adding to sequence
  PLAYING_SEQUENCE,
  WAITING_FOR_INPUT,
  // CHECKING_INPUT,
  GAME_OVER,
  LEVEL_UP
};
GameState gameState = GAME_START;

unsigned long timer = 0;
int sequenceIndex = 0;
int inputIndex = 0;
byte currentButton = 255; // Initialize to no button pressed
const unsigned long SEQUENCE_DELAY = 350;
const unsigned long LEVEL_UP_DELAY = 150;
const unsigned long BUTTON_DEBOUNCE_DELAY = 50;
unsigned long lastButtonPressTime = 0;
unsigned long inputTimer = 0; // Timer for input timeout

/**
   Set up the Arduino board and initialize Serial communication
*/
void setup() {
  Serial.begin(9600);
  for (byte i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  
  // sevseg.begin(COMMON_CATHODE, numDigits, digitPins, segmentPins, resistorsOnSegments);
  // sevseg.setBrightness(90);

  // The following line primes the random number generator.
  // It assumes pin A3 is floating (disconnected):
  randomSeed(analogRead(A3));
}

/* Digit table for the 7-segment display */
// TODO - this code block may not be needed for sevseg
const uint8_t digitTable[] = {
  0b11000000,
  0b11111001,
  0b10100100,
  0b10110000,
  0b10011001,
  0b10010010,
  0b10000010,
  0b11111000,
  0b10000000,
  0b10010000,
};
const uint8_t DASH = 0b10111111; //TODO - check if this old code is correct for sevseg

void sendScore(uint8_t high, uint8_t low) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, low);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, high);
  digitalWrite(LATCH_PIN, HIGH);

  //TODO .. write code for sevseg display
}

void displayScore() {
  int high = gameIndex % 100 / 10;
  int low = gameIndex % 10;
  sendScore(high ? digitTable[high] : 0xff, digitTable[low]);
}

/**
   Lights the given LED and plays a suitable tone
*/
void lightLedAndPlayTone(byte ledIndex, bool on) {
  // digitalWrite(ledPins[ledIndex], HIGH);
  // tone(SPEAKER_PIN, gameTones[ledIndex]);
  // delay(300);
  // digitalWrite(ledPins[ledIndex], LOW);
  // noTone(SPEAKER_PIN);
  digitalWrite(ledPins[ledIndex], on ? HIGH : LOW);
  if (on) {
    tone(SPEAKER_PIN, gameTones[ledIndex]);
  } else {
    noTone(SPEAKER_PIN);
  }
}

/**
   Plays the current sequence of notes that the user has to repeat
*/
void playSequence() {
  // for (int i = 0; i < gameIndex; i++) {
  //   byte currentLed = gameSequence[i];
  //   lightLedAndPlayTone(currentLed);
  //   delay(50);
  // }
  static bool lightOn = false; // Keep track of light state
  static int currentLed;
  
  if (millis() - timer >= SEQUENCE_DELAY) { // Combined light and pause into one timed event
    timer = millis();
    if (sequenceIndex < gameIndex) {
        if (!lightOn) {
            currentLed = gameSequence[sequenceIndex];
            lightLedAndPlayTone(currentLed, true); // Turn on light and tone
            lightOn = true;
        } else {
            lightLedAndPlayTone(currentLed, false); // Turn off light and tone
            lightOn = false;
            sequenceIndex++;
        }
    } else {
        lightOn = false; // Ensure light and tone are off after sequence
        noTone(SPEAKER_PIN);
        gameState = WAITING_FOR_INPUT;
        sequenceIndex = 0;
    }
  }
}

/**
    Waits until the user pressed one of the buttons,
    and returns the index of that button
*/
byte readButtons() {
  // while (true) {
  //   for (byte i = 0; i < 4; i++) {
  //     byte buttonPin = buttonPins[i];
  //     if (digitalRead(buttonPin) == LOW) {
  //       return i;
  //     }
  //   }
  //   delay(1);
  // }
  if (millis() - lastButtonPressTime > BUTTON_DEBOUNCE_DELAY) {
        for (byte i = 0; i < 4; i++) {
            if (digitalRead(buttonPins[i]) == LOW) {
                lastButtonPressTime = millis();
                return i;
            }
        }
    }
  return 255;
}

/**
  Play the game over sequence, and report the game score
*/
void gameOver() {
  Serial.print("Game over! your score: ");
  Serial.println(gameIndex - 1);
  gameIndex = 0;
  // delay(200);
  gameState = GAME_START;
  timer = millis();

  // Play a Wah-Wah-Wah-Wah sound
  tone(SPEAKER_PIN, NOTE_DS5);
  delay(150);
  tone(SPEAKER_PIN, NOTE_D5);
  delay(150);
  tone(SPEAKER_PIN, NOTE_CS5);
  delay(150);
  // for (byte i = 0; i < 10; i++) {
  //   for (int pitch = -10; pitch <= 10; pitch++) {
  //     tone(SPEAKER_PIN, NOTE_C5 + pitch);
  //     delay(5);
  //   }
  // }
  noTone(SPEAKER_PIN);

  sendScore(DASH, DASH);
  // delay(500);
}

/**
   Get the user's input and compare it with the expected sequence.
*/
bool checkUserSequence() {
  // for (int i = 0; i < gameIndex; i++) {
  //   byte expectedButton = gameSequence[i];
  //   byte actualButton = readButtons();
  //   lightLedAndPlayTone(actualButton);
  //   if (expectedButton != actualButton) {
  //     return false;
  //   }
  // }
  if (inputIndex < gameIndex) {
        if (millis() - inputTimer >= 3000) { // 3 second timeout
            return false;
        }
        currentButton = readButtons();
        if (currentButton != 255) {
            inputTimer = millis();
            lightLedAndPlayTone(currentButton, true);
            delay(150);
            lightLedAndPlayTone(currentButton, false);
            if (currentButton == gameSequence[inputIndex]) {
                inputIndex++;
                currentButton = 255;
            } else {
                return false;
            }
        }
      return true; // Still waiting for more input!!!  
    } else {
        inputIndex = 0;
        return true;
    }
}

/**
   Plays a hooray sound whenever the user finishes a level
*/
void playLevelUpSound() {
  // tone(SPEAKER_PIN, NOTE_E4);
  // delay(150);
  // tone(SPEAKER_PIN, NOTE_G4);
  // delay(150);
  // tone(SPEAKER_PIN, NOTE_E5);
  // delay(150);
  // tone(SPEAKER_PIN, NOTE_C5);
  // delay(150);
  // tone(SPEAKER_PIN, NOTE_D5);
  // delay(150);
  // tone(SPEAKER_PIN, NOTE_G5);
  // delay(150);
  // noTone(SPEAKER_PIN);  
  if (millis() - timer >= LEVEL_UP_DELAY) {
        timer = millis();
        switch(sequenceIndex) {
            case 0: tone(SPEAKER_PIN, NOTE_E4); break;
            case 1: tone(SPEAKER_PIN, NOTE_G4); break;
            case 2: tone(SPEAKER_PIN, NOTE_E5); break;
            case 3: tone(SPEAKER_PIN, NOTE_C5); break;
            case 4: tone(SPEAKER_PIN, NOTE_D5); break;
            case 5: tone(SPEAKER_PIN, NOTE_G5); break;
            default: noTone(SPEAKER_PIN); gameState = PLAYING_SEQUENCE; sequenceIndex = 0; return;
        }
        sequenceIndex++;
  }
}

/**
   The main game loop
*/
void loop() {
  displayScore();

  // // Add a random color to the end of the sequence
  // gameSequence[gameIndex] = random(0, 4);
  // gameIndex++;
  // if (gameIndex >= MAX_GAME_LENGTH) {
  //   gameIndex = MAX_GAME_LENGTH - 1;
  // }

  // playSequence();
  // if (!checkUserSequence()) {
  //   gameOver();
  // }

  // delay(300);

  // if (gameIndex > 0) {
  //   playLevelUpSound();
  //   delay(300);
  // }

  switch (gameState) {
    case GAME_START:
      // gameSequence[gameIndex] = random(0, 4);
      // gameIndex++;
      // if (gameIndex >= MAX_GAME_LENGTH) {
      //   gameIndex = MAX_GAME_LENGTH - 1;
      // }
      // gameState = PLAYING_SEQUENCE;
      // timer = millis(); // Initialize timer
      // sequenceIndex = 0;
      // break;
      gameIndex = 0; // Reset game index at the start of a new game
      gameState = NEW_LEVEL;
      break;
    case NEW_LEVEL: // Add to sequence here
      if (gameIndex < MAX_GAME_LENGTH) {
        gameSequence[gameIndex] = random(0, 4);
        gameIndex++;
      } else {
        // Handle maximum game length reached (optional)
        gameState = GAME_OVER; //Or some other appropriate action
        break;
      }
      gameState = PLAYING_SEQUENCE;
      timer = millis();
      sequenceIndex = 0;
      break;

    case PLAYING_SEQUENCE:
      playSequence();
      break;

    case WAITING_FOR_INPUT:
      // inputTimer = millis();
      // if (!checkUserSequence()) {
      //   gameState = GAME_OVER;
      // } else if (inputIndex >= gameIndex) {
      //   gameState = LEVEL_UP;
      //   timer = millis();
      //   sequenceIndex = 0;
      // }
      // break;
      if (inputIndex == 0) { // Only reset timer at the beginning of input
        inputTimer = millis();
      }
      if (!checkUserSequence()) {
          gameState = GAME_OVER;
      } else if (inputIndex >= gameIndex) {
          gameState = LEVEL_UP;
          timer = millis();
          sequenceIndex = 0;
      }
      break;

    case GAME_OVER:
      gameOver();
      break;
      
    case LEVEL_UP:
        playLevelUpSound();
        if (sequenceIndex >= 6) { //After level up sound is complete
          gameState = NEW_LEVEL; // Go to the next level
        }
        break;
  }
}

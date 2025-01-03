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

SevSeg sevseg; //Initiate a seven segment controller object
byte numDigits = 4;  
byte digitPins[] = {2, 3, 4, 5};
byte segmentPins[] = {6, 7, 30, 9, 10, 11, 12, 13}; //changed 8 to 30
bool resistorsOnSegments = 0; 
// // variable above indicates that 4 resistors were placed on the digit pins.
// // set variable to 1 if you want to use 8 resistors on the segment pins.
byte hardwareConfig = COMMON_ANODE; // See README.md for options
// byte hardwareConfig = COMMON_CATHODE; // See README.md for options
bool updateWithDelays = false; // Default 'false' is Recommended
bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
bool disableDecPoint = false; // Use 'true' if your decimal point doesn't exist or isn't connected

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
  NEW_LEVEL, 
  PLAYING_SEQUENCE,
  WAITING_FOR_INPUT,
  CHECKING_INPUT,
  PLAYING_INPUT,
  IS_THERE_MORE,
  GAME_OVER,
  LEVEL_UP,
  LEVEL_UP_SEQUENCE,
};
GameState gameState = GAME_START;

enum LightAndToneState {
  LIGHT_AND_TONE_OFF,
  LIGHT_AND_TONE_ON,
  LIGHT_AND_TONE_WAIT,
  LIGHT_AND_TONE_DONE,
};
LightAndToneState lightAndToneState = LIGHT_AND_TONE_OFF;

enum PlaySequenceState {
  PLAYING_SEQUENCE_OFF,
  PLAYING_SEQUENCE_ON,
  PLAYING_SEQUENCE_PLAY_WAIT,
  PLAYING_SEQUENCE_INBETWEEN_WAIT,
  PLAYING_SEQUENCE_CHECK,
  PLAYING_SEQUENCE_DONE,
};
PlaySequenceState playSequenceState = PLAYING_SEQUENCE_OFF;

enum SingleButtonPlayState {
  BUTTON_PLAYING_OFF,
  BUTTON_PLAYING_ON,
  BUTTON_PLAYING_PLAY_WAIT,
  BUTTON_PLAYING_DONE,
};
SingleButtonPlayState singleButtonPlayState = BUTTON_PLAYING_OFF;

enum ReadButtonState {
  READ_BUTTON_OFF,
  READ_BUTTON_ON,
  READ_BUTTON_WAIT,
  READ_BUTTON_DONE,
};
ReadButtonState readButtonState = READ_BUTTON_OFF;

enum GameOverSequenceState {
  GAME_OVER_OFF,
  GAME_OVER_START_WAIT,
  GAME_OVER_T1_ON,
  GAME_OVER_T1_WAIT,
  GAME_OVER_T2_ON,
  GAME_OVER_T2_WAIT,
  GAME_OVER_T3_ON,
  GAME_OVER_T3_WAIT,
  GAME_OVER_T4_ON,
  GAME_OVER_T_OFF,
  GAME_OVER_END_ON,
  GAME_OVER_END_WAIT,
};
GameOverSequenceState gameOverSequenceState = GAME_OVER_OFF;

//APro's Constants code
// bool debugModeOn = true; // Set to true to enable debug mode
bool debugModeOn = false; // Set to true to enable debug mode
unsigned long lightAndToneDuration = 300; // Timer duration in milliseconds for light and tone duration
unsigned long playSequenceWaitDuration = 50; // Timer duration in milliseconds to wait between light/tone steps
unsigned long playSequenceTimer = 0; // Timer allocated for playSequence
unsigned long readButtonWaitDuration = 1; // Timer duration in milliseconds to wait between reads
unsigned long readButtonTimer = 0; // Timer allocated for readButtons
unsigned long singlePlayWaitDuration = 50; // Timer duration in milliseconds to wait between light/tone steps
unsigned long singlePlayTimer = 0; // Timer allocated for singlePlay
unsigned long gameOverStartWaitDuration = 200; // Timer duration in milliseconds to wait before game over sequence
unsigned long gameOverT1Duration = 300; // Timer duration in milliseconds for gameOver tone 1
unsigned long gameOverT2Duration = 300; // Timer duration in milliseconds for gameOver tone 2
unsigned long gameOverT3Duration = 300; // Timer duration in milliseconds for gameOver tone 3
unsigned long gameOverT4Duration = 900; // Timer duration in milliseconds for gameOver tone 4
unsigned long gameOverEndWaitDuration = 200; // Timer duration in milliseconds to wait after game over sequence
// end APro's code

//APro's Variables code
byte expectedButton = 255; // Initialize to no button pressed
byte actualButton = 255; // Initialize to no button pressed
byte buttonIndex = 255; // Initialize to no button pressed (internal index)

unsigned long timer = 0; //Replaced with playSequenceTimer and???
int sequenceIndex = 0;
int inputIndex = 0;
byte currentButton = 255; // Initialize to no button pressed

int checkIndex = 0;
// const unsigned long SEQUENCE_DELAY = 350; //Replaced with playSequenceWaitDuration
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
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments,
  updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(90);

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
  // int high = gameIndex % 100 / 10;
  // int low = gameIndex % 10;
  // sendScore(high ? digitTable[high] : 0xff, digitTable[low]);
  sevseg.setNumber(gameIndex);
  // sevseg.setNumber(3145,3);
  sevseg.refreshDisplay();
}

/**
   Lights the given LED and plays a suitable tone
*/
void lightLedAndPlayTone(byte ledIndex, bool on) {
  if (on) {
    digitalWrite(ledPins[ledIndex], HIGH);
    tone(SPEAKER_PIN, gameTones[ledIndex]);
  } else {
    digitalWrite(ledPins[ledIndex], LOW);
    noTone(SPEAKER_PIN);
  }
  
  // digitalWrite(ledPins[ledIndex], HIGH);
  // tone(SPEAKER_PIN, gameTones[ledIndex]);
  // delay(300);
  // digitalWrite(ledPins[ledIndex], LOW);
  // noTone(SPEAKER_PIN);
  
  // digitalWrite(ledPins[ledIndex], on ? HIGH : LOW);
  // if (on) {
  //   tone(SPEAKER_PIN, gameTones[ledIndex]);
  // } else {
  //   noTone(SPEAKER_PIN);
  // }
}

/**
   Plays the current sequence of notes that the user has to repeat
*/
void playSequence() {
  switch (playSequenceState) {
    case PLAYING_SEQUENCE_OFF:
      playSequenceState = PLAYING_SEQUENCE_ON;
      if (debugModeOn)
        Serial.println("playSequence-Off to On ");
      playSequenceTimer = millis();
      sequenceIndex = 0;
      break;
    case PLAYING_SEQUENCE_ON:
      lightAndToneState = LIGHT_AND_TONE_ON;
      lightLedAndPlayTone(gameSequence[sequenceIndex], true); //turn on
      lightAndToneState = LIGHT_AND_TONE_WAIT;
      playSequenceState = PLAYING_SEQUENCE_PLAY_WAIT;
      break;
    case PLAYING_SEQUENCE_PLAY_WAIT:
      switch (lightAndToneState) {
        case LIGHT_AND_TONE_WAIT:
          if (millis() - playSequenceTimer >= lightAndToneDuration) {
            playSequenceTimer = millis();
            lightAndToneState = LIGHT_AND_TONE_DONE;
          }
          break;
        case LIGHT_AND_TONE_DONE:
          lightLedAndPlayTone(gameSequence[sequenceIndex], false); //turn off  
          lightAndToneState = LIGHT_AND_TONE_OFF;
          playSequenceState = PLAYING_SEQUENCE_INBETWEEN_WAIT;
          break;
      }
      break; // Should never reach here
    case PLAYING_SEQUENCE_INBETWEEN_WAIT: // Wait between lights and tones
      if (millis() - playSequenceTimer >= playSequenceWaitDuration) {
        playSequenceTimer = millis();
        sequenceIndex++;
        playSequenceState = PLAYING_SEQUENCE_CHECK;
      }
      break;
    case PLAYING_SEQUENCE_CHECK:
      if (sequenceIndex < gameIndex) {
        playSequenceState = PLAYING_SEQUENCE_ON;
      } else {
        playSequenceState = PLAYING_SEQUENCE_DONE;
      }
      break;
    case PLAYING_SEQUENCE_DONE:
      playSequenceState = PLAYING_SEQUENCE_OFF;
      gameState = WAITING_FOR_INPUT;
      if (debugModeOn)
        Serial.println("gameState: PLAYING_SEQUNCE to WAITING_FOR_INPUT ");
      break;
      
  }
}

/**
   Plays the current note and LED based on what was just pressed
*/
void playCurrentNoteAndLed() {
  switch (singleButtonPlayState) {
    case BUTTON_PLAYING_OFF:
      singleButtonPlayState = BUTTON_PLAYING_ON;
      
      if (debugModeOn)
        Serial.println("playCurrentNoteAndLed: BUTTON_PLAYING_OFF to BUTTON_PLAYING_ON ");
      singlePlayTimer = millis();
      break;
    case BUTTON_PLAYING_ON:
      lightAndToneState = LIGHT_AND_TONE_ON;
      lightLedAndPlayTone(currentButton, true);
      lightAndToneState = LIGHT_AND_TONE_WAIT;
      singleButtonPlayState = BUTTON_PLAYING_PLAY_WAIT;
      
      if (debugModeOn)
        Serial.println("playCurrentNoteAndLed: BUTTON_PLAYING_ON to BUTTON_PLAYING_PLAY_WAIT ");
      break;
    case BUTTON_PLAYING_PLAY_WAIT:
      switch (lightAndToneState) {
        case LIGHT_AND_TONE_WAIT:
          if (millis() - singlePlayTimer >= lightAndToneDuration) {
            singlePlayTimer = millis();
            lightAndToneState = LIGHT_AND_TONE_DONE;
          }
          break;
        case LIGHT_AND_TONE_DONE:
          lightLedAndPlayTone(currentButton, false);
          lightAndToneState = LIGHT_AND_TONE_OFF;
          singleButtonPlayState = BUTTON_PLAYING_DONE;
          break;
      }
      break;
    case BUTTON_PLAYING_DONE:
      singleButtonPlayState = BUTTON_PLAYING_OFF;
      gameState = IS_THERE_MORE;
      if (debugModeOn)
        Serial.println("gameState: PLAYING_INPUT to IS_THERE_MORE ");
      break;
  }
  // lightLedAndPlayTone(currentButton, true);
  // delay(150);
  // lightLedAndPlayTone(currentButton, false);
}

/**
    Waits until the user pressed one of the buttons,
    and returns the index of that button
*/
void readButtons() {
  switch (readButtonState) {
    case READ_BUTTON_OFF:
      buttonIndex = 255;
      actualButton = 255;
      readButtonState = READ_BUTTON_ON;
      readButtonTimer = millis();
      break;
    case READ_BUTTON_ON:
      // if (millis() - lastButtonPressTime > BUTTON_DEBOUNCE_DELAY) {
        for (byte i = 0; i < 4; i++) {
          byte buttonPin = buttonPins[i];
          if (digitalRead(buttonPin) == LOW) {
            buttonIndex = i;
          }
        }
        readButtonState = READ_BUTTON_WAIT;

      // }
      break;
    case READ_BUTTON_WAIT:
      if (millis() - readButtonTimer >= readButtonWaitDuration) {
        if (buttonIndex != 255) {
          readButtonState = READ_BUTTON_DONE;
          actualButton = buttonIndex;
          if (debugModeOn) {
            Serial.println("actual button is");
            Serial.println(actualButton);
          }
        } else {
          readButtonState = READ_BUTTON_ON;
        }
      }
      // if (millis() - lastButtonPressTime >= BUTTON_DEBOUNCE_DELAY) {
        // readButtonState = READ_BUTTON_DONE;
      // }
      break;
    case READ_BUTTON_DONE:
      // readButtonState = READ_BUTTON_OFF;
      

      break; 
  }
  
  
  // while (true) {
  //   for (byte i = 0; i < 4; i++) {
  //     byte buttonPin = buttonPins[i];
  //     if (digitalRead(buttonPin) == LOW) {
  //       return i;
  //     }
  //   }
  //   delay(1);
  // }
  
  
  // if (millis() - lastButtonPressTime > BUTTON_DEBOUNCE_DELAY) {
  //       for (byte i = 0; i < 4; i++) {
  //           if (digitalRead(buttonPins[i]) == LOW) {
  //               lastButtonPressTime = millis();
  //               return i;
  //           }
  //       }
  //   }
  // return 255;
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
        // currentButton = readButtons();
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
  tone(SPEAKER_PIN, NOTE_E4);
  delay(150);
  tone(SPEAKER_PIN, NOTE_G4);
  delay(150);
  tone(SPEAKER_PIN, NOTE_E5);
  delay(150);
  tone(SPEAKER_PIN, NOTE_C5);
  delay(150);
  tone(SPEAKER_PIN, NOTE_D5);
  delay(150);
  tone(SPEAKER_PIN, NOTE_G5);
  delay(150);
  noTone(SPEAKER_PIN);  
  // if (millis() - timer >= LEVEL_UP_DELAY) {
  //       timer = millis();
  //       switch(sequenceIndex) {
  //           case 0: tone(SPEAKER_PIN, NOTE_E4); break;
  //           case 1: tone(SPEAKER_PIN, NOTE_G4); break;
  //           case 2: tone(SPEAKER_PIN, NOTE_E5); break;
  //           case 3: tone(SPEAKER_PIN, NOTE_C5); break;
  //           case 4: tone(SPEAKER_PIN, NOTE_D5); break;
  //           case 5: tone(SPEAKER_PIN, NOTE_G5); break;
  //           default: noTone(SPEAKER_PIN); gameState = PLAYING_SEQUENCE; sequenceIndex = 0; return;
  //       }
  //       sequenceIndex++;
  // }
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
    case GAME_START: {
      gameIndex = 0; // Reset game index at the start of a new game
      gameState = NEW_LEVEL;
      if (debugModeOn)
        Serial.println("gameState: GAME_START to NEW_LEVEL ");

      lightAndToneState = LIGHT_AND_TONE_OFF;
      playSequenceState = PLAYING_SEQUENCE_OFF;
      readButtonState = READ_BUTTON_OFF; 
      gameOverSequenceState = GAME_OVER_OFF;

      expectedButton = 255; // Initialize to no button pressed
      actualButton = 255; // Initialize to no button pressed

      checkIndex = 0;
      break;
    }
    case NEW_LEVEL: {
      if (gameIndex < MAX_GAME_LENGTH) {
        gameSequence[gameIndex] = random(0, 4);
        gameIndex++;
      } else {
        // Handle maximum game length reached (optional)
        gameState = GAME_OVER; //Or some other appropriate action
        break;
      }
      gameState = PLAYING_SEQUENCE;
      if (debugModeOn)
        Serial.println("gameState: NEW_LEVEL to PLAYING_SEQUENCE ");
      timer = millis();
      sequenceIndex = 0;
      checkIndex = 0;

      readButtonState = READ_BUTTON_OFF;
      break;
    }
    case PLAYING_SEQUENCE: {
      playSequence();
      break;
    }

    case WAITING_FOR_INPUT: {
      if (debugModeOn)
        Serial.println("WAITING_FOR_INPUT: before readButtons ");
          
      readButtons();
      if (debugModeOn)
        Serial.println("WAITING_FOR_INPUT: after readButtons ");
      if (readButtonState == READ_BUTTON_DONE) {
        currentButton = actualButton;
        
        if (debugModeOn) {
          Serial.println("WAITING_FOR_INPUT: currentButton is ");
          Serial.println(currentButton);
        }
        readButtonState = READ_BUTTON_OFF;
        gameState = CHECKING_INPUT;
        
        if (debugModeOn)
          Serial.println("gameState: WAITING_FOR_INPUT to CHECKING_INPUT ");
      }
      break;
    }

    case CHECKING_INPUT: {
      expectedButton = gameSequence[checkIndex];
      if (expectedButton != currentButton) {
        gameState = GAME_OVER;
        Serial.println("gameState: CHECKING_INPUT to GAME_OVER ");
      } else {
        singleButtonPlayState = BUTTON_PLAYING_OFF;
        gameState = PLAYING_INPUT;
        if (debugModeOn)
          Serial.println("gameState: CHECKING_INPUT to PLAYING_INPUT ");
      }
      break;
    }

    case PLAYING_INPUT: {
      if (debugModeOn)
        Serial.println("PLAYING INPUT: Prior to playCurrentNoteAndLed ");
      playCurrentNoteAndLed();
      
      if (debugModeOn)
        Serial.println("PLAYING INPUT: after playCurrentNoteAndLed ");
      break;
    }

    case GAME_OVER: {
      gameOver();
      break;
    }
      
    case IS_THERE_MORE: {
      checkIndex++; 
      if (checkIndex < gameIndex) {
        gameState = WAITING_FOR_INPUT;
        
        if (debugModeOn)
          Serial.println("gameState: IS_THERE_MORE to WAITING_FOR_INPUT ");
      } else {
        gameState = LEVEL_UP;
        
        if (debugModeOn)
          Serial.println("gameState: IS_THERE_MORE to LEVEL_UP ");
      }
      break;
    }

    case LEVEL_UP: {
      playLevelUpSound();
      // gameState = LEVEL_UP_SEQUENCE;
      // if (sequenceIndex >= 6) { //After level up sound is complete
      //   gameState = NEW_LEVEL; // Go to the next level
      // }
      gameState = NEW_LEVEL;

      if (debugModeOn)
        Serial.println("gameState: LEVEL_UP to NEW_LEVEL ");
      break;
    }
  }
}

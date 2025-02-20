#include <Adafruit_NeoPixel.h>
#include "WiFiEsp.h"
// ***************************************
// Georgie UNI-ULM 
// **************************************
// ***************************************
// Hardware-Definitionen
// ***************************************

#define LED_RING_PIN_PLAYER1 9
#define LED_RING_PIN_PLAYER2 10

const int sensorPinsPlayer1[8] = {1, 2, 3, 4, 5, 6, 7, 8};
const int sensorPinsPlayer2[8] = {32, 31, 16, 17, 18, 19, 20, 21};

const int ledStripPins[9] = {22, 23, 24, 25, 26, 27, 28, 29, 30};

#define NUM_LEDS_PER_RING 35
#define NUM_RINGS 8
#define NUM_LEDS_PER_STRIP 60
#define NUM_STRIPS 9

// LED-Ringe für die Spieler:
Adafruit_NeoPixel ledRingPlayer1(NUM_RINGS * NUM_LEDS_PER_RING, LED_RING_PIN_PLAYER1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ledRingPlayer2(NUM_RINGS * NUM_LEDS_PER_RING, LED_RING_PIN_PLAYER2, NEO_GRB + NEO_KHZ800);
// LED-Stripes (Mapping):
// ledStrips[0]  = Stripe 1 (Schuss-Effekt)
// ledStrips[1]  = Stripe 2 (Punkteanzeige Spieler Rot)
// ledStrips[2]  = Stripe 3 (Schuss-Effekt)
// ledStrips[3]  = Stripe 4 (Random-Effekt)
// ledStrips[4]  = Stripe 5 (Schuss-Effekt)
// ledStrips[5]  = Stripe 6 (Random-Effekt)
// ledStrips[6]  = Stripe 7 (Schuss-Effekt)
// ledStrips[7]  = Stripe 8 (Punkteanzeige Spieler Blau)
// ledStrips[8]  = Stripe 9 (Schuss-Effekt)
Adafruit_NeoPixel ledStrips[NUM_STRIPS];

// ***************************************
// Globale Variablen – Game Mode
// ***************************************

int activeRingPlayer1 = -1;
int activeRingPlayer2 = -1;
int scorePlayer1 = 0;
int scorePlayer2 = 0;
bool player1Ready = false;
bool player2Ready = false;
unsigned long gameStartTime = 0;

unsigned long activeRingActivationTime1 = 0;
unsigned long activeRingActivationTime2 = 0;
#define MAX_REACTION_TIMES 100
unsigned long reactionTimesPlayer1[MAX_REACTION_TIMES];
unsigned long reactionTimesPlayer2[MAX_REACTION_TIMES];
int reactionCountPlayer1 = 0;
int reactionCountPlayer2 = 0;

const unsigned long MIN_VALID_REACTION_TIME = 50;
bool lastActiveSensorState1 = HIGH;
bool lastActiveSensorState2 = HIGH;
bool lastReadySensorState1 = HIGH;
bool lastReadySensorState2 = HIGH;
const unsigned long debounceDelay = 20;

bool sensorProcessed1 = false;
bool sensorProcessed2 = false;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;

bool readyProcessed1 = false;
bool readyProcessed2 = false;
unsigned long lastDebounceTimeReady1 = 0;
unsigned long lastDebounceTimeReady2 = 0;

unsigned long cheatingBlockUntil1 = 0;
unsigned long cheatingBlockUntil2 = 0;
const unsigned long CHEATING_BLOCK_TIME = 1000;
bool gameModeStarted = false;

// ***************************************
// LED Strip Effekte
// ***************************************

struct ShootingEffect {
  int stripNum;
  int startLed;
  int endLed;
  unsigned long lastUpdate;
  uint16_t hue;
  bool active;
};

#define MAX_EFFECTS 8
ShootingEffect activeEffects[MAX_EFFECTS];

struct ScoreDisplay {
  int stripNum;
  bool isPlayer1;
};

// Wir nutzen hier:
ScoreDisplay scoreDisplays[2] = {
  {1, true},  // Punkteanzeige Spieler Rot auf ledStrips[1]
  {7, false}  // Punkteanzeige Spieler Blau auf ledStrips[7]
};

// ***************************************
// WiFi & Webserver
// ***************************************

const char* ssid = "DEINWLAN";
const char* wifiPassword = "DEINPW";
WiFiEspServer server(80);
int currentMode = 1;

// ***************************************
// Memory Game Variablen
// ***************************************

struct MemoryGameState {
  bool initialized;
  int round;
  int currentInputIndex;
  int sequence[16];
  unsigned long sequenceDisplayStart;
  int sequenceDisplayIndex;
  bool showingSequence;
  bool waitingForInput;
  unsigned long lastInputTime;
  bool failed;
  bool inputProcessed;
};

MemoryGameState memoryState1, memoryState2;
unsigned long memoryGameStartTime = 0;

// ***************************************
// Ambient Mode Variablen
// ***************************************

uint32_t ambientColor1[8] = {0};
uint32_t ambientColor2[8] = {0};

// ***************************************
// Funktionsprototypen
// ***************************************

void lightUpRing(int ringIndex, int r, int g, int b, int player);
void turnOffRing(int ringIndex, int player);
int countTouchedSensors(int player);
void resetAllLEDs();
void stopCurrentMode();
void updateReadyState();
void activateRandomRing(int player);
void checkSensors();
void endGame();
void resetGame();
void gameModeLoop();
void memoryGameInit();
void generateMemorySequence(MemoryGameState &state);
void memoryGameProcessPlayer(int player, MemoryGameState &state);
void memoryGameLoop();
uint32_t randomColor();
void ambientTableLoop();
void handleClient();
void rainbowRing(int ringIndex, int player);
void blinkWinnerLoser(int winner);
void setupStripEffects();
void startShootingEffect(int stripNum, int start, int end);
void updateShootingEffects();
void updateScoreDisplays();
void handlePlayerHit(int player, int ring);
void updateRandomEffects();

// ***************************************
// Funktionen
// ***************************************

void lightUpRing(int ringIndex, int r, int g, int b, int player) {
  Adafruit_NeoPixel &ledRing = (player == 1) ? ledRingPlayer1 : ledRingPlayer2;
  int start = ringIndex * NUM_LEDS_PER_RING;
  for (int i = start; i < start + NUM_LEDS_PER_RING; i++) {
    ledRing.setPixelColor(i, ledRing.Color(r, g, b));
  }
  ledRing.show();
}

void turnOffRing(int ringIndex, int player) {
  lightUpRing(ringIndex, 0, 0, 0, player);
}

void resetAllLEDs() {
  ledRingPlayer1.clear(); ledRingPlayer1.show();
  ledRingPlayer2.clear(); ledRingPlayer2.show();
  for (int i = 0; i < NUM_STRIPS; i++) {
    ledStrips[i].clear();
    ledStrips[i].show();
  }
}

void stopCurrentMode() {
  resetAllLEDs();
  resetGame();
  gameModeStarted = false;
  player1Ready = false;
  player2Ready = false;
  memoryGameInit();
  for (int i = 0; i < 8; i++) {
    ambientColor1[i] = 0;
    ambientColor2[i] = 0;
  }
}

void rainbowRing(int ringIndex, int player) {
  static uint16_t hue = 0;
  Adafruit_NeoPixel &ledRing = (player == 1) ? ledRingPlayer1 : ledRingPlayer2;
  int start = ringIndex * NUM_LEDS_PER_RING;
  for (int i = 0; i < NUM_LEDS_PER_RING; i++) {
    ledRing.setPixelColor(start + i, ledRing.ColorHSV(hue + (i * 65536L / NUM_LEDS_PER_RING)));
  }
  ledRing.show();
  hue += 512;
}

int countTouchedSensors(int player) {
  int count = 0;
  for (int i = 0; i < 8; i++) {
    if ((player == 1 && digitalRead(sensorPinsPlayer1[i]) == LOW) ||
        (player == 2 && digitalRead(sensorPinsPlayer2[i]) == LOW))
      count++;
  }
  return count;
}

// ---------- Game Mode ----------
void updateReadyState() {
  if (!player1Ready) {
    rainbowRing(4, 1);
    bool currentReady1 = digitalRead(sensorPinsPlayer1[4]);
    if (lastReadySensorState1 == HIGH && currentReady1 == LOW)
      lastDebounceTimeReady1 = millis();
    if (currentReady1 == LOW && (millis() - lastDebounceTimeReady1 >= debounceDelay) && !readyProcessed1) {
      readyProcessed1 = true;
      player1Ready = true;
      lightUpRing(4, 255, 0, 0, 1);
    }
    if (currentReady1 == HIGH)
      readyProcessed1 = false;
    lastReadySensorState1 = currentReady1;
  }
  
  if (!player2Ready) {
    rainbowRing(4, 2);
    bool currentReady2 = digitalRead(sensorPinsPlayer2[4]);
    if (lastReadySensorState2 == HIGH && currentReady2 == LOW)
      lastDebounceTimeReady2 = millis();
    if (currentReady2 == LOW && (millis() - lastDebounceTimeReady2 >= debounceDelay) && !readyProcessed2) {
      readyProcessed2 = true;
      player2Ready = true;
      lightUpRing(4, 0, 0, 255, 2);
    }
    if (currentReady2 == HIGH)
      readyProcessed2 = false;
    lastReadySensorState2 = currentReady2;
  }
  
  if (player1Ready && player2Ready && !gameModeStarted) {
    static unsigned long readyStart = millis();
    if (millis() - readyStart >= 1000) {
      turnOffRing(4, 1);
      turnOffRing(4, 2);
      gameStartTime = millis();
      activateRandomRing(1);
      activateRandomRing(2);
      gameModeStarted = true;
      readyStart = millis();
    }
  }
}

void activateRandomRing(int player) {
  int newRing;
  do {
    newRing = random(0, NUM_RINGS);
  } while ((player == 1 && newRing == activeRingPlayer1) ||
           (player == 2 && newRing == activeRingPlayer2));
           
  if (player == 1) {
    activeRingPlayer1 = newRing;
    activeRingActivationTime1 = millis();
    sensorProcessed1 = false;
    lightUpRing(newRing, 255, 0, 0, 1);
    lastActiveSensorState1 = digitalRead(sensorPinsPlayer1[newRing]);
  } else {
    activeRingPlayer2 = newRing;
    activeRingActivationTime2 = millis();
    sensorProcessed2 = false;
    lightUpRing(newRing, 0, 0, 255, 2);
    lastActiveSensorState2 = digitalRead(sensorPinsPlayer2[newRing]);
  }
}

void checkSensors() {
  if (activeRingPlayer1 != -1) {
    if (millis() < cheatingBlockUntil1) {
      lastActiveSensorState1 = digitalRead(sensorPinsPlayer1[activeRingPlayer1]);
    } else {
      bool currentState1 = digitalRead(sensorPinsPlayer1[activeRingPlayer1]);
      if (lastActiveSensorState1 == HIGH && currentState1 == LOW)
        lastDebounceTime1 = millis();
      if (currentState1 == LOW && (millis() - lastDebounceTime1 >= debounceDelay) && !sensorProcessed1) {
        unsigned long reactionTime = millis() - activeRingActivationTime1;
        if (reactionTime >= MIN_VALID_REACTION_TIME) {
          if (countTouchedSensors(1) > 2) {
            cheatingBlockUntil1 = millis() + CHEATING_BLOCK_TIME;
            turnOffRing(activeRingPlayer1, 1);
            activateRandomRing(1);
          } else {
            sensorProcessed1 = true;
            reactionTimesPlayer1[reactionCountPlayer1++] = reactionTime;
            scorePlayer1++;
            handlePlayerHit(1, activeRingPlayer1);
            turnOffRing(activeRingPlayer1, 1);
            activateRandomRing(1);
          }
        }
      }
      if (currentState1 == HIGH)
        sensorProcessed1 = false;
      lastActiveSensorState1 = currentState1;
    }
  }

  if (activeRingPlayer2 != -1) {
    if (millis() < cheatingBlockUntil2) {
      lastActiveSensorState2 = digitalRead(sensorPinsPlayer2[activeRingPlayer2]);
    } else {
      bool currentState2 = digitalRead(sensorPinsPlayer2[activeRingPlayer2]);
      if (lastActiveSensorState2 == HIGH && currentState2 == LOW)
        lastDebounceTime2 = millis();
      if (currentState2 == LOW && (millis() - lastDebounceTime2 >= debounceDelay) && !sensorProcessed2) {
        unsigned long reactionTime = millis() - activeRingActivationTime2;
        if (reactionTime >= MIN_VALID_REACTION_TIME) {
          if (countTouchedSensors(2) > 2) {
            cheatingBlockUntil2 = millis() + CHEATING_BLOCK_TIME;
            turnOffRing(activeRingPlayer2, 2);
            activateRandomRing(2);
          } else {
            sensorProcessed2 = true;
            reactionTimesPlayer2[reactionCountPlayer2++] = reactionTime;
            scorePlayer2++;
            handlePlayerHit(2, activeRingPlayer2);
            turnOffRing(activeRingPlayer2, 2);
            activateRandomRing(2);
          }
        }
      }
      if (currentState2 == HIGH)
        sensorProcessed2 = false;
      lastActiveSensorState2 = currentState2;
    }
  }
}

void resetGame() {
  scorePlayer1 = 0;
  scorePlayer2 = 0;
  activeRingPlayer1 = -1;
  activeRingPlayer2 = -1;
  player1Ready = false;
  player2Ready = false;
}

void endGame() {
  Serial.println("Spiel beendet!");
  Serial.print("Spieler 1: ");
  Serial.println(scorePlayer1);
  Serial.print("Spieler 2: ");
  Serial.println(scorePlayer2);
  
  if(scorePlayer1 > scorePlayer2) {
    blinkWinnerLoser(1);
  } else if(scorePlayer2 > scorePlayer1) {
    blinkWinnerLoser(2);
  } else {
    for(int i = 0; i < 3; i++) {
      lightUpRing(4, 255, 255, 0, 1);
      lightUpRing(4, 255, 255, 0, 2);
      delay(300);
      turnOffRing(4, 1);
      turnOffRing(4, 2);
      delay(300);
    }
  }
  
  resetGame();
  resetAllLEDs();
  gameModeStarted = false;
}

void gameModeLoop() {
  if (!player1Ready || !player2Ready) {
    updateReadyState();
    return;
  }
  if (millis() - gameStartTime >= 30000) {
    endGame();
    return;
  }
  checkSensors();
  updateShootingEffects();
  //updateRandomEffects();
  updateScoreDisplays();
}

// ---------- LED Strip Funktionen ----------
void setupStripEffects() {
  for (int i = 0; i < NUM_STRIPS; i++) {
    ledStrips[i].clear();
    ledStrips[i].show();
  }
}

void startShootingEffect(int stripNum, int start, int end) {
  for (int i = 0; i < MAX_EFFECTS; i++) {
    if (!activeEffects[i].active) {
      activeEffects[i] = {
        stripNum,
        start,
        end,
        millis(),
        0,
        true
      };
      break;
    }
  }
}

void updateShootingEffects() {
  for (int i = 0; i < MAX_EFFECTS; i++) {
    if (activeEffects[i].active) {
      Adafruit_NeoPixel &strip = ledStrips[activeEffects[i].stripNum];
      int direction = (activeEffects[i].endLed > activeEffects[i].startLed) ? 1 : -1;
      
      if (millis() - activeEffects[i].lastUpdate > 15) {
        activeEffects[i].hue += 2048;
        activeEffects[i].lastUpdate = millis();
        activeEffects[i].startLed += direction;
        
        if (activeEffects[i].startLed == activeEffects[i].endLed) {
          activeEffects[i].active = false;
          strip.clear();
          strip.show();
          continue;
        }
      }
      
      strip.clear();
      for (int j = 0; j < 10; j++) {
        int pos = activeEffects[i].startLed + (j * direction);
        if (pos >= 0 && pos < strip.numPixels()) {
          strip.setPixelColor(pos, strip.gamma32(strip.ColorHSV(activeEffects[i].hue + (j * 500))));
        }
      }
      strip.show();
    }
  }
}

void updateScoreDisplays() {
  // Punkteanzeige Spieler Rot auf LED-Stripe 2 (ledStrips[1])
  Adafruit_NeoPixel &strip1 = ledStrips[1];
  strip1.clear();
  for (int i = 0; i < scorePlayer1; i++) {
    if (i < strip1.numPixels()) {
      strip1.setPixelColor(i, strip1.Color(255, 0, 0));
    }
  }
  strip1.show();

  // Punkteanzeige Spieler Blau auf LED-Stripe 8 (ledStrips[7])
  Adafruit_NeoPixel &strip2 = ledStrips[7];
  strip2.clear();
  for (int i = 0; i < scorePlayer2; i++) {
    int pos = strip2.numPixels() - 1 - i;
    if (pos >= 0) {
      strip2.setPixelColor(pos, strip2.Color(0, 0, 255));
    }
  }
  strip2.show();
}

void handlePlayerHit(int player, int ring) {
  if (player == 1) { // Spieler Rot
    switch (ring) {
      case 0: // LED-Ring 1 → LED-Stripe 1 (ledStrips[0]): Schuss startet bei LED 21 bis 60
        startShootingEffect(0, 21, 60);
        break;
      case 1: // LED-Ring 2 → LED-Stripe 1: Schuss startet bei LED 10 bis 60
        startShootingEffect(0, 10, 60);
        break;
      case 2: // LED-Ring 3 → LED-Stripe 3 (ledStrips[2]): Schuss startet bei LED 21 bis 60
        startShootingEffect(2, 15, 60);
        break;
      case 3: // LED-Ring 4 → LED-Stripe 5 (ledStrips[4]): Schuss startet bei LED 10 bis 60
        startShootingEffect(4, 10, 60);
        break;
      case 4: // LED-Ring 5 → LED-Stripe 5: Schuss startet bei LED 21 bis 60
        startShootingEffect(4, 15, 60);
        break;
      case 5: // LED-Ring 6 → LED-Stripe 7 (ledStrips[6]): Schuss startet bei LED 21 bis 60
        startShootingEffect(6, 21, 60);
        break;
      case 6: // LED-Ring 7 → LED-Stripe 9 (ledStrips[8]): Schuss startet bei LED 10 bis 60
        startShootingEffect(8, 10, 60);
        break;
      case 7: // LED-Ring 8 → LED-Stripe 9: Schuss startet bei LED 21 bis 60
        startShootingEffect(8, 21, 60);
        break;
      default:
        break;
    }
  } else { // Spieler Blau
    switch (ring) {
      case 0: // LED-Ring 1 → LED-Stripe 1: Schuss startet bei LED 36 bis 0
        startShootingEffect(0, 39, 0);
        break;
      case 1: // LED-Ring 2 → LED-Stripe 1: Schuss startet bei LED 45 bis 0
        startShootingEffect(0, 50, 0);
        break;
      case 2: // LED-Ring 3 → LED-Stripe 3: Schuss startet bei LED 36 bis 0
        startShootingEffect(2, 45, 0);
        break;
      case 3: // LED-Ring 4 → LED-Stripe 5: Schuss startet bei LED 45 bis 0
        startShootingEffect(4, 50, 0);
        break;
      case 4: // LED-Ring 5 → LED-Stripe 5: Schuss startet bei LED 36 bis 0
        startShootingEffect(4, 39, 0);
        break;
      case 5: // LED-Ring 6 → LED-Stripe 7: Schuss startet bei LED 36 bis 0
        startShootingEffect(6, 45, 0);
        break;
      case 6: // LED-Ring 7 → LED-Stripe 9: Schuss startet bei LED 45 bis 0
        startShootingEffect(8, 50, 0);
        break;
      case 7: // LED-Ring 8 → LED-Stripe 9: Schuss startet bei LED 36 bis 0
        startShootingEffect(8, 39, 0);
        break;
      default:
        break;
    }
  }
}

void updateRandomEffects() {
  static unsigned long lastRandomUpdate = 0;
  if (millis() - lastRandomUpdate > 100) {
    // Random-Effekt auf Stripe 4 (ledStrips[3])
    ledStrips[3].setPixelColor(random(NUM_LEDS_PER_STRIP), randomColor());
    ledStrips[3].show();
    
    // Random-Effekt auf Stripe 6 (ledStrips[5])
    ledStrips[5].setPixelColor(random(NUM_LEDS_PER_STRIP), randomColor());
    ledStrips[5].show();
    
    lastRandomUpdate = millis();
  }
}

// ---------- Memory Game ----------
void memoryGameInit() {
  memoryState1.initialized = true;
  memoryState1.round = 0;
  memoryState1.currentInputIndex = 0;
  memoryState1.showingSequence = false;
  memoryState1.waitingForInput = false;
  memoryState1.sequenceDisplayIndex = 0;
  memoryState1.sequenceDisplayStart = 0;
  memoryState1.lastInputTime = 0;
  memoryState1.failed = false;
  memoryState1.inputProcessed = false;
  memoryState2 = memoryState1;
  memoryGameStartTime = millis();
  for (int i = 0; i < NUM_RINGS; i++) {
    turnOffRing(i, 1);
    turnOffRing(i, 2);
  }
}

void generateMemorySequence(MemoryGameState &state) {
  for (int i = 0; i < state.round; i++) {
    state.sequence[i] = random(0, NUM_RINGS);
  }
}

void memoryGameProcessPlayer(int player, MemoryGameState &state) {
  if (!state.showingSequence && !state.waitingForInput && !state.failed) {
    state.round++;
    state.currentInputIndex = 0;
    generateMemorySequence(state);
    state.showingSequence = true;
    state.sequenceDisplayIndex = 0;
    state.sequenceDisplayStart = millis();
  }

  if (state.showingSequence) {
    unsigned long currentTime = millis();
    int totalElements = state.round;
    int displayTime = 50;
    int gapTime = 50;
    if (state.sequenceDisplayIndex < totalElements) {
      unsigned long elementStart = state.sequenceDisplayStart + state.sequenceDisplayIndex * (displayTime + gapTime);
      if (currentTime >= elementStart && currentTime < elementStart + displayTime) {
        int ringIndex = state.sequence[state.sequenceDisplayIndex];
        uint32_t col = randomColor();
        lightUpRing(ringIndex, (col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF, player);
      } else if (currentTime >= elementStart + displayTime) {
        turnOffRing(state.sequence[state.sequenceDisplayIndex], player);
        state.sequenceDisplayIndex++;
      }
    }
    if (state.sequenceDisplayIndex >= totalElements) {
      state.showingSequence = false;
      state.waitingForInput = true;
      state.lastInputTime = millis();
    }
  }

  if (state.waitingForInput && !state.failed) {
    int touchedCount = countTouchedSensors(player);
    
    if (touchedCount == 1 && !state.inputProcessed) {
      int pressed = -1;
      for (int i = 0; i < 8; i++) {
        if ((player == 1 && digitalRead(sensorPinsPlayer1[i]) == LOW) ||
            (player == 2 && digitalRead(sensorPinsPlayer2[i]) == LOW)) {
          pressed = i;
          break;
        }
      }
      
      if (pressed != -1 && (millis() - state.lastInputTime > debounceDelay)) {
        int expected = state.sequence[state.currentInputIndex];
        if (pressed == expected) {
          lightUpRing(pressed, 0, 0, 255, player);
          state.currentInputIndex++;
          state.lastInputTime = millis();
          state.inputProcessed = true;
          
          if (state.currentInputIndex >= state.round) {
            for (int r = 0; r < NUM_RINGS; r++) turnOffRing(r, player);
            state.waitingForInput = false;
            delay(300);
          }
        } else {
          lightUpRing(pressed, 255, 0, 0, player);
          state.failed = true;
        }
      }
    } 
    else if (touchedCount == 0 && state.inputProcessed) {
      state.inputProcessed = false;
      turnOffRing(state.sequence[state.currentInputIndex-1], player);
    }
  }
}

void memoryGameLoop() {
  if (memoryState1.failed && !memoryState2.failed) {
    blinkWinnerLoser(2);
    memoryGameInit();
  } else if (memoryState2.failed && !memoryState1.failed) {
    blinkWinnerLoser(1);
    memoryGameInit();
  } else if (memoryState1.round >= 8 && memoryState2.round < 8) {
    blinkWinnerLoser(1);
    memoryGameInit();
  } else if (memoryState2.round >= 8 && memoryState1.round < 8) {
    blinkWinnerLoser(2);
    memoryGameInit();
  } else if (memoryState1.round >= 8 && memoryState2.round >= 8) {
    memoryGameInit();
  }
  
  memoryGameProcessPlayer(1, memoryState1);
  memoryGameProcessPlayer(2, memoryState2);
}

// ---------- Ambient Table ----------
uint32_t randomColor() {
  uint8_t r = random(0, 256);
  uint8_t g = random(0, 256);
  uint8_t b = random(0, 256);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void ambientTableLoop() {
  for (int i = 0; i < 8; i++) {
    if (digitalRead(sensorPinsPlayer1[i]) == LOW) {
      if (ambientColor1[i] == 0)
        ambientColor1[i] = randomColor();
      uint8_t r = (ambientColor1[i] >> 16) & 0xFF;
      uint8_t g = (ambientColor1[i] >> 8) & 0xFF;
      uint8_t b = ambientColor1[i] & 0xFF;
      lightUpRing(i, r, g, b, 1);
    } else {
      ambientColor1[i] = 0;
      turnOffRing(i, 1);
    }
  }
  
  for (int i = 0; i < 8; i++) {
    if (digitalRead(sensorPinsPlayer2[i]) == LOW) {
      if (ambientColor2[i] == 0)
        ambientColor2[i] = randomColor();
      uint8_t r = (ambientColor2[i] >> 16) & 0xFF;
      uint8_t g = (ambientColor2[i] >> 8) & 0xFF;
      uint8_t b = ambientColor2[i] & 0xFF;
      lightUpRing(i, r, g, b, 2);
    } else {
      ambientColor2[i] = 0;
      turnOffRing(i, 2);
    }
  }
  
  static unsigned long lastStripUpdate = 0;
  const unsigned long stripUpdateInterval = 20;  // Aktualisierungsintervall (ms)
  if (millis() - lastStripUpdate >= stripUpdateInterval) {
    lastStripUpdate = millis();
    for (int i = 0; i < NUM_STRIPS; i++) {
      ledStrips[i].clear();
      int randIndex = random(0, NUM_LEDS_PER_STRIP);
      uint32_t col = randomColor();
      
      // Zufällige Helligkeit zwischen z.B. 50 und 255
      uint8_t brightness = random(50, 256);
      uint8_t r = ((col >> 16) & 0xFF) * brightness / 255;
      uint8_t g = ((col >> 8) & 0xFF) * brightness / 255;
      uint8_t b = (col & 0xFF) * brightness / 255;
      
      ledStrips[i].setPixelColor(randIndex, ledStrips[i].Color(r, g, b));
      ledStrips[i].show();
    }
  }
}


void blinkWinnerLoser(int winner) {
  for (int i = 0; i < 5; i++) {
    if (winner == 1) {
      lightUpRing(4, 0, 255, 0, 1);
      lightUpRing(4, 255, 0, 0, 2);
    } else {
      lightUpRing(4, 255, 0, 0, 1);
      lightUpRing(4, 0, 255, 0, 2);
    }
    delay(300);
    turnOffRing(4, 1);
    turnOffRing(4, 2);
    delay(300);
  }
}

// ---------- Webserver ----------
void handleClient() {
  WiFiEspClient client = server.available();
  if (!client) return;
  
  String request = client.readStringUntil('\r');
  client.flush();
  
  if (request.indexOf("GET /setmode?mode=") != -1) {
    int modeStart = request.indexOf("mode=") + 5;
    int modeEnd = request.indexOf(" ", modeStart);
    String modeStr = request.substring(modeStart, modeEnd);
    int mode = modeStr.toInt();
    
    if (mode >= 1 && mode <= 3) {
      stopCurrentMode();
      currentMode = mode;
      client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nMode set to " + String(currentMode));
    } else {
      client.print("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid mode");
    }
    client.stop();
    return;
  }
  
  String page = "<html><head><title>Mode Selection</title></head><body>";
  page += "<h1>Select Mode PHOS&#178</h1>";
  page += "<form action=\"/setmode\" method=\"GET\">";
  page += "<input type=\"radio\" name=\"mode\" value=\"1\" id=\"mode1\"" + String(currentMode == 1 ? " checked" : "") + "><label for=\"mode1\">Game Mode</label><br>";
  page += "<input type=\"radio\" name=\"mode\" value=\"2\" id=\"mode2\"" + String(currentMode == 2 ? " checked" : "") + "><label for=\"mode2\">Memory Game Mode</label><br>";
  page += "<input type=\"radio\" name=\"mode\" value=\"3\" id=\"mode3\"" + String(currentMode == 3 ? " checked" : "") + "><label for=\"mode3\">Ambient Table Mode</label><br><br>";
  page += "<input type=\"submit\" value=\"Set Mode\">";
  page += "</form></body></html>";
  
  client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + page);
  delay(1);
  client.stop();
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  
  WiFi.init(&Serial3);
  if (WiFi.status() == WL_NO_SHIELD) {
    while (true);
  }
  
  for (int i = 0; i < 8; i++) {
    pinMode(sensorPinsPlayer1[i], INPUT_PULLUP);
    pinMode(sensorPinsPlayer2[i], INPUT_PULLUP);
  }
  
  ledRingPlayer1.begin();
  ledRingPlayer2.begin();
  ledRingPlayer1.show();
  ledRingPlayer2.show();
  
  for (int i = 0; i < NUM_STRIPS; i++) {
    ledStrips[i] = Adafruit_NeoPixel(NUM_LEDS_PER_STRIP, ledStripPins[i], NEO_GRB + NEO_KHZ800);
    ledStrips[i].begin();
    ledStrips[i].setBrightness(100);
    ledStrips[i].show();
  }
  
  randomSeed(analogRead(A0));
  setupStripEffects();
  
  Serial.print("Verbinde mit WiFi...");
  int status = WiFi.begin(ssid, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  server.begin();
  currentMode = 1;
}

void loop() {
  handleClient();
  switch (currentMode) {
    case 1: gameModeLoop(); break;
    case 2: memoryGameLoop(); break;
    case 3: ambientTableLoop(); break;
  }
}
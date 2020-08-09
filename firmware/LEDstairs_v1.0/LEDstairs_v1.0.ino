/*
  Скетч к проекту "Подсветка лестницы"
  Страница проекта (схемы, описания): https://alexgyver.ru/ledstairs/
  Исходники на GitHub: https://github.com/AlexGyver/LEDstairs
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2019
  https://AlexGyver.ru/
*/

#define STEP_AMOUNT 12     // количество ступенек
#define STEP_LENGTH 8    // количество чипов WS2811 на ступеньку

#define AUTO_BRIGHT 0     // автояркость 0/1 вкл/выкл (с фоторезистором)
#define CUSTOM_BRIGHT 150  // ручная яркость

#define FADR_SPEED 500
#define START_EFFECT   RAINBOW    // режим при старте COLOR, RAINBOW, FIRE
#define ROTATE_EFFECTS 1      // 0/1 - автосмена эффектов
#define TIMEOUT 20            // секунд, таймаут выключения ступенек, если не сработал конечный датчик

// пины
// если перепутаны сенсоры - можно поменять их местами в коде! Вот тут
#define SENSOR_START 3
#define SENSOR_END 2
#define STRIP_PIN 13    // пин ленты
#define PHOTO_PIN A0

// для разработчиков
#define ORDER_BRG       // порядок цветов ORDER_GRB / ORDER_RGB / ORDER_BRG
#define COLOR_DEBTH 3   // цветовая глубина: 1, 2, 3 (в байтах)
#include "microLED.h"
#define NUMLEDS STEP_AMOUNT * STEP_LENGTH // кол-во светодиодов
LEDdata leds[NUMLEDS];  // буфер ленты
microLED strip(leds, STRIP_PIN, STEP_LENGTH, STEP_AMOUNT, ZIGZAG, LEFT_BOTTOM, DIR_RIGHT);  // объект матрица

int effSpeed;
int8_t effDir;
byte curBright = CUSTOM_BRIGHT;
enum {S_IDLE, S_WORK} systemState = S_IDLE;
enum {RAINBOW, FIRE, COLOR, RUNNING, LAST_ONE} curEffect = START_EFFECT;
#define TOTAL_EFFECTS (LAST_ONE)
byte effectCounter;

// ==== удобные макросы ====
#define FOR_i(from, to) for(int i = (from); i < (to); i++)
#define FOR_j(from, to) for(int j = (from); j < (to); j++)
#define FOR_k(from, to) for(int k = (from); k < (to); k++)
#define EVERY_MS(x) \
  static uint32_t every_ms_tmr;\
  bool flag = millis() - every_ms_tmr >= (x);\
  if (flag) every_ms_tmr = millis();\
  if (flag)
//===========================

// ФЛ для функции Noise
#include "FastLED.h"
int counter = 0;
CRGBPalette16 firePalette;

void setup() {
  Serial.begin(115200);
  //FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUMLEDS).setCorrection( TypicalLEDStrip );
  strip.setBrightness(curBright);    // яркость (0-255)
  strip.clear();
  strip.show();
  // для кнопок
  //pinMode(SENSOR_START, INPUT_PULLUP);
  //pinMode(SENSOR_END, INPUT_PULLUP);
  firePalette = CRGBPalette16(
                  getFireColor(0 * 16),
                  getFireColor(1 * 16),
                  getFireColor(2 * 16),
                  getFireColor(3 * 16),
                  getFireColor(4 * 16),
                  getFireColor(5 * 16),
                  getFireColor(6 * 16),
                  getFireColor(7 * 16),
                  getFireColor(8 * 16),
                  getFireColor(9 * 16),
                  getFireColor(10 * 16),
                  getFireColor(11 * 16),
                  getFireColor(12 * 16),
                  getFireColor(13 * 16),
                  getFireColor(14 * 16),
                  getFireColor(15 * 16)
                );
  delay(100);
  strip.clear();
  strip.show();
  Serial.print("setup: Current effect "); Serial.println(curEffect);
}

void loop() {
  getBright();
  readSensors();
  effectFlow();
}

#define IS_MODE( _check_mode_ ) systemState == (_check_mode_)
#define SET_MODE( _new_mode_ ) systemState = _new_mode_

void getBright() {
#if (AUTO_BRIGHT == 1)
  if (IS_MODE(S_IDLE)) {  // в режиме простоя
    EVERY_MS(3000) {            // каждые 3 сек
      Serial.println(analogRead(PHOTO_PIN));
      curBright = map(analogRead(PHOTO_PIN), 30, 800, 10, 200);
      strip.setBrightness(curBright);
    }
  }
#endif
}

// крутилка эффектов в режиме активной работы
void effectFlow() {
  if (IS_MODE(S_WORK)) {
      EVERY_MS(effSpeed) {
        switch (curEffect) {
          case COLOR: staticColor(effDir, 0, STEP_AMOUNT); break;
          case RAINBOW: rainbowStripes(-effDir, 0, STEP_AMOUNT); break; // rainbowStripes - приёмный
          case FIRE: fireStairs(effDir, 0, 0); break;
          case RUNNING: runningStep(effDir, 0, STEP_AMOUNT); break;
        }
      }
      strip.show();
  }
}

// читаем сенсоры
void readSensors() {
  static bool activated_from_start = false, activated_from_end = false;
  static uint32_t timeoutCounter = millis();
  static short  sensor_fired = 0;

  if (IS_MODE(S_WORK)) {
    // ТАЙМАУТ
    if (millis() - timeoutCounter >= (TIMEOUT * 1000L)) {
      Serial.print("readSensors: Timeout event -> ");Serial.println(millis());
      SET_MODE(S_IDLE);
      int changeBright = curBright;
      while (1) {
        EVERY_MS(50) {
          changeBright -= 5;
          if (changeBright < 0) break;
          strip.setBrightness(changeBright);
          strip.show();
        }
      }
      strip.clear();
      strip.setBrightness(curBright);
      strip.show();
    }
    else {
//      Serial.println("readSensors: Wait for timeout");
    }
  }

  EVERY_MS(50) {
    Serial.print("readSensors: Current effect "); Serial.println(curEffect);
    Serial.print("readSensors: Now -> ");Serial.println(millis());
    if (IS_MODE(S_IDLE) && sensor_fired != 0) {
      // Switch effect while on idle
      if (sensor_fired == SENSOR_START) {
        Serial.println("readSensors: effect direction start -> end");
        effDir = 1;
      } else {
        Serial.println("readSensors: effect direction end -> start");
        effDir = -1;
      }
      sensor_fired = 0;
      if (ROTATE_EFFECTS) {
        Serial.print("readSensors: Rotating effects -> ");Serial.println(curEffect);
        curEffect = ++effectCounter % TOTAL_EFFECTS;
      }
    }
    // СЕНСОР У НАЧАЛА ЛЕСТНИЦЫ
    if (digitalRead(SENSOR_START)) {
      Serial.print("readSensors: Start sensor detected "); Serial.println(SENSOR_START);
      if (!activated_from_start) {
        sensor_fired = SENSOR_START; // Save which sensor was fired
        activated_from_start = true; // Prevent from double initialization
        timeoutCounter = millis();   // Save event time
        Serial.print("readSensors: Event time "); Serial.println(timeoutCounter);
        switch (systemState) {
          case S_IDLE:
            Serial.println("Current mode: IDLE");
            stepFader(1, 0);
            SET_MODE(S_WORK);
            break;
          case S_WORK:
            Serial.println("Current mode: WORK");
            if (effDir == -1) {
              stepFader(0, 1);
              SET_MODE(S_IDLE);
              strip.clear();
              strip.show();
              return;
            }
            break;
        }
      }
    } else {
      if (activated_from_start)
        activated_from_start = false;
    }

    // СЕНСОР У КОНЦА ЛЕСТНИЦЫ
    if (digitalRead(SENSOR_END)) {
      Serial.print("readSensors: Start sensor detected "); Serial.println(SENSOR_END);
      if (!activated_from_end) {
        sensor_fired = SENSOR_END; // Save which sensor was fired
        activated_from_end = true; // Prevent from double initialization
        timeoutCounter = millis(); // Save event time
        Serial.print("readSensors: Event time "); Serial.println(timeoutCounter);
        switch (systemState) {
          case S_IDLE:
            Serial.println("Current mode: IDLE");
            stepFader(0, 0); SET_MODE(S_WORK); break;
          case S_WORK:
            Serial.println("Current mode: WORK");
            if (effDir == 1) {
              stepFader(1, 1); SET_MODE(S_IDLE);
              strip.clear(); strip.show(); return;
            }
            break;
        }
      }
    } else {
      if (activated_from_end)
        activated_from_end = false;
    }
  }
}

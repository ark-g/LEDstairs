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
#define START_EFFECT   COLOR    // режим при старте COLOR, RAINBOW, FIRE
#define ROTATE_EFFECTS 0      // 0/1 - автосмена эффектов
#define TIMEOUT 15            // секунд, таймаут выключения ступенек, если не сработал конечный датчик

// пины
// если перепутаны сенсоры - можно поменять их местами в коде! Вот тут
#define SENSOR_START 3
#define SENSOR_END 2
#define STRIP_PIN 13    // пин ленты
#define PHOTO_PIN A0

#define DIR_S2E   0     // Start to End
#define DIR_E2S   1     // End to Start

#define PWR_ON_2_OFF   0     // Turn on power
#define PWR_OFF_2_ON   1     // Turn off power

typedef enum {EVENT_NONE,
              EVENT_START,
              EVENT_END,
              EVENT_TIMEOUT,
              EVENT_LAST_ONE} event_t;

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
  processEvent( getEvent() );
  effectFlow();
  //readSensors();
  //effectFlow();
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
          case COLOR: staticColor(effDir, 0, STEP_AMOUNT-1); break;
          case RAINBOW: rainbowStripes(-effDir, 0, STEP_AMOUNT); break; // rainbowStripes - приёмный
          case FIRE: fireStairs(effDir, 0, 0); break;
          case RUNNING: runningStep(effDir, 0, STEP_AMOUNT); break;
        }
      }
      strip.show();
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get system event
#define SENSOR_EVENT_TIMEOUT 2  // Sensor double detection prevention timeout in seconds

event_t getEvent(void) {
  event_t event = EVENT_NONE;
  static bool sensor_active = true;
  static uint32_t last_event_ts = 0;

  // Timeout for double sensor activation
  if ( sensor_active == false &&
       (millis() - last_event_ts >= SENSOR_EVENT_TIMEOUT * 1000) ) {
    sensor_active = true;
  }

  // СЕНСОР У НАЧАЛА ЛЕСТНИЦЫ
  if (digitalRead(SENSOR_START)) {
    if ( sensor_active ) {
      Serial.print("getEvent: Start sensor detected "); Serial.println(SENSOR_START);
      sensor_active = false;       // Prevent from double initialization till timeout event
      last_event_ts = millis();    // Save event time
      Serial.print("getEvent: Event time "); Serial.println(last_event_ts);
      return EVENT_START;
    }
  }
  // СЕНСОР У КОНЦА ЛЕСТНИЦЫ
  if (digitalRead(SENSOR_END)) {
    if (sensor_active) {
      Serial.print("getEvent: End sensor detected "); Serial.println(SENSOR_END);
      sensor_active = false;     // Prevent from double initialization till next timeout event
      last_event_ts = millis();  // Save event time
      Serial.print("getEvent: Event time "); Serial.println(last_event_ts);
      return EVENT_END;
    }
  }

  if ( IS_MODE(S_WORK) && (millis() - last_event_ts >= (TIMEOUT * 1000)) ) {
    Serial.print("getEvent: Timeout event: "); Serial.println(millis());
    sensor_active = true;
    last_event_ts = millis();  // Save event time
    return EVENT_TIMEOUT;
  }

  return EVENT_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

int turnOn( int eff_dir, int effect ) {
  int step_start, step_inc, step_end;
  int step_counter = STEP_AMOUNT;

  Serial.print("turnOn: direction ");Serial.print(eff_dir);
  Serial.print(" effect ");Serial.println(effect);
  effDir = eff_dir;
  if (eff_dir == DIR_S2E) {
    step_start = 0;
    step_inc = 1;
    step_end = STEP_AMOUNT;
  } else {
    step_start = STEP_AMOUNT - 1;
    step_inc = -1;
    step_end = 0;
  }
  step_counter = step_start;
  while( step_counter != step_end ) {
    EVERY_MS(FADR_SPEED) {
      switch (effect) {
        case RUNNING:
          runningStep(eff_dir, 0, STEP_AMOUNT);
          break;
        case COLOR:
          staticColor(eff_dir, step_start, step_counter);
          break;
      }
      strip.show();
      step_counter += step_inc;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
#define FADEOUT_STEP   3

int turnOff( int eff_dir, int effect ) {
  Serial.print("turnOff: direction ");Serial.print(eff_dir);
  Serial.print(" effect ");Serial.println(effect);
  int fadeout_bright = curBright;

  while( fadeout_bright >= FADEOUT_STEP ) {
    EVERY_MS(50) {
      fadeout_bright -= FADEOUT_STEP;
      strip.setBrightness(fadeout_bright);
      strip.show();
    }
  }
  strip.clear();
  strip.setBrightness(curBright);
  strip.show();
  Serial.println("turnOff: Done");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

int processEvent( event_t event ) {
  int effect_dir;
  static int curr_effect = START_EFFECT;
  static event_t prev_event;

  switch (event) {
    case EVENT_START:
      Serial.println("processEvent: event start");
      switch (systemState) {
        case S_WORK:
          turnOff(DIR_E2S, curr_effect);
          SET_MODE(S_IDLE);
          break;
        case S_IDLE:
          turnOn(DIR_S2E, curr_effect);
          SET_MODE(S_WORK);
          break;
      }
      break;
    case EVENT_END:
      Serial.println("processEvent: event end");
      switch (systemState) {
        case S_WORK:
          turnOff(DIR_S2E, curr_effect);
          SET_MODE(S_IDLE);
          break;
        case S_IDLE:
          turnOn(DIR_E2S, curr_effect);
          SET_MODE(S_WORK);
          break;
      }
      break;
    case EVENT_TIMEOUT:
      Serial.println("processEvent: event timeout");
      // Look on what was the last event to set effect direction for fadeout
      switch (prev_event) {
        case EVENT_START:
          effect_dir = DIR_S2E;
          break;
        case EVENT_END:
          effect_dir = DIR_E2S;
          break;
        default:
          // Should not happen, but just in case ...
          effect_dir = DIR_S2E;
          break;
      }
      switch (systemState) {
        case S_WORK:
          turnOff(effect_dir, curr_effect);
          SET_MODE(S_IDLE);
          break;
        default:
          break;
      }
      break;
    case EVENT_NONE:
    default:
      // Non meaningfull event - do not count it, just report.
      return 0;
  }

  if ( IS_MODE(S_IDLE) && ROTATE_EFFECTS ) {
    Serial.print("processEvent: Rotating effects -> ");Serial.println(curr_effect);
    curr_effect = ++curr_effect % TOTAL_EFFECTS;
  }
  // Save meaningfull event
  prev_event = event;
  return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

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
      fadeout(effDir, curBright);
    }
    else {
//      Serial.println("readSensors: Wait for timeout");
    }
  }

  EVERY_MS(50) {
    if (sensor_fired != 0) {
      // On sensor activity detection change system state accordingly
      switch (systemState) {
        case S_IDLE:
          Serial.println("Current mode: IDLE");
          stepFader(effDir, PWR_OFF_2_ON);
          SET_MODE(S_WORK);
          break;
        case S_WORK:
          Serial.println("Current mode: WORK");
          fadeout(effDir, curBright);
          SET_MODE(S_IDLE);
          break;
      }
      sensor_fired = 0;
      // Switch effect while on idle
      if ( IS_MODE(S_IDLE) ) {
        if (ROTATE_EFFECTS) {
          Serial.print("readSensors: Rotating effects -> ");Serial.println(curEffect);
          curEffect = ++effectCounter % TOTAL_EFFECTS;
        }
      }
    }
    // СЕНСОР У НАЧАЛА ЛЕСТНИЦЫ
    if (digitalRead(SENSOR_START)) {
      Serial.print("readSensors: Start sensor detected "); Serial.println(SENSOR_START);
      if (!activated_from_start) {
        sensor_fired = SENSOR_START; // Save which sensor was fired
        activated_from_start = true; // Prevent from double initialization
        timeoutCounter = millis();   // Save event time
        Serial.println("readSensors: effect direction start -> end");
        effDir = DIR_S2E;
        Serial.print("readSensors: Event time "); Serial.println(timeoutCounter);
      }
    } else {
      if (activated_from_start)
        activated_from_start = false;
    }

    // СЕНСОР У КОНЦА ЛЕСТНИЦЫ
    if (digitalRead(SENSOR_END)) {
      Serial.print("readSensors: End sensor detected "); Serial.println(SENSOR_END);
      if (!activated_from_end) {
        sensor_fired = SENSOR_END; // Save which sensor was fired
        activated_from_end = true; // Prevent from double initialization
        timeoutCounter = millis(); // Save event time
        Serial.println("readSensors: effect direction end -> start");
        effDir = DIR_E2S;
      }
    } else {
      if (activated_from_end)
        activated_from_end = false;
    }
  }
}

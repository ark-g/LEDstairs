/*
  Скетч к проекту "Подсветка лестницы"
  Страница проекта (схемы, описания): https://alexgyver.ru/ledstairs/
  Исходники на GitHub: https://github.com/AlexGyver/LEDstairs
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2019
  https://AlexGyver.ru/
*/

#define STEP_AMOUNT 12     // количество ступенек
#define STEP_LENGTH 8      // количество чипов WS2811 на ступеньку

#define AUTO_BRIGHT 0      // автояркость 0/1 вкл/выкл (с фоторезистором)
#define CUSTOM_BRIGHT 150  // ручная яркость

#define FADR_SPEED 500
#define START_EFFECT       RUNNING  // режим при старте COLOR, RAINBOW, FIRE, RUNNING
#define ROTATE_EFFECTS     1        // 0/1 - автосмена эффектов
#define TIMEOUT 30                  // секунд, таймаут выключения ступенек, если не сработал конечный датчик

// пины
// если перепутаны сенсоры - можно поменять их местами в коде! Вот тут
#define SENSOR_START 3
#define SENSOR_END 2
#define STRIP_PIN 13    // пин ленты
#define PHOTO_PIN A0

#define DIR_S2E   0     // Start to End
#define DIR_E2S   1     // End to Start

typedef enum {EVENT_NONE,
              EVENT_START,
              EVENT_END,
              EVENT_TIMEOUT,
              EVENT_LAST_ONE} event_t;

// для разработчиков
#define ORDER_BRG       // порядок цветов ORDER_GRB / ORDER_RGB / ORDER_BRG
#define COLOR_DEBTH 3   // цветовая глубина: 1, 2, 3 (в байтах)
//#define DEBUG         // Uncomment for debug prints

#include "microLED.h"
#define NUMLEDS STEP_AMOUNT * STEP_LENGTH // кол-во светодиодов
LEDdata leds[NUMLEDS];  // буфер ленты
microLED strip(leds, STRIP_PIN, STEP_LENGTH, STEP_AMOUNT, ZIGZAG, LEFT_BOTTOM, DIR_RIGHT);  // объект матрица

int effSpeed;
int8_t effDir;
byte curBright = CUSTOM_BRIGHT;
enum {S_IDLE, S_WORK} systemState = S_IDLE;
// ========================
enum {RAINBOW, FIRE, COLOR, RUNNING, LAST_ONE} gCurEffect = START_EFFECT;
#define EFFECT_GET()          gCurEffect
#define EFFECT_SET(_new_eff_) gCurEffect = _new_eff_
#define TOTAL_EFFECTS (LAST_ONE)

// ==== Debug tools =====
#ifdef DEBUG
#define DPRINT(_prm_)    Serial.print(_prm_)
#define DPRINTLN(_prm_)  Serial.println(_prm_)
#else
#define DPRINT(_prm_)
#define DPRINTLN(_prm_)
#endif

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
  DPRINT("setup: Current effect "); DPRINTLN(EFFECT_GET());
}

void loop() {
  getBright();
  processEvent( getEvent() );
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
        switch (EFFECT_GET()) {
          case COLOR: staticColor(effDir, 0, STEP_AMOUNT-1); break;
          case RAINBOW: rainbowStripes(effDir, effDir == DIR_S2E ? 0 : STEP_AMOUNT-1, STEP_AMOUNT); break;
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
      DPRINT("getEvent: Start sensor detected "); DPRINTLN(SENSOR_START);
      sensor_active = false;       // Prevent from double initialization till timeout event
      last_event_ts = millis();    // Save event time
      DPRINT("getEvent: Event time "); DPRINTLN(last_event_ts);
      return EVENT_START;
    }
  }
  // СЕНСОР У КОНЦА ЛЕСТНИЦЫ
  if (digitalRead(SENSOR_END)) {
    if (sensor_active) {
      DPRINT("getEvent: End sensor detected "); DPRINTLN(SENSOR_END);
      sensor_active = false;     // Prevent from double initialization till next timeout event
      last_event_ts = millis();  // Save event time
      DPRINT("getEvent: Event time "); DPRINTLN(last_event_ts);
      return EVENT_END;
    }
  }

  if ( IS_MODE(S_WORK) && (millis() - last_event_ts >= (TIMEOUT * 1000)) ) {
    DPRINT("getEvent: Timeout event: "); DPRINTLN(millis());
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

  DPRINT("turnOn: direction ");DPRINT(eff_dir); DPRINT(" effect ");DPRINTLN(effect);
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
  DPRINT("turnOn: start ");DPRINT(step_start); DPRINT(" stop ");DPRINTLN(step_end);
  while( step_counter != step_end ) {
    EVERY_MS(FADR_SPEED) {
      DPRINT("turnOn: cycle ");DPRINTLN(step_counter);
      switch (effect) {
        case FIRE:
          fireStairs(eff_dir, 0, 0);
          // No need to pass on each step, just jump to real effect flow
          return;
          break;
        case RUNNING:
          runningStep(eff_dir, 0, STEP_AMOUNT);
          break;
        case COLOR:
          staticColor(eff_dir, step_start, step_counter);
          break;
        case RAINBOW:
          rainbowStripes(eff_dir, step_start, abs(step_start - step_counter));
          break;
      }
      strip.show();
      step_counter += step_inc;
    }
  }
  DPRINTLN("turnOn: Done");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
#define FADEOUT_STEP   3

int turnOff( int eff_dir, int effect ) {
  DPRINT("turnOff: direction "); DPRINT(eff_dir); DPRINT(" effect "); DPRINTLN(effect);
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
  DPRINTLN("turnOff: Done");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

int processEvent( event_t event ) {
  int effect_dir;
  int currEff;
  static event_t prev_event;

  switch (event) {
    case EVENT_START:
      DPRINTLN("processEvent: event start");
      switch (systemState) {
        case S_WORK:
          turnOff(DIR_E2S, EFFECT_GET());
          SET_MODE(S_IDLE);
          break;
        case S_IDLE:
          turnOn(DIR_S2E, EFFECT_GET());
          SET_MODE(S_WORK);
          break;
      }
      break;
    case EVENT_END:
      DPRINTLN("processEvent: event end");
      switch (systemState) {
        case S_WORK:
          turnOff(DIR_S2E, EFFECT_GET());
          SET_MODE(S_IDLE);
          break;
        case S_IDLE:
          turnOn(DIR_E2S, EFFECT_GET());
          SET_MODE(S_WORK);
          break;
      }
      break;
    case EVENT_TIMEOUT:
      DPRINTLN("processEvent: event timeout");
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
          turnOff(effect_dir, EFFECT_GET());
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
    DPRINT("processEvent: Rotating effects -> ");DPRINTLN(EFFECT_GET());
    currEff = EFFECT_GET();
    EFFECT_SET(++currEff % TOTAL_EFFECTS);
  }
  // Save meaningfull event
  prev_event = event;
  DPRINTLN("processEvent: event processed");
  return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

// ============== ЭФФЕКТЫ =============
// ========= огонь
// настройки пламени
#define HUE_GAP 45      // заброс по hue
#define FIRE_STEP 90    // шаг изменения "языков" пламени
#define HUE_START 2     // начальный цвет огня (0 красный, 80 зелёный, 140 молния, 190 розовый)
#define MIN_BRIGHT 150  // мин. яркость огня
#define MAX_BRIGHT 255  // макс. яркость огня
#define MIN_SAT 220     // мин. насыщенность
#define MAX_SAT 255     // макс. насыщенность

void fireStairs(int8_t dir, byte from, byte to) {
  effSpeed = 30;
  static uint16_t counter = 0;
  FOR_i(0, STEP_LENGTH) {
    FOR_j(0, STEP_AMOUNT) {
      strip.setPix(i, j, mHEX(getPixColor(ColorFromPalette(
                                            firePalette,
                                            (inoise8(i * FIRE_STEP, j * FIRE_STEP, counter)),
                                            255,
                                            LINEARBLEND
                                          ))));
    }
  }
  counter += 10;
}
uint32_t getPixColor(CRGB thisPixel) {
  return (((uint32_t)thisPixel.r << 16) | (thisPixel.g << 8) | thisPixel.b);
}
CRGB getFireColor(int val) {
  // чем больше val, тем сильнее сдвигается цвет, падает насыщеность и растёт яркость
  return CHSV(
           HUE_START + map(val, 0, 255, 0, HUE_GAP),                    // H
           constrain(map(val, 0, 255, MAX_SAT, MIN_SAT), 0, 255),       // S
           constrain(map(val, 0, 255, MIN_BRIGHT, MAX_BRIGHT), 0, 255)  // V
         );
}

// ========= смена цвета общая
void staticColor(byte eff_dir, byte from, byte to) {
  int i;
  static uint32_t colorCounter = 0;

  effSpeed = 100;
  colorCounter += 4;
  if ( from < to ) {
    for(i = from; i <= to; i++) {
      fillStep(i, mHSV(colorCounter, 255, 255));
    }
  } else {
    for(i = from; i >= to; i--) {
      fillStep(i, mHSV(colorCounter, 255, 255));
    }
  }
}

// ========= Running
void runningStep(byte eff_dir, byte from, byte num) {
  effSpeed = 100;
  static int cntr = 0;
  LEDdata color;
  // Need to declare volatile here, because with small number of STEP_LENGTH
  // somtimes funtcion probably optimized out by compiler ? sic !?
  volatile int i, k;

//  Serial.print(from); Serial.print(" to "); Serial.println(num);
  color = ( cntr % 3 == 0 ) ? WHITE : BLACK;
  if ( eff_dir == DIR_S2E ) {
    for( i = STEP_AMOUNT-1; i >= 1; i--) {
      for( k = i * STEP_LENGTH; k < i * STEP_LENGTH + STEP_LENGTH; k++) {
        leds[k] = leds[k - STEP_LENGTH];
      }
    }
//    Serial.println("Shift forward");
    fillStep(0, color);
  } else {
    for( i = 0; i <= STEP_AMOUNT-2; i++ ) {
      for( k = i * STEP_LENGTH; k < i * STEP_LENGTH + STEP_LENGTH; k++) {
        leds[k] = leds[k + STEP_LENGTH];
      }
    }
//    Serial.println("Shift backward");
    fillStep(STEP_AMOUNT-1, color);
  }
  cntr++;
}

// ========= полоски радужные
void rainbowStripes(byte eff_dir, byte from, byte num) {
  effSpeed = 40;
  static byte colorCounter = 0;
  colorCounter += 3;
  volatile int i;

  //Serial.print("Rainbow: "); Serial.print(from); Serial.print(" steps "); Serial.println(num);
  if ( eff_dir == DIR_S2E ) {
//    Serial.println("Rainbow: start -> end");
    for( i = from; i < from + num; i++) {
      fillStep(i, mHSV(colorCounter + (float)i * 255 / STEP_AMOUNT, 255, 255));
    }
  } else {
//    Serial.println("Rainbow: end -> start");
    for( i = from; i > from-num; i-- ) {
      fillStep( i, mHSV(colorCounter + (float)(STEP_AMOUNT-i) * 255 / STEP_AMOUNT, 255, 255));
    }
  }
}

// ========= залить ступеньку цветом (служебное)
void fillStep(int8_t num, LEDdata color) {
  if (num >= STEP_AMOUNT || num < 0) return;
  FOR_i(num * STEP_LENGTH, num * STEP_LENGTH + STEP_LENGTH) {
    leds[i] = color;
  }
}

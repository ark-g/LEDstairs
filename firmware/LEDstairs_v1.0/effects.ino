// плавный включатель-выключатель эффектов
void stepFader(byte eff_direction, byte pwr_transition) {
  // eff_direction is one of:
  //    DIR_S2E        Start to End
  //    DIR_E2S        End to Start
  // pwr_transition is one of:
  //    PWR_ON_2_OFF   Turn on power
  //    PWR_OFF_2_ON   Turn off power
  byte counter = 0;
  while (1) {
    EVERY_MS(FADR_SPEED) {
      counter++;
      Serial.print("stepFader: Current effect "); Serial.println(curEffect);
      switch (curEffect) {
        case RUNNING:
            runningStep( eff_direction, 0, STEP_AMOUNT );
          break;
 /*
        case COLOR:
          switch (mode) {
            case 0: staticColor(1, 0, counter); break;
            case 1: staticColor(1, counter, STEP_AMOUNT); break;
            case 2: staticColor(-1, STEP_AMOUNT - counter, STEP_AMOUNT); break;
            case 3: staticColor(-1, 0, STEP_AMOUNT - counter); break;
          }
          break;
        case RAINBOW:
          switch (mode) {
            case 0: rainbowStripes(-1, STEP_AMOUNT - counter, STEP_AMOUNT); break;
            case 1: rainbowStripes(-1, 0, STEP_AMOUNT - counter); break;
            case 2: rainbowStripes(1, STEP_AMOUNT - counter, STEP_AMOUNT); break;
            case 3: rainbowStripes(1, 0, STEP_AMOUNT - counter); break;
          }
          break;
 */
        case FIRE:
          if (pwr_transition == PWR_OFF_2_ON ) {
            int changeBright = curBright;
            while (1) {
              EVERY_MS(50) {
                changeBright -= 5;
                if (changeBright < 0) break;
                strip.setBrightness(changeBright);
                fireStairs(0, 0, 0);
                strip.show();
              }
            }
            strip.clear();
            strip.setBrightness(curBright);
            strip.show();
          } else {
            int changeBright = 0;
            strip.setBrightness(0);
            while (1) {
              EVERY_MS(50) {
                changeBright += 5;
                if (changeBright > curBright) break;
                strip.setBrightness(changeBright);
                fireStairs(0, 0, 0);
                strip.show();
              }
              strip.setBrightness(curBright);
            }
          }
          return;
          break;
      }
      strip.show();
      if (counter == STEP_AMOUNT) break;
    }
  }
  if (pwr_transition == PWR_OFF_2_ON) {
    strip.clear();
    strip.show();
  }
}

int fadeout(int eff_direction, int startBright) {
  Serial.print("fadeout: Start event ");Serial.println(millis());
  static int fadeout_bright = CUSTOM_BRIGHT;

  if ( fadeout_bright <= 0 ) {
    fadeout_bright = startBright;
  }

  while( fadeout_bright >= 0 ) {
    EVERY_MS(50) {
      fadeout_bright -= 2;
      strip.setBrightness(fadeout_bright);
      strip.show();
    }
  }
  strip.clear();
  strip.setBrightness(0);
  strip.show();
  fadeout_bright = 0;
  Serial.print("fadeout: End event ");Serial.println(millis());
  return 0;
}


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
void staticColor(int8_t dir, byte from, byte to) {
  effSpeed = 100;
  byte thisBright;
  static byte colorCounter = 0;
  colorCounter += 2;
  FOR_i(0, STEP_AMOUNT) {
    thisBright = 255;
    if (i < from || i >= to) thisBright = 0;
    fillStep(i, mHSV(colorCounter, 255, thisBright));
  }
}

// ========= Running
void runningStep(byte eff_dir, byte from, byte to) {
  effSpeed = 100;
  static int cntr = 0;
  LEDdata color;
  int i, k;
//  Serial.print(from); Serial.print(" to "); Serial.println(to);
  color = ( cntr % 3 == 0 ) ? WHITE : BLACK;
  if ( eff_dir == DIR_S2E ) {
    for( i = STEP_AMOUNT-1; i >= 1; i--) {
      for( k = i * STEP_LENGTH; k < i * STEP_LENGTH + STEP_LENGTH; k++) {
        leds[k] = leds[k - STEP_LENGTH];
      }
    }
    Serial.println("Shift forward");
    fillStep(0, color);
  } else {
    for( i = 0; i <= STEP_AMOUNT-2; i++ ) {
      for( k = i * STEP_LENGTH; k < i * STEP_LENGTH + STEP_LENGTH; k++) {
        leds[k] = leds[k + STEP_LENGTH];
      }
    }
    Serial.println("Shift backward");
    fillStep(STEP_AMOUNT-1, color);
  }
  cntr++;
}

// ========= полоски радужные
void rainbowStripes(int8_t dir, byte from, byte to) {
  effSpeed = 40;
  static byte colorCounter = 0;
  colorCounter += 2;
  byte thisBright;
  FOR_i(0, STEP_AMOUNT) {
    thisBright = 255;
    if (i < from || i >= to) thisBright = 0;
    fillStep((dir == DIR_S2E) ? (i) : (STEP_AMOUNT - 1 - i), mHSV(colorCounter + (float)i * 255 / STEP_AMOUNT, 255, thisBright));
  }
}

// ========= залить ступеньку цветом (служебное)
void fillStep(int8_t num, LEDdata color) {
  if (num >= STEP_AMOUNT || num < 0) return;
  FOR_i(num * STEP_LENGTH, num * STEP_LENGTH + STEP_LENGTH) {
    leds[i] = color;
  }
}

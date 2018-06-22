#include <JC_Button.h>
#include <bitswap.h>
#include <chipsets.h>
#include <color.h>
#include <colorpalettes.h>
#include <colorutils.h>
#include <controller.h>
#include <cpp_compat.h>
#include <dmx.h>
#include <FastLED.h>
#include <fastled_config.h>
#include <fastled_delay.h>
#include <fastled_progmem.h>
#include <fastpin.h>
#include <fastspi.h>
#include <fastspi_bitbang.h>
#include <fastspi_dma.h>
#include <fastspi_nop.h>
#include <fastspi_ref.h>
#include <fastspi_types.h>
#include <hsv2rgb.h>
#include <led_sysdefs.h>
#include <lib8tion.h>
#include <noise.h>
#include <pixelset.h>
#include <pixeltypes.h>
#include <platforms.h>
#include <power_mgt.h>

// Fixed definitions

#define LED_DT 8                                             // Serial data pin
#define BTN_DT 4                                             // Button  Data Pin
#define LED_TYPE WS2813                                      // WS2813s have 2 data lines so that even if an LED breaks, the rest continue to work
#define COLOR_ORDER GRB                                      // GRB for WS family
#define NUM_LEDS 75                                          // Number of LED's
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

Button patternBtn(BTN_DT);                                   // Declare the button

// Global variables.

uint8_t maxBrightness = 40;                                  // Overall brightness definition. It can be changed on the fly.
struct CRGB leds[NUM_LEDS];                                  // Initialize our LED array.
uint8_t fastDelay = 5;                                       // slowRainbow() delay in Ms
uint8_t slowDelay = 25;                                      // fastRainbow() delay in Ms
uint8_t deltahue = 2;                                        // Hue change between pixels for rainbow
uint8_t gHue = 0;                                            // rotating "base color" used by many of the patterns
uint8_t baseHue = 0;                                            // rotating "base color" used by many of the patterns
boolean patternOn = false;                                   // reusable bool for flashing patterns
uint16_t heartFlashDelay = 1000;                              // For heartPattern()
uint8_t BeatsPerMinute = 30;                                 // For bpm()

//Pixel Midpoints map for convenient reference
//    05    06
// 17    16    15
//    27    28
// 38   NONE   37
//    47    48
// 60    59    58
//    69    70

// Core code

void setup() {                                               // Runs on startup
  Serial.begin(57600);
  delay(3000);                                               // 3 second delay to stabilize power draw
  patternBtn.begin();
  LEDS.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(maxBrightness);
  set_max_power_in_volts_and_milliamps(5, 1000);             // Power management - 5V, 1000mA. My powerboost is rated for ~1A continuous
}

typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {allBlack, juggle, confetti, bpm, heartPattern, slowRainbow, fastRainbow};
uint8_t gCurrentPatternNumber = 0;                           // Index number of which pattern is current

void loop () {                                               // Runs... more often than setup?
  gPatterns[gCurrentPatternNumber]();                        // Call the current pattern function
  show_at_max_brightness_for_power();
  processButton();                                           // Read the button
  EVERY_N_MILLISECONDS( 20 ) {
    gHue++;  // Cycle the base color through the rainbow
  }
}

void processButton() {                                       // Read the button
  patternBtn.read();
  if (patternBtn.wasReleased()) {
    nextPattern();
  }
}


// Patterns

void allBlack() {
  fill_solid(&leds[0], 75, CRGB::Black);
  leds[0] = CRGB::Green;                                     // This LED is mounted on the side of the mask and indicates the mask is on
}

void nextPattern()                                           // Cycles patterns. Wraps at the end.
{
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void heartPattern() {
  EVERY_N_MILLISECONDS(heartFlashDelay) {
    flashHeart();
  }
}

void flashHeart() {
  fill_solid(&leds[0], 75, CRGB::Chartreuse);                // Green Background
  if (patternOn == false) {
    fill_solid(&leds[3],  2, CRGB::DarkMagenta);
    fill_solid(&leds[7],  2, CRGB::DarkMagenta);
    fill_solid(&leds[13], 3, CRGB::DarkMagenta);
    fill_solid(&leds[17], 3, CRGB::DarkMagenta);
    fill_solid(&leds[24], 8, CRGB::DarkMagenta);
    fill_solid(&leds[37], 2, CRGB::DarkMagenta);
    fill_solid(&leds[45], 6, CRGB::DarkMagenta);
    fill_solid(&leds[57], 5, CRGB::DarkMagenta);
    fill_solid(&leds[68], 4, CRGB::DarkMagenta);
    patternOn = true;
  }
  else {
    patternOn = false;
  }
}

void confetti()                                              // Random colored speckles that blink in and fade smoothly
{
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void bpm()                                                   // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
{
  CRGBPalette16 palette = RainbowStripeColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void slowRainbow() {
  EVERY_N_MILLISECONDS(slowDelay) {
    rainbowMarch();
  }
}

void fastRainbow() {
  EVERY_N_MILLISECONDS(fastDelay) {
    rainbowMarch();
  }
}

void rainbowMarch() {
  baseHue++;                                                  // Increment the starting hue.
  fill_rainbow(leds, NUM_LEDS, baseHue, deltahue);            // Use FastLED's fill_rainbow routine.
}

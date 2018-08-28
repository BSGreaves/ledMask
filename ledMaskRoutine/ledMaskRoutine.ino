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

// For Handsfree Patterns
#define FORWARD 0
#define BACKWARD 1
#define SLOW 250
#define MEDIUM 50
#define FAST 5
boolean direction = FORWARD;

Button patternBtn(BTN_DT);                                   // Declare the button

// Global variables.

uint8_t maxBrightness = 40;                                  // Overall brightness definition. It can be changed on the fly.
struct CRGB leds[NUM_LEDS];                                  // Initialize our LED array.
uint8_t fastDelay = 5;                                       // slowRainbow() delay in Ms
uint8_t slowDelay = 18;                                      // fastRainbow() delay in Ms
uint8_t deltahue = 2;                                        // Hue change between pixels for rainbow
uint8_t gHue = 0;                                            // rotating "base color" used by many of the patterns
uint8_t baseHue = 0;                                         // rotating "base color" used by many of the patterns
boolean patternOn = false;                                   // reusable bool for flashing patterns
uint16_t heartFlashDelay = 1000;                             // For heartPattern()
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
SimplePatternList gPatterns = {allBlack, allBlack, juggle, confetti, bpm, heartPattern, slowRainbow, alternatingStripes, fastRainbow};
SimplePatternList gRandomPatterns = {alternatingStripes, juggle, confetti, bpm, slowRainbow, fastRainbow};
uint8_t gCurrentPatternNumber = 0;                           // Index number of which pattern is current
uint8_t gRandomPatternNumber = 0;                            // For the handsfree pattern manager

void loop () {                                               // Runs... more often than setup?
  if (gCurrentPatternNumber == 1) 
  {
    gRandomPatterns[gRandomPatternNumber]();
  }
  else
  {
    gPatterns[gCurrentPatternNumber]();                      // Call the current pattern function
  }
  show_at_max_brightness_for_power();
  processButton();                                           // Read the button
  EVERY_N_MILLISECONDS(20) {
    gHue++;                                                  // Cycle the base color through the rainbow
  }
  EVERY_N_SECONDS(45) {
    gRandomPatternNumber = random(6);
  }
}

void processButton() {                                       
  patternBtn.read();
  if (patternBtn.wasReleased()) {
    nextPattern();
  }
}

void nextPattern() {                                           // Cycles patterns. Wraps at the end.
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

// Patterns

void handsfreePatternManager() {
  gRandomPatterns[gRandomPatternNumber]();
  EVERY_N_SECONDS(30) {
    gRandomPatternNumber = random(6);
  }
}

void allBlack() {
  fill_solid(&leds[0], 75, CRGB::Black);
  leds[0] = CRGB::Green;                                     // This LED is mounted on the side of the mask and indicates the mask is on
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

void alternatingStripes() {
    CRGB color1 = randomColor();
    CRGB color2 = randomColor();
    stripes(color1, color2, (random(5) + 1));
    delay(500);
}

void lightningTrigger() {
    lightning(NULL,15,50,MEDIUM);
}

void testPatterns() {

  rainbow(0,NULL);
  delay(3000);
  colorWipe(CRGB::Black,FORWARD,FAST);
  allRandom();
  delay(3000);
  disolve(15,100,MEDIUM);

  for(int i=0; i<3; i++){
    CRGB c1 = randomColor();
    CRGB c2 = randomColor();
    stripes(c1,c2,3);
    delay(2000);
    stripes(c2,c1,5);
    delay(2000);
    stripes(c1,c2,7);
    delay(2000);
  }

  for(int i=0; i<2; i++){
    cylon(randomColor(), 10,FAST);
  }

  lightning(NULL,15,50,MEDIUM);
  lightning(CRGB::White,20,50,MEDIUM);

  for(int i=0; i<3; i++){
    theaterChase(randomColor(),10,SLOW);
  }

  theaterChaseRainbow(1,MEDIUM);

  rainbow(FAST,1);
  
  flash(NULL,1000,100);

  for(int i=0; i<20; i++){ 
    colorWipe(randomColor(),FAST,direction);
    direction = !direction;
  }
}

// Changes all LEDS to given color
void allColor(CRGB c){
  for(int i=0; i<NUM_LEDS; i++){
    leds[i] = c;
  }
  FastLED.show();
}

void allRandom(){
  for(int i=0; i<NUM_LEDS; i++){
    leds[i] = randomColor();
  }
  FastLED.show(); 
}

// Random disolve colors
void disolve(int simultaneous, int cycles, int speed){
  for(int i=0; i<cycles; i++){
    for(int j=0; j<simultaneous; j++){
      int idx = random(NUM_LEDS);
      leds[idx] = CRGB::Black;
    }
    FastLED.show();
    delay(speed);
  }

  allColor(CRGB::Black);
}

// Flashes given color
// If c==NULL, random color flash
void flash(CRGB c, int count, int speed){
  for(int i=0; i<count; i++){
    if(c){
      allColor(c);
    }
    else{
      allColor(randomColor());
    }
    delay(speed);
    allColor(CRGB::Black);
    delay(speed);
  }
}

// Wipes color from end to end
void colorWipe(CRGB c, int speed, int direction){
  for(int i=0; i<NUM_LEDS; i++){
    if(direction == FORWARD){
      leds[i] = c;
    }
    else{
      leds[NUM_LEDS-1-i] = c;
    }
    FastLED.show();
    delay(speed);
  }
}

// Rainbow colors that slowly cycle across LEDs
void rainbow(int cycles, int speed){ // TODO direction
  if(cycles == 0){
    for(int i=0; i< NUM_LEDS; i++) {
      leds[i] = Wheel(((i * 256 / NUM_LEDS)) & 255);
    }
    FastLED.show();
  }
  else{
    for(int j=0; j<256*cycles; j++) {
      for(int i=0; i< NUM_LEDS; i++) {
        leds[i] = Wheel(((i * 256 / NUM_LEDS) + j) & 255);
      }
      FastLED.show();
      delay(speed);
    }
  }
}

// Theater-style crawling lights
void theaterChase(CRGB c, int cycles, int speed){ // TODO direction

  for (int j=0; j<cycles; j++) {  
    for (int q=0; q < 3; q++) {
      for (int i=0; i < NUM_LEDS; i=i+3) {
        int pos = i+q;
        leds[pos] = c;    //turn every third pixel on
      }
      FastLED.show();

      delay(speed);

      for (int i=0; i < NUM_LEDS; i=i+3) {
        leds[i+q] = CRGB::Black;        //turn every third pixel off
      }
    }
  }
}

// Theater-style crawling lights with rainbow effect
void theaterChaseRainbow(int cycles, int speed){ // TODO direction, duration
  for (int j=0; j < 256 * cycles; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (int i=0; i < NUM_LEDS; i=i+3) {
        int pos = i+q;
        leds[pos] = Wheel( (i+j) % 255);    //turn every third pixel on
      }
      FastLED.show();

      delay(speed);

      for (int i=0; i < NUM_LEDS; i=i+3) {
        leds[i+q] = CRGB::Black;  //turn every third pixel off
      }
    }
  }
}

// Random flashes of lightning
void lightning(CRGB c, int simultaneous, int cycles, int speed){
  int flashes[simultaneous];

  for(int i=0; i<cycles; i++){
    for(int j=0; j<simultaneous; j++){
      int idx = random(NUM_LEDS);
      flashes[j] = idx;
      leds[idx] = c ? c : randomColor();
    }
    FastLED.show();
    delay(speed);
    for(int s=0; s<simultaneous; s++){
      leds[flashes[s]] = CRGB::Black;
    }
    delay(speed);
  }
}

// Sliding bar across LEDs
void cylon(CRGB c, int width, int speed){
  // First slide the leds in one direction
  for(int i = 0; i <= NUM_LEDS-width; i++) {
    for(int j=0; j<width; j++){
      leds[i+j] = c;
    }

    FastLED.show();

    // now that we've shown the leds, reset to black for next loop
    for(int j=0; j<5; j++){
      leds[i+j] = CRGB::Black;
    }
    delay(speed);
  }

  // Now go in the other direction.  
  for(int i = NUM_LEDS-width; i >= 0; i--) {
    for(int j=0; j<width; j++){
      leds[i+j] = c;
    }
    FastLED.show();
    for(int j=0; j<width; j++){
      leds[i+j] = CRGB::Black;
    }

    delay(speed);
  }
}

// Display alternating stripes
void stripes(CRGB c1, CRGB c2, int width){
  for(int i=0; i<NUM_LEDS; i++){
    if(i % (width * 2) < width){
      leds[i] = c1;
    }
    else{
      leds[i] = c2;
    } 
  }
  FastLED.show();
}

// Theater-style crawling of stripes
void stripesChase(CRGB c1, CRGB c2, int width, int cycles, int speed){ // TODO direction

}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
CRGB Wheel(byte WheelPos) {
  if(WheelPos < 85) {
    return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else {
    WheelPos -= 170;
    return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

CRGB randomColor(){
  return Wheel(random(256)); 
}


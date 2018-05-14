#include <FastLED.h>

/* BASIC CONFIGURATION  */
// Model of the LEDs
#define LED_TYPE NEOPIXEL
// The amount of LEDs in the setup
#define NUM_LEDS 47
// The pin that controls the LEDs
#define LED_PIN 6
// The pins that we read sensor values form
#define LEFT_CHANNEL A2
#define RIGHT_CHANNEL A4
// Arduino loop delay (for stablity, probably)
#define DELAY 1

// Confirmed volume low value, and max value
#define VOLUME_LOW 0.35
#define VOLUME_HIGH 93.85

// Minimum and maximum brightness
#define MIN_BRIGHTNESS 7
#define MAX_BRIGHTNESS 255

// Lighting modes
#define PULSE 0
#define VISUALIZATION 1

// Pulse mode configuration
#define BREATH_IN 0
#define BREATH_OUT 1
#define BREATH_LIGHT_STEP 1
#define BREATH_DIM_STEP 1
#define PULSE_MODE_DELAY 5
#define RBG_STEP 1
#define RED 0
#define GREEN 1
#define BLUE 2

// Visualization mode configuration
#define VISUALIZATION_MODE_DELAY 0

// How many previous sensor values effects the operating average?
#define AVG_LEN 30

// How many previous sensor values decides if we detect the sound
#define LONG_SECTOR 120

// How long do we keep the "current average" sound, before restarting the measuring
//#define MSECS 1 * 1000
//#define CYCLES MSECS / VISUALIZATION_MODE_DELAY

/**
 * Sometimes readings are wrong or strange. How much is a reading allowed
 * to deviate from the average to not be discarded?
 */
//#define DEVAITE_THRESH 0.8

// Volumes
float volume, mappedVolume, avgVolume, longerAvgVolume;

// Average sound measurement the last CYCLES
//float songAvg;

// The amount of iterations since the songAvg was reset
//int iter = 0;

// The speed the LEDs fade to black if not relit
//float fadeScale = 1.2;

// LED array
struct CRGB leds[NUM_LEDS];

/**
 * Short sound avg used to "normalize" the input values.
 * We use the short average instead of using the sensor input directly
 */
float volumes[AVG_LEN] = {-1};

// Longer sound avg
float longerVolumes[LONG_SECTOR] = {-1};

// Mode setting
int mode;

// handle brightness and rainbow
int breathDirection;
int decreasingColor;

// Keeping track how often, and how long times we hit a certain mode
struct timeKeeping {
  unsigned long timesStart;
  unsigned long times;
};

// How much to increment or decrement each color every cycle
struct color {
  int r;
  int g;
  int b;
};

// How much to increment or decrement brightness every cycle
struct brightness {
  int up;
  int down;
};

struct timeKeeping visualization;
struct color Color;
struct brightness Brightness;

// For testing
float lowestVolume = 99999;
float highestVolume = 0;
bool isVolumeTestingMode = 0;

void setup () {
  Serial.begin(9600);
  mode = PULSE;
  visualization.timesStart = 0;
  visualization.times= 0;
  Color.r = 1;
  Color.g = 1;
  Color.b = 1;
  Brightness.up = 1;
  Brightness.down = 1;
  // Set all lights to make sure all are working as expected
  FastLED.addLeds<LED_TYPE, LED_PIN>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.setBrightness(MAX_BRIGHTNESS);
  breathDirection = BREATH_OUT;
  fill_solid(leds, NUM_LEDS, CRGB(0, 255, 0));
  decreasingColor = GREEN;
}

void loop () {
  if (isVolumeTestingMode) {
    testVolume();
  } else {
    updateMode();
    switch (mode) {
      case PULSE:
        breath();
        delay(PULSE_MODE_DELAY);
        break;
      case VISUALIZATION:
        visualize();
        delay(VISUALIZATION_MODE_DELAY);
        break;
      default:
        break;
    }
  }
//  delay(DELAY);
}

void visualize () {
  // Color
  handleRainbow();
  // Map volume to brightness
  float volume = readVolume(160);
  int brightness = (int)fscale(VOLUME_LOW, VOLUME_HIGH, MIN_BRIGHTNESS, MAX_BRIGHTNESS, volume, 2.0);
//  Serial.println(brightness);
  FastLED.setBrightness(brightness);
  FastLED.show();
}

void breath () {
  // Color
  handleRainbow();
  // Pulse
  handlePulse();
  FastLED.show();
}

void updateMode () {
  // Actual sensor value
  volume = readVolume(20);
  
  // If 0, discard immediately. Probably not right and save CPU.
//  if (volume == 0) return;

//  mappedVolume = getMappedVolume();
//  insert(mappedVolume, volumes, AVG_LEN);
//  insert(volume, volumes, AVG_LEN);
//  avgVolume = getAvgVolume();
//  insert(avgVolume, longerVolumes, LONG_SECTOR);
  insert(volume, longerVolumes, LONG_SECTOR);
  longerAvgVolume = getLongAvgVolume();
  
  if (longerAvgVolume > VOLUME_LOW) {
    if (visualization.times != 0) {
      if (millis() - visualization.timesStart > 300) {
        changeMode(PULSE);
      } else {
        visualization.timesStart = millis();
        visualization.times++;
      }
    } else {
      visualization.timesStart = millis();
      visualization.times++;
    }
  }
//  int currentBrightness = FastLED.getBrightness();
//  Serial.print(volume);
//  Serial.print('\t');
//  Serial.print(longerAvgVolume);
//  Serial.print('\t');
//  Serial.print(visualization.times);
//  Serial.print('\t');
//  Serial.print(millis() - visualization.timesStart);
//  Serial.print('\t');
//  Serial.print(visualization.times > 15 && millis() - visualization.timesStart < 25);
//  Serial.print('\t');
//  Serial.print(millis() - visualization.timesStart > 300);
//  Serial.print('\t');
//  Serial.println(currentBrightness);
  if (visualization.times > 15 && millis() - visualization.timesStart < 25) {
    changeMode(VISUALIZATION);
  } else if (millis() - visualization.timesStart > 300) {
    changeMode(PULSE);
  }
}

/**
 * Helper functions
 */
void handleRainbow () {
  int r, g, b;
  if (decreasingColor == RED) {
    r = leds[0].r - Color.r;
    g = leds[0].g + Color.g;
    b = leds[0].b;
  } else if (decreasingColor == GREEN) {
    r = leds[0].r;
    g = leds[0].g - Color.g;
    b = leds[0].b + Color.b;
  } else if (decreasingColor == BLUE) {
    r = leds[0].r + Color.r;
    g = leds[0].g;
    b = leds[0].b - Color.b;
  }
  if (r <= 0) {
    r = 0;
    if (decreasingColor == RED) g = 255;
  }
  if (r >= 255) {
    r = 255;
    if (decreasingColor == BLUE) b = 0;
  }
  if (g <= 0) {
    g = 0;
    if (decreasingColor == GREEN) b = 255;
  }
  if (g >= 255) {
    g = 255;
    if (decreasingColor == RED) r = 0;
  }
  if (b <= 0) {
    b = 0;
    if (decreasingColor == BLUE) r = 255;
  }
  if (b >= 255) {
    b = 255;
    if (decreasingColor == GREEN) g = 0;
  }
  if (r == 255) decreasingColor = RED;
  if (g == 255) decreasingColor = GREEN;
  if (b == 255) decreasingColor = BLUE;
//  int currentBrightness = FastLED.getBrightness();
//  Serial.print(r);
//  Serial.print('\t');
//  Serial.print(g);
//  Serial.print('\t');
//  Serial.print(b);
//  Serial.print('\t');
//  Serial.println(currentBrightness);
  fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
}

void handlePulse () {
  int currentBrightness = FastLED.getBrightness();
  int brightness;
  if (breathDirection == BREATH_IN) {
    brightness = currentBrightness + Brightness.up;
  }
  if (breathDirection == BREATH_OUT) {
    brightness = currentBrightness - Brightness.down;
  }
  if (brightness > MAX_BRIGHTNESS) {
    brightness = MAX_BRIGHTNESS;
    breathDirection = BREATH_OUT;
  }
  if (brightness < MIN_BRIGHTNESS) {
    brightness = MIN_BRIGHTNESS;
    breathDirection = BREATH_IN;
  }
//  Serial.println(brightness);
  FastLED.setBrightness(brightness);
}

void changeMode (int mode) {
  if (mode == PULSE) _changeToPulseMode();
  if (mode == VISUALIZATION) _changeToVisualizationMode();
}

void _changeToPulseMode () {
  Color.r = RBG_STEP;
  Color.g = RBG_STEP;
  Color.b = RBG_STEP;
  Brightness.up = BREATH_LIGHT_STEP;
  Brightness.down = BREATH_DIM_STEP;
  visualization.timesStart = 0;
  visualization.times = 0;
  mode = PULSE;
//  Serial.println("Pulse mode");
}

void _changeToVisualizationMode () {
//  Color.r = 5;
//  Color.g = 5;
//  Color.b = 5;
  Color.r = (int)fscale(VOLUME_LOW, VOLUME_HIGH, 5, 15, longerAvgVolume, 2.0);
  Color.g = (int)fscale(VOLUME_LOW, VOLUME_HIGH, 5, 15, longerAvgVolume, 2.0);
  Color.b = (int)fscale(VOLUME_LOW, VOLUME_HIGH, 5, 15, longerAvgVolume, 2.0);
//  Serial.print(Color.r);
//  Serial.print('\t');
//  Serial.print(Color.g);
//  Serial.print('\t');
//  Serial.println(Color.b);
//  visualization.timesStart = millis();
//  visualization.times++;
  mode = VISUALIZATION;
//  Serial.println("Visualization mode");
} 

float readVolume (int cycles) {
  // 1ms can run around 4 cycles
  float total = 0;
//  long before = millis();
  for (int i = 0; i < cycles; i++) {
    int left = analogRead(LEFT_CHANNEL);
    int right = analogRead(RIGHT_CHANNEL);
    float volume = (left + right) / 2;
    total += volume;
  }
//  Serial.println(millis() - before);
  return (total / cycles);
}

float getMappedVolume () {
  return fscale(VOLUME_LOW, VOLUME_HIGH, VOLUME_LOW, VOLUME_HIGH, volume, 2.0);
}

float getAvgVolume () {
  return computeAverage(volumes, AVG_LEN);
}

float getLongAvgVolume () {
  return computeAverage(longerVolumes, LONG_SECTOR);
}

void testVolume () {
  volume = readVolume(100);
  if (volume < lowestVolume) lowestVolume = volume;
  if (volume > highestVolume) highestVolume = volume;
//  mappedVolume = getMappedVolume();
//  insert(mappedVolume, volumes, AVG_LEN);
//  insert(volume, volumes, AVG_LEN);
//  avgVolume = getAvgVolume();
//  insert(avgVolume, longerVolumes, LONG_SECTOR);
  insert(volume, longerVolumes, LONG_SECTOR);
  longerAvgVolume = getLongAvgVolume();
  Serial.print(lowestVolume);
  Serial.print('\t');
  Serial.print(highestVolume);
  Serial.print('\t');
//  Serial.print(mappedVolume);
//  Serial.print('\t');
  Serial.print(volume);
  Serial.print('\t');
//  Serial.print(avgVolume);
  Serial.print('\t');
  Serial.println(longerAvgVolume);
}

/**
 * Utilities
 */
// Compute average of a int array, given the starting pointer and the length
float computeAverage (float arr[], int len) {
  float sum = 0;
  for (int i = 0; i < len; i++) {
    sum += arr[i];
  }
  return (sum / len);
}

// Insert a value into an array, and shift it down removing
// the first value if array already full 
void insert (float val, float *arr, int len) {
  for (int i = 0; i < len; i++) {
    if (arr[i] == -1) {
      arr[i] = val;
      return;
    }  
  }
  for (int i = 1; i < len; i++) {
    arr[i - 1] = arr[i];
  }
  arr[len - 1] = val;
}

//Function imported from the arduino website.
//Basically map, but with a curve on the scale (can be non-uniform).
float fscale (float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve) {
  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;

  // condition curve parameter
  // limit range
  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;

  curve = (curve * -.1) ; // - invert and scale - this seems more intuitive - postive numbers give more weight to high end on output 
  curve = pow(10, curve); // convert linear scale into lograthimic exponent for other pow function

  // Check for out of range inputValues
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Zero Refference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin){ 
    NewRange = newEnd - newBegin;
  } else {
    NewRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal = zeroRefCurVal / OriginalRange;   // normalize to 0 - 1 float

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine
  if (originalMin > originalMax) {
    return 0;
  }

  if (invFlag == 0) {
    rangedValue = (pow(normalizedCurVal, curve) * NewRange) + newBegin;
  } else {
    // invert the ranges
    rangedValue = newBegin - (pow(normalizedCurVal, curve) * NewRange);
  }

  return rangedValue;
}
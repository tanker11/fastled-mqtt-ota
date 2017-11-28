/*

   _____ _    ____ _____ _     _____ ____    _____ _   _ _   _  ____ _____ ___ ___  _   _ ____
  |  ___/ \  / ___|_   _| |   | ____|  _ \  |  ___| | | | \ | |/ ___|_   _|_ _/ _ \| \ | / ___|
  | |_ / _ \ \___ \ | | | |   |  _| | | | | | |_  | | | |  \| | |     | |  | | | | |  \| \___ \
  |  _/ ___ \ ___) || | | |___| |___| |_| | |  _| | |_| | |\  | |___  | |  | | |_| | |\  |___) |
  |_|/_/   \_\____/ |_| |_____|_____|____/  |_|    \___/|_| \_|\____| |_| |___\___/|_| \_|____/

*/



void pastelizeColors() {
  //handles the saturation
  //adds proportion of WHITE tint depending on the globalSaturation number
  //makes similar behavior to HSV saturation variable, but in case of palette coloring, there is no such possibility
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] += CRGB(255 - globalSaturation, 255 - globalSaturation, 255 - globalSaturation);



  }
}


void gradientRedGreen() {
  fill_gradient(leds, 0, CHSV(HUE_RED, 255, 255), NUM_LEDS - 1, CHSV(HUE_GREEN, 255, 255), SHORTEST_HUES);

  FastLED.show();
  //ugyanezt az is megcsinálja, hogy összesen két színt teszünk a palettára. És átfolyatja egymásba a színeket
}

void tripleBlink(CRGB color) {
  for (int j = 0; j < 3; j++) {
    fill_solid( leds, NUM_LEDS, color);
    FastLED.show();
    delay(100);
    fill_solid( leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
  }
}

void steady(int from, int to, CRGB color) {

  int fromLED = (from < 0) ? 0 : from;
  int toLED = (to > NUM_LEDS) ? NUM_LEDS : to;

  for (int i = fromLED; i < toLED + 1; i++) {
    leds[i] = color;
  }
}



//####SteadyLight class

void FillLEDsFromPaletteColors( ) {
  uint8_t brightness = 255;
  int paletteStep = (int)256 / NUM_LEDS;
  //currentBlending = NOBLEND;
  currentBlending = LINEARBLEND;
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( currentPalette, i * paletteStep + gHue , brightness, currentBlending);


  }
}

void all_off() {
  fadeToBlackBy( leds, NUM_LEDS, blendSpeed);
}

void all_off_immediate() {
  fill_solid( leds, NUM_LEDS, CRGB::Black);
}


void rainbow()
{
  // FastLED's built-in rainbow generator
  //fill_rainbow( leds, NUM_LEDS, gHue, 7);
  FillLEDsFromPaletteColors();
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;

  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(currentPalette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void move()
{
  uint8_t brightness = 255;
  int paletteStep = (int)256 / NUM_LEDS;
  currentBlending = NOBLEND;
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( currentPalette, i * paletteStep + gHue , brightness, currentBlending);


  }
}

//NOISE FUNCTIONS-------------------------------------------------
//
// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
//
uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  if ( kMatrixSerpentineLayout == false) {
    i = (y * kMatrixWidth) + x;
  }
  if ( kMatrixSerpentineLayout == true) {
    if ( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (kMatrixWidth - 1) - x;
      i = (y * kMatrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * kMatrixWidth) + x;
    }
  }
  return i;
}
// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  // If we're runing at a low "speed", some 8-bit artifacts become visible
  // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
  // The amount of data smoothing we're doing depends on "speed".
  uint8_t dataSmoothing = 0;
  if ( globalSpeed < 50) {
    dataSmoothing = 200 - (globalSpeed); //globalSpeed*4 volt itt
  }

  for (int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    for (int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = scale * j;

      uint8_t data = inoise8(x + ioffset, y + joffset, z);

      // The range of the inoise8 function is roughly 16-238.
      // These two operations expand those values out to roughly 0..255
      // You can comment them out if you want the raw noise data.
      data = qsub8(data, 16);
      data = qadd8(data, scale8(data, 39));

      if ( dataSmoothing ) {
        uint8_t olddata = noise[i][j];
        uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
        data = newdata;
      }

      noise[i][j] = data;
    }
  }

  //z += globalSpeed;
  z += globalSpeed / 8;

  // apply slow drift to X and Y, just for visual variation.
  //x += globalSpeed / 8;
  x += globalSpeed / 64;
  //y -= globalSpeed / 16;
  y -= globalSpeed / 128;
}

void mapNoiseToLEDsUsingPalette()
{
  static uint8_t ihue = 0;

  for (int i = 0; i < kMatrixWidth; i++) {
    for (int j = 0; j < kMatrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.

      uint8_t index = noise[j][i];
      uint8_t bri =   noise[i][j];

      // if this palette is a 'loop', add a slowly-changing base value
      if ( gHueRoll) {
        index += ihue;
      }

      // brighten up, as the color palette itself often contains the
      // light/dark dynamic range desired
      if ( bri > 127 ) {
        bri = 255;
      } else {
        bri = dim8_raw( bri * 2);
      }

      CRGB color = ColorFromPalette( currentPalette, index, bri);
      leds[XY(i, j)] = color;
    }
  }

  ihue += 1;
}


void handleNoise() {

  // generate noise data
  fillnoise8();

  // convert the noise data to colors in the LED array
  // using the current palette
  mapNoiseToLEDsUsingPalette();

}

//NOISE FUNCTIONS END------------------------------------------

/**************FastLED FUNCTIONS END******************************/

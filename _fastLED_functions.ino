void setLEDMode() {


  switch (LEDMode) {
    case OFF:   fill_solid( targetPalette, 16, CRGB::Black); all_off() ; gHueRoll = false; speedMod = 1; break;
    case STEADY: gHueRoll = false; steady(0, NUM_LEDS - 1, CHSV(0, globalSaturation, 255)); speedMod = 1; break;
    case RAINBOW: targetPalette = RainbowColors_p; gHueRoll = true; rainbow(); pastelizeColors(); speedMod = 1; break;
    case RAINBOW_G: targetPalette = RainbowColors_p; gHueRoll = true; rainbowWithGlitter(); pastelizeColors(); speedMod = 1; break;
    case GLITTERONLY: gHueRoll = false; speedMod = 4; glitterOnly();  break;
    case CONFETTI: gHueRoll = true; scaleMod = 0.5; speedMod = 0.1; confetti();  break;
    case SINELON: gHueRoll = true; scaleMod = 1; speedMod = 0.1; sinelon();  break;
    case JUGGLE: gHueRoll = true; scaleMod = 0.5; speedMod = 0.05; juggle();  break;
    case HEARTBEAT: gHueRoll = false; scaleMod = 2 ; heartBeat();  break;
    case FIRE: gHueRoll = false; scaleMod = 5; speedMod = 1.8; mirroredFire();  break; //speed->COOLING  scale->SPARKING
    case RND_WANDER: gHueRoll = false ; randomWander();  break;
    case BPM_PULSE: targetPalette = PartyColors_p; gHueRoll = false; bpm(); pastelizeColors(); speedMod = 1; break;
    case AQUAORANGE: setDualPalette(CHSV( 110, 255, 255), CHSV( 20, 255, 255)); gHueRoll = false; dualColor(CUBIC); pastelizeColors(); speedMod = 0.5; break;
    case AQUAGREEN: setDualPalette(CHSV( 128, 255, 255), CHSV( 100, 255, 255)); gHueRoll = false; dualColor(SINUS); pastelizeColors(); speedMod = 1; break;
    case AQUAGREEN_NOISE: setDualPalette(CHSV( 128, 255, 255), CHSV( 100, 255, 255)); gHueRoll = false; scaleMod = 2.5; speedMod = 1; handleNoise(); pastelizeColors(); break;
    case NOISE_OCEAN: targetPalette = OceanColors_p; gHueRoll = false; scaleMod = 25; speedMod = 1; handleNoise(); pastelizeColors(); break;
    case NOISE_RND1: RandomPalette(1); gHueRoll = false; scaleMod = 2.5; speedMod = 1; handleNoise(); pastelizeColors(); break;//1 color
    case NOISE_RND2: RandomPalette(2); gHueRoll = false; scaleMod = 2; speedMod = 1; handleNoise(); pastelizeColors(); break;//2 colors
    case NOISE_RND3: RandomPalette(3); gHueRoll = false; scaleMod = 2; speedMod = 1; handleNoise(); pastelizeColors(); break;//3 colors
    case NOISE_RND4: RandomPalette(4); gHueRoll = false; scaleMod = 2; speedMod = 1; handleNoise(); pastelizeColors(); break;//3 colors
    case NOISE_LAVA: targetPalette = LavaColors_p; gHueRoll = false; scaleMod = 1.5; speedMod = 1; handleNoise(); pastelizeColors(); break;
    case NOISE_PARTY: targetPalette = PartyColors_p; gHueRoll = true; scaleMod = 2; speedMod = 1; handleNoise(); pastelizeColors(); break;
    case NOISE_BW: setBlackAndWhiteStripedPalette(); gHueRoll = true; scaleMod = 2; speedMod = 1; handleNoise(); pastelizeColors(); break;
  /*NINCS KÉSZ NEM NOISE LESZ*/    case NOISE_LIGHTNING: setLightningPalette(); gHueRoll = false; scaleMod = 0.1; speedMod = 3; handleNoise(); pastelizeColors(); break;
    case EMERGENCY: /*Nothing to do here, handled separately*/ break;
    case TEST: gHueRoll = false; steady(testFrom, testTo, CHSV(testHue, globalSaturation, 255));  break;
    case ALARM: currentPalette = RainbowColors_p; gHueRoll = false; speedMod = 1; myAlarmLight->setColor(almMode);
      if (myAlarmLight->blinkStatus) myAlarmLight->On();
      if (!myAlarmLight->blinkStatus) myAlarmLight->Off();
      EVERY_N_MILLISECONDS( 1000 ) {
        myAlarmLight->blinkStatus = !myAlarmLight->blinkStatus;
      };
      break;
    case AMBERBLINK: currentPalette = trafficLightPalette; gHueRoll = false; speedMod = 1;
      if (myAmberLight->blinkStatus) {
        myAmberLight->On();
        myRedLight->Off();
        myGreenLight->Off();
      }
      if (!myAmberLight->blinkStatus) myAmberLight->Off();
      EVERY_N_MILLISECONDS( 800 ) {
        myAmberLight->blinkStatus = !myAmberLight->blinkStatus;
      };
      break;
    case TRAFFICLIGHT: currentPalette = trafficLightPalette; gHueRoll = false; speedMod = 1;
      if (isTimeout(tlMillis, tlTimingLamp)) {
        tlMillis = millis();
        tlStatusVar++;

      }
      if (tlStatusVar > 3) tlStatusVar = 0;
      switch (tlStatusVar) {
        case 0: {
            myRedLight->On();
            myAmberLight->Off();
            myGreenLight->Off();
            tlTimingLamp = 8000;
          }
          break;
        case 1: {
            myRedLight->On();
            myAmberLight->On();
            myGreenLight->Off();
            tlTimingLamp = 2000;
          }
          break;
        case 2: {
            myRedLight->Off();
            myAmberLight->Off();
            myGreenLight->On();
            tlTimingLamp = 8000;
          }
          break;
        case 3: {
            myRedLight->Off();
            myAmberLight->On();
            myGreenLight->Off();
            tlTimingLamp = 2000;
          }
          break;

      }

      break;

  }
  modifiedSpeed = globalSpeed * speedMod; //modifying the speed according to the pattern we want
  modifiedScale = scale * scaleMod; //modifying the speed according to the pattern we want


  if (LEDMode != oldLEDMode) {
    Serial.println("LED MODE CHANGED");
    LEDModeChanged = true;
    oldLEDMode = LEDMode;
  } else LEDModeChanged = false;

  if (globalSpeed != oldGlobalSpeed) {
    Serial.println("SPEED CHANGED");
    globalSpeedChanged = true;
    oldGlobalSpeed = globalSpeed;
  }else globalSpeedChanged = false;

  if (scale != oldScale) {
    Serial.println("SCALE CHANGED");
    scaleChanged = true;
    oldScale = scale;
  }else scaleChanged = false;

  if (blendSpeed != oldBlendSpeed) {
    Serial.println("BLENDSPEED CHANGED");
    blendSpeedChanged = true;
    oldBlendSpeed = blendSpeed;
  }else blendSpeedChanged = false;

  if (BeatsPerMinute != oldBeatsPerMinute) {
    Serial.println("BPM CHANGED");
    BeatsPerMinuteChanged = true;
    oldBeatsPerMinute = BeatsPerMinute;
  }else BeatsPerMinuteChanged = false;

}


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

void FillLEDsFromPaletteColors(TBlendType requestedBlending ) {
  uint8_t brightness = 255;
  int paletteStep = (int)256 / NUM_LEDS;
  //currentBlending = NOBLEND;
  //currentBlending = LINEARBLEND;
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( currentPalette, i * paletteStep + gHue , brightness, requestedBlending);


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
  FillLEDsFromPaletteColors(LINEARBLEND);
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80); //eredetileg 80 volt a paraméter
}

void glitterOnly()
{
  fadeToBlackBy( leds, NUM_LEDS, (int)modifiedSpeed / 2); //eredetileg 20 volt az utolsó paraméter
  addGlitter(modifiedScale); //eredetileg 20 volt a paraméter

}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)


  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(currentPalette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos;
  pos = random16(NUM_LEDS);
  if (random8() < 8 * modifiedScale) {
    leds[pos] += CHSV( gHue + random8(modifiedScale * 2), 200, 255); //eredetileg gHue+random8(64) volt az első paraméter
  }
}

void sinelon()
{
  // a colored dot sweeping
  // back and forth, with
  // fading trails
  fadeToBlackBy( leds, NUM_LEDS, 2 * modifiedScale + 5); //eredetileg 20 volt az utolsó paraméter
  int pos = beatsin16(20 - (int)modifiedScale / 2, 0, NUM_LEDS - 1); //eredetileg 13 volt a BPM paraméter
  static int prevpos = 0;
  if ( pos < prevpos ) {
    fill_solid( leds + pos, (prevpos - pos) + 1, CHSV(gHue, 220, 255));
  } else {
    fill_solid( leds + prevpos, (pos - prevpos) + 1, CHSV( gHue, 220, 255));
  }
  prevpos = pos;
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16(i + (int)modifiedScale / 2, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255); //i+7 volt a beatsin első paramétere
    dothue += 32;
  }
}

/*
  void move()
  {
  uint8_t brightness = 255;
  int paletteStep = (int)256 / NUM_LEDS;
  currentBlending = NOBLEND;
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( currentPalette, i * paletteStep + gHue , brightness, currentBlending);


  }
  }*/

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
  if ( modifiedSpeed < 50) {
    dataSmoothing = 200 - (modifiedSpeed); //modifiedSpeed*4 volt itt
  }

  for (int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = modifiedScale * i;
    for (int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = modifiedScale * j;

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

  //z += modifiedSpeed;
  z += modifiedSpeed / 8;

  // apply slow drift to X and Y, just for visual variation.
  //x += modifiedSpeed / 8;
  x += modifiedSpeed / 64;
  //y -= modifiedSpeed / 16;
  y -= modifiedSpeed / 128;
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



void heartBeat() {
  fadeToBlackBy( leds, NUM_LEDS, 4 * modifiedScale);

  int ownBPM = BeatsPerMinute;
  if (NUM_LEDS <= 48) ownBPM = BeatsPerMinute * 2; //BPM is manipulated because if a second front is emitted, it needs to be half of the original
  int pos1 = beat8(ownBPM / 2, 0) * NUM_LEDS / 256;
  int pos2 = beat8(ownBPM / 2, 0 + 300) * NUM_LEDS / 256;
  int pos3 = beat8(ownBPM / 2, 0 + 60000 / ownBPM) * NUM_LEDS / 256; //delay of the second "front" is 1000*(BPM/60)
  int pos4 = beat8(ownBPM / 2, 0 + 60000 / ownBPM + 300) * NUM_LEDS / 256;

  if (flowDirection < 0) {
    pos1 = NUM_LEDS - pos1 - 1;
    pos2 = NUM_LEDS - pos2 - 1;
    pos3 = NUM_LEDS - pos3 - 1;
    pos4 = NUM_LEDS - pos4 - 1;
  }

  leds[pos1] = CHSV( bloodHue, bloodSat, 255 );
  leds[pos2] = CHSV( bloodHue , bloodSat, 255 );
  if (NUM_LEDS > 48) { //if the LED strip is long enough, a second "front" of pulses is visible
    leds[pos3] = CHSV( bloodHue, bloodSat, 255 );
    leds[pos4] = CHSV( bloodHue, bloodSat, 255 );
  }
}

void randomWander() {
  CRGB colorA;
  CRGB colorB;
  CRGB colorC;
  EVERY_N_MILLISECONDS(5000) {//Change color in every 5 seconds
    // Set pixel color
    colorA = CHSV( random8(), 255, 255);
    colorB = CHSV( random8(), 200, 100);
    colorC = CHSV( random8(), 150, 200);
  }

  fadeToBlackBy( leds, NUM_LEDS, 60);
  EVERY_N_MILLISECONDS(holdTime) {

    leds[positionA] = colorA;
    leds[positionB] = colorB;
    leds[positionC] = colorC;
    // Set new position, moving (forward or backward) by delta.
    // NUM_LEDS is added to the position before doing the modulo
    // to cover cases where delta is a negative value.
    if (random8() < 10) deltaA *= -1; //10/255*100% eséllyel megfordul az irány
    positionA = (positionA + deltaA + NUM_LEDS) % NUM_LEDS;
    if (random8() < 30) deltaB *= -1; //30/255*100% eséllyel megfordul az irány
    positionB = (positionB + deltaB + NUM_LEDS) % NUM_LEDS;
    if (random8() < 50) deltaC *= -1; //50/255*100% eséllyel megfordul az irány
    positionC = (positionC + deltaC + NUM_LEDS) % NUM_LEDS;

  }
}


//---------------------------------------------------------------
// ***** NOTE: NUM_LEDS was replaced with NUM_LEDS/2 anywhere it was found
//       below.  This makes the fire only run on the first half of the strip. *****
//---------------------------------------------------------------
void mirroredFire()
{
  COOLING = modifiedSpeed;
  SPARKING = modifiedScale;

  Serial.println(COOLING);
  Serial.println(SPARKING);
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS / 2];

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS / 2; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS / 2) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS / 2 - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS / 2; j++) {
    CRGB color = HeatColor( heat[(NUM_LEDS / 2) - 1 - j]); //heat[j] volt eredetileg, ettől középről kifelé megy
    int pixelnumber;
    if ( gReverseDirection ) {
      pixelnumber = (NUM_LEDS / 2) - 1 - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
  mirror2ndHalf();
}


//---------------------------------------------------------------
void mirror2ndHalf() {
  // copy in reverse order first half of strip to second half
  for (uint8_t i = 0; i < NUM_LEDS / 2; i++) {
    leds[NUM_LEDS - 1 - i] = leds[i];
  }
}

void dualColor(short drawMode) {
  //MEGCSINÁLNI SIZE AND BLENDTYPE

  //Fills the LED matrix with two colors in the DualPalette
  int localSize = 8;
  bool colorFlip = false;
  int posColor; //pick this color from the dual palette
  int scaled; //scaled value of the given wave
  int shiftedPosition; //shift position with the time according to the speed param


  globalPosShift += globalSpeed / 100.0; //use global position shift variable

  for ( int i = 0; i < NUM_LEDS; i++) {
    switch (drawMode) {
      case SQUARE:
        if ( (i + 1) % localSize == 0) colorFlip = !colorFlip;
        posColor = colorFlip * 255;
        break;
      case SINUS:
        scaled = (i) * 128 / (localSize); //scales to the sine waveform to the range of LEDs
        posColor = sin8(scaled);
        break;
      case CUBIC:
        scaled = (i) * 256 / (localSize); //scales to the cubic waveform to the range of LEDs
        posColor = cubicwave8(scaled);
        break;
    }

    int shiftedPosition = (i + (int)globalPosShift + NUM_LEDS) % NUM_LEDS;
    leds[shiftedPosition] = ColorFromPalette( targetPalette, posColor , 255, NOBLEND);
  }

}

/**************FastLED FUNCTIONS END******************************/

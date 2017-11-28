

/*

  _   _ _____ _     ____  _____ ____    _____ _   _ _   _  ____ _____ ___ ___  _   _ ____
  | | | | ____| |   |  _ \| ____|  _ \  |  ___| | | | \ | |/ ___|_   _|_ _/ _ \| \ | / ___|
  | |_| |  _| | |   | |_) |  _| | |_) | | |_  | | | |  \| | |     | |  | | | | |  \| \___ \
  |  _  | |___| |___|  __/| |___|  _ <  |  _| | |_| | |\  | |___  | |  | | |_| | |\  |___) |
  |_| |_|_____|_____|_|   |_____|_| \_\ |_|    \___/|_| \_|\____| |_| |___\___/|_| \_|____/

*/

boolean isTimeout(unsigned long checkTime, unsigned long timeWindow)
{
  unsigned long now = millis();
  if (checkTime <= now)
  {
    if ( (unsigned long)(now - checkTime )  < timeWindow )
      return false;
  }
  else
  {
    if ( (unsigned long)(checkTime - now) < timeWindow )
      return false;
  }

  return true;
}

void flip()
{
  int state = digitalRead(WIFI_LED_PIN);  // get the current state of GPIO2 pin
  digitalWrite(WIFI_LED_PIN, !state);     // set pin to the opposite state
  if (state) leds[0] = CRGB::Blue; else leds[0] = CRGB::Black;
  FastLED.show();
}


void blinkErrorLED(CRGB color) {
  //blinks the first LED according to the requested color
  EVERY_N_MILLISECONDS( 1000 ) {

    errorBlinkStatus = !errorBlinkStatus;
  }
  if (errorStatus) {
    if (errorBlinkStatus) {
      leds[0] = color;
    }
    else {
      leds[0] = CRGB::Black;
    }
  }
}

void handleEmergency() {
  emergencyState++;
  if (emergencyState > 4) emergencyState = 0;
  switch (emergencyState) {
    case 0: LEDMode = prevLEDMode; Serial.println("Emergency mode off"); FastLED.setBrightness(MAX_BRIGHTNESS); break;
    case 1: if (LEDMode != EMERGENCY) prevLEDMode = LEDMode; LEDMode = EMERGENCY; Serial.println("Emergency mode 25%"); break;
    case 2:  if (LEDMode != EMERGENCY) prevLEDMode = LEDMode; LEDMode = EMERGENCY; Serial.println("Emergency mode 50%"); break;
    case 3:  if (LEDMode != EMERGENCY) prevLEDMode = LEDMode; LEDMode = EMERGENCY; Serial.println("Emergency mode 75%"); break;
    case 4:  if (LEDMode != EMERGENCY) prevLEDMode = LEDMode; LEDMode = EMERGENCY; Serial.println("Emergency mode 100%"); break;
  }
  if (emergencyState) {
    fill_solid( leds, NUM_LEDS, CRGB::White);
    FastLED.setBrightness(emergencyState * 64 - 1);
    FastLED.show();
  }
}

void handleKeypresses() {

  switch (S.read())
  {
    case MD_KeySwitch::KS_NULL:       /* Serial.println("NULL"); */   break;
    case MD_KeySwitch::KS_PRESS:      Serial.print("\nSINGLE PRESS\n"); handleEmergency(); break;
    case MD_KeySwitch::KS_DPRESS:     Serial.print("\nDOUBLE PRESS\n"); break;
    case MD_KeySwitch::KS_LONGPRESS:  Serial.print("\nLONG PRESS\n");   break;
    case MD_KeySwitch::KS_RPTPRESS:   Serial.print("\nREPEAT PRESS\n"); break;
    default:                          Serial.print("\nUNKNOWN\n");      break;
  }
}

/**************HELPER FUNCTIONS END******************************/


/*
  _____ ___ _____  _      _____ _   _ _   _  ____ _____ ___ ___  _   _ ____
  |  ___/ _ \_   _|/ \    |  ___| | | | \ | |/ ___|_   _|_ _/ _ \| \ | / ___|
  | |_ | | | || | / _ \   | |_  | | | |  \| | |     | |  | | | | |  \| \___ \
  |  _|| |_| || |/ ___ \  |  _| | |_| | |\  | |___  | |  | | |_| | |\  |___) |
  |_|   \___/ |_/_/   \_\ |_|    \___/|_| \_|\____| |_| |___\___/|_| \_|____/

*/


void checkForUpdates() {

  String fwURL = String( fwUrlBase );
  fwURL.concat( alias );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );

  Serial.println( "Checking for firmware updates." );
  Serial.print( "Alias: " );
  Serial.println( alias );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );
  //Blink three times with BLUE color to show the checking phase
  tripleBlink(CRGB::Blue);

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if ( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if ( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );
      //Changing all LEDs to static pattern to make the update status visible
      gradientRedGreen();


      //Publish FW found status
      client.publish(TOPIC_DEV_STATUS, "FW found, downloading");
      gradientRedGreen();

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      Serial.println( "Update started." );
      ESPhttpUpdate.rebootOnUpdate(false); //do not allow reboot after update so that we can feed back to the user
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          //Publish failed status
          client.publish(TOPIC_DEV_STATUS, "HTTP update failed");
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          //Publish no HTTP update status
          client.publish(TOPIC_DEV_STATUS, "No HTTP updates");
          break;

        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_SUCCESSFUL");
          client.publish(TOPIC_DEV_STATUS, "HTTP update successful");
          tripleBlink(CRGB::Green);
          delay(1000);
          ESP.restart();
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
      //Publish FW found status
      client.publish(TOPIC_DEV_STATUS, "Already on latest version");
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
    //Publish failed status
    client.publish(TOPIC_DEV_STATUS, "Could not check available FW");
  }
  httpClient.end();
}


/**************FOTA FUNCTIONS END******************************/

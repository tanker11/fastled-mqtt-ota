/*
   __  __  ___ _____ _____   _____ _   _ _   _  ____ _____ ___ ___  _   _ ____
  |  \/  |/ _ \_   _|_   _| |  ___| | | | \ | |/ ___|_   _|_ _/ _ \| \ | / ___|
  | |\/| | | | || |   | |   | |_  | | | |  \| | |     | |  | | | | |  \| \___ \
  | |  | | |_| || |   | |   |  _| | |_| | |\  | |___  | |  | | |_| | |\  |___) |
  |_|  |_|\__\_\|_|   |_|   |_|    \___/|_| \_|\____| |_| |___\___/|_| \_|____/

*/
//Subscribe function
void subscribeToTopics() {

  //Note: multible subscription will not work without client.loop();

  client.subscribe(TOPIC_DEV_COMMAND);
  Serial.println("Subscribed to [" TOPIC_DEV_COMMAND "] topic");
  client.loop();
  client.subscribe(TOPIC_DEV_TEST);
  Serial.println("Subscribed to [" TOPIC_DEV_TEST "] topic");
  client.loop();
  client.subscribe(TOPIC_DEV_SETURL);
  Serial.println("Subscribed to [" TOPIC_DEV_SETURL "] topic");
  client.loop();
  client.subscribe(TOPIC_DEV_FASTLED);
  Serial.println("Subscribed to [" TOPIC_DEV_FASTLED "] topic");
  client.loop();
  client.subscribe(TOPIC_DEV_ALARM);
  Serial.println("Subscribed to [" TOPIC_DEV_ALARM "] topic");
  client.loop();
  client.subscribe(TOPIC_DEV_FOTA);
  Serial.println("Subscribed to [" TOPIC_DEV_FOTA "] topic");
  client.loop();
  client.subscribe(TOPIC_ALL_ALARM);
  Serial.println("Subscribed to [" TOPIC_ALL_ALARM "] topic");
  client.loop();
  client.subscribe(TOPIC_ALL_FOTA);
  Serial.println("Subscribed to [" TOPIC_ALL_FOTA "] topic");
  client.loop();
  client.subscribe(TOPIC_ALL_SETURL);
  Serial.println("Subscribed to [" TOPIC_ALL_SETURL "] topic");
  client.loop();
  client.subscribe(TOPIC_BGRP_FASTLED);
  Serial.println("Subscribed to [" TOPIC_BGRP_FASTLED "] topic");


}


void publishMyIP() {
  //Publishing own IP on device/[alias]/ip/ topic
  IPAddress local = WiFi.localIP();
  strcpy(msg, "");
  sprintf(msg, "IP: %i.%i.%i.%i", local[0], local[1], local[2], local[3]);
  client.publish(TOPIC_DEV_STATUS, msg, RETAINED);
  Serial.printf("MQTT topic: %s, message: %s\n", TOPIC_DEV_STATUS, msg);

}

void publishMyFW() {

  //Publishing own FW version on device/[alias]/fw/ topic
  strcpy(msg, "");
  sprintf(msg, "FW: %i", FW_VERSION);
  client.publish(TOPIC_DEV_STATUS, msg, RETAINED);
  Serial.printf("MQTT topic: %s, message: %s\n", TOPIC_DEV_STATUS, msg);

}

void publishMyHeap() {

  //Publishing own FW version on device/[alias]/fw/ topic
  strcpy(msg, "");
  sprintf(msg, "HEAP: %i", ESP.getFreeHeap());
  client.publish(TOPIC_DEV_STATUS, msg);
  Serial.printf("MQTT topic: %s, message: %s\n", TOPIC_DEV_STATUS, msg);

}

void publishMySketchSize() {

  //Publishing own FW version on device/[alias]/status/ topic
  strcpy(msg, "");
  sprintf(msg, "SKETCH: %i", ESP.getSketchSize());
  client.publish(TOPIC_DEV_STATUS, msg);
  Serial.printf("MQTT topic: %s, message: %s\n", TOPIC_DEV_STATUS, msg);

}


void publishMyLEDmode() {

  //Publishing current and previous LEDModes on device/[alias]/status/ topic
  strcpy(msg, "");
  sprintf(msg, "Current LEDMode: %i previous LEDMode: %i", LEDMode, prevLEDMode);
  client.publish(TOPIC_DEV_STATUS, msg);
  Serial.printf("MQTT topic: %s, message: %s\n", TOPIC_DEV_STATUS, msg);

}


void publishMyURL() {

  //Publishing actual FOTA URL on device/[alias]/status/ topic
  strcpy(msg, "");
  sprintf(msg, "FOTA URL: %s", fwUrlBase );
  client.publish(TOPIC_DEV_STATUS, msg);
  Serial.printf("MQTT topic: %s, message: %s\n", TOPIC_DEV_STATUS, msg);

}

void publishMyFreeSize() {

  //Publishing own FW version on device/[alias]/fw/ topic
  strcpy(msg, "");
  sprintf(msg, "FREE: %i", ESP.getFreeSketchSpace());
  client.publish(TOPIC_DEV_STATUS, msg);
  Serial.printf("MQTT topic: %s, message: %s\n", TOPIC_DEV_STATUS, msg);

}

// Callback function
void callback(char* topic, byte * payload, unsigned int length) {
  digitalWrite(WIFI_LED_PIN, LOW);   // Turn the LED on (Note that LOW is the voltage level

  Serial.print("Message arrived [");
  Serial.print(topic);
  strcpy(recTopic, "");
  strcpy(recTopic, topic); //storing received topic for further processing
  Serial.print("] ");
  int i;
  for (i = 0; i < length; i++) {
    // Serial.print((char)payload[i]);
    recMsg[i] = (char)payload[i];
  }
  recMsg[i] = '\0'; //Closing the C string
  Serial.println(recMsg);
  msgReceived = true;

  digitalWrite(WIFI_LED_PIN, HIGH);  // Turn the LED off by making the voltage HIGH
}

/**************MQTT FUNCTIONS END******************************/


/*

  __  __ _____ ____ ____    _    ____ _____   ____  ____   ___   ____ _____ ____ ____
  |  \/  | ____/ ___/ ___|  / \  / ___| ____| |  _ \|  _ \ / _ \ / ___| ____/ ___/ ___|
  | |\/| |  _| \___ \___ \ / _ \| |  _|  _|   | |_) | |_) | | | | |   |  _| \___ \___ \
  | |  | | |___ ___) |__) / ___ \ |_| | |___  |  __/|  _ <| |_| | |___| |___ ___) |__) |
  |_|  |_|_____|____/____/_/   \_\____|_____| |_|   |_| \_\\___/ \____|_____|____/____/

*/


void processRecMessage() {

  bool validContent = false;

  //FASTLED BRANCH


  if ((strcmp(recTopic, TOPIC_DEV_FASTLED) == 0) || (strcmp(recTopic, TOPIC_BGRP_FASTLED) == 0)) {
    strcpy(recCommand, "");
    strcpy(recValue, "");
    Serial.println(TOPIC_DEV_FASTLED " branch");

    //try to find segments separated by ":"
    char *found = strtok(recMsg, ":");  //find the first part before the ":"
    if (found != NULL) { //if found...
      strcpy(recCommand, found);
      found = strtok(NULL, ":"); //find the second part
      if (found != NULL) {
        validContent = true;
        strcpy(recValue, found);
      }
    }
    //If found, process it
    if (strcmp(recCommand, "ledmode") == 0) {
      if (LEDMode != ALARM) { //If LEDMode is not ALARM, change the mode
        newColor = true; //this is needed for refreshing the color if a random is needed and we send the same command (used for CSTEADY)

        if (atoi(recValue) == 0) { //seems to be a zero value (note: this is also zero if the value is a string, so further check is necessary
          if (strcmp(recValue, "0") != 0) { //it is a string after the ":", so further processing needed

            Serial.println("String value");
            if (strcmp(recValue, "CSTEADY") == 0) {
              prevLEDMode = LEDMode;
              LEDMode = CSTEADY;
              validContent = true;
            }
            if (strcmp(recValue, "WHITE_TEST") == 0) {
              prevLEDMode = LEDMode;
              LEDMode = WHITE_TEST;
              validContent = true;
            }
            if (strcmp(recValue, "BATHMIRROR") == 0) {
              prevLEDMode = LEDMode;
              LEDMode = BATHMIRROR;
              validContent = true;
            }
            if (strcmp(recValue, "BATHMIRRORG") == 0) {
              prevLEDMode = LEDMode;
              LEDMode = BATHMIRRORG;
              validContent = true;
            }
            if (strcmp(recValue, "PINK") == 0) {
              prevLEDMode = LEDMode;
              LEDMode = PINK;
              validContent = true;
            }
            if (strcmp(recValue, "NOISE_RND4") == 0) {
              prevLEDMode = LEDMode;
              LEDMode = NOISE_RND4;
              validContent = true;
            }
          }
          else
          { //it is indeed a zero value
            validContent = true;
            prevLEDMode = LEDMode;
            LEDMode = atoi(recValue);
          }
        } else
        { //this is a valid integer value
          validContent = true;
          prevLEDMode = LEDMode;
          LEDMode = atoi(recValue);
        }
        if (LEDMode == CSTEADY) newColor = true;
        Serial.printf("ledmode:%d\n", LEDMode);
      } else { //if in ALARM mode, change the prevLEDMode instead, then it will go back to the new prevMode after the ALARM status
        prevLEDMode = atoi(recValue);
        Serial.println("IN ALARM! Changing the prevledmode");
        Serial.printf("prevledmode:%d\n", prevLEDMode);
      }

    }

    if (strcmp(recCommand, "speed") == 0) {
      globalSpeed = atoi(recValue);
      Serial.printf("speed:%d\n", globalSpeed);

    }

    if (strcmp(recCommand, "brightness") == 0) {
      MAX_BRIGHTNESS = atoi(recValue);
      Serial.printf("brightness:%d\n", MAX_BRIGHTNESS);
      FastLED.setBrightness(MAX_BRIGHTNESS);
    }

    if (strcmp(recCommand, "ledbrightness") == 0) {//set the value of the white LED strip (if available)
      analogWrite(PWM_PIN, atoi(recValue));
      Serial.printf("LED brightness:%d\n", atoi(recValue));
    }

    if (strcmp(recCommand, "saturation") == 0) {
      globalSaturation = constrain(atoi(recValue),0,255);
      Serial.printf("saturation:%d\n", globalSaturation);
    }
    if (strcmp(recCommand, "scale") == 0) {
      scale = constrain(atoi(recValue),0,255);
      Serial.printf("scale:%d\n", scale);
    }
        if (strcmp(recCommand, "bspeed") == 0) {
      blendSpeed = constrain(atoi(recValue),0,255);
      Serial.printf("blendspeed:%d\n", blendSpeed);
    }
            if (strcmp(recCommand, "bpm") == 0) {
      BeatsPerMinute = constrain(atoi(recValue),0,255);
      Serial.printf("bpm:%d\n", BeatsPerMinute);
    }



  }//END OF FASTLED BRANCH


  // COMMAND BRANCH
  if (strcmp(recTopic, TOPIC_DEV_COMMAND) == 0) {
    validContent = true;
    Serial.println(TOPIC_DEV_COMMAND " branch");
    if (strcmp(recMsg, "getfw") == 0) {
      tripleBlink(CRGB::Blue);
      validContent = true;
      publishMyFW();
    }
    if (strcmp(recMsg, "getip") == 0) {
      validContent = true;
      publishMyIP();
    }
    if (strcmp(recMsg, "getsketchsize") == 0) {
      validContent = true;
      publishMySketchSize();
    }
    if (strcmp(recMsg, "getfreesize") == 0) {
      validContent = true;
      publishMyFreeSize();
    }
    if (strcmp(recMsg, "getfreeheap") == 0) {
      validContent = true;
      publishMyHeap();
    }

    if (strcmp(recMsg, "getledmode") == 0) {
      validContent = true;
      publishMyLEDmode();
    }

    if (strcmp(recMsg, "error") == 0) {
      errorStatus = true;
      validContent = true;
    }
    if (strcmp(recMsg, "noerror") == 0) {
      errorStatus = false;
      validContent = true;
    }

    if (strcmp(recMsg, "geturl") == 0) {
      publishMyURL();
      validContent = true;
    }

    if (strcmp(recMsg, "prevmode") == 0) {
      validContent = true;
      if (prevLEDMode != ALARM) {
        int tempMode = LEDMode;
        LEDMode = prevLEDMode;
        prevLEDMode = tempMode; //exchange LEDMode and prevLEDModes
        Serial.printf("ledmode:%d\n", LEDMode);
      } else Serial.println("PREVIOUS MODE IS ALARM! NOT CHANGING");

    }



    if (strcmp(recMsg, "reboot") == 0) {
      Serial.println("Rebooting...");
      tripleBlink(CRGB::Blue);
      delay(500);
      ESP.restart();
    }

  }// END OF COMMAND BRANCH

  //TEST BRANCH

  if (strcmp(recTopic, TOPIC_DEV_TEST) == 0) {
    Serial.println(TOPIC_DEV_TEST " branch");
    if (strcmp(recMsg, "clear") == 0) {
      validContent = true;
      LEDMode = OFF;

      Serial.println("test clear");
    }
    else
    { //if the message is not "clear", check the content
      strcpy(recValue, "");

      //try to find segments separated by ":"
      char *found = strtok(recMsg, ":");  //find the first part before the ":"
      if (found != NULL) { //if found...
        strcpy(recValue, found);
        testFrom = atoi(recValue);

        found = strtok(NULL, ":"); //find the second part
        if (found != NULL) {
          strcpy(recValue, "");

          strcpy(recValue, found);
          testTo = atoi(recValue);

          found = strtok(NULL, ":"); //find the third part
          if (found != NULL) {
            strcpy(recValue, "");
            validContent = true;
            strcpy(recValue, found);
            testHue = atoi(recValue);
            Serial.printf("test from:%d to:%d hue:%d\n", testFrom, testTo, testHue);
            LEDMode = TEST;
          }
        }
      }
    }
  }//END OF TEST BRANCH

  //FOTA BRANCH

  if (strcmp(recTopic, TOPIC_DEV_FOTA) == 0 || strcmp(recTopic, TOPIC_ALL_FOTA) == 0) {

    if (strcmp(recMsg, "checknew") == 0) { //command received for checking new FW
      validContent = true;
      checkForUpdates();
    }

  }//END OF FOTA BRANCH

  //SETURL BRANCH !!! NOTE:this can be for all devices! NOTE: any string is accepted
  if (strcmp(recTopic, TOPIC_DEV_SETURL) == 0 || strcmp(recTopic, TOPIC_ALL_SETURL) == 0) {
    strcpy(fwUrlBase, "");
    strcpy(fwUrlBase, recMsg);
    publishMyURL();
    validContent = true;

  }//END OF SETURL BRANCH


  //ALARM BRANCH

  if (strcmp(recTopic, TOPIC_DEV_ALARM) == 0 || strcmp(recTopic, TOPIC_ALL_ALARM) == 0) {
    Serial.println("ALARM branch");
    if ((strcmp(recMsg, "off") == 0)||(strcmp(recMsg, "-2") == 0)) { //command received for switching OFF blinking ALARM and go back to previous mode. Note: "-2" is an easy to send message from Node-red (it is defined there), so another way to switch alarm off
      LEDMode = prevLEDMode;
      Serial.println("ALARM off");
      Serial.printf("ledmode:%d\n", LEDMode);
      validContent = true;
    } else
    {
      Serial.printf("ALARM MODE:%d\n", atoi(recMsg));
      if (LEDMode != ALARM) prevLEDMode = LEDMode;
      LEDMode = ALARM;
      almMode = atoi(recMsg);
      Serial.printf("ledmode:%d\n", LEDMode);
      validContent = true;
    }


  }//END OF ALARM BRANCH

  if (!validContent) Serial.println("Not a valid content");
  msgReceived = false;
}


/**************MESSAGE PROCESS FUNCTIONS END******************************/

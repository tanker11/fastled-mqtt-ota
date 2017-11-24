/*
   Szeretném, ha adott MQTT channelen jövő üzenetre elindulna az upgrade check és az upgrade viszontválaszban leírva, hogy mi zajlik
   EEPROMBA kiírni egy jelecskét, amiből tudja, miért bootolt, és azt is megüzeni, ha felcsatlakozott az MQTT-re
   ellenőrizze, hogy iylen alias már ne legyen több az mqtt-n. Ha van,álljon le! ????

  Működés:
  Feliratkozik az ota/IP_CÍM csatornára
  Ha ezen jön egy üzenet, hogy új FW elérhető, akkor ellenőrzi az elérési helyet, és ha minden OK, akkor letölti. Pl. ellenőrzi, hogy az üzenetben
  hirdetett FW verzió nagyobb-e a mostaninál, ás egyezik-e az elérhetővel. Ha minden OK, frissít. Ha nem, akkor visszaválaszol a serialon és MQTT-n



*/

#include <MD_KeySwitch.h>
const uint8_t SWITCH_PIN = 0;       // switch connected to this pin
const uint8_t SWITCH_ACTIVE = LOW;  // digital signal when switch is pressed 'on'
MD_KeySwitch S(SWITCH_PIN, SWITCH_ACTIVE);

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <Ticker.h>




//Vigyázat, ESP-01 esetén a LED villogtatás miatt nincs többé serial interfész, de OTA-val mehet a frissítés

//FONTOS!!! A következő soroknak a "#include <FastLED.h>" elé kell kerülniük, különben nem érvényesülne
#define FASTLED_INTERRUPT_RETRY_COUNT 0 //to avoid flickering
#include <FastLED.h>
// Another line is needed at setup to avoid Wifi sleep in the setup section: WiFi.setSleepMode(WIFI_NONE_SLEEP);


#define LED_PIN1     5
#define LED_PIN2     6
#define LED_PIN3     7

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB //NEOPIXEL MATRIX and LED STRIPE
//#define COLOR_ORDER RGB //LED FÜZÉR (CHAIN)
int MAX_BRIGHTNESS =  20;



// If you don't use matrix, use =1 on one of the dimensions (for example: 60x1 led stripe)
const uint8_t kMatrixWidth  = 1;
const uint8_t kMatrixHeight = 64;
const bool    kMatrixSerpentineLayout = false;


#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)

#define MILLI_AMPERE      3840    // IMPORTANT: set here the max milli-Amps of your power supply 5V 2A = 2000
#define WIFI_LED_PIN 2 //2=NodeMCU vagy ESP-12, 1=ESP-01 beépített LED


#define RETAINED true

const int FW_VERSION = 1001;
char* fwUrlBase = "http://192.168.1.196/fwtest/fota/"; //FW files should be uploaded to this HTTP directory
// note: alias.bin and alias.version files should be there. Update will be performed if the version file contains bigger number than the FW_VERSION variable

#define ALIAS "alma"
#define TOPIC_DEV_STATUS "/device/" ALIAS "/status"
#define TOPIC_DEV_COMMAND "/device/" ALIAS "/command"
#define TOPIC_DEV_SETURL "/device/" ALIAS "/seturl"
#define TOPIC_DEV_TEST "/device/" ALIAS "/test"
#define TOPIC_DEV_FASTLED "/device/" ALIAS "/fastled"
#define TOPIC_DEV_ALARM "/device/" ALIAS "/alarm"
#define TOPIC_DEV_FOTA "/device/" ALIAS "/fota"
#define TOPIC_DEV_RGB "/device/" ALIAS "/rgb"

#define TOPIC_ALL_SETURL "/all/seturl"
#define TOPIC_ALL_ALARM "/all/alarm"
#define TOPIC_ALL_FOTA "/all/fota"
#define TOPIC_ALL_RGB "/all/rgb"

const char* ssid = "testm";
const char* password = "12345678";
const char* alias = ALIAS;


String topicTemp; //topic string used ofr various publishes
String publishTemp;
char msg[50];
char topic[50];
char recMsg[50];
char recTopic[50];
char recCommand[10];
char recValue[10];


boolean msgReceived; //shows if message received
char digitTemp[3];
unsigned long wifiCheckTimer;


// Replace with the IP address of your MQTT server
IPAddress mqttServerIP(192, 168, 1, 1);

Ticker flipper; //for blinking built-in LED to show wifi and MQTT status




WiFiClient wclient;
PubSubClient client(wclient);

/*

   _____ _    ____ _____ _     _____ ____    _____ _   _ _   _  ____ _____ ___ ___  _   _ ____
  |  ___/ \  / ___|_   _| |   | ____|  _ \  |  ___| | | | \ | |/ ___|_   _|_ _/ _ \| \ | / ___|
  | |_ / _ \ \___ \ | | | |   |  _| | | | | | |_  | | | |  \| | |     | |  | | | | |  \| \___ \
  |  _/ ___ \ ___) || | | |___| |___| |_| | |  _| | |_| | |\  | |___  | |  | | |_| | |\  |___) |
  |_|/_/   \_\____/ |_| |_____|_____|____/  |_|    \___/|_| \_|\____| |_| |___\___/|_| \_|____/

*/

// VARIABLES

enum {OFF, RAINBOW, BPM, MOVE, ALARM, STEADY, AMBERBLINK, TEST, TEST2, _LAST_}; //stores the FastLED modes _LAST_ is used for identify the max number for the sequence
int LEDMode = OFF;
int prevLEDMode = OFF;
int almMode = 0; //global variable for the ALARM color index


float gHue = 0; // rotating "base color" used by many of the patterns - should be float to be able to handle small amount of changes
int FRAMES_PER_SECOND = 50; // here you can control the refresh speed - note this will influence the globalSpeed itself
int globalSpeed = 50; //defines generic animation speed default
int globalSaturation = 255;
uint8_t blendSpeed = 10; //defines palette cross-blend speed

CRGB leds[kMatrixWidth * kMatrixHeight];     //allocate the vector for the LEDs, considering the matrix dimensions
CRGBPalette16 currentPalette( CRGB::Black ); //black palette
CRGBPalette16 targetPalette( CRGB::Black );  //black palette
TBlendType    currentBlending;

bool errorStatus = false;  //indicates if there is a need for error indication (by a blinink first LED in RED)
bool errorBlinkStatus = false;  //indicates the status of the first blinking LED
bool gHueRoll = false; //indicates if gHue roll is activated

int testFrom = 0, testTo = 0, testHue = 0; //variables for test mode

// PALETTES

// This function sets up a palette of purple and green stripes.
void SetFavoritePalette1()
{
  CRGB orange = CHSV( 20, 255, 255);
  CRGB cyan  = CHSV( 110, 255, 255);
  CRGB black  = CRGB::Black;

  targetPalette = CRGBPalette16(
                    cyan,  cyan,  cyan,  cyan,
                    orange, orange,  orange, orange,
                    cyan,  cyan,  cyan,  cyan,
                    orange, orange,  orange, orange );
}

//Traffic light color palette definition

CRGB trafficRed = CHSV( 0, 255, 255);
CRGB trafficAmber = CHSV( 15, 255, 255);
CRGB trafficGreen = CHSV( 110, 255, 255);
CRGB Black  = CRGB::Black;

//Color indeces
//0-15 RED
//16-31 AMBER
//32-47 GREEN
//48-255 BLACK

CRGBPalette16 trafficLightPalette = CRGBPalette16(
                                      trafficRed,  trafficAmber,  trafficGreen,  Black,
                                      Black, Black, Black,  Black,
                                      Black,  Black,  Black,  Black,
                                      Black, Black, Black,  Black );


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

/****************************************************************************
   steadyLight CLASS



 ****************************************************************************/
#define lightMaxNumLEDs NUM_LEDS
#define dimSpeedUp 40
#define dimSpeedDown 40
#define maxDim 255
#define minDim 10 //mindim is the value where the blinking will drop to 0 if below

class steadyLight
{
  public:
    boolean blinkStatus = true;
    int fromLED, toLED; //stores the start and end point of the range of LEDs this instance handles
    short LEDColorIndex;
    short elementDim = 0; //dim values for the element
    bool sinusoidal;


    void setElements (int from, int to, int colorIndex, bool sinus) { //set basic parameters
      LEDColorIndex = colorIndex;
      fromLED = from;
      toLED = to;
      sinusoidal = sinus;

    }

    void setColor (int colorIndex) { //set color only
      LEDColorIndex = colorIndex;
    }

    int saturateValue(int value) {
      int saturated = value;
      if (value < minDim) saturated = 0;
      if (value > maxDim) saturated = maxDim;
      return saturated;
    }

    void On() {
      if (elementDim + dimSpeedUp >= maxDim) {
        //ha a következő lépésben elérjük, vagy közel érünk a maximumhoz, akkor befixáljuk maximumra, és jelezzük, hogy kész
        elementDim = maxDim;
      } else {
        elementDim += dimSpeedUp;
      }
      processLEDBrightness();
    }

    void Off() {
      if (elementDim - dimSpeedDown <= -maxDim) {
        elementDim = -maxDim;
      } else {
        elementDim -= dimSpeedDown;
      }
      processLEDBrightness();
    }

    void processLEDBrightness() {
      /*
         How this works: there is a "profile" of brightness, a curve. The curve is virtually moved upwards and downwards with an offset.
         The result brightness is saturated at maxDim level.
         The remanence brightness (very small values) are dropped to zero if they are below minDim level) see saturateValue() function above
      */
      for (int currentLED = fromLED; currentLED < toLED + 1; currentLED++) {
        int posBrightness, scaled;

        if (sinusoidal) { //in case you need the edges to be shaded with a sinusoidal curve
          scaled = (currentLED - fromLED) * 128 / (toLED - fromLED); //scales to the sine waveform to the range of LEDs
          posBrightness = sin8(scaled);


          //scaled = (currentLED - fromLED) * 256 / (toLED - fromLED); //scales to the cubic waveform to the range of LEDs
          //posBrightness = cubicwave8(scaled);

        } else posBrightness = 128; //otherwise it makes is even brightness for all LEDs

        leds[currentLED] = ColorFromPalette( trafficLightPalette, LEDColorIndex , saturateValue(posBrightness + elementDim), LINEARBLEND);
      }
    }
};



//**********************************-Trafficlight cass vége

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



/**************FastLED FUNCTIONS END******************************/

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
  client.subscribe(TOPIC_DEV_RGB);
  Serial.println("Subscribed to [" TOPIC_DEV_RGB "] topic");
  client.loop();
  client.subscribe(TOPIC_ALL_ALARM);
  Serial.println("Subscribed to [" TOPIC_ALL_ALARM "] topic");
  client.loop();
  client.subscribe(TOPIC_ALL_FOTA);
  Serial.println("Subscribed to [" TOPIC_ALL_FOTA "] topic");
  client.loop();
  client.subscribe(TOPIC_ALL_RGB);
  Serial.println("Subscribed to [" TOPIC_ALL_RGB "] topic");
  client.loop();
  client.subscribe(TOPIC_ALL_SETURL);
  Serial.println("Subscribed to [" TOPIC_ALL_SETURL "] topic");
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
  if (strcmp(recTopic, TOPIC_DEV_FASTLED) == 0) {
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
        prevLEDMode = LEDMode;
        LEDMode = atoi(recValue);
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

    if (strcmp(recCommand, "saturation") == 0) {
      globalSaturation = atoi(recValue);
      if (globalSaturation > 255) globalSaturation = 255;
      if (globalSaturation < 0) globalSaturation = 0;
      Serial.printf("saturation:%d\n", globalSaturation);

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
        int tempMode=LEDMode;
        LEDMode = prevLEDMode;
        prevLEDMode=tempMode; //exchange LEDMode and prevLEDModes
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
    if (strcmp(recMsg, "off") == 0) { //command received for switching OFF blinking ALARM and go back to previous mode
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


/**************OTHER FUNCTIONS END******************************/

/*

  ____  _____ _____ _   _ ____
  / ___|| ____|_   _| | | |  _ \
  \___ \|  _|   | | | | | | |_) |
  ___) | |___  | | | |_| |  __/
  |____/|_____| |_|  \___/|_|

*/

steadyLight *myAlarmLight;
steadyLight *myAmberLight;

void setup()
{
  all_off(); //Clear the LED array
  delay(500); //safety delay
  FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  // FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  // FastLED.addLeds<LED_TYPE, LED_PIN3, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );

  FastLED.setBrightness(MAX_BRIGHTNESS);
  //set_max_power_in_volts_and_milliamps(5, MILLI_AMPERE); //depreciated
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPERE); //5Volt LEDs
  Serial.begin(115200);
  pinMode(WIFI_LED_PIN, OUTPUT);     // Initialize the INDICATOR_PIN pin as an output
  Serial.println("Booting...");

  //KeySwitch init
  S.begin();
  S.enableDoublePress(true);
  S.enableLongPress(true);
  S.enableRepeat(false);
  S.enableRepeatResult(false);
  S.setLongPressTime(1000);
  S.setDoublePressTime(500);


  Serial.println(__TIMESTAMP__);
  Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
  Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());
  Serial.print( "Current firmware version: " );
  Serial.println( FW_VERSION );
  Serial.println();

  //  connectWifi();
  client.setServer(mqttServerIP, 1883);
  client.setCallback(callback);

  // WiFi.setSleepMode(WIFI_NONE_SLEEP); //avoid wifi sleep to get rid of flickering AND wifi issues

  msgReceived = false;



  Serial.printf("Number of FastLED modes: %d\n", _LAST_);
  Serial.println();

  myAlarmLight = new steadyLight();
  myAlarmLight->setElements(0, NUM_LEDS - 1, 0, true); //define all the LEDs for ALARM with RED color as a basis

  myAmberLight = new steadyLight();
  myAmberLight->setElements(0, 63 - 16, 16, true); //watch out for LED color index from the trafficLightPalette (amber is on position 16)


}

/*

  __  __    _    ___ _   _   _     ___   ___  ____
  |  \/  |  / \  |_ _| \ | | | |   / _ \ / _ \|  _ \
  | |\/| | / _ \  | ||  \| | | |  | | | | | | | |_) |
  | |  | |/ ___ \ | || |\  | | |__| |_| | |_| |  __/
  |_|  |_/_/   \_\___|_| \_| |_____\___/ \___/|_|

*/

void loop() {

  /**************CONNECT/RECONNECT SEQUENCE ******************************/
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, password);
    if (LEDMode != OFF) prevLEDMode = LEDMode;
    LEDMode = OFF;
    FastLED.setBrightness(5);
    flipper.attach(0.1, flip); //blink quickly during wifi connection

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
    Serial.println("WiFi connected");
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (!client.connected()) {
      Serial.println("Connecting mqtt client...");

      if (LEDMode != OFF) prevLEDMode = LEDMode; //preserve the previous mode if not already OFF
      LEDMode = OFF;
      FastLED.setBrightness(5); //set a dimmed value
      flipper.attach(0.25, flip); //blink slower during client connection

      //connect with Last Will message as "offline"/retained. This will be published when incorrectly disconnected
      strcpy(msg, "");
      if (client.connect(alias, TOPIC_DEV_STATUS, 1, 1, "offline")) { //boolean connect (clientID, willTopic, willQoS, willRetain, willMessage)
        Serial.println("Client connected");
        Serial.println();


        LEDMode = prevLEDMode; //restore the previous LEDMode
        FastLED.setBrightness(MAX_BRIGHTNESS); //restore the MAX_BRIGHTNESS
        flipper.detach();
        digitalWrite(WIFI_LED_PIN, HIGH); //switch off the flashing LED when connected

        //Subscribe to topics
        subscribeToTopics();

        //Publish online status

        client.publish(TOPIC_DEV_STATUS, "online", RETAINED);

        publishMyIP();
        publishMyFW();
      }
      else
      {
        delay(3000); //if disconnected from mqtt, wait for a while
      }
    }
    /**************CONNECT/RECONNECT SEQUENCE END ******************************/


    /*

       ____ ___  ____  _____
      / ___/ _ \|  _ \| ____|
      | |  | | | | |_) |  _|
      | |__| |_| |  _ <| |___
      \____\___/|_| \_\_____|

    */

    if (client.connected()) client.loop();
    if (msgReceived) processRecMessage();


    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);

    // do some periodic updates
    EVERY_N_MILLISECONDS( 20 ) {
      //gHue++;  // slowly cycle the "base color" through the rainbow
      if (gHueRoll) gHue = gHue + globalSpeed / 10.00;
    }

    nblendPaletteTowardPalette( currentPalette, targetPalette, blendSpeed);

    // Set FastLED mode
    switch (LEDMode) {
      case OFF:   fill_solid( targetPalette, 16, CRGB::Black); all_off() ; gHueRoll = false; break;
      case STEADY: gHueRoll = false; steady(0, NUM_LEDS - 1, CHSV(0, globalSaturation, 255)); break;
      case RAINBOW: targetPalette = RainbowColors_p; gHueRoll = true; rainbow(); pastelizeColors(); break;
      case BPM: targetPalette = PartyColors_p; gHueRoll = false; bpm(); pastelizeColors(); break;
      case MOVE: gHueRoll = false; SetFavoritePalette1(); move(); pastelizeColors(); break; //EZT MÉG MEGCSINÁLNI MOZGÓRA! ESETLEG ELHALVÁNYÍTÁSSAL (MARQUEE)
      case TEST: gHueRoll = false; steady(testFrom, testTo, CHSV(testHue, globalSaturation, 255));  break;
      case ALARM: gHueRoll = false;  myAlarmLight->setColor(almMode);
        if (myAlarmLight->blinkStatus) myAlarmLight->On();
        if (!myAlarmLight->blinkStatus) myAlarmLight->Off();
        EVERY_N_MILLISECONDS( 1000 ) {
          myAlarmLight->blinkStatus = !myAlarmLight->blinkStatus;
        };
        break;
      case AMBERBLINK: gHueRoll = false;
        if (myAmberLight->blinkStatus) myAmberLight->On();
        if (!myAmberLight->blinkStatus) myAmberLight->Off();
        EVERY_N_MILLISECONDS( 800 ) {
          myAmberLight->blinkStatus = !myAmberLight->blinkStatus;
        };
        break;

    }

    blinkErrorLED(CRGB::Red); //blink the first LED in case of error
    // send the 'leds' array out to the actual LED strip
    FastLED.show();

    //Handle keypresses
    switch (S.read())
    {
      case MD_KeySwitch::KS_NULL:       /* Serial.println("NULL"); */   break;
      case MD_KeySwitch::KS_PRESS:      Serial.print("\nSINGLE PRESS"); break;
      case MD_KeySwitch::KS_DPRESS:     Serial.print("\nDOUBLE PRESS"); break;
      case MD_KeySwitch::KS_LONGPRESS:  Serial.print("\nLONG PRESS");   break;
      case MD_KeySwitch::KS_RPTPRESS:   Serial.print("\nREPEAT PRESS"); break;
      default:                          Serial.print("\nUNKNOWN");      break;
    }


  } //WIFI STATUS == CONNECTED

} //MAIN LOOP



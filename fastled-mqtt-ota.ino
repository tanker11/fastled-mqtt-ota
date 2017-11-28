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
const uint8_t kMatrixWidth  = 1; //Number of LEDs cannot be less than 3!!! If 3, sinusoid must be FALSE!!!
const uint8_t kMatrixHeight = 60;
const bool    kMatrixSerpentineLayout = false;
const bool sinusoid = true;


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
const unsigned long connectRepeatTimer = 5000; //needs to be at least about 5s, otherwise the reconnection is not guaranteed


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

// VARIABLES

enum {OFF, RAINBOW, BPM, MOVE, ALARM, STEADY, TRAFFICLIGHT, AMBERBLINK, NOISE, NOISE1, TEST, EMERGENCY, _LAST_}; //stores the FastLED modes _LAST_ is used for identify the max number for the sequence
int LEDMode = OFF;
int prevLEDMode = OFF;
int almMode = 0; //global variable for the ALARM color index


float gHue = 0; // rotating "base color" used by many of the patterns - should be float to be able to handle small amount of changes
int FRAMES_PER_SECOND = 50; // here you can control the refresh speed - note this will influence the globalSpeed itself
int globalSpeed = 50; //defines generic animation speed default
int globalSaturation = 255;
uint8_t blendSpeed = 10; //defines palette cross-blend speed

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise will be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
// Megjegyzés: ezt használjuk már animációkban is (nem csak 1-10-ig), ott átszámoljuk a megfelelő skálára
int scale = 20; // scale is set dynamically once we've started up
int new_scale = 20;
// The 16 bit version of our coordinates
static uint16_t x;
static uint16_t y;
static uint16_t z;

CRGB leds[kMatrixWidth * kMatrixHeight];     //allocate the vector for the LEDs, considering the matrix dimensions
CRGBPalette16 currentPalette( CRGB::Black ); //black palette
CRGBPalette16 targetPalette( CRGB::Black );  //black palette
TBlendType    currentBlending;

// This is the array that we keep our computed noise values in
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];

bool errorStatus = false;  //indicates if there is a need for error indication (by a blinink first LED in RED)
bool errorBlinkStatus = false;  //indicates the status of the first blinking LED
bool gHueRoll = false; //indicates if gHue roll is activated

int testFrom = 0, testTo = 0, testHue = 0; //variables for test mode

short tlStatusVar = 0; //Traffic light status variable 0:Red, 1:Red-Amber, 2: Green, 3: Amber
unsigned long tlTimingLamp; //Traffic light timing before the next stage
unsigned long tlMillis; //Timing millis for Traffic Light

short emergencyState = 0; //0: off, 1: 25 %, 2: 50 %, 3: 75 %, 4: 100 % white all LEDs

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

//####FastLED functions

//####FOTA functions

//####MQTT functions

//####Message process functions

//####Helper functions

/*

  ____  _____ _____ _   _ ____
  / ___|| ____|_   _| | | |  _ \
  \___ \|  _|   | | | | | | |_) |
  ___) | |___  | | | |_| |  __/
  |____/|_____| |_|  \___/|_|

*/

steadyLight *myAlarmLight;
steadyLight *myRedLight;
steadyLight *myAmberLight;
steadyLight *myGreenLight;

void setup()
{
  all_off_immediate(); //Clear the LED array
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
  S.setLongPressTime(3000);
  S.setDoublePressTime(1000);


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
  myAlarmLight->setElements(0, NUM_LEDS - 1, 0, sinusoid); //define all the LEDs for ALARM with RED color as a basis

  myRedLight = new steadyLight();
  myRedLight->setElements(0, round((NUM_LEDS / 3) - 1), 0, sinusoid); //watch out for LED color index from the trafficLightPalette (red is on position 0)
  myAmberLight = new steadyLight();
  myAmberLight->setElements(round(NUM_LEDS / 3), round((NUM_LEDS * 2 / 3) - 1), 16, sinusoid); //watch out for LED color index from the trafficLightPalette (amber is on position 16)
  myGreenLight = new steadyLight();
  myGreenLight->setElements(round(NUM_LEDS * 2 / 3), NUM_LEDS - 1 , 32, sinusoid); //watch out for LED color index from the trafficLightPalette (green is on position 32)


}

/*

  __  __    _    ___ _   _   _     ___   ___  ____
  |  \/  |  / \  |_ _| \ | | | |   / _ \ / _ \|  _ \
  | |\/| | / _ \  | ||  \| | | |  | | | | | | | |_) |
  | |  | |/ ___ \ | || |\  | | |__| |_| | |_| |  __/
  |_|  |_/_/   \_\___|_| \_| |_____\___/ \___/|_|

*/

void loop() {

  handleKeypresses();

  /**************CONNECT/RECONNECT SEQUENCE ******************************/
  if ((WiFi.status() != WL_CONNECTED) && (isTimeout(wifiCheckTimer, connectRepeatTimer))) {
    wifiCheckTimer = millis();
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, password);
    if (!emergencyState) {  //blinks only if not in emergency light mode
      if (LEDMode != OFF) prevLEDMode = LEDMode;
      LEDMode = OFF;
      FastLED.setBrightness(5);
      flipper.attach(0.1, flip); //blink quickly during wifi connection
    }
  }

  if (WiFi.status() == WL_CONNECTED) {

    if ((!client.connected()) && (isTimeout(wifiCheckTimer, connectRepeatTimer))) { //implement delay for retry here if not connected
      wifiCheckTimer = millis();
      Serial.println("Connecting mqtt client...");
      if (!emergencyState) {
        if (LEDMode != OFF) prevLEDMode = LEDMode; //preserve the previous mode if not already OFF
        LEDMode = OFF;
        FastLED.setBrightness(5); //set a dimmed value
        flipper.attach(0.25, flip); //blink slower during client connection
      }
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
        //delay(3000); //if disconnected from mqtt, wait for a while
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
      case NOISE: targetPalette = OceanColors_p; gHueRoll = true; scale = 25; handleNoise(); pastelizeColors(); break;
      case NOISE1: targetPalette = LavaColors_p; gHueRoll = true; scale = 15; handleNoise(); pastelizeColors(); break;
      case EMERGENCY: /*Nothing to do here, handled separately*/ break;
      case TEST: gHueRoll = false; steady(testFrom, testTo, CHSV(testHue, globalSaturation, 255));  break;
      case ALARM: gHueRoll = false;  myAlarmLight->setColor(almMode);
        if (myAlarmLight->blinkStatus) myAlarmLight->On();
        if (!myAlarmLight->blinkStatus) myAlarmLight->Off();
        EVERY_N_MILLISECONDS( 1000 ) {
          myAlarmLight->blinkStatus = !myAlarmLight->blinkStatus;
        };
        break;
      case AMBERBLINK: gHueRoll = false;
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
      case TRAFFICLIGHT: gHueRoll = false;
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

    blinkErrorLED(CRGB::Red); //blink the first LED in case of error
    // send the 'leds' array out to the actual LED strip

    FastLED.show();



  } //WIFI STATUS == CONNECTED

} //MAIN LOOP




/*
   Szeretném, ha adott MQTT channelen jövő üzenetre elindulna az upgrade check és az upgrade viszontválaszban leírva, hogy mi zajlik
   EEPROMBA kiírni egy jelecskét, amiből tudja, miért bootolt, és azt is megüzeni, ha felcsatlakozott az MQTT-re
   ellenőrizze, hogy iylen alias már ne legyen több az mqtt-n. Ha van,álljon le! ????

   MQTT üzenettel lehessen újraindítani is: ESP.restart();

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
#include <FastLED.h>

#include <FastLED.h>

//Vigyázat, ESP-01 esetén a LED villogtatás miatt nincs többé serial interfész, de OTA-val mehet a frissítés

//#define FASTLED_ALLOW_INTERRUPTS 0  //ki kell kommentelni, ha fényfüzérről van szó. A LED szalaghoz és NEOPIXEL mátrixhoz kell, különben van flicker
//Megfigyelés: ha ki van kommentelve, akkor úgy tűnik, megy az OTA...
#define FASTLED_INTERRUPT_RETRY_COUNT 3
#define LED_PIN     5
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB //NEOPIXEL MATRIX and LED STRIPE
//#define COLOR_ORDER RGB //LED FÜZÉR (CHAIN)
int MAX_BRIGHTNESS =           32;

const uint8_t kMatrixWidth  = 8  ;
const uint8_t kMatrixHeight = 8;
const bool    kMatrixSerpentineLayout = false;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)

// The leds
CRGB leds[kMatrixWidth * kMatrixHeight];

#define MILLI_AMPERE      3000    // IMPORTANT: set here the max milli-Amps of your power supply 5V 2A = 2000
#define FRAMES_PER_SECOND  100    // here you can control the speed. 


#define WIFI_LED_PIN 2 //2=NodeMCU vagy ESP-12, 1=ESP-01 beépített LED
#define RETAINED true

const int FW_VERSION = 1001;
const char* fwUrlBase = "http://192.168.1.196/fwtest/fota/"; //FW files should be uploaded to this HTTP directory
// note: alias.bin and alias.version files should be there. Update will be performed if the version file contains bigger number than the FW_VERSION variable

#define ALIAS "alma"
#define TOPIC_DEV_STATUS "/device/" ALIAS "/status"
#define TOPIC_DEV_COMMAND "/device/" ALIAS "/command"
#define TOPIC_DEV_TEST "/device/" ALIAS "/test"
#define TOPIC_DEV_SETURL "/device/" ALIAS "/seturl"
#define TOPIC_DEV_FASTLED "/device/" ALIAS "/fastled"
#define TOPIC_DEV_ALARM "/device/" ALIAS "/alarm"
#define TOPIC_DEV_FOTA "/device/" ALIAS "/fota"
#define TOPIC_DEV_RGB "/device/" ALIAS "/rgb"
#define TOPIC_DEV_BRIGHTNESS "/device/" ALIAS "/brightness"

#define TOPIC_ALL_ALARM "/all/alarm"
#define TOPIC_ALL_FOTA "/all/fota"
#define TOPIC_ALL_RGB "/all/rgb"
#define TOPIC_ALL_BRIGHTNESS "/all/brightness"

const char* ssid = "testm";
const char* password = "12345678";
const char* alias = ALIAS;


String topicTemp; //topic string used ofr various publishes
String publishTemp;
char msg[50];
char topic[50];
char recMsg[50];
char recTopic[50];
boolean msgReceived; //shows if message received
char digitTemp[3];
unsigned long wifiCheckTimer;


// Replace with the IP address of your MQTT server
IPAddress mqttServerIP(192, 168, 1, 1);

Ticker flipper;

WiFiClient wclient;
PubSubClient client(wclient);

/****************FastLED FUNCTIONS****************************/
void gradientRedGreen() {
  fill_gradient(leds, 0, CHSV(HUE_RED, 255, 255), NUM_LEDS - 1, CHSV(HUE_GREEN, 255, 255), SHORTEST_HUES);

  FastLED.show();

}

void tripleBlueBlink() {
  for (int j = 0; j < 3; j++) {
    fill_solid( leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(200);
    fill_solid( leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(400);
  }
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}





/**************FastLED FUNCTIONS END******************************/


/**************FOTA FUNCTIONS ******************************/
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
  tripleBlueBlink();

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

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      Serial.println( "Update started." );
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



/**************MQTT FUNCTIONS ******************************/
//Subscribe function
void subscribeToTopics() {

  client.subscribe(TOPIC_DEV_COMMAND);
  Serial.println("Subscribed to [" TOPIC_DEV_COMMAND "] topic");
  client.subscribe(TOPIC_DEV_TEST);
  Serial.println("Subscribed to [" TOPIC_DEV_TEST "] topic");
  client.subscribe(TOPIC_DEV_SETURL);
  Serial.println("Subscribed to [" TOPIC_DEV_SETURL "] topic");
  client.subscribe(TOPIC_DEV_FASTLED);
  Serial.println("Subscribed to [" TOPIC_DEV_FASTLED "] topic");
  client.subscribe(TOPIC_DEV_ALARM);
  Serial.println("Subscribed to [" TOPIC_DEV_ALARM "] topic");
  client.subscribe(TOPIC_DEV_FOTA);
  Serial.println("Subscribed to [" TOPIC_DEV_FOTA "] topic");
  client.subscribe(TOPIC_DEV_RGB);/*
  Serial.println("Subscribed to [" TOPIC_DEV_RGB "] topic");
  client.subscribe(TOPIC_DEV_BRIGHTNESS);
  Serial.println("Subscribed to [" TOPIC_DEV_BRIGHTNESS "] topic");
  
  client.subscribe(TOPIC_ALL_ALARM);
  Serial.println("Subscribed to [" TOPIC_ALL_ALARM "] topic");
  client.subscribe(TOPIC_ALL_FOTA);
  Serial.println("Subscribed to [" TOPIC_ALL_FOTA "] topic");
  client.subscribe(TOPIC_ALL_RGB);
  Serial.println("Subscribed to [" TOPIC_ALL_RGB "] topic");
  client.subscribe(TOPIC_ALL_BRIGHTNESS);
  Serial.println("Subscribed to [" TOPIC_ALL_BRIGHTNESS "] topic");*/



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

  //Publishing own FW version on device/[alias]/fw/ topic
  strcpy(msg, "");
  sprintf(msg, "SKETCH: %i", ESP.getSketchSize());
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

/**************MESSAGE PROCESS FUNCTIONS ******************************/


void processRecMessage() {

  if (strcmp(recTopic, TOPIC_DEV_COMMAND) == 0) {
    //Serial.println(TOPIC_DEV_COMMAND " branch");
    if (strcmp(recMsg, "getfw") == 0) {
      publishMyFW();
    }
    if (strcmp(recMsg, "getip") == 0) {
      publishMyIP();
    }
    if (strcmp(recMsg, "getsketchsize") == 0) {
      publishMySketchSize();
    }
    if (strcmp(recMsg, "getfreesize") == 0) {
      publishMyFreeSize();
    }

    if (strcmp(recMsg, "reboot") == 0) {
      Serial.println("Rebooting...");
      delay(1000);
      ESP.restart();
    }

  }
  if (strcmp(recTopic, TOPIC_DEV_FOTA) == 0 || strcmp(recTopic, TOPIC_ALL_FOTA) == 0) {
    Serial.println("FOTA branch");
    if (strcmp(recMsg, "checknew") == 0) { //command received for checking new FW
      checkForUpdates();
    }

  }
  msgReceived = false;
}

/**************MESSAGE PROCESS FUNCTIONS END******************************/

/**************OTHER FUNCTIONS******************************/

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

}


/**************OTHER FUNCTIONS END******************************/

void setup()
{
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

  msgReceived = false;

  delay(300); //safety delay
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(MAX_BRIGHTNESS);
  set_max_power_in_volts_and_milliamps(5, MILLI_AMPERE); //5Volt LEDs

}

void loop() {

  /**************CONNECT/RECONNECT SEQUENCE ******************************/
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, password);
    flipper.attach(0.1, flip); //blink quickly during wifi connection
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
    Serial.println("WiFi connected");
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (!client.connected()) {
      Serial.println("Connecting mqtt client...");
      flipper.attach(0.25, flip); //blink slower during client connection

      //connect with Last Will message as "offline"/retained. This will be published when incorrectly disconnected
      strcpy(msg, "");
      if (client.connect(alias, TOPIC_DEV_STATUS, 1, 1, "offline")) { //boolean connect (clientID, willTopic, willQoS, willRetain, willMessage)
        Serial.println("Client connected");
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


    if (client.connected()) client.loop();
    if (msgReceived) processRecMessage();


    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);

    bpm();
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


  }
}



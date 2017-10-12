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

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <Ticker.h>
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

const char* ssid = "testm";
const char* password = "12345678";
const char* alias = "flmqttota001";
String topicTemp; //topic string used ofr various publishes
String publishTemp;
char msg[50];
char topic[50];
char recMsg[50];
char recTopic[50];
boolean msgReceived; //shows if message received
char serMessage[100];
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
void flip()
{
  int state = digitalRead(WIFI_LED_PIN);  // get the current state of GPIO2 pin
  digitalWrite(WIFI_LED_PIN, !state);     // set pin to the opposite state

}

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
      strcpy(topic, "");
      sprintf(topic, "device/%s/status/", alias);
      client.publish(topic, "FW found, downloading");

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      Serial.println( "Update started." );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          //Publish failed status
          strcpy(topic, "");
          sprintf(topic, "device/%s/status/", alias);
          client.publish(topic, "HTTP update failed");
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          //Publish no HTTP update status
          strcpy(topic, "");
          sprintf(topic, "device/%s/status/", alias);
          client.publish(topic, "No HTTP updates");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
      //Publish FW found status
      strcpy(topic, "");
      sprintf(topic, "device/%s/status/", alias);
      client.publish(topic, "Already on latest version");
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
    //Publish failed status
    strcpy(topic, "");
    sprintf(topic, "device/%s/status/", alias);
    client.publish(topic, "Could not check available FW");
  }
  httpClient.end();
}

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


// Callback function
void callback(char* topic, byte* payload, unsigned int length) {
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

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(WIFI_LED_PIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(WIFI_LED_PIN, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void processRecMessage() {

  if (strcmp(recTopic, "fota") == 0) {
    Serial.println("Fota branch");
    if (strcmp(recMsg, "checknew") == 0) { //command received for checking new FW
      checkForUpdates();
    }
    if (strcmp(recMsg, "reboot") == 0) { //command received for reboot
      Serial.println("Rebooting...");
      delay(1000);
      ESP.restart();
    }

  }
  if (strcmp(recTopic, "alarm") == 0) {
    Serial.println("Alarm branch");
    if (strcmp(recMsg, "valami") == 0) {

    }

  }


  msgReceived = false;
}

void setup()
{
  Serial.begin(115200);
  pinMode(WIFI_LED_PIN, OUTPUT);     // Initialize the INDICATOR_PIN pin as an output
  Serial.println("Booting...");


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
      strcpy(msg, "");
      strcpy(topic, "");
      sprintf(topic, "device/%s/status/", alias);
      if (client.connect(alias, topic, 1, 1, "offline")) { //boolean connect (clientID, willTopic, willQoS, willRetain, willMessage)
        //connect with Last Will message as "offline"/retained. This will be published when incorrectly disconnected
        Serial.println("Client connected");
        flipper.detach();
        digitalWrite(WIFI_LED_PIN, HIGH); //switch off the flashing LED when connected

        //Subscribe to alarm and fota topics
        client.subscribe("alarm");
        Serial.println("Subscribed to [alarm] topic");
        client.subscribe("fota");
        Serial.println("Subscribed to [fota] topic");

        //Publish online status
        strcpy(topic, "");
        sprintf(topic, "device/%s/status/", alias);
        client.publish(topic, "online", RETAINED);

        IPAddress local = WiFi.localIP();

        //Publishing own IP on device/[alias]/ip/ topic
        strcpy(msg, "");
        strcpy(topic, "");
        sprintf(msg, "%i.%i.%i.%i", local[0], local[1], local[2], local[3]);
        sprintf(topic, "device/%s/ip/", alias);
        client.publish(topic, msg, RETAINED);
        strcpy(serMessage, "");
        sprintf(serMessage, "MQTT topic: %s, message: %s", topic, msg);
        Serial.println(serMessage);

        //Publishing own FW version on device/[alias]/fw/ topic
        strcpy(msg, "");
        strcpy(topic, "");
        sprintf(msg, "%i", FW_VERSION);
        sprintf(topic, "device/%s/fw/", alias);
        client.publish(topic, msg, RETAINED);
        strcpy(serMessage, "");
        sprintf(serMessage, "MQTT topic: %s, message: %s", topic, msg);
        Serial.println(serMessage);
        /*
          Serial.println("Publishing " + topicTemp + publishTemp);
          topicTemp = "device/" + alias + "/fw/";
          publishTemp = String(FW_VERSION);
          client.publish(topicTemp, publishTemp, RETAINED);
          Serial.println("Publishing " + topicTemp + publishTemp);*/
      }
      else
      {
        delay(3000); //if disconnected from mqtt, wait for a while
      }
    }

    if (client.connected()) client.loop();
    if (msgReceived) processRecMessage();


    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);


    // send the 'leds' array out to the actual LED strip
    FastLED.show();
  }
}



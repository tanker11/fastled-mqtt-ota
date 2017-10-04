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

//Vigyázat, ESP-01 esetén a LED villogtatás miatt nincs többé serial interfész, de OTA-val mehet a frissítés

#define LED_PIN 2 //2=NodeMCU vagy ESP-12, 1=ESP-01 beépített LED

const int FW_VERSION = 1244;
const char* fwUrlBase = "http://192.168.1.196/fwtest/fota/"; //FW files should be uploaded to this HTTP directory
// note: alias.bin and alias.version files should be there. Update will be performed if the version file contains bigger number than the FW_VERSION variable

const char* ssid = "testm";
const char* password = "12345678";
String alias = "alma";

unsigned long wifiCheckTimer;

IPAddress myLocalIP;
// Replace with the IP address of your MQTT server
IPAddress mqttServerIP(192, 168, 1, 1);

Ticker flipper;

WiFiClient wclient;
PubSubClient client(wclient, mqttServerIP);

void flip()
{
  int state = digitalRead(LED_PIN);  // get the current state of GPIO2 pin
  digitalWrite(LED_PIN, !state);     // set pin to the opposite state

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

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      Serial.println( "Update started." );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
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
void callback(const MQTT::Publish& pub) {
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.

  // Copy the payload to a new message
  MQTT::Publish newpub("outTopic", pub.payload(), pub.payload_len());
  client.publish(newpub);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);     // Initialize the INDICATOR_PIN pin as an output
  Serial.println("Booting...");


  Serial.println(__TIMESTAMP__);
  Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
  Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());
  Serial.print( "Current firmware version: " );
  Serial.println( FW_VERSION );
  Serial.println();

  //  connectWifi();
  delay(100);

}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, password);
    flipper.attach(0.1, flip); //blink quickly during wiwif connection
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
    Serial.println("WiFi connected");
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (!client.connected()) {
      Serial.println("Connecting mqtt client...");
      flipper.attach(0.25, flip); //blink slower during client connection
      if (client.connect(alias)) {
        Serial.println("Client connected");
        client.publish("outtopic", "hello world");
        Serial.println("Published on outtopic: hello world");
        client.set_callback(callback);
        Serial.println("Callback set");
        client.subscribe("intopic");
        Serial.println("Subscribed to intopic");
      }
      else
      {
        delay(3000); //if disconnected from mqtt, wait for a while
      }
    }

    if (client.connected())
      client.loop();
    flipper.detach();
    digitalWrite(LED_PIN, HIGH); //switch off the flashing LED when connected
  }
}



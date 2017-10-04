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
String alias="alma";

unsigned long wifiCheckTimer;

IPAddress myLocalIP;
// Replace with the IP address of your MQTT server
IPAddress mqttServerIP(192, 168, 1, 1);

Ticker flipper;

void flip()
{
  int state = digitalRead(LED_PIN);  // get the current state of GPIO2 pin
  digitalWrite(LED_PIN, !state);     // set pin to the opposite state

}

void connectWifi() {

  Serial.println("Starting wifi client connection...");

  WiFi.mode(WIFI_STA);


  //WiFi.hostname(myHostname); Nem megy a hosztnév definiálása
  WiFi.begin ( ssid, password );

  int connRes = WiFi.waitForConnectResult();

  if (connRes == 3) {
    flipper.attach(0.3, flip); //success, start blinking the indicator LED with 0.3 freq
    Serial.print("Connected to: ");
    Serial.println(ssid);
    Serial.print("IP: ");


    myLocalIP = WiFi.localIP();
    Serial.println(myLocalIP);
    Serial.print("Last digit: ");
    Serial.println(myLocalIP[3]);
  }
  else {
    Serial.print("Error, could not connect to ");
    Serial.println(ssid);
    flipper.attach(0.1, flip); //error, flip quickly
  }
}

/** Decodes the WLAN status into strings and writes to the serial interface **/
void decodeWifiStatuses(int wifiStatus) {
  switch (wifiStatus) {
    case 255  :
      Serial.println (" = WL_NO_SHIELD");
      break;
    case 0  :
      Serial.println (" = WL_IDLE_STATUS");
      break; //optional
    case 1  :
      Serial.println (" = WL_NO_SSID_AVAIL");
      break; //optional
    case 2  :
      Serial.println (" = WL_SCAN_COMPLETED");
      break; //optional
    case 3  :
      Serial.println (" = WL_CONNECTED");
      break; //optional
    case 4  :
      Serial.println (" = WL_CONNECT_FAILED");
      break; //optional
    case 5  :
      Serial.println (" = WL_CONNECTION_LOST");
      break; //optional
    case 6  :
      Serial.println (" = WL_DISCONNECTED");
      break; //optional
    // you can have any number of case statements.
    default : //Optional
      Serial.println (" Not identified status");
  }
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

int checkWifiStatus() {
  int connRes = WiFi.waitForConnectResult();
  Serial.print("Checking WiFi status");
  decodeWifiStatuses(connRes);
  return connRes;
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);     // Initialize the INDICATOR_PIN pin as an output
  Serial.println("Booting...");
  flipper.attach(0.1, flip); //flip quickly during boot

  Serial.println(__TIMESTAMP__);
  Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
  Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());
  Serial.print( "Current firmware version: " );
  Serial.println( FW_VERSION );
  Serial.println();

  connectWifi();
  delay(100);
  WiFiClient wclient;
  PubSubClient client(wclient, mqttServerIP);
}

void loop()
{

  if (isTimeout(wifiCheckTimer, 5000)) {
    checkWifiStatus();
    wifiCheckTimer = millis();
  }


}



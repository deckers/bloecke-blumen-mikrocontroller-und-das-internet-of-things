/* 
               _ _      
 ___ _ __ ___ (_) | ___ 
/ __| '_ ` _ \| | |/ _ \
\__ \ | | | | | | |  __/
|___/_| |_| |_|_|_|\___|

Internet-Wetter-Lampe v1.0.6 - letzte Aenderung am 30. August 2019 - entwickelt an der Abteilung "Didaktik der Informatik" an der Universitaet in Oldenburg

*/

/* 
Hier: Mit Anpassungen für die Verwendung von sehr preiswerten 0.96"-OLED Displays (128*64) [I2C SSD1306 12864 LCD Screen Board]. 
      Mit Adafruit_SSD1306.h werden diese nicht erkannt, stattdessen kann SSD1306.h von ThingPulse, Fabrice Weinberg verwendet werden.
      Dazu ist "ESP8266 and ESP32 OLED driver for SSD1306 displays" in der Arduino IDE über den Boardverwalter zu installieren. (jd)     
*/      

#include "FastLED.h"                      // Bibliothek einbinden, um LED ansteuern zu koennen
CRGB leds[1];                              // Instanziieren der LED
#define LED_DATA_PIN D4                    // an welchem Pin liegt die LED an?

#include <SPI.h>                          // Bibliotheken einbinden, um das OLED Display ansteuern zu koennen
#include <Wire.h>

#include <SSD1306.h>
#define OLED_RESET 0                       // "0" fuer ESP8266
#define SCREEN_WIDTH 128                   // OLED display width, in pixels
#define SCREEN_HEIGHT 64                   // OLED display height, in pixels
SSD1306 display(0x3C,D2,D5);               // D2 für SCK, D5 für SDA 

#include <RestClient.h>                   // Bibliothek einbinden, um Get-Requests senden zu koennen
RestClient client = RestClient("api.openweathermap.org");  // RestClient der Openweathermap-API (Hinweis: Port mit Komma uebergeben)
/* Achtung: In RestClient.cpp gibt es in RestClient::request ein delay, das bei schnellen Internetverbindungen zu Problemen führt:
 *     //make sure you write all those bytes.#
 *     delay(100);
 * Bei schnellen Verbindugen kann es sein, dass die Verbindung serverseitig nach diesen 0,1 Sekunden bereits geschlossen ist.
 * Ggf. ist die delay-Zeile daher auszukommentieren!
 * Alternativ sollte eine Änderung in RestClient::readResponse helfen:
 * statt  while (client.connected())  kann dort auch   while (client.available()) verwendet werden.
 * (jd)
 */


#include <ArduinoJson.h>                  // Bibliothek einbinden, um JSONs parsen zu koennen

#include <math.h>                         // Bibliothek einbinden, um Temperaturen runden zu koennen

#include <WiFiManager.h>                  // Bibliothek einbinden, um Uebergabe der WiFi Credentials ueber einen AP zu ermoeglichen
WiFiManager wifiManager;



// ========================  hier deinen API-Key eintragen!!!  ============================================================================================================

const String api_key = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";    // dein Open-Weather-Map-API-Schluessel, kostenlos beziehbar ueber https://openweathermap.org/

// ========================================================================================================================================================================



int weatherID = 0;
int weatherID_shortened = weatherID / 100;
String weatherforecast_shortened = " ";
float temperature_Kelvin = 0.0;
float temperature_Celsius = temperature_Kelvin - 273;      // Hinweis: Celsiuswert + 273 = Kelvinwert
int temperature_Celsius_Int;
unsigned long systemtime = 0;                              // Zeit, die seit dem Start des Mikrocontrollers vergangen ist
unsigned long lastcheck = 0;                               // Zeitpunkt des letzten Checks



void setup() {
  Serial.begin(115200);                               // fuer die Ausgabe des seriellen Monitors, Geschwindigkeit ggf. anpassen!

  display.init();                                     // das Display initialisieren, 
  display.flipScreenVertically();                     // den Bildschirm vertikal spiegeln (sodass die vier Kontakte oberhalb des Displays sind)
  display.clear();                                    // ... den Inhalt löschen ...
  display.drawString(0, 16, "Verbinde dich mit");
  display.drawString(0, 28, "dem WLAN SmarteLampe");
  display.drawString(0, 40, "und öffne im Browser");
  display.drawString(0, 52, "192.168.4.1");
  display.display();                                // ... und die Änderungen anzeigen

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(leds, 1); // LED instanziieren
  FastLED.show();

  leds[0] = CRGB::Green;                       // LED zu Beginn grün setzen, um sie zu testen
  FastLED.setBrightness(64);
  FastLED.show();
  // wifiManager.resetSettings();              // ggf. zum Löschen bisher im ESP8266 gespeicherter AccessPoint-Zugangsdaten
  wifiManager.autoConnect("SmarteLampe");      // hier kann der Name des Hotspots deiner Lampe angepasst werden

  getCurrentWeatherConditions();               // nach dem (Neu-)Start erstmalig das aktuelle Wetter laden
  updateDisplay();
}


void loop() {
  // Die folgende Verzweigung sorgt dafuer, dass nur alle 5 Minuten (also 300.000 ms) das aktuelle Wetter abgefragt wird; dies spart Web-Traffic 
  // Openweathermap erlaubt pro freiem Account max. 60 Abfragen pro Minute (1 Mio/Monat) - Stand Juli 2020.
  if (millis() - lastcheck >= 300000) {            // millis() gibt die Zeit aus, die seit dem Start des Mikrocontrollers verstrichen ist
    getCurrentWeatherConditions();
    lastcheck = millis();                           // lastcheck auf aktuelle Systemzeit setzen
    updateDisplay();
  }

  // unten werden nun je nach Wetterbedingung (die in der Variablen "weatherID_shortened" steckt) eine Funktion aufgerufen, die die Lampe unterschiedlich leuchten laesst
  if (weatherID == 800) LED_effect_clearSky();     // nur wenn die "weatherID" 800 ist, ist es klar/heiter/sonnig...
  else {
    switch (weatherID_shortened) {                 // sonst ist es je nach Hunderterbereich unterschiedlich: Werte hierfuer entstammen aus https://openweathermap.org/weather-conditions
      case 2: LED_effect_thunder(); break;         // "Gewitter"
      case 3: LED_effect_drizzle(); break;         // "Nieselregen"
      case 5: LED_effect_rain(); break;            // "Regen"
      case 6: LED_effect_snow(); break;            // "Schnee"
      case 7: LED_effect_fog(); break;             // "Nebel"
      case 8: LED_effect_cloudy(); break;          // "Wolken"
    }
  }
  // und die loop-Schleife beginnt nun wieder von vorne ... :)
}



// ========================================================================================================================================================================
/* es folgen Funktionen, die von der Lampe benoetigt werden*/


void updateDisplay() {                                                                    // Funktion zum Aktualisieren des Inhalts auf dem Display
  display.clear();
  
  // obere Zeile
  display.drawString(32,16,weatherforecast_shortened);

  Serial.println(weatherforecast_shortened);
  Serial.println(weatherforecast_shortened.length());

  // untere Zeile
  if (weatherforecast_shortened.length() != 0) {                          // nur, wenn weatherforecast_shortened nicht leer ist (dann naemlich keine Server-Antwort)

    Serial.println(temperature_Celsius_Int);
    Serial.println(String(temperature_Celsius_Int,DEC));
    int digitsTemperature = String(temperature_Celsius_Int,DEC).length();  // wie lang (wie viele Ziffern) ist die Anzeige der Temperatur?
    display.drawString(77 - 12 * digitsTemperature, 50,String(temperature_Celsius_Int,DEC));
    

    // Grad Celsius: C
    display.drawString(86,50,"C");

    // Grad Celsius: Kreis
    display.drawCircle(80, 52, 2);
  } else {
    display.drawString(32,16,"Keine");
    display.drawString(32,28,"Serverantwort!");
  }
  display.display();
}


void getCurrentWeatherConditions() {                                                      // Funktion zum Abrufen der Wetterdaten von der Openweathermap-API
  
  String address = "/data/2.5/weather?id=2866647&appid=" + api_key;  // id hier für Neheim, DE. Syntax siehe https://openweathermap.org/current
  char address2[100];
  address.toCharArray(address2, 100);
  Serial.println(address2);
  String response = "";
  int statusCode = client.get(address2, &response);                                       // Wetter von heute über die openweathermap-API
  Serial.print("Status code from server: "); Serial.println(statusCode);
  Serial.print("Response body from server: "); Serial.println(response);

  //an dieser Stelle wird die Antwort vom Server zurechtgeschnitten (geparsed); weitere Hinweise hierzu unter arduinojson.org/assistant
  StaticJsonDocument<1000> doc;
  char json[1000]; // Antwort in char umwandeln; Groesse ueber den arduinojson.org/assistant berechnen
  response.toCharArray(json, 1000);
  DeserializationError error = deserializeJson(doc, json);
  JsonObject root = doc.as<JsonObject>();
  JsonObject weather = root["weather"][0];
  JsonObject weatherdaten = root["main"];

  weatherID = weather["id"];
  temperature_Kelvin = weatherdaten["temp"];
  Serial.println(temperature_Kelvin);
  weatherforecast_shortened = "";
  temperature_Celsius = temperature_Kelvin - 273;                                // Hinweis: Celsiuswert + 273 = Kelvinwert

  temperature_Celsius_Int = (int)temperature_Celsius;
  
  weatherID_shortened = weatherID / 100;
  switch (weatherID_shortened) {                                                 // Werte hierfuer stammen aus https://openweathermap.org/weather-conditions
    case 2: weatherforecast_shortened = "Gewitter"; break;
    case 3: weatherforecast_shortened = "Nieselreg."; break;
    case 5: weatherforecast_shortened = "Regen"; break;
    case 6: weatherforecast_shortened = "Schnee"; break;
    case 7: weatherforecast_shortened = "Nebel"; break;
    case 8: weatherforecast_shortened = "Wolken"; break;
    default: weatherforecast_shortened = ""; break;                              // wenn kein anderer Wert passt (z.B. weil Server nicht antwortet), ist die weatherlage ungewiss
  } if (weatherID == 800) weatherforecast_shortened = "klar";                    // nur fuer den Fall, dass die weather-ID genau 800 ist, ist es "klar"
}


// ========================================================================================================================================================================
// folgende Methode erleichtert das Modellieren von Farbverlaeufen von einem RGB-Wert

void fade(int led_position, uint16_t duration, uint16_t delay_val, uint16_t startR, uint16_t startG, uint16_t startB, uint16_t endR, uint16_t endG, uint16_t endB) {
    int16_t redDiff = endR - startR;
    int16_t greenDiff = endG - startG;
    int16_t blueDiff = endB - startB;
    int16_t steps = duration*1000 / delay_val;
    int16_t redValue, greenValue, blueValue;
    for (int16_t i = 0 ; i < steps - 1 ; ++i) {
        redValue = (int16_t)startR + (redDiff * i / steps);
        greenValue = (int16_t)startG + (greenDiff * i / steps);
        blueValue = (int16_t)startB + (blueDiff * i / steps);
        leds[0]=CRGB(redValue, greenValue, blueValue);
        FastLED.show();
        delay(delay_val);
    }
    leds[led_position]=CRGB(endR, endG, endB);
 }


/*
                            
###### #    # #####  ###### 
#      #    # #    # #      
#####  #    # #    # #####  
#      #    # #####  #      
#      #    # #   #  #      
######  ####  #    # ###### 
                            
#######                                          
#       ###### ###### ###### #    # ##### ###### 
#       #      #      #      #   #    #   #      
#####   #####  #####  #####  ####     #   #####  
#       #      #      #      #  #     #   #      
#       #      #      #      #   #    #   #      
####### #      #      ###### #    #   #   ###### 
                                                 
                                                          
###### # #    # ###### #    # ######  ####  ###### #    # 
#      # ##   # #      #    # #      #    # #      ##   # 
#####  # # #  # #####  #    # #####  #      #####  # #  # 
#      # #  # # #      #    # #      #  ### #      #  # # 
#      # #   ## #      #    # #      #    # #      #   ## 
###### # #    # #       ####  ######  ####  ###### #    # 



          ########
          ########
          ########
          ########
          ########
          ########
          ########
          ########
          ########
          ########
     ##################
       ##############
         ##########
           ######
             ## 
*/

void LED_effect_clearSky() { // Effekt, der angezeigt wird, wenn der Himmel klar ist
  FastLED.setBrightness(255);
  leds[0] = CRGB::Yellow;
  FastLED.show();
  delay(500);
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(500);
}


void LED_effect_thunder() {

}

void LED_effect_drizzle() {

}

void LED_effect_rain() {

}

void LED_effect_snow() {

}

void LED_effect_fog() {

}

void LED_effect_cloudy() {

}

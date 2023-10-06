#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


// Select a Timer Clock
#define USING_TIM_DIV1                true           // for shortest and most accurate timer
#define USING_TIM_DIV16               false           // for medium time and medium accurate timer
#define USING_TIM_DIV256              false            // for longest timer but least accurate. Default

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"

// Init ESP8266 timer 1
ESP8266Timer ITimer;
// Init ESP8266_ISR_Timer
ESP8266_ISR_Timer ISR_Timer;

// the pin on which the zero signal is received
#define PIN_ZERO 5 // D1 - GPIO5
// the pin on which the SCR signal is emitted
#define PIN_SCR 4 // D2 - GPIO4


long start = 0l;
long pulseDelay = 0l;
bool pulsing = false;

//debug
long prevNanosPulseStart = 0l;
long nanosPulseInterval = 0l;

void IRAM_ATTR TimerHandler()
{
  if (pulseDelay > 0l) {
    long n = micros();
    // a pulse has been asked
    if (n > start + pulseDelay) {
       // make a pulse with a duration of one interrupt
       if (!pulsing) {
        nanosPulseInterval = n - prevNanosPulseStart;
        prevNanosPulseStart = n;
        digitalWrite(PIN_SCR, HIGH);
        pulsing = true;
       } else {
        digitalWrite(PIN_SCR, LOW);
        //reset pulsing
        pulsing = false;
        pulseDelay = 0l;
       }
    }
  }
}

#ifndef STASSID
#define STASSID "xxxxx"
#define STAPSK "xxxxx"
#endif


const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);


int power = 96;

////// POWER MANAGEMENT //////

// ISR called on zero crossing
void IRAM_ATTR onZero() {
        // generate a pulse after this zero - delay is in microSeconds
        // power=100%: no wait, power=0%: wait 10ms
        start = micros();
        pulseDelay = power==100? 1 : (100-power)*100;

}

///// SERVER //////

void handleRoot() {
  server.send(200, "text/plain", "{\"power\": " + String(power) + "}");
}

void handleSet() {
  for (uint8_t i = 0; i < server.args(); i++) { 
    String name = server.argName(i);
    if (name == "power") {
      int tmp = server.arg(i).toInt();
      if (tmp > 96) { // more than 96 leads to less consumption - let limit it to 96
        power = 96;
      } else if (tmp < 0 ) {
        power = 0;
      } else {
        power = tmp;
      }
    }
  }
  handleRoot();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) { message += " " + server.argName(i) + ": " + server.arg(i) + "\n"; }
  server.send(404, "text/plain", message);
}

void setup(void) {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 1);
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  Serial.println(String(LED_BUILTIN));
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, 1);
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, 0);
    delay(500);
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) { Serial.println("MDNS responder started"); }

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  // setup the interrupt on the zero crossing signal
  pinMode(PIN_ZERO, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ZERO), onZero, CHANGE);
  // setup the SCR control pin
  pinMode(PIN_SCR, OUTPUT);
  digitalWrite(PIN_SCR, LOW);

  if (ITimer.attachInterruptInterval(25, TimerHandler))
  {
    Serial.println("Timer Running");
  }
  else
    Serial.println("Can't set ITimer. Select another freq. or timer");

  Serial.println("SRC MANGEMENT STARTED");

}

void loop(void) {
  server.handleClient();
  MDNS.update();
}

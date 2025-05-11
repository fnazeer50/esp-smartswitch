#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>  // WiFiManager library

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

bool ledState = false;

unsigned long buttonPressStart = 0;
bool resetInProgress = false;

// Function declarations
void handleRoot();
void turnLedOn();
void turnLedOff();
void checkWifiResetButton();

void setup() {
  Serial.begin(115200);
  
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW); // LED off (active LOW)

  pinMode(14, INPUT); // For reset trigger

  WiFiManager wm;

  // Reset WiFi if pin 14 is HIGH
  unsigned long startTime = millis();
  while (millis() - startTime < 8000) {
    checkWifiResetButton();
    delay(100);  // Poll every 100ms
  }
  

  // Start configuration portal if no known WiFi is found
  if (!wm.autoConnect("ESP32-LED-Control", "12345678")) {
    Serial.println("Failed to connect. Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Connected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // DNS redirect to self (captive portal trick)
  dnsServer.start(DNS_PORT, "*", WiFi.localIP());

  // Set up web routes and captive portal paths
  server.on("/", handleRoot);
  server.on("/led/on", HTTP_GET, turnLedOn);
  server.on("/led/off", HTTP_GET, turnLedOff);
  server.on("/generate_204", handleRoot); // Android
  server.on("/fwlink", handleRoot);       // Windows
  server.on("/hotspot-detect.html", handleRoot); // Apple
  server.onNotFound(handleRoot); // Catch all unknown paths
  server.on("/reset", []() {
    server.send(200, "text/html", "<h3>WiFi credentials erased.<br>Rebooting...</h3>");
    delay(1000);                       // Let message reach the client
    WiFi.disconnect(true, true);      // Erase and disconnect fully
    delay(1000);                      // Allow time to settle
    ESP.restart();                    // Restart to trigger AP mode
  });  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  checkWifiResetButton(); // One-time call — works only if pin is HIGH at that moment

}

void handleRoot() {
  String html = "<html><head><style>";
  html += "button { padding: 10px 20px; font-size: 16px; margin: 10px; border: none; }";
  html += ".green { background-color: green; color: white; }";
  html += ".red { background-color: red; color: white; }";
  html += ".white { background-color: white; color: black; border: 1px solid #ccc; }";
  html += "</style></head><body>";
  html += "<h1>ESP32 LED Control</h1>";
  html += "<p><a href=\"/reset\"><button class=\"blue\">Reset WiFi Settings</button></a></p>";

  if (ledState) {
    html += "<p><a href=\"/led/on\"><button class=\"green\">Turn LED On</button></a></p>";
    html += "<p><a href=\"/led/off\"><button class=\"white\">Turn LED Off</button></a></p>";
  } else {
    html += "<p><a href=\"/led/on\"><button class=\"white\">Turn LED On</button></a></p>";
    html += "<p><a href=\"/led/off\"><button class=\"red\">Turn LED Off</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void turnLedOn() {
  digitalWrite(2, HIGH); // ON (active LOW)
  ledState = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

void turnLedOff() {
  digitalWrite(2, LOW); // OFF
  ledState = false;
  server.sendHeader("Location", "/");
  server.send(303);
}
void checkWifiResetButton() {
  static unsigned long buttonPressStart = 0;
  static bool resetInProgress = false;

  if (digitalRead(14) == HIGH) {
    if (!resetInProgress) {
      buttonPressStart = millis();
      resetInProgress = true;
    } else if (millis() - buttonPressStart >= 8000) {
      Serial.println("GPIO 14 held HIGH for 8s. Resetting WiFi...");
      WiFi.disconnect(true, true);  // Fully clear and disconnect
      delay(500);
      ESP.restart();
    }
  } else {
    resetInProgress = false;
  }
}


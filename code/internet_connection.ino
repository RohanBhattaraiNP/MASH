#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Caltech Visitor";
const char* password = NULL;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Check internet connectivity
  Serial.println("\n--- Testing Internet Connection ---");
  checkInternet();
}

void checkInternet() {
  HTTPClient http;
  
  Serial.println("Trying to reach google.com...");
  http.begin("http://www.google.com");
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    Serial.println("SUCCESS: Internet is working!");
    Serial.print("HTTP Response Code: ");
    Serial.println(httpCode);
    Serial.println("You have full internet access.");
  } else {
    Serial.println("FAILED: No internet access!");
    Serial.print("Error Code: ");
    Serial.println(httpCode);
    Serial.println("WiFi connected but internet blocked.");
  }
  
  http.end();
}

void loop() {
  delay(10000);
  Serial.println("\n--- Checking again ---");
  checkInternet();
}

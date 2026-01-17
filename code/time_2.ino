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
  
  Serial.println("\n--- Getting Time from HTTP API ---");
  getTimeFromHTTP();
}

void getTimeFromHTTP() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Using worldtimeapi.org - returns simple text format
    Serial.println("Fetching time from worldtimeapi.org...");
    http.begin("http://worldtimeapi.org/api/timezone/America/Los_Angeles");
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String response = http.getString();
      Serial.println("\n✓ SUCCESS! Time received:");
      Serial.println("=====================================");
      
      // Extract datetime
      int datetimeIndex = response.indexOf("\"datetime\":\"");
      String datetime = "";
      if (datetimeIndex > 0) {
        int start = datetimeIndex + 12;
        int end = response.indexOf("\"", start);
        datetime = response.substring(start, end);
      }
      
      // Extract unix time
      long unixtime = 0;
      int unixtimeIndex = response.indexOf("\"unixtime\":");
      if (unixtimeIndex > 0) {
        int start = unixtimeIndex + 11;
        int end = response.indexOf(",", start);
        String unixtimeStr = response.substring(start, end);
        unixtime = unixtimeStr.toInt();
      }
      
      // Parse and print readable time
      // Format: 2026-01-16T14:23:45
      if (datetime.length() > 0) {
        String date = datetime.substring(0, 10);  // 2026-01-16
        String time = datetime.substring(11, 19); // 14:23:45
        
        Serial.print("Date: ");
        Serial.println(date);
        Serial.print("Time: ");
        Serial.println(time);
        Serial.print("Unix Timestamp: ");
        Serial.println(unixtime);
      }
      
      Serial.println("=====================================");
      
    } else {
      Serial.println("✗ FAILED!");
      Serial.print("HTTP Error Code: ");
      Serial.println(httpCode);
    }
    
    http.end();
    
  } else {
    Serial.println("WiFi disconnected!");
  }
}

void loop() {
  delay(5000);
  Serial.println("\n--- Refreshing ---");
  getTimeFromHTTP();
}

//1/23/26 led working 
//1/21/26 code after router works 

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "time.h"

const char* influxRootCACert = \
"-----BEGIN CERTIFICATE-----\n" \
"... insert InfluxDB PEM certification here ...\n" \
"-----END CERTIFICATE-----\n";

typedef struct {
  float distanceBatch[30];
  bool motionBatch[30];
  int buzzerBatch[30];
  int ledBatch[30];
  unsigned long long timestampBatch[30];
  int batchSize;
  bool sent;
} SharedData;

const int BATCH_SIZE = 30;
SharedData sharedData;
SemaphoreHandle_t lock; 

#define TRIG_PIN 13
#define ECHO_PIN 12
#define BUZZER_PIN 27
#define MOTION_PIN 14
#define LED_PIN 26

const char* ssid = "Caltech Visitor";
const char* password = NULL;
const char* host = "us-east-1-1.aws.cloud2.influxdata.com";
const int httpsPort = 443;
const char* org = "GreenLabs";
const char* bucket = "prototype25";
const char* token = "7EItUdlN9AjZr1aq7V_-XSWef9gybgy9kaFJbhYawogxBDncvHVA6z1ODcuNAs7hpH80zRpCB6LdgbiFewe-DQ==";
const int minDistance = 4;
const int maxDistance = 67;

bool motionDetected = false; 
bool wasMotionDetectedBefore = false; 
unsigned long lastMotionTime = 0; 
const unsigned long delayTime = 3000;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -28800;
const int daylightOffset_sec = 3600;
WiFiClientSecure secureClient;
HTTPClient https;

unsigned long long getInfluxTimestamp() { 
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
    }
  time_t epochTime = mktime(&timeinfo);
  unsigned long long nanoTime = (unsigned long long)epochTime * 1000000000ULL;
  return nanoTime; 
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%B %d %Y %H:%M:%S");
}

void TaskSensorLogic(void *parameter) {
  float distanceBatch[BATCH_SIZE];
  bool motionBatch[BATCH_SIZE];
  int buzzerBatch[BATCH_SIZE];
  int ledBatch[BATCH_SIZE];
  unsigned long long timestampBatch[BATCH_SIZE];
  int currentIndex = 0;

  while (true) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
      long duration = pulseIn(ECHO_PIN, HIGH);
      float measuredDistance = duration * 0.034 / 2.0;
      motionDetected = digitalRead(MOTION_PIN);
      int buzzerInt = 0;
      int ledInt= 1;
    
      unsigned long long timestamp = getInfluxTimestamp();

      if (measuredDistance <= minDistance) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerInt = 0;
      }
      else if (measuredDistance > minDistance && measuredDistance && motionDetected) {
        lastMotionTime = millis();
        wasMotionDetectedBefore = true; 
        digitalWrite(BUZZER_PIN, LOW);
        buzzerInt = 0;
      }
      else if (measuredDistance > minDistance && measuredDistance < maxDistance && !motionDetected && (millis() - lastMotionTime) > delayTime) {
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerInt = 1;
        wasMotionDetectedBefore = false;
      }  

    distanceBatch[currentIndex] = measuredDistance;
    motionBatch[currentIndex] = motionDetected;
    buzzerBatch[currentIndex] = buzzerInt; 
    ledBatch[currentIndex] = ledInt;
    timestampBatch[currentIndex] = timestamp;
    currentIndex++;

    Serial.print("Distance: ");
    Serial.print(measuredDistance);
    Serial.print("cm | Motion: ");
    Serial.print(motionDetected ? "YES" : "NO");
    Serial.print(" | Buzzer: ");
    Serial.println(buzzerInt ? "ON" : "OFF");
    Serial.print(" | LED: ");
    Serial.println(ledInt ? "ON" : "OFF");
    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    if (currentIndex >= BATCH_SIZE) {
      if (xSemaphoreTake(lock, portMAX_DELAY)) {
        memcpy(sharedData.distanceBatch, distanceBatch, sizeof(distanceBatch));
        memcpy(sharedData.motionBatch, motionBatch, sizeof(motionBatch));
        memcpy(sharedData.buzzerBatch, buzzerBatch, sizeof(buzzerBatch));
        memcpy(sharedData.ledBatch, ledBatch, sizeof(ledBatch));
        memcpy(sharedData.timestampBatch, timestampBatch, sizeof(timestampBatch));
        sharedData.batchSize = BATCH_SIZE;
        sharedData.sent = false; 
        xSemaphoreGive(lock);
        currentIndex = 0;
      }
      }
      delay(1000);
}
}

void TaskUploadLogic(void *parameter) { 
  while (true) {
    delay(1000);
    if (xSemaphoreTake(lock, portMAX_DELAY)) {
      if (!sharedData.sent && sharedData.batchSize > 0) {
        float localDistance[BATCH_SIZE];
        bool localMotion[BATCH_SIZE];
        int localBuzzer[BATCH_SIZE];
        int localLed[BATCH_SIZE];
        unsigned long long localTimeStamp[BATCH_SIZE];
        int localSize = sharedData.batchSize;
        memcpy(localDistance, sharedData.distanceBatch, sizeof(localDistance));
        memcpy(localMotion, sharedData.motionBatch, sizeof(localMotion));
        memcpy(localBuzzer, sharedData.buzzerBatch, sizeof(localBuzzer));
        memcpy(localTimeStamp, sharedData.timestampBatch, sizeof(localTimeStamp));
        memcpy(localLed, sharedData.timestampBatch, sizeof(localLed));
        sharedData.sent = true; 
        sharedData.batchSize = 0;
        xSemaphoreGive(lock);

        for (int i = 0; i < localSize; i++) {
          sendDataToInfluxDB(localDistance[i], localMotion[i], localBuzzer[i], localTimeStamp[i], localLed[i]);
        }
      } else {
          xSemaphoreGive(lock);
      }
    }
  }
}

void sendDataToInfluxDB(float distance, bool motion, int buzzer, unsigned long long timestamp, int led) {
  int ledInt = 1;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFI disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
    int attemps = 0; 
    while (WiFi.status() != WL_CONNECTED && attemps < 10) {
      delay(500);
      Serial.print(".");
      attemps++;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to reconnect!");
      return; 
    }
    Serial.println("Reconnected!");
  }

  String data = "sensors,device=patentv61,location=kornfieldlab distance=" +
    String(distance) + ",motion=" +
    String(motion ? "true" : "false") +
    ",buzzer=" + String(buzzer) + " " + 
    String(timestamp);

  String url = "https://"+ String(host) + "/api/v2/write?org=" + 
    String(org) + "&bucket=" + String(bucket) + "&precision=ns";
    
  secureClient.setInsecure();  // Add this line
  
  if (https.begin(secureClient, url)) { 
    https.addHeader("Authorization", "Token " + String(token));
    https.addHeader("Content-Type", "text/plain; charset=utf-8");
    https.addHeader("Accept", "application/json");
    https.setTimeout(10000);  // Add timeout

    int httpCode = https.POST(data);
    if (httpCode == HTTP_CODE_NO_CONTENT || httpCode == 204) { 
      Serial.println("Data sent successfully");
      digitalWrite(LED_PIN, HIGH);
      ledInt = 1;

    } else {
      Serial.print("Error sending data. Code: "); 
      Serial.println(httpCode); 
    
    
      Serial.print("Response: ");
      Serial.println(https.getString());
    }
    https.end(); 
  } else {
    Serial.println("Failed to connect to InfluxDB"); 
    digitalWrite(LED_PIN, LOW);
    ledInt = 0;
  }      
}

void setup() { 
  Serial.begin(115200); 
  delay(1000); 
  Serial.println("Setup starting...");
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  secureClient.setInsecure();
  WiFi.begin(ssid, password); 
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); }
    Serial.println("\nWi-Fi Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
      Serial.println("Error creaing mutex");
      while(1);
    }

  sharedData.batchSize = 0;  
  sharedData.sent = true;
  xTaskCreatePinnedToCore(    TaskSensorLogic,   
  "SensorTask",      
  10000,             
  NULL,              
  1,                 
  NULL,              
  0                  
  );

  xTaskCreatePinnedToCore(    
  TaskUploadLogic,    
  "UploadTask",       
  15000,             
  NULL,               
  1,                  
  NULL,               
  1                   
  );
  Serial.println("System ready");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  
  Serial.println("Waiting for NTP time sync...");  
  delay(2000);  
  printLocalTime();
}

void loop() {
  delay(1000);
}

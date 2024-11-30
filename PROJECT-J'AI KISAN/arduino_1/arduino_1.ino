#include "esp_camera.h"
#include "SD_MMC.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// ESP32-CAM Configuration
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      5
#define Y2_GPIO_NUM      4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22
#define SD_CS_GPIO_NUM   5  // CS pin for SD card module

// Arduino Configuration
#define TEMP_SENSOR_PIN 2
#define PH_SENSOR_PIN 36
#define LDR_SENSOR_PIN 39
#define HEATER_PIN 8
#define FAN_PIN 9
#define MOTOR1_PIN 10
#define MOTOR2_PIN 11
#define FEEDER_PIN 12

// OneWire and DallasTemperature for temperature sensor
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);

unsigned long lastFeedTime = 0;

// Camera configuration
camera_config_t config = {
  .pin_pwdn = PWDN_GPIO_NUM,
  .pin_reset = RESET_GPIO_NUM,
  .pin_xclk = XCLK_GPIO_NUM,
  .pin_sccb_sda = SIOD_GPIO_NUM,
  .pin_sccb_scl = SIOC_GPIO_NUM,
  .pin_d7 = Y9_GPIO_NUM,
  .pin_d6 = Y8_GPIO_NUM,
  .pin_d5 = Y7_GPIO_NUM,
  .pin_d4 = Y6_GPIO_NUM,
  .pin_d3 = Y5_GPIO_NUM,
  .pin_d2 = Y4_GPIO_NUM,
  .pin_d1 = Y3_GPIO_NUM,
  .pin_d0 = Y2_GPIO_NUM,
  .pin_vsync = VSYNC_GPIO_NUM,
  .pin_href = HREF_GPIO_NUM,
  .pin_pclk = PCLK_GPIO_NUM,
  .xclk_freq_hz = 20000000,
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_JPEG, // YUV422, GRAYSCALE, RGB565, JPEG
  .frame_size = FRAMESIZE_SVGA,   // QQVGA-UXGA Do not use sizes above QVGA when not JPEG
  .jpeg_quality = 10,             // 0-63 lower number means higher quality
  .fb_count = 1                   // if more than one, i2s runs in continuous mode. Use only with JPEG
};

void setup() {
  Serial.begin(115200);

  // Initialize SD card
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  // Initialize camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }

  // Initialize sensors and pins
  sensors.begin();
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(MOTOR1_PIN, OUTPUT);
  pinMode(MOTOR2_PIN, OUTPUT);
  pinMode(FEEDER_PIN, OUTPUT);
  
  digitalWrite(HEATER_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(MOTOR1_PIN, LOW);
  digitalWrite(MOTOR2_PIN, LOW);
  digitalWrite(FEEDER_PIN, LOW);

  Serial.println("Camera and SD card initialized");
}

void loop() {
  // Capture and save an image every 10 seconds
  delay(10000);
  captureAndSaveImage();
  
  // Read sensors and control actuators
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  int phValue = analogRead(PH_SENSOR_PIN);
  int lightIntensity = analogRead(LDR_SENSOR_PIN);

  controlHeaterFan(temperature);
  controlWaterTransfer(phValue);
  controlLight(lightIntensity);
  controlFeeder();
  
  sendToPythonScript(temperature, phValue, lightIntensity);
}

void captureAndSaveImage() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  String path = "/sdcard/image_" + String(millis()) + ".jpg";
  fs::FS &fs = SD_MMC;

  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
  } else {
    file.write(fb->buf, fb->len);
    Serial.printf("File saved: %s\n", path.c_str());
  }
  file.close();
  esp_camera_fb_return(fb);
}

void controlHeaterFan(float temperature) {
  if (temperature < 25.0) {
    digitalWrite(HEATER_PIN, HIGH);
    digitalWrite(FAN_PIN, LOW);
  } else {
    digitalWrite(HEATER_PIN, LOW);
    digitalWrite(FAN_PIN, HIGH);
  }
}

void controlWaterTransfer(int ph) {
  if (ph < 6 || ph > 8) {
    digitalWrite(MOTOR1_PIN, HIGH);
    delay(5000);
    digitalWrite(MOTOR1_PIN, LOW);
    digitalWrite(MOTOR2_PIN, HIGH);
    delay(5000);
    digitalWrite(MOTOR2_PIN, LOW);
  }
}

void controlLight(int lightIntensity) {
 if (lightIntensity < 200) { 
  digitalWrite(LIGHT_PIN, HIGH); 
 // Turn lights on 
 } 
 else if (lightIntensity > 800) 
 { digitalWrite(LIGHT_PIN, LOW); 
 // Turn lights off 
 }

}

void controlFeeder() {
  unsigned long currentTime = millis();
  if (currentTime - lastFeedTime > 8 * 3600 * 1000) {
    digitalWrite(FEEDER_PIN, HIGH);
    delay(5000);
    digitalWrite(FEEDER_PIN, LOW);
    lastFeedTime = currentTime;
    logFeedTime();
  }
}

void sendToPythonScript(float temp, int ph, int light) {
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.print(", pH: ");
  Serial.print(ph);
  Serial.print(", Light Intensity: ");
  Serial.println(light);
}

void logFeedTime() {
  Serial.print("Feed time: ");
  Serial.println(millis());
}

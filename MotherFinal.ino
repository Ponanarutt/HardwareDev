#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define RED_PIN 42
#define YELLOW_PIN 41
#define GREEN_PIN 40
#define ANALOG_PIN 4
#define SW_PIN 2
int count = 0;
int stade = 0;  // Game state: 0 = waiting, 1 = started, 2 = ended
unsigned long lastButtonPress = 0;
const int debounceDelay = 200;
unsigned long lastDrawTime = 0;
const int drawInterval = 3500;  // Draw a number every 3.5 seconds
unsigned long winStartTime = 0;
const unsigned long claimTimeout = 5000; // 5 seconds to claim
bool drawnNumbers[76] = { false };  // Track drawn bingo numbers (1-75)
int numbersDrawn = 0;
int drawHistory[75];  // Store history of drawn numbers
unsigned long endTime = 0;

WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to WiFi %s\n", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected.");
}

void connect_mqtt() {
    Serial.printf("Connecting to MQTT broker at %s\n", MQTT_BROKER);
    while (!mqtt.connected()) {
        if (mqtt.connect("", MQTT_USER, MQTT_PASS)) {
            Serial.println("MQTT broker connected.");

            // ✅ Ensure re-subscription to both topics
            mqtt.subscribe("b6510503735/bingo/switch"); 
            mqtt.subscribe("b6510503735/bingo/response");  
        } else {
            Serial.println("Failed to connect to MQTT broker. Retrying...");
            delay(1000);
        }
    }
}


void publishGameState() {
  String payload = String(stade);
  mqtt.publish("bingo/game_state", payload.c_str());
  Serial.printf("Published Game State: %d\n", stade);
}

void displayMessage(const char* message) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 25);
  display.print(message);
  display.display();
}

void resetGame() {
  Serial.println("Resetting game...");
  stade = 0;
  numbersDrawn = 0;
  memset(drawnNumbers, false, sizeof(drawnNumbers));
  memset(drawHistory, 0, sizeof(drawHistory));  // Clear history
  display.clearDisplay();
  publishGameState();
  displayMessage("Waiting for Player...");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(48, 47);

  mqtt.setCallback(callback);
  mqtt.subscribe("b6510503735/bingo/switch");  // ให้ ESP32 ฟังคำสั่งจาก Node-RED


  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.display();

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(SW_PIN, INPUT_PULLUP);

  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);

  randomSeed(analogRead(ANALOG_PIN));

  connect_wifi();
  connect_mqtt();
  publishGameState();

  displayMessage("Waiting  for Player...");  // ✅ Ensure OLED shows this at startup
}

void startCountdown() {
  displayMessage("3");
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(GREEN_PIN, LOW);  // ✅ Ensure GREEN is OFF before countdown
  delay(1000);

  displayMessage("2");
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, HIGH);
  delay(1000);

  displayMessage("1");
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, HIGH);
  delay(1000);

  displayMessage("GO!");
  delay(500);

  digitalWrite(GREEN_PIN, LOW);  // ✅ Ensure GREEN LED turns OFF after countdown
}

void waitingAnimation() {
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  delay(500);

  digitalWrite(RED_PIN, HIGH);
  digitalWrite(YELLOW_PIN, HIGH);
  digitalWrite(GREEN_PIN, LOW);
  delay(500);

  digitalWrite(RED_PIN, HIGH);
  digitalWrite(YELLOW_PIN, HIGH);
  digitalWrite(GREEN_PIN, HIGH);
  delay(500);

  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  delay(500);
}

void endCountdown() {
  displayMessage("END");
  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_PIN, HIGH);
    digitalWrite(YELLOW_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    delay(500);

    digitalWrite(RED_PIN, LOW);
    digitalWrite(YELLOW_PIN, HIGH);
    digitalWrite(GREEN_PIN, LOW);
    delay(500);

    digitalWrite(RED_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
    delay(500);
  }

  // ✅ Ensure all LEDs turn OFF after end animation
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
}


void publishBingoNumber(int number) {
  String payload = String(number);
  mqtt.publish("b6510503735/bingo/number", payload.c_str());  // ✅ Deploy `number`
  Serial.printf("Published Bingo Number: %d\n", number);
}

// Call this when the game state changes
void updateGameState(int newStade) {
  stade = newStade;
  publishGameState(stade);
}

// Call this when a new bingo number is drawn
void handleNewBingoNumber(int drawnNumber) {
  if (drawnNumber != -1) {
    publishBingoNumber(drawnNumber);
  }
}

void publishGameState(int stade) {
  String payload = String(stade);
  mqtt.publish("b6510503735/bingo/game_state", payload.c_str());  // ✅ Deploy `stade`
  Serial.printf("Published Game State: %d\n", stade);
}
int drawBingoNumber() {
  if (stade != 1 || numbersDrawn >= 75) return -1;

  int num;
  int attempts = 0;
  do {
    num = random(1, 76);
    attempts++;
    if (attempts > 100) return -1;
  } while (drawnNumbers[num]);

  drawnNumbers[num] = true;
  drawHistory[numbersDrawn++] = num;

  Serial.printf("Drawn Number: %d\n", num);

  // ✅ FIX: Publish correctly using the right function
  publishBingoNumber(num);


          return num;
}


void updateOLED(int number) {
  if (stade == 1) {
    digitalWrite(RED_PIN, HIGH);
    delay(1000);
    digitalWrite(RED_PIN, LOW);

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.print("Number:");
    display.setCursor(40, 40);
    display.print(number);
    display.display();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0';  // Convert payload to a string
    String message = String((char*)payload);

    Serial.printf("Received MQTT Message: %s\n", message.c_str());

    if (String(topic) == "b6510503735/bingo/response") {
        if (message == "99") {
            Serial.println("Winner detected! Ending game.");
            stade = 2;  // ✅ Change state to End Game

            // ✅ Publish state once to avoid recursive loop
            mqtt.publish("b6510503735/bingo/game_state", "2");  

            // ✅ Show "Winner!" on OLED
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(20, 20);
            display.print("WINNER!");
            display.display();

            endCountdown();
            endTime = millis();  // ✅ Start reset countdown
        }
    }
}


void loop() {
    mqtt.loop();

    if (!mqtt.connected()) {
        connect_mqtt();  // ✅ Reconnect and re-subscribe
    }

    bool buttonState = digitalRead(SW_PIN);

    if (buttonState == LOW && millis() - lastButtonPress > debounceDelay) {
        stade = (stade + 1) % 3;
        mqtt.publish("b6510503735/bingo/game_state", String(stade).c_str());

        Serial.printf("Game State changed to: %d\n", stade);

        if (stade == 1) {
            startCountdown();
        } else if (stade == 2) {
            endCountdown();
            endTime = millis();  // ✅ Start timer for 5-second delay
        } else if (stade == 0) {
            resetGame();
        }

        lastButtonPress = millis();
    }

    if (stade == 0) {
        waitingAnimation();
    }

    if (stade == 1 && millis() - lastDrawTime > drawInterval) {
        lastDrawTime = millis();
        int drawnNumber = drawBingoNumber();

        if (drawnNumber != -1) {
            updateOLED(drawnNumber);
        }
    }

    if (stade == 2) {
        // ✅ Ensure a 5-second delay before changing back to State 0
        if (millis() - endTime >= 5000) {
            Serial.println("Transitioning from State 2 to State 0 after 5 seconds...");
            resetGame();
            mqtt.publish("b6510503735/bingo/game_state", "0");  // ✅ Ensure reset is published
        }
    }

    if (stade != 1) {
        digitalWrite(GREEN_PIN, LOW);
    }

    delay(100);
}

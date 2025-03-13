#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// 📌 TFT SPI Pins (ESP32-S3)
#define TFT_SCLK  5  // SCL (Clock)
#define TFT_MOSI   6  // SDA (Data)
#define TFT_RST   7  // RES (Reset)
#define TFT_DC    15 // DC (Data/Command)
#define TFT_CS    16 // CS (Chip Select)
#define TFT_BLK   17 // BLK (Backlight)

#define LCD_ADDRESS 0x27

#define SDA_PIN 20
#define SCL_PIN 21

// 📌 OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_I2C_ADDR 0x3C

// 📌 Button for Claiming Bingo
#define BUTTON_PIN 19

// 📌 Buzzer Pin
#define BUZZER_PIN 4

// 📌 MQTT
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);

// 📌 Create TFT, OLED, LCD Objects
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

// 📌 Game Variables
int winning = 0;  // 0 = Not won, 1 = Won
int gameState = 0; // 0 = Waiting, 1 = Playing, 2 = Game End
bool gameWon = false;
int lastDrawnNumber = -1;
bool drawnNumbers[76] = { false };
bool serialMarked[5][5] = { false };  // Tracking marked numbers
unsigned long lastPressTime = 0;
const int debounceDelay = 300;
unsigned long winStartTime = 0;
const unsigned long claimTimeout = 5000; // 5 seconds to claim

// 📌 Bingo Card
const int SIZE = 5;
int bingoCard[SIZE][SIZE];

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
            mqtt.subscribe("b6510503735/bingo/game_state");
            mqtt.subscribe("b6510503735/bingo/number");
        } else {
            Serial.println("Failed to connect to MQTT broker. Retrying...");
            delay(1000);
        }
    }
}

// 🔊 Buzzer Sound Function
void beepBuzzer(int duration) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
}

// 📌 Generate Unique Bingo Card
void generateBingoCard() {
    randomSeed(analogRead(0));

    // ✅ Fix: Ensure the array is cleared before generating numbers
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            bingoCard[row][col] = 0;  // ✅ Reset all values to prevent old data issues
        }
    }

    for (int col = 0; col < SIZE; col++) {
        int minVal = col * 15 + 1;
        int maxVal = (col + 1) * 15;
        int nums[15];
        for (int i = 0; i < 15; i++) nums[i] = minVal + i;

        for (int row = 0; row < SIZE; row++) {
            if (row == 2 && col == 2) {
                bingoCard[row][col] = -1;  // Free Space in the center
                serialMarked[row][col] = true;
                continue;
            }
            int index = random(0, maxVal - minVal + 1);
            bingoCard[row][col] = nums[index];
            nums[index] = nums[maxVal - minVal];
            maxVal--;
        }
    }
}


// 📌 Last 5 Drawn Numbers Display
int history[5] = { 0 };  // Stores last 5 numbers

void updateLCDHistory() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.setCursor(0, 1);

  // 📌 Display stored numbers with spacing
  for (int i = 0; i < 5; i++) {
    if (history[i] != 0) {
      lcd.print(history[i]);
      lcd.print(" ");
    }
  }
}

int lastGameState = -1;  // ✅ Store the last known game state

// 📌 Function to update the Bingo card when a number is received
void markBingoNumber(int num) {
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            if (bingoCard[row][col] == num) {
                serialMarked[row][col] = true;
                beepBuzzer(200);  // ✅ Play sound when a number is marked
            }
        }
    }

    // ✅ Shift old numbers to keep last 5 history
    for (int i = 4; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0] = num;  // Store latest number

    updateLCDHistory();  // ✅ Correct: Pass the drawn number as an argument
    displayBingoCardTFT();  // ✅ Refresh the Bingo Card on TFT
    printBingoCardSerial();  // ✅ Debug: Print card in Serial Monitor
}

// 📌 Print Bingo Card for Debugging
void printBingoCardSerial() {
    Serial.println("\n======= BINGO CARD =======");
    Serial.println(" B   I   N   G   O");
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            if (bingoCard[row][col] == -1) {
                Serial.print(" ** ");
            } else if (serialMarked[row][col]) {
                Serial.print("[X] ");
            } else {
                Serial.printf("%2d  ", bingoCard[row][col]);
            }
        }
        Serial.println();
    }
    Serial.println("==========================");
}


// 📌 Check for a Bingo win
bool checkWin() {
    for (int i = 0; i < SIZE; i++) {
        if (serialMarked[i][0] && serialMarked[i][1] && serialMarked[i][2] && serialMarked[i][3] && serialMarked[i][4]) return true;
        if (serialMarked[0][i] && serialMarked[1][i] && serialMarked[2][i] && serialMarked[3][i] && serialMarked[4][i]) return true;
    }
    if (serialMarked[0][0] && serialMarked[1][1] && serialMarked[2][2] && serialMarked[3][3] && serialMarked[4][4]) return true;
    if (serialMarked[0][4] && serialMarked[1][3] && serialMarked[2][2] && serialMarked[3][1] && serialMarked[4][0]) return true;

    return false;
}


// 📌 Function to send "99" to MQTT if player wins
void sendBingoWin() {
    mqtt.publish("b6510503735/bingo/response", "99");
    Serial.println("Sent BINGO response: 99");
}

// 📌 MQTT Callback function
void callback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0';
    String message = String((char*)payload);

    Serial.printf("Received MQTT Message [%s]: %s\n", topic, message.c_str());

    if (String(topic) == "b6510503735/bingo/game_state") {
        gameState = message.toInt();
        Serial.printf("Game State updated: %d\n", gameState);

        if (gameState == 0) {
            lcd.clear();
            lcd.print("Waiting...");
            tft.fillScreen(ST7735_BLACK);
            displayState0();
        } else if (gameState == 1) {
            lcd.clear();
            lcd.print("Bingo Started!");
            tft.fillScreen(ST7735_BLACK);
            displayBingoCardTFT();
} else if (gameState == 2) {
    // ✅ Only show "YOU LOSE!" if the player DID NOT win
    if (winning == 0) {
        Serial.println("Forced transition to GAME OVER (2). Showing 'YOU LOSE!'");
        displayLoseScreen();
        delay(3000);  // ✅ Show "YOU LOSE!" before resetting
    }
    
    resetGame();  // ✅ Reset everything after game end
}
    }

    if (String(topic) == "b6510503735/bingo/number") {
        lastDrawnNumber = message.toInt();
        Serial.printf("New Bingo Number: %d\n", lastDrawnNumber);

        markBingoNumber(lastDrawnNumber);
        displayBingoCardTFT();
    }
}

// 📌 Display Bingo Card on TFT// 📌 Display Bingo Card on TFT
void displayBingoCardTFT() {
    tft.fillScreen(ST7735_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(20, 5);
    tft.print(" B   I   N   G   O");

    int cellSize = 24;
    int startX = 5, startY = 15;

    for (int i = 0; i <= SIZE; i++) {
        tft.drawLine(startX, startY + i * cellSize, startX + SIZE * cellSize, startY + i * cellSize, ST7735_WHITE);
        tft.drawLine(startX + i * cellSize, startY, startX + i * cellSize, startY + SIZE * cellSize, ST7735_WHITE);
    }

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            int xPos = startX + col * cellSize + 6; 
            int yPos = startY + row * cellSize + 6;

            if (serialMarked[row][col]) {
                tft.fillRect(startX + col * cellSize + 1, startY + row * cellSize + 1, cellSize - 2, cellSize - 2, ST7735_GREEN);
                tft.setTextColor(ST7735_BLACK);
            } else {
                tft.setTextColor(ST7735_WHITE);
            }

            tft.setCursor(xPos, yPos);
            if (bingoCard[row][col] == -1) {
                tft.print("**");
            } else {
                tft.print(bingoCard[row][col]);
            }
        }
    }
}


void setup() {
    Wire.begin(SDA_PIN, SCL_PIN);
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);

    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.initR(INITR_BLACKTAB);

    pinMode(TFT_BLK, OUTPUT);
    digitalWrite(TFT_BLK, HIGH);

    tft.fillScreen(ST7735_BLACK);
    display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
    lcd.backlight();
    lcd.clear();

    connect_wifi();
    connect_mqtt();
    mqtt.setCallback(callback);

    // ✅ Fix: Ensure Bingo card is generated before first print
    generateBingoCard();  
    printBingoCardSerial();  // ✅ Print the first card AFTER generation
    displayBingoCardTFT();
}

// 📌 Reset Game After Winning
void resetGame() {
    gameState = 0;
    gameWon = false;
    winning = 0; // ✅ Reset winning state for next round

    lastDrawnNumber = -1;

    // ✅ Clear all drawn numbers
    for (int i = 0; i < 76; i++) {
        drawnNumbers[i] = false;
    }

    // ✅ Clear history
    for (int i = 0; i < 5; i++) {
        history[i] = 0;
    }

    // ✅ Reset the marking arrays
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            serialMarked[row][col] = false;
        }
    }

    // ✅ Generate a brand-new Bingo card
    generateBingoCard();

    // ✅ Display STATE 0 after Reset
    displayState0();
}

// 📌 STATE 0: Register / Reset Screen
void displayState0() {
    tft.fillScreen(ST7735_BLACK);
    
    // 🎨 Centered Title
    int titleX = (tft.width() - (6 * 12)) / 2;  // Center "CPE BINGO"
    tft.setCursor(11, 5);
    tft.setTextSize(2);
    tft.setTextColor(ST7735_CYAN);
    tft.print("CPE BINGO");
    // tft.setCursor(12, 15);
    // tft.setTextSize(2);
    // tft.setTextColor(ST7735_CYAN);
    // tft.print("CPE BINGO");
    // tft.setCursor(15, 25);
    // tft.setTextSize(2);
    // tft.setTextColor(ST7735_CYAN);
    // tft.print("CPE BINGO");
    // 🎨 Draw a bordered rectangle for instruction
    tft.drawRect(10, 25, 105, 30, ST7735_WHITE);
    tft.setCursor(15, 35);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_YELLOW);
    tft.print("PLEASE REGISTER!");

    // 🎨 Add Name and ID at the Bottom (Centered)
    int nameX = (tft.width() - (15 * 6)) / 2;  // Center text
    tft.setCursor(2, tft.height() - 10);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.print("Ruttanapon Oupthong");
    
    tft.setCursor(2, tft.height() - 20);
    tft.print("6510503735");

    // ✅ LCD Update
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WELCOME ....");
    lcd.setCursor(0, 1);
    lcd.print("   PLEASE REGISTER!");


}


void displayState2() {
    tft.fillScreen(ST7735_BLACK);
    
    // 🎨 Draw Game Over Text
    tft.setCursor(25, 10);
    tft.setTextSize(2);
    tft.setTextColor(ST7735_RED);
    tft.print("GAME OVER");

    // 🎨 Draw border for message
    tft.drawRect(10, 40, 105, 30, ST7735_WHITE);
    tft.setCursor(15, 50);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_YELLOW);
    tft.print("THANKS FOR PLAYING!");

    // ✅ LCD Update
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GAME OVER!");
    lcd.setCursor(0, 1);
    lcd.print("SEE YOU NEXT TIME!");

    // ✅ OLED Update
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.print("GAME OVER!");
    display.setCursor(15, 40);
    display.print("THANKS FOR PLAYING!");
    display.display();
}

void displayWinnerScreen() {
    for (int i = 0; i < 3; i++) { // Blink effect 3 times
        tft.fillScreen(ST7735_RED);
        tft.setCursor(20, 78);
        tft.setTextSize(2);
        tft.setTextColor(ST7735_WHITE);
        tft.print("YOU WIN!");
        delay(500);

        tft.fillScreen(ST7735_GREEN);
        tft.setCursor(20, 78);
        tft.setTextSize(2);
        tft.setTextColor(ST7735_BLACK);
        tft.print("YOU WIN!");
        delay(500);
    }

    // ✅ LCD Update
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CONGRATS!");
    lcd.setCursor(0, 1);
    lcd.print("YOU ARE WINNER!");

    // ✅ OLED Update
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.print("YOU ARE WINNER!");
    display.setCursor(15, 40);
    display.print("   PRESS BUTTON!");
    display.display();

    // 🔊 Buzzer Sound
    for (int i = 0; i < 3; i++) {
        beepBuzzer(500);
        delay(200);
    }
}

void displayLoseScreen() {
    tft.fillScreen(ST7735_BLACK);
    
    // 🎨 Display "YOU LOSE!" on TFT
    tft.setCursor(20, 78);
    tft.setTextSize(2);
    tft.setTextColor(ST7735_RED);
    tft.print("YOU LOSE!");

    // ✅ LCD Update
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GAME OVER!");
    lcd.setCursor(0, 1);
    lcd.print("YOU LOSE!");

    // ✅ OLED Update
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.print("GAME OVER!");
    display.setCursor(15, 40);
    display.print("YOU LOSE!");
    display.display();

    // 🔊 Buzzer Sound for Losing
    for (int i = 0; i < 2; i++) {
        beepBuzzer(300);
        delay(200);
    }
}


void loop() {
    mqtt.loop();
    if (!mqtt.connected()) {
        connect_mqtt();
    }

    // 📌 Check for state change
    if (gameState != lastGameState) {  
        Serial.printf("Game State Changed: %d\n", gameState);
        lastGameState = gameState;  

        if (gameState == 0) {
            displayState0();
            winning = 0; // ✅ Reset winning state when waiting
        } else if (gameState == 1) {
            displayBingoCardTFT();
        } else if (gameState == 2) {
            // ✅ Only show "YOU LOSE!" if the player DID NOT win
            if (winning == 0) {
                Serial.println("Forced transition to GAME OVER (2). Showing 'YOU LOSE!'");
                displayLoseScreen();
            }
            resetGame();  // ✅ Reset everything after game end
        }
    }

    // 📌 Check if the player wins
    if (gameState == 1 && checkWin()) {
        Serial.println("BINGO! Press the button to claim.");

        winStartTime = millis(); // Start timer

        // 📌 Wait for button press to claim BINGO
        while (millis() - winStartTime < claimTimeout) {
            if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPressTime > debounceDelay) {
                sendBingoWin();  // ✅ Send "99" to MQTT
                displayWinnerScreen();
                lastPressTime = millis();

                winning = 1; // ✅ Mark player as won
                gameState = 2;  // ✅ Force game state to 2 (GAME OVER)
                return; // ✅ Exit loop to prevent losing screen
            }
        }

        // 📌 If timeout occurs, only show "YOU LOSE!" if still in game state 1
        if (gameState == 1) {  
            Serial.println("Time's up! YOU LOSE.");
            displayLoseScreen();
            delay(3000);
            resetGame();
        }
    }
}

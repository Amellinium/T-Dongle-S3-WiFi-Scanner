//T-Dongle S3 Wifiscanner v0.7 by Amellinium

// Required libraries
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <FastLED.h>

// === FastLED Configuration for T-Dongle S3 ===
#define NUM_LEDS 1 // The board has one built-in APA102 LED
#define DATA_PIN 40
#define CLOCK_PIN 39

// Create an array for our LED, representing its color state
CRGB leds[NUM_LEDS];
// ============================================

TFT_eSPI tft = TFT_eSPI();

const int maxNetworks = 20;
const int networksPerPage = 8;
String ssidList[maxNetworks];
int rssiList[maxNetworks];
int totalNetworks = 0;

int currentStartIndex = 0;
unsigned long lastScanTime = 0;
const unsigned long scanInterval = 5000; // 5 seconds

// === Button Configuration ===
#define BUTTON_PIN 0 // The button on the T-Dongle S3 is connected to GPIO 0
volatile bool buttonPressed = false;
volatile unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// === Screen Rotation State ===
// Initial rotation is set to 1, which you confirmed is the correct upright orientation.
int screenRotation = 1;

// === Scanning Indicator State ===
unsigned long lastIndicatorChange = 0;
const unsigned long indicatorInterval = 250; // Update indicator every 250ms
int indicatorState = 0;

// Function to handle the button interrupt
// The IRAM_ATTR attribute ensures this function is placed in RAM, allowing it to execute quickly.
void IRAM_ATTR handleButtonPress() {
  // Simple debounce logic to prevent multiple button presses from a single physical press.
  if (millis() - lastDebounceTime > debounceDelay) {
    buttonPressed = true;
    lastDebounceTime = millis();
  }
}

// Function to create a smooth "breathing" effect on the LED.
void breatheLED(CRGB color, int maxBrightness, int duration) {
  // Fade in
  for (int brightness = 0; brightness <= maxBrightness; brightness++) {
    FastLED.setBrightness(brightness);
    leds[0] = color;
    FastLED.show();
    delay(duration);
  }

  // Fade out
  for (int brightness = maxBrightness; brightness >= 0; brightness--) {
    FastLED.setBrightness(brightness);
    leds[0] = color;
    FastLED.show();
    delay(duration);
  }
}

void scanNetworks() {
  Serial.println("Starting WiFi scan...");

  // Pulse the LED with the CRGB::Red color before starting the scan.
  breatheLED(CRGB::Red, 25, 30);

  // Perform the WiFi scan (this is a blocking call)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  totalNetworks = WiFi.scanNetworks();
  if (totalNetworks > maxNetworks) totalNetworks = maxNetworks;

  for (int i = 0; i < totalNetworks; ++i) {
    ssidList[i] = WiFi.SSID(i);
    rssiList[i] = WiFi.RSSI(i);
  }

  WiFi.scanDelete();

  Serial.println("WiFi scan complete.");
}

void drawNetworks(int startIndex) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED, TFT_BLACK);

  if (totalNetworks == 0) {
    tft.println("No networks found.");
    return;
  }

  int linesDrawn = 0;
  for (int i = startIndex; i < totalNetworks && linesDrawn < networksPerPage; ++i, ++linesDrawn) {
    int networkNumber = i + 1;
    int maxSsidLength = 14;

    // Adjust the max SSID length for two-digit numbers to maintain alignment
    if (networkNumber >= 10) {
      maxSsidLength = 13;
    }

    String truncatedSsid = ssidList[i].length() > maxSsidLength ? ssidList[i].substring(0, maxSsidLength) : ssidList[i];
    
    // Print the truncated SSID with the correct number and alignment
    tft.printf("%2d. %-*s %3d dBm\n", networkNumber, maxSsidLength, truncatedSsid.c_str(), rssiList[i]);
  }
}

// A new function to draw the animated scanning indicator on the last line.
void drawScanningIndicator() {
  // Clear the bottom line before redrawing the indicator
  tft.fillRect(0, tft.height() - tft.fontHeight(), tft.width(), tft.fontHeight(), TFT_BLACK);
  
  // Set the cursor for the left-aligned text
  tft.setCursor(0, tft.height() - tft.fontHeight());
  
  // Choose the text based on the current animation state
  switch(indicatorState) {
    case 0:
      tft.print("Scanning.");
      break;
    case 1:
      tft.print("Scanning..");
      break;
    case 2:
      tft.print("Scanning...");
      break;
    case 3:
      tft.print("Scanning   "); // Add spaces to clear previous dots
      break;
  }
  
  // Set the cursor for the right-aligned text
  // Calculate the x-position based on screen width and text length
  int wifiTextWidth = tft.textWidth("WiFi");
  tft.setCursor(tft.width() - wifiTextWidth, tft.height() - tft.fontHeight());
  tft.print("WiFi");
}

void setup() {
  Serial.begin(115200);
  
  // === Button Setup ===
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

  tft.init();
  // Set the initial rotation to the original, correct orientation (1).
  tft.setRotation(screenRotation);
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED, TFT_BLACK);

  // === FastLED initialization ===
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
  // The brightness will be controlled dynamically by the breatheLED function.
  // ==============================

  scanNetworks();
  drawNetworks(0);
  drawScanningIndicator(); // Draw the indicator immediately after the initial list
  lastScanTime = millis();
}

void loop() {
  unsigned long now = millis();

  // Handle button press to toggle screen orientation
  if (buttonPressed) {
    buttonPressed = false;
    
    // Toggle the rotation between 1 (upright) and 3 (upside-down).
    if (screenRotation == 1) {
      screenRotation = 3;
    } else {
      screenRotation = 1;
    }
    tft.setRotation(screenRotation);
    drawNetworks(currentStartIndex); // Redraw the screen with the new rotation
    drawScanningIndicator(); // Redraw the indicator after the rotation
  }

  // The breathing effect and scan now happen every 5 seconds.
  if (now - lastScanTime > scanInterval) {
    scanNetworks();
    currentStartIndex = 0;
    drawNetworks(currentStartIndex);
    drawScanningIndicator(); // Redraw the indicator after a new scan
    lastScanTime = now;
  }

  // Update the scanning indicator every 250ms
  if (now - lastIndicatorChange > indicatorInterval) {
    lastIndicatorChange = now;
    indicatorState = (indicatorState + 1) % 4;
    drawScanningIndicator();
  }

  // Handle network list scrolling
  if (totalNetworks > networksPerPage) {
    // We now use a non-blocking check for the scroll delay
    static unsigned long lastScrollTime = 0;
    const unsigned long scrollInterval = 3000; // 3 seconds
    if (now - lastScrollTime > scrollInterval) {
      currentStartIndex += networksPerPage;
      if (currentStartIndex >= totalNetworks) {
        currentStartIndex = 0; // wrap
      }
      drawNetworks(currentStartIndex);
      drawScanningIndicator(); // Redraw the indicator after scrolling
      lastScrollTime = now;
    }
  }
}

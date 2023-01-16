/*
  Display.cpp - Class responsible of controlling the display
  
  Copyright (C) 2020 -2021 @G4lile0, @gmag12 and @dev_4m1g0

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "Display.h"
#include "graphics.h"
#include "secrets.h"

#define OLED_TXTLEN            15
#define OLED_LINES_CNT         4

SSD1306* display;
OLEDDisplayUi* ui = NULL;

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
uint8_t overlaysCount = 1;
OverlayCallback overlays[] = { msOverlay };

unsigned long tick_interval;
int tick_timing = 100;
int graphVal = 1;
int delta = 1;
uint8_t oldOledBright = 100;

static const String TINYGPS_USER_ID = TINYGPS_USER_ID_ENV;

void displayInit()
{
  board_type board = ConfigManager::getInstance().getBoardConfig();
  display = new SSD1306(board.OLED__address, board.OLED__SDA, board.OLED__SCL);

  ui = new OLEDDisplayUi(display);
  ui->setTargetFPS(60);
  ui->setActiveSymbol(activeSymbol);
  ui->setInactiveSymbol(inactiveSymbol);
  ui->setIndicatorPosition(BOTTOM);
  ui->setIndicatorDirection(LEFT_RIGHT);
  ui->setFrameAnimation(SLIDE_LEFT);
  ui->setOverlays(overlays, overlaysCount);
  ui->init();
  pinMode(board.OLED__RST,OUTPUT);
  digitalWrite(board.OLED__RST, LOW);     
  delay(50);
  digitalWrite(board.OLED__RST, HIGH);   
  display->init();
  display->clear();

  if (ConfigManager::getInstance().getFlipOled())
    display->flipScreenVertically();
}

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state)
{
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);

  struct tm* timeinfo;
  time_t currenttime = time (NULL);
  if(currenttime < 0)
  {
    Serial.println("Failed to obtain time");
    return;
  }
  timeinfo = localtime (&currenttime);

  String thisTime="";
  if (timeinfo->tm_hour < 10){ thisTime=thisTime + " ";} // add leading space if required
  thisTime = String (timeinfo->tm_hour) + ":";
  if (timeinfo->tm_min < 10) { thisTime = thisTime + "0"; } // add leading zero if required
  thisTime = thisTime + String (timeinfo->tm_min) + ":";
  if (timeinfo->tm_sec < 10) { thisTime = thisTime + "0"; } // add leading zero if required
  thisTime = thisTime + String (timeinfo->tm_sec);
  const char* newTime = (const char*) thisTime.c_str();
  display->drawString(128, 0, newTime);

  if (ConfigManager::getInstance().getDayNightOled())
  {
    if (timeinfo->tm_hour < 6 || timeinfo->tm_hour > 18) display->normalDisplay(); else display->invertDisplay(); // change the OLED according to the time. 
  }


  if (oldOledBright!=ConfigManager::getInstance().getOledBright())
  {
    oldOledBright = ConfigManager::getInstance().getOledBright(); 
    if (ConfigManager::getInstance().getOledBright()==0) {
      display->displayOff();
    }
    else
    {
      display->setBrightness(2*ConfigManager::getInstance().getOledBright());
    }
  }
}


void displayLoraFailed() {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, "LoRa initialization failed.");
  display->drawString(0, 14, "Browse " + WiFi.localIP().toString());
  display->display();
}


void dispalyClearStringLine(int line_id) {
  display->setColor(BLACK);
  for (int i = -1; i < 16 + 1; i++) {
    int y = line_id * 16 + i;
    if (y < 0) {
      continue;
    }
    display->drawHorizontalLine(0, y, display->width());
  }
  display->setColor(WHITE);
}


void displayClearAt(int16_t xPos, int16_t yPos) {
  display->setColor(BLACK);
  for (int i = 0; i < 16; i++) {
    display->drawHorizontalLine(xPos, yPos + i, display->width() - xPos);
  }
  display->setColor(WHITE);
}


void displayNoMessages(int16_t yPos) {
  displayClearAt(0, yPos);
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, yPos, "(NO MESSAGES)");
  display->display();
}


void displayTimestamp(const struct timeval& timestamp, int16_t xPos, int16_t yPos) {
  struct tm timeinfo;
  char strftime_buf[64];
  localtime_r(&timestamp.tv_sec, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%H:%M:%S", &timeinfo);
  displayClearAt(xPos, yPos);
  display->drawString(xPos, yPos, strftime_buf);
}

void displayPrintln(const String& large_string, int line_start) {
    const size_t len = large_string.length();
    int n_lines = len / OLED_TXTLEN;
    const int n_lines_max = OLED_LINES_CNT - line_start;
    if (n_lines > n_lines_max) {
        n_lines = n_lines_max;
    }
    dispalyClearStringLine(line_start);
    for (int li = 0; li < n_lines; li++) {
      dispalyClearStringLine(li + line_start);
      display->drawString(0, (li + line_start) * OLED_TXTLEN, large_string.substring(li * OLED_TXTLEN, (li + 1) * OLED_TXTLEN));
    }
    const int last_bytes = len % OLED_TXTLEN;
    if (n_lines < n_lines_max && last_bytes > 0) {
        display->drawString(0, (n_lines + line_start) * OLED_TXTLEN, large_string.substring(len - last_bytes));
    }
}


void displayCurrTime() {
  struct timeval timestamp;
  gettimeofday(&timestamp, NULL);
  displayTimestamp(timestamp, 0, 0);
}


void displayRadioReceived(const String& message, const struct timeval& timestamp)
{
  if (message.length() == 0) {
    displayNoMessages(16);
    return;
  }
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  if (message.startsWith(TINYGPS_USER_ID)) {
    displayTimestamp(timestamp, display->width() / 2 + 25, 0);
    String message_cut = message.substring(TINYGPS_USER_ID.length());
    displayPrintln(message_cut, 1);
  }
  display->display();
}

void displayShowApMode()
{
  display->clear();
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 6,"Connect to AP:");
  display->drawString(0,18,"->"+String(ConfigManager::getInstance().getThingName()));
  display->drawString(5,32,"to configure your Station");
  display->drawString(10,52,"IP:   192.168.4.1");
  display->display();
}

void displayShowStaMode(bool ap)
{
  display->clear();
  display->drawXbm(34, 0 , WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 , 35 , "Connecting " + String(ConfigManager::getInstance().getWiFiSSID()));
  if (ap)
    display->drawString(64 , 52 , "Config AP available");
  display->display();
}

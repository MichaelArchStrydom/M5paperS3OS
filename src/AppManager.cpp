#include "AppManager.h"
#include "BloomScreenSaver.h"
#include <WiFi.h>

// Static state tracking for system status
static int lastBatteryLevel = -1;
static std::string lastWiFiStatus = "";
static wl_status_t lastRawWiFiStatus = WL_IDLE_STATUS;
static bool touchHandled = false;
static bool internalFooterUpdate =
    false; // Local flag to replace the missing header variable

// Battery and Power protection constants
unsigned long lastBatteryCheck = 0;
const int BATTERY_CHECK_INTERVAL = 10000;
const int LOW_BATTERY_THRESHOLD = 10;
const int WARNING_BATTERY_THRESHOLD = 20;
unsigned long lastActivityTime = millis();
const unsigned long IDLE_TIMEOUT = 300000; // 5 minutes

void AppManager::setup() { M5.Display.clear(); }

void AppManager::update() {
  M5.update();

  // 1. Throttle WiFi status checks (every 1000ms)
  // This allows the footer to update automatically when connection is
  // established
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 1000) {
    lastWiFiCheck = millis();
    wl_status_t currentRawStatus = WiFi.status();
    if (currentRawStatus != lastRawWiFiStatus) {
      lastRawWiFiStatus = currentRawStatus;
      internalFooterUpdate = true;
    }
  }

  // 2. Update current application logic
  if (currentApp) {
    currentApp->update();
    if (currentApp->needsRedraw()) {
      needsRedraw = true;
    }
  }

  // 3. Handle Touch Input
  if (M5.Touch.getCount() > 0) {
    lastActivityTime = millis(); // Reset idle timer
    if (!touchActive) {
      touchActive = true;
      touchHandled = false;
      auto touch = M5.Touch.getDetail();

      // Check navigation area first
      handleNavigationTouch(touch.x, touch.y);

      // Wants feedback mostly used by sudoku
      bool appWantsFeedback =
          !currentApp || currentApp->wantsTouchFeedback(touch.x, touch.y);

      // If not handled by nav buttons, pass to the app
      if (!touchHandled && currentApp) {
        currentApp->handleTouch(touch.x, touch.y);
      }
      // User interaction triggers a beep and full redraw unless the app opted
      // out
      if (touchHandled || appWantsFeedback) {
        needsRedraw = true;
      }
    }
  } else {
    touchActive = false;
  }

  // 4. Redraw Logic (Differentiates between User-Action and Background-Update)
  if (needsRedraw || internalFooterUpdate) {
    // Only beep if the user did something or the APP requested a refresh
    if (needsRedraw) {
      M5.Speaker.setVolume(128);
      M5.Speaker.tone(800, 50);
      if (currentApp) {
        currentApp->draw();
      }
    }

    // Footer always draws if there's a status change or full redraw
    drawFooter();

    M5.Display.display();

    needsRedraw = false;
    internalFooterUpdate = false;
  }

  // 5. Idle / Power Management
  if (millis() - lastActivityTime > IDLE_TIMEOUT) {
    if (M5.Display.isEPD()) {
      M5.Display.setEpdMode(epd_mode_t::epd_quality);
    }
    M5.Display.setRotation(2);
    showScreensaver();
    M5.Display.display();
    delay(1000);
    M5.Power.deepSleep();
  }

  // 6. Battery Level Monitoring
  if (millis() - lastBatteryCheck > BATTERY_CHECK_INTERVAL) {
    lastBatteryCheck = millis();
    int batteryLevel = M5.Power.getBatteryLevel();
    float batteryVoltage = M5.Power.getBatteryVoltage() / 1000.0;

    if (batteryLevel < LOW_BATTERY_THRESHOLD || batteryVoltage < 3.0) {
      M5.Display.display();
      delay(2000);
      M5.Power.deepSleep();
    }

    if (batteryLevel != lastBatteryLevel) {
      internalFooterUpdate = true;
    }
  }
}

void AppManager::registerApp(const std::string &name, App *app) {
  appRegistry.push_back({name, app});
}

AppRecord *AppManager::findApp(const std::string &name) {
  for (auto &record : appRegistry) {
    if (record.name == name) {
      return &record;
    }
  }
  return nullptr;
}

void AppManager::switchApp(const std::string &name, bool isNav) {
  Serial.print("Switching app to: ");
  Serial.println(name.c_str());

  AppRecord *record = findApp(name);
  if (!record)
    return;

  if (currentApp) {
    currentApp->exit();
    if (!isNav) {
      navHistoryBack.push_back({currentAppName, ""});
      navHistoryForward.clear();
    }
  }

  currentApp = record->appInstance;
  currentAppName = record->name;
  currentApp->setup();
  needsRedraw = true;
}

void AppManager::drawFooter() {
  int currentBatteryLevel = M5.Power.getBatteryLevel();

  // Convert WiFi status to readable string
  std::string currentWiFiStatus = "Off";
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    currentWiFiStatus = "WiFi";
  } else if (WiFi.getMode() != WIFI_OFF && status != WL_CONNECTED) {
    currentWiFiStatus = "...";
  }

  m5::rtc_time_t time;
  M5.Rtc.getTime(&time);
  m5::rtc_date_t date;
  M5.Rtc.getDate(&date);

  // Tracking time change to trigger footer update
  static int lastMin = -1;
  bool timeChanged = (time.minutes != lastMin);

  // Redraw if any status changed, or if a full screen redraw was requested
  if (currentBatteryLevel != lastBatteryLevel ||
      currentWiFiStatus != lastWiFiStatus || timeChanged ||
      internalFooterUpdate || needsRedraw) {
    lastMin = time.minutes;

    M5.Display.fillRect(0, 910, 540, 50, WHITE);
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextColor(BLACK);

    char dateBuffer[16];
    sprintf(dateBuffer, "%04d/%02d/%02d", date.year, date.month, date.date);

    char timeBuffer[16];
    sprintf(timeBuffer, "%02d:%02d", time.hours, time.minutes);

    std::string statusStr = "Bat: " + std::to_string(currentBatteryLevel) +
                            "% | " + currentWiFiStatus + " | " + dateBuffer +
                            " | " + timeBuffer;

    M5.Display.drawString(statusStr.c_str(), 60, 935);
    M5.Display.drawString("<", 20, 933);
    M5.Display.drawString(">", 500, 933);

    lastBatteryLevel = currentBatteryLevel;
    lastWiFiStatus = currentWiFiStatus;
  }
}

void AppManager::handleNavigationTouch(int x, int y) {
  // Only handle if in the bottom footer area
  if (y < 910)
    return;

  // Back button area
  if (x >= 0 && x <= 65) {
    bool handledByApp = false;
    if (currentApp) {
      handledByApp = currentApp->handleBackPress();
    }

    if (!handledByApp && !navHistoryBack.empty()) {
      NavigationState previousState = navHistoryBack.back();
      navHistoryBack.pop_back();
      navHistoryForward.push_back({currentAppName, ""});
      switchApp(previousState.appName, true);
    }
    touchHandled = true;
    needsRedraw = true;
  }

  // Forward button area
  if (x >= 475 && x <= 540) {
    bool handledByApp = false;
    if (currentApp) {
      handledByApp = currentApp->handleForwardPress();
    }

    if (!handledByApp && !navHistoryForward.empty()) {
      NavigationState nextState = navHistoryForward.back();
      navHistoryForward.pop_back();
      navHistoryBack.push_back({currentAppName, ""});
      switchApp(nextState.appName, true);
    }
    touchHandled = true;
    needsRedraw = true;
  }
}

#include "AppManager.h"
#include "BloomScreenSaver.h"
#include <SD.h>
#include <WiFi.h>

namespace {

// The PaperS3 measures the cell through a 1:2 divider on GPIO3. Readings sag
// under load (WiFi, EPD refresh), so several consecutive low readings are
// required before shutting down.
constexpr int BATTERY_EMPTY_MV = 3300;
constexpr int BATTERY_FULL_MV = 4150;
constexpr int BATTERY_WARN_MV = 3600;
constexpr int BATTERY_CRITICAL_MV = 3450;
constexpr int CRITICAL_READINGS_BEFORE_SHUTDOWN = 3;
constexpr unsigned long BATTERY_CHECK_INTERVAL_MS = 10000;
constexpr unsigned long STATUS_CHECK_INTERVAL_MS = 1000;
constexpr unsigned long LOOP_DELAY_MS = 10;

constexpr int FOOTER_Y = 910;

// Not exposed by M5Unified 0.2.2; values match upstream M5Unified.
constexpr int PAPERS3_CHG_STAT_PIN = 4;    // low while charging
constexpr int PAPERS3_POWER_HOLD_PIN = 44; // pulsing low releases the latch

bool isPaperS3() { return M5.getBoard() == m5::board_t::board_M5PaperS3; }

int readBatteryMv() {
  long sum = 0;
  int samples = 0;
  for (int i = 0; i < 8; i++) {
    int mv = M5.Power.getBatteryVoltage();
    if (mv > 0) {
      sum += mv;
      samples++;
    }
  }
  return samples ? sum / samples : -1;
}

int mvToPercent(int mv) {
  if (mv <= 0)
    return -1;
  int level = (mv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
  return level < 0 ? 0 : level > 100 ? 100 : level;
}

bool readCharging() {
  return isPaperS3() && digitalRead(PAPERS3_CHG_STAT_PIN) == LOW;
}

bool isCritical(int mv, bool charging) {
  return !charging && mv > 0 && mv < BATTERY_CRITICAL_MV;
}

} // namespace

void AppManager::setup() {
  if (isPaperS3())
    pinMode(PAPERS3_CHG_STAT_PIN, INPUT);

  batteryMv = readBatteryMv();
  charging = readCharging();
  batteryPercent = mvToPercent(batteryMv);
  if (isCritical(batteryMv, charging)) {
    powerDown("Battery empty", "Charge via USB-C, then press power.");
  }

  lastActivityTime = millis();
  lastBatteryCheck = millis();
  M5.Display.clear();
}

void AppManager::update() {
  M5.update();

  checkStatus();
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL_MS) {
    checkBattery();
  }

  if (currentApp) {
    currentApp->update();
    if (currentApp->needsRedraw()) {
      needsRedraw = true;
    }
  }

  // Touch: act on the press edge only
  if (M5.Touch.getCount() > 0) {
    lastActivityTime = millis();
    if (!touchActive) {
      touchActive = true;
      touchHandled = false;
      auto touch = M5.Touch.getDetail();

      M5.Speaker.tone(800, 30);

      handleNavigationTouch(touch.x, touch.y);

      // Apps that do their own partial redraws (Sudoku, Workout) opt out of
      // the full-screen redraw on every touch.
      bool appWantsFeedback =
          !currentApp || currentApp->wantsTouchFeedback(touch.x, touch.y);

      if (!touchHandled && currentApp) {
        currentApp->handleTouch(touch.x, touch.y);
      }
      if (touchHandled || appWantsFeedback ||
          (currentApp && currentApp->needsRedraw())) {
        needsRedraw = true;
      }
    }
  } else {
    touchActive = false;
  }

  // Batch the whole frame into one write so the EPD refreshes once, covering
  // only the region that changed. (M5GFX auto-displays at the outermost
  // endWrite; unbatched draw calls would each trigger a refresh.)
  if (needsRedraw || footerDirty) {
    M5.Display.startWrite();
    if (needsRedraw && currentApp) {
      currentApp->draw();
    }
    drawFooter();
    M5.Display.endWrite();

    needsRedraw = false;
    footerDirty = false;
  }

  unsigned long idleTimeout =
      currentApp ? currentApp->idleTimeoutMs() : 300000UL;
  if (millis() - lastActivityTime > idleTimeout) {
    powerDown(nullptr, nullptr);
  }

  // Yield instead of spinning flat out; touch is still polled at ~100Hz.
  delay(LOOP_DELAY_MS);
}

void AppManager::checkStatus() {
  if (millis() - lastStatusCheck < STATUS_CHECK_INTERVAL_MS)
    return;
  lastStatusCheck = millis();

  int wifiState = WiFi.getMode() == WIFI_OFF        ? 0
                  : WiFi.status() == WL_CONNECTED ? 2
                                                  : 1;
  if (wifiState != lastWiFiState) {
    lastWiFiState = wifiState;
    footerDirty = true;
  }

  m5::rtc_time_t time;
  M5.Rtc.getTime(&time);
  if (time.minutes != lastFooterMinute) {
    footerDirty = true;
  }
}

void AppManager::checkBattery() {
  lastBatteryCheck = millis();

  int mv = readBatteryMv();
  bool nowCharging = readCharging();
  int percent = mvToPercent(mv);
  bool low = !nowCharging && mv > 0 && mv < BATTERY_WARN_MV;

  if (percent != batteryPercent || nowCharging != charging ||
      low != batteryLow) {
    footerDirty = true;
  }
  batteryMv = mv;
  batteryPercent = percent;
  charging = nowCharging;
  batteryLow = low;

  if (isCritical(mv, nowCharging)) {
    criticalReadings++;
    Serial.printf("Battery critical: %dmV (%d/%d)\n", mv, criticalReadings,
                  CRITICAL_READINGS_BEFORE_SHUTDOWN);
    if (criticalReadings >= CRITICAL_READINGS_BEFORE_SHUTDOWN) {
      powerDown("Battery empty", "Charge via USB-C, then press power.");
    }
  } else {
    criticalReadings = 0;
  }
}

// Saves app state, leaves a message (or the screensaver) on the e-ink panel,
// which persists without power, then cuts power.
void AppManager::powerDown(const char *title, const char *detail) {
  Serial.println(title ? title : "Idle timeout, powering down");
  if (currentApp) {
    currentApp->exit();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  M5.Display.setRotation(2);
  if (M5.Display.isEPD()) {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
  }
  M5.Display.startWrite();
  if (title) {
    M5.Display.fillScreen(WHITE);
    M5.Display.setTextColor(BLACK);
    M5.Display.setFont(&fonts::FreeSansBold24pt7b);
    M5.Display.drawCentreString(title, 270, 380);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawCentreString(detail ? detail : "", 270, 460);
    M5.Display.drawCentreString("Your data has been saved.", 270, 500);
  } else {
    showScreensaver();
  }
  M5.Display.endWrite();
  M5.Display.waitDisplay();

  SD.end();

  if (isPaperS3()) {
    // The PaperS3 power latch only releases on repeated pulses.
    pinMode(PAPERS3_POWER_HOLD_PIN, OUTPUT);
    for (int i = 0; i < 5; ++i) {
      digitalWrite(PAPERS3_POWER_HOLD_PIN, LOW);
      delay(50);
      digitalWrite(PAPERS3_POWER_HOLD_PIN, HIGH);
      delay(50);
    }
  }

  // Still running: USB is holding the power rail up. Sleep instead.
  M5.Power.deepSleep(0, false);
  while (true) {
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
  m5::rtc_time_t time;
  M5.Rtc.getTime(&time);
  m5::rtc_date_t date;
  M5.Rtc.getDate(&date);
  lastFooterMinute = time.minutes;

  const char *wifi = lastWiFiState == 2   ? "WiFi"
                     : lastWiFiState == 1 ? "..."
                                          : "Off";

  char battery[16];
  if (batteryPercent < 0) {
    snprintf(battery, sizeof(battery), "Bat --");
  } else {
    snprintf(battery, sizeof(battery), "%s %d%%",
             charging ? "Chg" : batteryLow ? "LOW" : "Bat", batteryPercent);
  }

  char rest[48];
  snprintf(rest, sizeof(rest), " | %s | %04d/%02d/%02d | %02d:%02d", wifi,
           date.year, date.month, date.date, time.hours, time.minutes);

  M5.Display.fillRect(0, FOOTER_Y, 540, 50, WHITE);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.setTextDatum(top_left);

  int x = 60;
  int batteryW = M5.Display.textWidth(battery);
  if (batteryLow) {
    // Inverted so a low battery stands out
    M5.Display.fillRect(x - 4, FOOTER_Y + 20, batteryW + 8, 26, BLACK);
    M5.Display.setTextColor(WHITE);
  }
  M5.Display.drawString(battery, x, 935);
  M5.Display.setTextColor(BLACK);
  M5.Display.drawString(rest, x + batteryW, 935);

  M5.Display.drawString("<", 20, 933);
  M5.Display.drawString(">", 500, 933);
}

void AppManager::handleNavigationTouch(int x, int y) {
  // Only handle if in the bottom footer area
  if (y < FOOTER_Y)
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

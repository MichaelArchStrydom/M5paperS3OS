#pragma once
#include <vector>
#include <string>
#include "App.h"

struct NavigationState {
    std::string appName;
    std::string statePayload;
};

struct AppRecord {
    std::string name;
    App* appInstance;
};

class AppManager {
public:
    // Call right after M5.begin(): refuses to boot on a critically low battery.
    void setup();
    void update();
    void registerApp(const std::string& name, App* app);
    void switchApp(const std::string& name, bool isNav = false);

private:
    std::vector<AppRecord> appRegistry;
    App* currentApp = nullptr;
    std::string currentAppName;
    bool needsRedraw = true;
    bool footerDirty = false;
    bool touchActive = false;
    bool touchHandled = false;

    std::vector<NavigationState> navHistoryBack;
    std::vector<NavigationState> navHistoryForward;

    // Power state
    unsigned long lastActivityTime = 0;
    unsigned long lastStatusCheck = 0;
    unsigned long lastBatteryCheck = 0;
    int batteryMv = -1;
    int batteryPercent = -1;
    bool charging = false;
    bool batteryLow = false;
    int criticalReadings = 0;
    int lastFooterMinute = -1;
    int lastWiFiState = -1;

    void checkStatus();
    void checkBattery();
    [[noreturn]] void powerDown(const char* title, const char* detail);

    void drawFooter();
    void handleNavigationTouch(int x, int y);
    AppRecord* findApp(const std::string& name);
};

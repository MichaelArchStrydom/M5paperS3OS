#include "SettingsApp.h"
#include "AppManager.h"
#include "ui/LabelButton.h"
#include "ui/Incrementer.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <WiFi.h>
#include "ui/ListSelector.h"

// Initialize all UI elements in the constructor initializer list
SettingsApp::SettingsApp(AppManager* manager) : appManager(manager),
    yearInc(70, 150, 80, 40),
    monthInc(220, 150, 80, 40),
    dateInc(370, 150, 80, 40),
    hourInc(70, 400, 80, 40),
    minuteInc(220, 400, 80, 40),
    secondInc(370, 400, 80, 40),
    saveButton(190, 800, 160, 60, "Save"),
    networkSelector(70, 150, 400, 400, 60),
    _keyboard(0, 0, 960, 540)
{}

void SettingsApp::setup(){
    Serial.println("Loading settings app");
    currentState = SettingsState::MAIN_SETTINGS;
    _needsRedraw = true;
    _isScanning = false;
    _pendingSSID = "";
    
    mainSettingsButtons.clear();
    // FIXED: Changed argument order to match LabelButton(x, y, w, h, label)
    mainSettingsButtons.push_back(ui::LabelButton(120, 200, 300, 80, "Date and Time"));
    mainSettingsButtons.push_back(ui::LabelButton(120, 320, 300, 80, "Network"));
}

// IMPROVED: Logic to prevent constant redrawing
bool SettingsApp::needsRedraw() {
    // If keyboard is visible, only redraw if the keyboard itself says it needs to
    if (_keyboard.isVisible()) {
        // FIXED: Using isVisible() or a public check instead of private _needsRedraw
        // If your Keyboard class doesn't have a public needsRedraw getter, 
        // we use isVisible() logic or update the Keyboard class.
        return _keyboard.isVisible() && _needsRedraw; 
    }
    return _needsRedraw;
}

void SettingsApp::update() {
    if (_keyboard.isVisible()) return;

    if(_isScanning) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            scannedNetworksSSID.clear();
            for (int i = 0; i < n; i++) {
                scannedNetworksSSID.push_back(WiFi.SSID(i));
            }
            networkSelector.setOptions(scannedNetworksSSID); 
            _isScanning = false;
            _needsRedraw = true;
        }
    }
}

void SettingsApp::draw() {
    // If keyboard is active, let it handle its own drawing logic
    // Keyboard::draw() internally checks _needsRedraw, so it won't flicker
    if (_keyboard.isVisible()) {
        _keyboard.draw();
        return;
    }

    if (!_needsRedraw) return;
    
    M5.Display.clear();
    switch (currentState) {
        case SettingsState::MAIN_SETTINGS:
            drawMainSettings();
            break;
        case SettingsState::DATE_TIME:
            drawDateTimeScreen();
            break;
        case SettingsState::NETWORK:
            drawNetworkScreen();
            break;
    }
    _needsRedraw = false;
}

void SettingsApp::handleTouch(int x, int y) {
    if (_keyboard.isVisible()) {
        _keyboard.handleTouch(x, y);
        return;
    }

    switch (currentState) {
        case SettingsState::MAIN_SETTINGS:
            handleMainSettingsTouch(x, y);
            break;
        case SettingsState::DATE_TIME:
            handleDateTimeTouch(x, y);
            break;
        case SettingsState::NETWORK:
            handleNetworkTouch(x, y);
            break;
    }
}

void SettingsApp::drawMainSettings() {
    M5.Display.setFont(&fonts::FreeSansBold24pt7b);
    M5.Display.drawCentreString("Settings", 270, 100);
    for (auto& button : mainSettingsButtons) {
        button.draw();
    }
}

void SettingsApp::handleMainSettingsTouch(int x, int y){
    for (auto& button : mainSettingsButtons) {
        if (button.isPressed(x, y)) {
            if (button.getLabel() == "Date and Time") {
                currentState = SettingsState::DATE_TIME;
                M5.Rtc.getTime(&_time);
                M5.Rtc.getDate(&_date);
            } else if (button.getLabel() == "Network") {
                // WiFi is off by default to save battery
                if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
                WiFi.scanNetworks(true);
                _isScanning = true;
                currentState = SettingsState::NETWORK;
            }
            _needsRedraw = true;
        }
    }
}

void SettingsApp::drawDateTimeScreen(){
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.drawCentreString("Set Date & Time", 270, 60);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextColor(BLACK);

    M5.Display.drawCentreString("Year", 110, 130);
    M5.Display.drawCentreString("Month", 260, 130);
    M5.Display.drawCentreString("Date", 410, 130);
    M5.Display.drawCentreString("Hour", 110, 380);
    M5.Display.drawCentreString("Minute", 260, 380);
    M5.Display.drawCentreString("Second", 410, 380);
    
    yearInc.draw(_date.year);
    monthInc.draw(_date.month);
    dateInc.draw(_date.date);
    hourInc.draw(_time.hours);
    minuteInc.draw(_time.minutes);
    secondInc.draw(_time.seconds);
    saveButton.draw();
}

static int wrapValue(int value, int min, int max) {
    if (value < min) return max;
    if (value > max) return min;
    return value;
}

static int daysInMonth(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 31; // unset RTC can report month 0
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return (month == 2 && leap) ? 29 : days[month - 1];
}

void SettingsApp::handleDateTimeTouch(int x, int y){
    bool changed = true;
    if(yearInc.isIncrementPressed(x, y)) _date.year = wrapValue(_date.year + 1, 2000, 2099);
    else if (yearInc.isDecrementPressed(x, y)) _date.year = wrapValue(_date.year - 1, 2000, 2099);
    else if(monthInc.isIncrementPressed(x, y)) _date.month = wrapValue(_date.month + 1, 1, 12);
    else if (monthInc.isDecrementPressed(x, y)) _date.month = wrapValue(_date.month - 1, 1, 12);
    else if(dateInc.isIncrementPressed(x, y)) _date.date = wrapValue(_date.date + 1, 1, daysInMonth(_date.year, _date.month));
    else if (dateInc.isDecrementPressed(x, y)) _date.date = wrapValue(_date.date - 1, 1, daysInMonth(_date.year, _date.month));
    else if(hourInc.isIncrementPressed(x, y)) _time.hours = wrapValue(_time.hours + 1, 0, 23);
    else if (hourInc.isDecrementPressed(x, y)) _time.hours = wrapValue(_time.hours - 1, 0, 23);
    else if(minuteInc.isIncrementPressed(x, y)) _time.minutes = wrapValue(_time.minutes + 1, 0, 59);
    else if (minuteInc.isDecrementPressed(x, y)) _time.minutes = wrapValue(_time.minutes - 1, 0, 59);
    else if(secondInc.isIncrementPressed(x, y)) _time.seconds = wrapValue(_time.seconds + 1, 0, 59);
    else if (secondInc.isDecrementPressed(x, y)) _time.seconds = wrapValue(_time.seconds - 1, 0, 59);
    else changed = false;

    // Keep the day valid after a month/year change (31 Mar -> 30 Apr)
    int maxDay = daysInMonth(_date.year, _date.month);
    if (_date.date > maxDay) _date.date = maxDay;
    if (_date.date < 1) _date.date = 1;

    if (saveButton.isPressed(x, y)) {
        m5::rtc_datetime_t dt; dt.date = _date; dt.time = _time;
        M5.Rtc.setDateTime(dt);
        currentState = SettingsState::MAIN_SETTINGS;
        changed = true;
    }
    if (changed) _needsRedraw = true;
}

void SettingsApp::drawNetworkScreen(){
    M5.Display.setFont(&fonts::FreeSansBold24pt7b);
    M5.Display.drawCentreString("Choose Network", 270, 60);

    if (_isScanning) {
        M5.Display.setFont(&fonts::FreeSansBold18pt7b);
        M5.Display.drawCentreString("Searching...", 270, 300);
    } else {
        networkSelector.draw(); 
        M5.Display.fillRect(170, 800, 200, 60, BLACK);
        M5.Display.setTextColor(WHITE);
        M5.Display.setFont(&fonts::FreeSansBold12pt7b);
        M5.Display.drawCentreString("CONNECT", 270, 828);
    }
}

void SettingsApp::handleNetworkTouch(int x, int y){
    if (_isScanning) return;

    if (networkSelector.handleTouch(x, y)) {
        networkSelector.draw(); 
    }

    if (x >= 170 && x <= 370 && y >= 800 && y <= 860) {
        String arduinoSelected = networkSelector.getSelectedString();
        if (arduinoSelected.length() > 0) {
            std::string selected = arduinoSelected.c_str();
            _pendingSSID = arduinoSelected; 
            
            std::string msg = "Enter password for " + selected;
            _keyboard.show(msg, [this](const std::string& password) {
                this->saveNetwork(this->_pendingSSID, String(password.c_str()));
            });
        }
    }
}

void SettingsApp::saveNetwork(String ssid, String password) {
    Serial.printf("Connecting to %s...\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    currentState = SettingsState::MAIN_SETTINGS;
    _needsRedraw = true; 
}
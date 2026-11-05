#include <M5Unified.h>
#include <SD.h>      
#include <WiFi.h>
#include "AppManager.h"
#include "MenuApp.h"
#include "SDExplorerApp.h" 
#include "WorkoutApp.h"
#include "SettingsApp.h"
#include "PianoApp.h"
#include "WidgetApp.h"
#include "SudokuApp.h"
// #include "TemplateApp.h"

SPIClass sdSPI(FSPI);
AppManager appManager;
MenuApp menuApp(&appManager);
SettingsApp settingsApp(&appManager);
SDExplorerApp sdExplorerApp(&appManager);
WorkoutApp workoutApp(&appManager);
PianoApp pianoApp(&appManager);
WidgetApp widgetApp(&appManager);
SudokuApp sudokuApp(&appManager);
// TemplateApp templateApp(&appManager);



void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    if (M5.Display.isEPD())
    {
        M5.Display.setEpdMode(epd_mode_t::epd_fast); // or epd_fastest
    }
    // Rotation portrait flipped
    M5.Lcd.setRotation(2);
    M5.Speaker.setVolume(128);

    Serial.begin(115200); 
    Serial.println("I'm ONNN!");

    // Checks the battery first and powers straight back off if it is empty
    appManager.setup();

    // The radio is the biggest power draw; Settings > Network turns it on.
    WiFi.mode(WIFI_OFF);

    sdSPI.begin(39, 40, 38);
    if (!SD.begin(47, sdSPI)) {
        // Keep running: apps that need the card show their own message.
        Serial.println("SD Card Mount Failed");
    }

    appManager.registerApp("Menu", &menuApp);
    appManager.registerApp("SettingsApp", &settingsApp); 
    appManager.registerApp("SD_Explorer", &sdExplorerApp); 
    appManager.registerApp("Workout", &workoutApp);
    appManager.registerApp("Piano", &pianoApp);
    appManager.registerApp("Widget", &widgetApp);
    appManager.registerApp("Sudoku", &sudokuApp);
    //appManager.registerApp("Template", &templateApp);
    
    appManager.switchApp("Menu");
}

void loop() {
    appManager.update();
}
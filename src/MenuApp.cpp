#include "MenuApp.h"
#include "AppManager.h"

MenuApp::MenuApp(AppManager *manager) : appManager(manager) {}

void MenuApp::setup() {
  buttons.clear();
  buttons.push_back({120, 200, 300, 80, "SD Explorer", "SD_Explorer"});
  buttons.push_back({120, 300, 300, 80, "Workout Logger", "Workout"});
  buttons.push_back({120, 400, 300, 80, "Settings", "SettingsApp"});
  buttons.push_back({120, 500, 300, 80, "Piano", "Piano"});
  buttons.push_back({120, 600, 300, 80, "Sudoku", "Sudoku"});
  // buttons.push_back({120, 500, 300, 80, "Template", "TemplateApp"});
}

void MenuApp::update() {}

void MenuApp::draw() {
  M5.Display.clear();
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawCentreString("Main Menu", 270, 50);

  for (const auto &btn : buttons) {
    drawButton(btn);
  }
}

void MenuApp::handleTouch(int x, int y) {
  for (const auto &btn : buttons) {
    if (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h) {
      appManager->switchApp(btn.targetAppName);
      break;
    }
  }
}

void MenuApp::drawButton(const MenuButton &button) {
  M5.Display.setTextSize(1);
  M5.Display.fillRect(button.x, button.y, button.w, button.h, WHITE);
  M5.Display.drawRect(button.x, button.y, button.w, button.h, BLACK);
  M5.Display.drawCentreString(button.label.c_str(), button.x + button.w / 2,
                              button.y + button.h / 2 - 10);
}

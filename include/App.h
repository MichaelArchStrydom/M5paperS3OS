#pragma once
#include <M5Unified.h>
#include <string>

class App {
public:
  virtual ~App() {}
  virtual void setup() = 0;
  virtual void update() = 0;
  virtual void draw() = 0;
  virtual void handleTouch(int x, int y) = 0;

  virtual bool needsRedraw() { return false; }

  virtual bool wantsTouchFeedback(int x, int y) { return true; }

  virtual bool handleBackPress() { return false; }
  virtual bool handleForwardPress() { return false; }

  // Called before switching away and before the device powers down
  // (idle or low battery). Persist anything that must survive here.
  virtual void exit() {};

  // How long without touches before the device powers down.
  virtual unsigned long idleTimeoutMs() { return 300000; }
};

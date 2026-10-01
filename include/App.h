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

  virtual void exit() {};
};

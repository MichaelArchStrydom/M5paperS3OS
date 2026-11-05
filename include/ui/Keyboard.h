#pragma once
#include <M5Unified.h>
#include <functional>
#include <string>
#include <vector>

namespace ui {

class Keyboard {
public:
    using Callback = std::function<void(const std::string&)>;

    // Default constructor
    Keyboard(int x = 0, int y = 0, int w = 960, int h = 540);

    // Updated show method to accept an optional placeholder
    void show(const std::string& placeholder = "Type here...", Callback onDone = nullptr,
              bool startInNumbers = false, const std::string& initialText = "");
    void hide();
    bool isVisible() const { return _visible; }
    void draw();
    void handleTouch(int x, int y);

    // Helper to get text
    std::string getText() const { return _text; }
    void clearText() { _text.clear(); }

private:
    void drawTextBox();
    void updateTextBox();
    void drawKeyboard();
    void drawKey(int x, int y, int w, int h, const String& label, uint16_t color = DARKGREY);

    void appendChar(char c);
    void backspace();
    void enter();
    void toggleShift();
    void toggleMode(); 

    int _x, _y, _w, _h;
    bool _visible = false;
    bool _needsRedraw = true;
    std::string _text;
    std::string _placeholder; // Added to store the dynamic placeholder
    Callback _onDone;

    bool _shift = false; 
    bool _capsLock = false; 
    bool _numbersMode = false; 

    const std::vector<std::string> _letterRows = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    const std::vector<std::string> _numberRows = {"1234567890", "-/:;()$&@", ".,?!'\""};
};

}
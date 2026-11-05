#include "SDExplorerApp.h"
#include "AppManager.h"
#include <M5Unified.h>
#include <lgfx/v1/misc/DataWrapper.hpp>  

enum class ExplorerState { FILE_BROWSER, IMAGE_VIEWER };
static ExplorerState currentState = ExplorerState::FILE_BROWSER;

class SDDataWrapper : public lgfx::v1::DataWrapper {
private:
    File _file;
public:
    SDDataWrapper(const char* path) {
        _file = SD.open(path);
    }
    ~SDDataWrapper() override {
        if (_file) _file.close();
    }

    // virtual methods
    int read(uint8_t *buf, uint32_t len) override {
        if (!_file) return 0;
        return _file.read(buf, len);
    }
    void skip(int32_t offset) override {
        if (_file) _file.seek(_file.position() + offset);
    }
    bool seek(uint32_t offset) override {
        if (!_file) return false;
        return _file.seek(offset);
    }
    void close(void) override {
        if (_file) _file.close();
    }
    int32_t tell(void) override {
        if (!_file) return 0;
        return _file.position();
    }

    // Helper: was the file opened?
    bool isValid() const { return _file; }
};

SDExplorerApp::SDExplorerApp(AppManager* manager)
    : appManager(manager),
      _needsFullRedraw(true)
{
}

void SDExplorerApp::setup() {
    // M5 and the SD card are initialised once in main.cpp
    folderHistory = {};
    folderHistoryForward = {};
    openDirectory("/");
}

void SDExplorerApp::update() {}

void SDExplorerApp::draw() {
    if (_needsFullRedraw) {
        M5.Display.clear();
        switch (currentState) {
            case ExplorerState::FILE_BROWSER: drawFileBrowser(); break;
            case ExplorerState::IMAGE_VIEWER:  drawImageViewer(); break;
        }
        _needsFullRedraw = false;
    }
}

void SDExplorerApp::drawFileBrowser() {
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextColor(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.drawString(("Path: " + currentPath).c_str(), 20, 20);
    M5.Display.drawLine(10, 530, 530, 530, BLACK);

    int y = 80;
    for (const auto& fi : fileList) {
        std::string txt = fi.isDirectory ? "[D] " + fi.name
                        : fi.isImage    ? "[IMG] " + fi.name
                                        : "[F] " + fi.name;
        M5.Display.drawString(txt.c_str(), 20, y);
        y += 35;
        if (y > 900) break;
    }
}

void SDExplorerApp::drawImageViewer() {
    if (!SD.exists(_currentImagePath.c_str())) {
        M5.Display.drawString("File not found!", 20, 20);
        Serial.printf("Error: %s missing\n", _currentImagePath.c_str());
        return;
    }

    const int w = M5.Display.width();
    const int h = M5.Display.height();

    std::string ext = _currentImagePath.substr(_currentImagePath.length() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Use custom wrapper to avoid M5GFX bug
    SDDataWrapper wrapper(_currentImagePath.c_str());
    if (!wrapper.isValid()) {
        M5.Display.drawString("Open failed!", 20, 20);
        Serial.printf("Error: Cannot open %s\n", _currentImagePath.c_str());
        return;
    }

    //slow down
    if (M5.Display.isEPD())
    {
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
    }

    M5.Display.startWrite();
    if (ext == ".jpg")
    {
        M5.Display.drawJpgFile(&wrapper, _currentImagePath.c_str(), 0, 0, w, h,
                               0, 0, 0.0f, 0.0f, middle_center);
    }
    else if (ext == ".png")
    {
        M5.Display.drawPngFile(&wrapper, _currentImagePath.c_str(), 0, 0, w, h,
                               0, 0, 0.0f, 0.0f, middle_center);
    }
    else if (ext == ".bmp")
    {
        M5.Display.drawBmpFile(&wrapper, _currentImagePath.c_str(), 0, 0, w, h,
                               0, 0, 0.0f, 0.0f, middle_center);
    }

    M5.Display.endWrite();
    // Push the image now: the EPD mode is applied when display() runs, and
    // the frame batch in AppManager would otherwise flush it in fast mode.
    M5.Display.display();

    //speed up
    if (M5.Display.isEPD()) {
        M5.Display.setEpdMode(epd_mode_t::epd_fast);
    }
}

/* ────────────────────────────── touch ──────────────────────────── */
void SDExplorerApp::handleTouch(int x, int y) {
    switch (currentState) {
        case ExplorerState::FILE_BROWSER: {
            if (y < 80 || y >= 900) break;
            int idx = (y - 80) / 35;
            if (idx < 0 || idx >= (int)fileList.size()) break;
            const FileInfo& fi = fileList[idx];

            if (fi.isDirectory) {
                if (fi.name == "..") { handleBackPress(); break; }
                folderHistory.push(currentPath);
                folderHistoryForward = {};
                std::string np = currentPath;
                if (np != "/") np += "/";
                np += fi.name;
                openDirectory(np);
            } else if (fi.isImage) {
                std::string ip = currentPath;
                if (ip != "/") ip += "/";
                ip += fi.name;
                showImage(ip);
            }
            break;
        }

        case ExplorerState::IMAGE_VIEWER: {
            // Tap anywhere → go back
            handleBackPress();
            break;
        }
    }
}

bool SDExplorerApp::handleBackPress() {
    if (currentState == ExplorerState::IMAGE_VIEWER) {
        currentState = ExplorerState::FILE_BROWSER;
        _needsFullRedraw = true;
        return true;
    }
    if (!folderHistory.empty()) {
        folderHistoryForward.push(currentPath);
        openDirectory(folderHistory.top());
        folderHistory.pop();
        return true;
    }
    return false;
}

bool SDExplorerApp::handleForwardPress() {
    if (currentState == ExplorerState::IMAGE_VIEWER || folderHistoryForward.empty())
        return false;
    folderHistory.push(currentPath);
    openDirectory(folderHistoryForward.top());
    folderHistoryForward.pop();
    return true;
}

void SDExplorerApp::openDirectory(const std::string& path) {
    currentState = ExplorerState::FILE_BROWSER;
    currentPath = path;
    fileList.clear();
    _needsFullRedraw = true;

    if (currentDirectory) currentDirectory.close();
    currentDirectory = SD.open(path.c_str());
    if (!currentDirectory.isDirectory()) {
        fileList.push_back({"<Cannot open>", false, false});
        return;
    }

    if (path != "/") fileList.push_back({"..", true, false});

    currentDirectory.rewindDirectory();
    File f;
    while (f = currentDirectory.openNextFile()) {
        const char* n = f.name();
        if (n && n[0] != '.') {
            std::string full(n);
            size_t slash = full.find_last_of('/');
            std::string name = full.substr(slash + 1);

            bool img = false;
            if (name.length() > 4) {
                std::string e = name.substr(name.length() - 4);
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                img = (e == ".jpg" || e == ".bmp" || e == ".png");
            }
            fileList.push_back({name, f.isDirectory(), img});
        }
        f.close();
    }

    if (path != "/" && fileList.size() == 1)
        fileList.push_back({"<Empty>", false, false});
    else if (path == "/" && fileList.empty())
        fileList.push_back({"<Empty>", false, false});
}

void SDExplorerApp::showImage(const std::string& imagePath) {
    currentState = ExplorerState::IMAGE_VIEWER;
    _currentImagePath = imagePath;
    _needsFullRedraw = true;
}
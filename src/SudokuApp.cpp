#include "SudokuApp.h"
#include "AppManager.h"
#include "Storage.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>
#include <cstdio>
#include <utility>

static const int SCREEN_W = 540;

// Main menu
static const int MENU_BTN_X = 120, MENU_BTN_W = 300, MENU_BTN_H = 80;
static const int MENU_NEWGAME_Y = 160;
static const int MENU_CONTINUE_Y = 280;
static const int MENU_PUZZLES_Y = 400;

// Difficulty select
static const int DIFF_BTN_Y0 = 160, DIFF_BTN_GAP = 120;

// Grid
static const int CELL_SIZE = 54;
static const int GRID_SIZE = CELL_SIZE * 9;
static const int GRID_X = (SCREEN_W - GRID_SIZE) / 2;
static const int GRID_Y = 60;

// Number keypad
static const int NUM_BTN_W = 54, NUM_BTN_H = 70, NUM_GAP = 4;
static const int NUM_START_X = (SCREEN_W - (9 * NUM_BTN_W + 8 * NUM_GAP)) / 2;
static const int NUMROW_Y = GRID_Y + GRID_SIZE + 20;
static const int ERASE_W = 200, ERASE_H = 60;
static const int ERASE_X = (SCREEN_W - ERASE_W) / 2;
static const int ERASE_Y = NUMROW_Y + NUM_BTN_H + 20;

// Check, Cheat, Reveal row
static const int ACTION_BTN_Y = ERASE_Y + ERASE_H + 20;
static const int ACTION_BTN_H = 60;
static const int CHECK_BTN_X = 60, CHECK_BTN_W = 190;
static const int CHEAT_BTN_X = 290, CHEAT_BTN_W = 190;

// Puzzle list
static const int PUZZLES_PER_PAGE = 6;
static const int ROW_START_Y = 90;
static const int ROW_HEIGHT = 110;
static const int LIST_NAV_Y = 850, LIST_NAV_H = 50;

static const char *SUDOKU_DIR = "/sudoku";
static const char *SUDOKU_FILE = "/sudoku/puzzles.json";

static bool isSafe(int grid[9][9], int row, int col, int num) {
  for (int i = 0; i < 9; i++) {
    if (grid[row][i] == num || grid[i][col] == num)
      return false;
  }
  int br = row - row % 3, bc = col - col % 3;
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++)
      if (grid[br + r][bc + c] == num)
        return false;
  return true;
}

static bool findMRVCell(int grid[9][9], int &outRow, int &outCol) {
  int minCount = 10;
  outRow = -1;
  outCol = -1;
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      if (grid[r][c] != 0)
        continue;
      int count = 0;
      for (int num = 1; num <= 9; num++)
        if (isSafe(grid, r, c, num))
          count++;
      if (count < minCount) {
        minCount = count;
        outRow = r;
        outCol = c;
        if (minCount == 0)
          return true; // dead end, stop early
      }
    }
  }
  return outRow != -1;
}

static int countSolutions(int grid[9][9], int limit) {
  int row, col;
  if (!findMRVCell(grid, row, col))
    return 1; // no empty cells: one solution

  int found = 0;
  for (int num = 1; num <= 9 && found < limit; num++) {
    if (isSafe(grid, row, col, num)) {
      grid[row][col] = num;
      found += countSolutions(grid, limit - found);
      grid[row][col] = 0;
    }
  }
  return found;
}

static void generateSolvedGrid(int outGrid[9][9]) {
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      outGrid[r][c] = ((r * 3 + r / 3 + c) % 9) + 1;

  int digitMap[10];
  for (int i = 1; i <= 9; i++)
    digitMap[i] = i;
  for (int i = 9; i > 1; i--) {
    int j = random(1, i + 1);
    std::swap(digitMap[i], digitMap[j]);
  }
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      outGrid[r][c] = digitMap[outGrid[r][c]];

  int rowOrder[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  for (int band = 0; band < 3; band++) {
    for (int i = 2; i > 0; i--) {
      int j = random(0, i + 1);
      std::swap(rowOrder[band * 3 + i], rowOrder[band * 3 + j]);
    }
  }
  int bandOrder[3] = {0, 1, 2};
  for (int i = 2; i > 0; i--) {
    int j = random(0, i + 1);
    std::swap(bandOrder[i], bandOrder[j]);
  }
  int finalRowOrder[9];
  for (int b = 0; b < 3; b++)
    for (int i = 0; i < 3; i++)
      finalRowOrder[b * 3 + i] = rowOrder[bandOrder[b] * 3 + i];

  int colOrder[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  for (int stack = 0; stack < 3; stack++) {
    for (int i = 2; i > 0; i--) {
      int j = random(0, i + 1);
      std::swap(colOrder[stack * 3 + i], colOrder[stack * 3 + j]);
    }
  }
  int stackOrder[3] = {0, 1, 2};
  for (int i = 2; i > 0; i--) {
    int j = random(0, i + 1);
    std::swap(stackOrder[i], stackOrder[j]);
  }
  int finalColOrder[9];
  for (int s = 0; s < 3; s++)
    for (int i = 0; i < 3; i++)
      finalColOrder[s * 3 + i] = colOrder[stackOrder[s] * 3 + i];

  int temp[9][9];
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      temp[r][c] = outGrid[finalRowOrder[r]][finalColOrder[c]];

  bool doTranspose = random(0, 2) == 1;
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      outGrid[r][c] = doTranspose ? temp[c][r] : temp[r][c];
}

static void createPuzzleFromSolution(const int solved[9][9], int clueCount,
                                     int outPuzzle[9][9]) {
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      outPuzzle[r][c] = solved[r][c];

  int cellsToRemove = 81 - clueCount;

  int order[81];
  for (int i = 0; i < 81; i++)
    order[i] = i;
  for (int i = 80; i > 0; i--) {
    int j = random(0, i + 1);
    std::swap(order[i], order[j]);
  }

  int removed = 0;
  int workGrid[9][9];
  for (int idx = 0; idx < 81 && removed < cellsToRemove; idx++) {
    int r = order[idx] / 9;
    int c = order[idx] % 9;
    int backup = outPuzzle[r][c];
    if (backup == 0)
      continue;

    outPuzzle[r][c] = 0;
    for (int rr = 0; rr < 9; rr++)
      for (int cc = 0; cc < 9; cc++)
        workGrid[rr][cc] = outPuzzle[rr][cc];

    if (countSolutions(workGrid, 2) == 1) {
      removed++;
    } else {
      outPuzzle[r][c] = backup;
    }
  }
}

static std::string gridToString(const int g[9][9]) {
  std::string s;
  s.reserve(81);
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      s += char('0' + g[r][c]);
  return s;
}

static void stringToGrid(const std::string &s, int g[9][9]) {
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      int idx = r * 9 + c;
      g[r][c] = (idx < (int)s.length() && s[idx] >= '0' && s[idx] <= '9')
                    ? (s[idx] - '0')
                    : 0;
    }
  }
}

SudokuApp::SudokuApp(AppManager *manager)
    : appManager(manager), _screen(Screen::MAIN_MENU), _puzzles(), _nextId(1),
      _activePuzzleIndex(-1), _grid{}, _isGiven{}, _errorCells{}, _selRow(-1),
      _selCol(-1), _checkMessage(), _cheatRevealed(false),
      _forceFullRedraw(false), _sessionStartMillis(0), _accumulatedSeconds(0),
      _puzzleListPage(0), _newGameBtn(MENU_BTN_X, MENU_NEWGAME_Y, MENU_BTN_W,
                                      MENU_BTN_H, "New Game"),
      _continueBtn(MENU_BTN_X, MENU_CONTINUE_Y, MENU_BTN_W, MENU_BTN_H,
                   "Continue"),
      _myPuzzlesBtn(MENU_BTN_X, MENU_PUZZLES_Y, MENU_BTN_W, MENU_BTN_H,
                    "My Puzzles"),
      _easyBtn(MENU_BTN_X, DIFF_BTN_Y0, MENU_BTN_W, MENU_BTN_H, "Easy"),
      _mediumBtn(MENU_BTN_X, DIFF_BTN_Y0 + DIFF_BTN_GAP, MENU_BTN_W, MENU_BTN_H,
                 "Medium"),
      _hardBtn(MENU_BTN_X, DIFF_BTN_Y0 + 2 * DIFF_BTN_GAP, MENU_BTN_W,
               MENU_BTN_H, "Hard"),
      _expertBtn(MENU_BTN_X, DIFF_BTN_Y0 + 3 * DIFF_BTN_GAP, MENU_BTN_W,
                 MENU_BTN_H, "Expert"),
      _puzzlePrevBtn(40, LIST_NAV_Y, 140, LIST_NAV_H, "< Prev"),
      _puzzleNextBtn(360, LIST_NAV_Y, 140, LIST_NAV_H, "Next >"), _numButtons(),
      _eraseBtn(ERASE_X, ERASE_Y, ERASE_W, ERASE_H, "Erase"),
      _checkBtn(CHECK_BTN_X, ACTION_BTN_Y, CHECK_BTN_W, ACTION_BTN_H, "Check"),
      _cheatBtn(CHEAT_BTN_X, ACTION_BTN_Y, CHEAT_BTN_W, ACTION_BTN_H, "Cheat"),
      _revealBtn(CHEAT_BTN_X, ACTION_BTN_Y, CHEAT_BTN_W, ACTION_BTN_H,
                 "Reveal"),
      _completeNewGameBtn(MENU_BTN_X, 500, MENU_BTN_W, MENU_BTN_H, "New Game"),
      _completeMenuBtn(MENU_BTN_X, 620, MENU_BTN_W, MENU_BTN_H, "Menu") {
  for (int n = 1; n <= 9; n++) {
    int bx = NUM_START_X + (n - 1) * (NUM_BTN_W + NUM_GAP);
    _numButtons.emplace_back(bx, NUMROW_Y, NUM_BTN_W, NUM_BTN_H,
                             std::to_string(n));
  }
}

void SudokuApp::setup() {
  loadPuzzles();
  _screen = Screen::MAIN_MENU;
  _activePuzzleIndex = -1;
  _selRow = -1;
  _selCol = -1;
  _puzzleListPage = 0;
  _cheatRevealed = false;
  _checkMessage.clear();
  _forceFullRedraw = false;
}

bool SudokuApp::needsRedraw() {
  if (_forceFullRedraw) {
    _forceFullRedraw = false;
    return true;
  }
  return false;
}

bool SudokuApp::wantsTouchFeedback(int x, int y) {
  return _screen != Screen::PLAYING;
}

void SudokuApp::exit() {
  if (_screen == Screen::PLAYING) {
    pauseAndPersist();
  }
}

void SudokuApp::update() {}

void SudokuApp::draw() {
  M5.Display.clear();
  switch (_screen) {
  case Screen::MAIN_MENU:
    drawMainMenu();
    break;
  case Screen::DIFFICULTY_SELECT:
    drawDifficultySelect();
    break;
  case Screen::PUZZLE_LIST:
    drawPuzzleList();
    break;
  case Screen::PLAYING:
    drawPlaying();
    break;
  case Screen::COMPLETE:
    drawComplete();
    break;
  }
}

bool SudokuApp::handleBackPress() {
  if (_screen == Screen::PLAYING) {
    pauseAndPersist();
    _screen = Screen::MAIN_MENU;
    return true;
  }
  if (_screen == Screen::COMPLETE || _screen == Screen::DIFFICULTY_SELECT ||
      _screen == Screen::PUZZLE_LIST) {
    _screen = Screen::MAIN_MENU;
    return true;
  }
  return false;
}

void SudokuApp::handleTouch(int x, int y) {
  switch (_screen) {
  case Screen::MAIN_MENU:
    handleMainMenuTouch(x, y);
    break;
  case Screen::DIFFICULTY_SELECT:
    handleDifficultySelectTouch(x, y);
    break;
  case Screen::PUZZLE_LIST:
    handlePuzzleListTouch(x, y);
    break;
  case Screen::PLAYING:
    handlePlayingTouch(x, y);
    break;
  case Screen::COMPLETE:
    handleCompleteTouch(x, y);
    break;
  }
}

bool SudokuApp::ensureSudokuFile() {
  if (!SD.exists(SUDOKU_DIR)) {
    SD.mkdir(SUDOKU_DIR);
  }
  if (!SD.exists(SUDOKU_FILE)) {
    File f = SD.open(SUDOKU_FILE, FILE_WRITE);
    if (!f)
      return false;
    f.print("{\"nextId\":1,\"puzzles\":[]}");
    f.close();
  }
  return true;
}

bool SudokuApp::loadPuzzles() {
  _puzzles.clear();
  _nextId = 1;

  if (!ensureSudokuFile())
    return false;

  PsramJsonDocument doc(65536);
  if (!storage::readJson(SUDOKU_FILE, doc))
    return false;

  _nextId = doc["nextId"] | 1;
  JsonArray arr = doc["puzzles"].as<JsonArray>();
  for (JsonObject o : arr) {
    PuzzleRecord rec;
    rec.id = o["id"] | 0;
    rec.difficulty = (const char *)(o["difficulty"] | "Medium");
    rec.given = (const char *)(o["given"] | "");
    rec.solution = (const char *)(o["solution"] | "");
    rec.state = (const char *)(o["state"] | "");
    rec.complete = o["complete"] | false;
    rec.elapsedSeconds = o["elapsedSeconds"] | 0;
    rec.completedAt = (const char *)(o["completedAt"] | "");
    rec.createdAt = (const char *)(o["createdAt"] | "");

    if (rec.given.length() == 81 && rec.solution.length() == 81) {
      if (rec.state.length() != 81)
        rec.state = rec.given;
      _puzzles.push_back(rec);
    }
  }
  return true;
}

bool SudokuApp::savePuzzles() {
  PsramJsonDocument doc(65536);
  doc["nextId"] = _nextId;
  JsonArray arr = doc.createNestedArray("puzzles");
  for (const auto &rec : _puzzles) {
    JsonObject o = arr.createNestedObject();
    o["id"] = rec.id;
    o["difficulty"] = rec.difficulty;
    o["given"] = rec.given;
    o["solution"] = rec.solution;
    o["state"] = rec.state;
    o["complete"] = rec.complete;
    o["elapsedSeconds"] = rec.elapsedSeconds;
    o["completedAt"] = rec.completedAt;
    o["createdAt"] = rec.createdAt;
  }

  return storage::writeJsonAtomic(SUDOKU_FILE, doc);
}

int SudokuApp::cluesForDifficulty(Difficulty d) {
  switch (d) {
  case Difficulty::EASY:
    return 42;
  case Difficulty::MEDIUM:
    return 34;
  case Difficulty::HARD:
    return 28;
  case Difficulty::EXPERT:
    return 24;
  }
  return 34;
}

std::string SudokuApp::difficultyName(Difficulty d) {
  switch (d) {
  case Difficulty::EASY:
    return "Easy";
  case Difficulty::MEDIUM:
    return "Medium";
  case Difficulty::HARD:
    return "Hard";
  case Difficulty::EXPERT:
    return "Expert";
  }
  return "Medium";
}

std::string SudokuApp::formatTime(unsigned long seconds) {
  unsigned long h = seconds / 3600;
  unsigned long m = (seconds % 3600) / 60;
  unsigned long s = seconds % 60;
  char buf[16];
  if (h > 0)
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", h, m, s);
  else
    snprintf(buf, sizeof(buf), "%02lu:%02lu", m, s);
  return std::string(buf);
}

std::string SudokuApp::nowTimestamp() {
  m5::rtc_date_t date;
  M5.Rtc.getDate(&date);
  m5::rtc_time_t time;
  M5.Rtc.getTime(&time);
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", date.year, date.month,
           date.date, time.hours, time.minutes);
  return std::string(buf);
}

int SudokuApp::findMostRecentInProgress() const {
  for (int i = (int)_puzzles.size() - 1; i >= 0; i--) {
    if (!_puzzles[i].complete)
      return i;
  }
  return -1;
}

void SudokuApp::loadActiveIntoGrid() {
  const auto &rec = _puzzles[_activePuzzleIndex];
  int givenGrid[9][9];
  stringToGrid(rec.given, givenGrid);
  stringToGrid(rec.state, _grid);
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      _isGiven[r][c] = (givenGrid[r][c] != 0);
      _errorCells[r][c] = false;
    }
  }
  _checkMessage.clear();
  _cheatRevealed = false;
}

void SudokuApp::startNewGame(Difficulty diff) {
  M5.Display.clear();
  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.drawCentreString("Generating puzzle...", SCREEN_W / 2, 460);
  M5.Display.display();

  int solved[9][9];
  std::string solvedStr;
  const int MAX_ATTEMPTS = 10;
  for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
    generateSolvedGrid(solved);
    solvedStr = gridToString(solved);
    bool duplicate = false;
    for (const auto &p : _puzzles) {
      if (p.solution == solvedStr) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
      break;
  }

  int puzzleGrid[9][9];
  createPuzzleFromSolution(solved, cluesForDifficulty(diff), puzzleGrid);

  PuzzleRecord rec;
  rec.id = _nextId++;
  rec.difficulty = difficultyName(diff);
  rec.given = gridToString(puzzleGrid);
  rec.solution = solvedStr;
  rec.state = rec.given;
  rec.complete = false;
  rec.elapsedSeconds = 0;
  rec.completedAt = "";
  rec.createdAt = nowTimestamp();

  _puzzles.push_back(rec);
  savePuzzles();

  _activePuzzleIndex = (int)_puzzles.size() - 1;
  loadActiveIntoGrid();
  _sessionStartMillis = millis();
  _accumulatedSeconds = 0;
  _selRow = -1;
  _selCol = -1;
  _screen = Screen::PLAYING;
}

void SudokuApp::resumePuzzle(int index) {
  _activePuzzleIndex = index;
  loadActiveIntoGrid();
  _accumulatedSeconds = _puzzles[index].elapsedSeconds;
  _sessionStartMillis = millis();
  _selRow = -1;
  _selCol = -1;
  _screen = _puzzles[index].complete ? Screen::COMPLETE : Screen::PLAYING;
}

void SudokuApp::checkCompletion() {
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      if (_grid[r][c] == 0)
        return;

  std::string current = gridToString(_grid);
  auto &rec = _puzzles[_activePuzzleIndex];
  if (current != rec.solution)
    return;

  rec.elapsedSeconds =
      _accumulatedSeconds + (millis() - _sessionStartMillis) / 1000;
  rec.complete = true;
  rec.completedAt = nowTimestamp();
  rec.state = current;
  savePuzzles();
  _screen = Screen::COMPLETE;
  _forceFullRedraw = true;
}

void SudokuApp::pauseAndPersist() {
  if (_activePuzzleIndex < 0)
    return;
  auto &rec = _puzzles[_activePuzzleIndex];
  if (rec.complete)
    return;
  rec.elapsedSeconds =
      _accumulatedSeconds + (millis() - _sessionStartMillis) / 1000;
  rec.state = gridToString(_grid);
  savePuzzles();
}

void SudokuApp::setCellValue(int r, int c, int value) {
  _grid[r][c] = value;
  _errorCells[r][c] = false;
  _puzzles[_activePuzzleIndex].state = gridToString(_grid);

  bool hadMessage = !_checkMessage.empty();
  _checkMessage.clear();

  redrawCell(r, c);
  if (hadMessage) {
    drawHeader();
    M5.Display.display(0, 0, SCREEN_W, GRID_Y);
  }

  checkCompletion();
}

void SudokuApp::runCheck() {
  if (_activePuzzleIndex < 0)
    return;
  const auto &rec = _puzzles[_activePuzzleIndex];

  int mistakes = 0;
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      int idx = r * 9 + c;
      bool wrong = (_grid[r][c] != 0 && !_isGiven[r][c] &&
                    (char)('0' + _grid[r][c]) != rec.solution[idx]);
      _errorCells[r][c] = wrong;
      if (wrong)
        mistakes++;
    }
  }

  _checkMessage =
      (mistakes == 0)
          ? "No mistakes so far"
          : (std::to_string(mistakes) +
             (mistakes == 1 ? " mistake found" : " mistakes found"));

  drawHeader();
  drawGrid();
  M5.Display.display(0, 0, SCREEN_W, GRID_Y + GRID_SIZE);
}

void SudokuApp::revealSelectedCell() {
  if (_activePuzzleIndex < 0 || _selRow < 0 || _isGiven[_selRow][_selCol])
    return;
  const auto &rec = _puzzles[_activePuzzleIndex];
  int idx = _selRow * 9 + _selCol;
  setCellValue(_selRow, _selCol, rec.solution[idx] - '0');
}

void SudokuApp::cancelCheatArming() {
  if (!_cheatRevealed)
    return;
  _cheatRevealed = false;
  redrawCheatButton();
}

void SudokuApp::drawMainMenu() {
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.drawCentreString("Sudoku", SCREEN_W / 2, 60);

  _newGameBtn.draw();
  _continueBtn.draw();
  _myPuzzlesBtn.draw();

  if (findMostRecentInProgress() < 0) {
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextColor(DARKGREY);
    M5.Display.drawCentreString("(no puzzle in progress)", SCREEN_W / 2,
                                MENU_CONTINUE_Y + MENU_BTN_H + 10);
  }
}

void SudokuApp::drawDifficultySelect() {
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.drawCentreString("Select Difficulty", SCREEN_W / 2, 60);

  _easyBtn.draw();
  _mediumBtn.draw();
  _hardBtn.draw();
  _expertBtn.draw();
}

void SudokuApp::drawPuzzleList() {
  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.drawCentreString("My Puzzles", SCREEN_W / 2, 25);

  if (_puzzles.empty()) {
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawCentreString("No saved puzzles yet", SCREEN_W / 2, 400);
    return;
  }

  int total = (int)_puzzles.size();
  int startIdx = total - 1 - _puzzleListPage * PUZZLES_PER_PAGE;

  for (int i = 0; i < PUZZLES_PER_PAGE; i++) {
    int idx = startIdx - i;
    if (idx < 0)
      break;
    const auto &rec = _puzzles[idx];
    int rowY = ROW_START_Y + i * ROW_HEIGHT;

    M5.Display.drawRect(20, rowY, 500, ROW_HEIGHT - 10, BLACK);

    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    std::string line1 = "#" + std::to_string(rec.id) + "  " + rec.difficulty;
    M5.Display.drawString(line1.c_str(), 32, rowY + 10);

    M5.Display.setFont(&fonts::FreeSans9pt7b);
    std::string status = (rec.complete ? "Completed - " : "In progress - ") +
                         formatTime(rec.elapsedSeconds);
    M5.Display.drawString(status.c_str(), 32, rowY + 48);

    std::string dateStr = rec.complete ? rec.completedAt : rec.createdAt;
    M5.Display.drawRightString(dateStr.c_str(), 505, rowY + 48);
  }

  int totalPages = (total + PUZZLES_PER_PAGE - 1) / PUZZLES_PER_PAGE;
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  std::string pageLabel = "Page " + std::to_string(_puzzleListPage + 1) + "/" +
                          std::to_string(totalPages);
  M5.Display.drawCentreString(pageLabel.c_str(), SCREEN_W / 2, 825);

  _puzzlePrevBtn.draw();
  _puzzleNextBtn.draw();
}

void SudokuApp::drawCellContents(int r, int c) {
  int cx = GRID_X + c * CELL_SIZE;
  int cy = GRID_Y + r * CELL_SIZE;
  bool selected = (r == _selRow && c == _selCol);

  M5.Display.fillRect(cx, cy, CELL_SIZE, CELL_SIZE,
                      selected ? LIGHTGREY : WHITE);

  int topThick = (r % 3 == 0) ? 3 : 2;
  int botThick = ((r + 1) % 3 == 0) ? 3 : 2;
  int leftThick = (c % 3 == 0) ? 3 : 2;
  int rightThick = ((c + 1) % 3 == 0) ? 3 : 2;

  M5.Display.fillRect(cx, cy - topThick / 2, CELL_SIZE, topThick, BLACK);
  M5.Display.fillRect(cx, cy + CELL_SIZE - botThick / 2, CELL_SIZE, botThick,
                      BLACK);
  M5.Display.fillRect(cx - leftThick / 2, cy, leftThick, CELL_SIZE, BLACK);
  M5.Display.fillRect(cx + CELL_SIZE - rightThick / 2, cy, rightThick,
                      CELL_SIZE, BLACK);

  int val = _grid[r][c];
  if (val != 0) {
    M5.Display.setTextColor(BLACK);
    M5.Display.setFont(_isGiven[r][c] ? &fonts::FreeSansBold18pt7b
                                      : &fonts::FreeSans18pt7b);
    char buf[2] = {char('0' + val), 0};
    M5.Display.drawCentreString(buf, cx + CELL_SIZE / 2,
                                cy + CELL_SIZE / 2 - 14);
  }

  if (_errorCells[r][c]) {
    M5.Display.fillCircle(cx + 10, cy + 10, 4, BLACK);
  }
}

void SudokuApp::drawGrid() {
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++)
      drawCellContents(r, c);
}

void SudokuApp::redrawCell(int r, int c) {
  drawCellContents(r, c);
  int cx = GRID_X + c * CELL_SIZE;
  int cy = GRID_Y + r * CELL_SIZE;
  M5.Display.display(cx - 2, cy - 2, CELL_SIZE + 4, CELL_SIZE + 4);
}

void SudokuApp::redrawSelection(int newRow, int newCol) {
  int oldRow = _selRow, oldCol = _selCol;
  _selRow = newRow;
  _selCol = newCol;
  if (oldRow >= 0)
    redrawCell(oldRow, oldCol);
  redrawCell(newRow, newCol);
}

void SudokuApp::redrawCheatButton() {
  if (_cheatRevealed)
    _revealBtn.draw();
  else
    _cheatBtn.draw();
  M5.Display.display(CHEAT_BTN_X, ACTION_BTN_Y, CHEAT_BTN_W, ACTION_BTN_H);
}

void SudokuApp::drawKeypad() {
  for (auto &btn : _numButtons)
    btn.draw();
  _eraseBtn.draw();
  _checkBtn.draw();
  if (_cheatRevealed)
    _revealBtn.draw();
  else
    _cheatBtn.draw();
}

void SudokuApp::drawHeader() {
  if (_activePuzzleIndex < 0)
    return;
  const auto &rec = _puzzles[_activePuzzleIndex];

  M5.Display.fillRect(0, 0, SCREEN_W, GRID_Y, WHITE);

  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.drawString(rec.difficulty.c_str(), 20, 15);

  unsigned long elapsed =
      _accumulatedSeconds + (millis() - _sessionStartMillis) / 1000;
  M5.Display.drawRightString(formatTime(elapsed).c_str(), 520, 15);

  if (!_checkMessage.empty()) {
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawCentreString(_checkMessage.c_str(), SCREEN_W / 2, 40);
  }
}

void SudokuApp::drawPlaying() {
  if (_activePuzzleIndex < 0)
    return;
  drawHeader();
  drawGrid();
  drawKeypad();
}

void SudokuApp::drawComplete() {
  if (_activePuzzleIndex < 0)
    return;
  const auto &rec = _puzzles[_activePuzzleIndex];

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextColor(BLACK);
  M5.Display.drawCentreString("Solved!", SCREEN_W / 2, 150);

  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  std::string diffLine = rec.difficulty + " puzzle";
  M5.Display.drawCentreString(diffLine.c_str(), SCREEN_W / 2, 260);

  std::string timeLine = "Time: " + formatTime(rec.elapsedSeconds);
  M5.Display.drawCentreString(timeLine.c_str(), SCREEN_W / 2, 330);

  _completeNewGameBtn.draw();
  _completeMenuBtn.draw();
}

void SudokuApp::handleMainMenuTouch(int x, int y) {
  if (_newGameBtn.isPressed(x, y)) {
    _screen = Screen::DIFFICULTY_SELECT;
    return;
  }
  if (_continueBtn.isPressed(x, y)) {
    int idx = findMostRecentInProgress();
    if (idx >= 0)
      resumePuzzle(idx);
    return;
  }
  if (_myPuzzlesBtn.isPressed(x, y)) {
    _puzzleListPage = 0;
    _screen = Screen::PUZZLE_LIST;
    return;
  }
}

void SudokuApp::handleDifficultySelectTouch(int x, int y) {
  if (_easyBtn.isPressed(x, y)) {
    startNewGame(Difficulty::EASY);
    return;
  }
  if (_mediumBtn.isPressed(x, y)) {
    startNewGame(Difficulty::MEDIUM);
    return;
  }
  if (_hardBtn.isPressed(x, y)) {
    startNewGame(Difficulty::HARD);
    return;
  }
  if (_expertBtn.isPressed(x, y)) {
    startNewGame(Difficulty::EXPERT);
    return;
  }
}

void SudokuApp::handlePuzzleListTouch(int x, int y) {
  if (_puzzlePrevBtn.isPressed(x, y)) {
    if (_puzzleListPage > 0)
      _puzzleListPage--;
    return;
  }
  if (_puzzleNextBtn.isPressed(x, y)) {
    int totalPages =
        ((int)_puzzles.size() + PUZZLES_PER_PAGE - 1) / PUZZLES_PER_PAGE;
    if (_puzzleListPage < totalPages - 1)
      _puzzleListPage++;
    return;
  }

  int total = (int)_puzzles.size();
  int startIdx = total - 1 - _puzzleListPage * PUZZLES_PER_PAGE;
  for (int i = 0; i < PUZZLES_PER_PAGE; i++) {
    int idx = startIdx - i;
    if (idx < 0)
      break;
    int rowY = ROW_START_Y + i * ROW_HEIGHT;
    if (x >= 20 && x <= 520 && y >= rowY && y <= rowY + ROW_HEIGHT - 10) {
      resumePuzzle(idx);
      return;
    }
  }
}

void SudokuApp::handlePlayingTouch(int x, int y) {
  if (x >= GRID_X && x < GRID_X + GRID_SIZE && y >= GRID_Y &&
      y < GRID_Y + GRID_SIZE) {
    cancelCheatArming();
    int newRow = (y - GRID_Y) / CELL_SIZE;
    int newCol = (x - GRID_X) / CELL_SIZE;
    if (newRow != _selRow || newCol != _selCol) {
      redrawSelection(newRow, newCol);
    }
    return;
  }

  for (int n = 1; n <= 9; n++) {
    if (_numButtons[n - 1].isPressed(x, y)) {
      cancelCheatArming();
      if (_selRow >= 0 && !_isGiven[_selRow][_selCol]) {
        setCellValue(_selRow, _selCol, n);
      }
      return;
    }
  }

  if (_eraseBtn.isPressed(x, y)) {
    cancelCheatArming();
    if (_selRow >= 0 && !_isGiven[_selRow][_selCol]) {
      setCellValue(_selRow, _selCol, 0);
    }
    return;
  }

  if (_checkBtn.isPressed(x, y)) {
    cancelCheatArming();
    runCheck();
    return;
  }

  if (!_cheatRevealed) {
    if (_cheatBtn.isPressed(x, y)) {
      _cheatRevealed = true;
      redrawCheatButton();
    }
    return;
  }

  if (_revealBtn.isPressed(x, y)) {
    revealSelectedCell();
    _cheatRevealed = false;
    redrawCheatButton();
    return;
  }

  cancelCheatArming();
}

void SudokuApp::handleCompleteTouch(int x, int y) {
  if (_completeNewGameBtn.isPressed(x, y)) {
    _screen = Screen::DIFFICULTY_SELECT;
    return;
  }
  if (_completeMenuBtn.isPressed(x, y)) {
    _screen = Screen::MAIN_MENU;
    return;
  }
}

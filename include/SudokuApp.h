#pragma once
#include "App.h"
#include "ui/LabelButton.h"
#include <M5Unified.h>
#include <string>
#include <vector>

class AppManager;

class SudokuApp : public App {
public:
  SudokuApp(AppManager *manager);

  void setup() override;
  void update() override;
  void draw() override;
  void handleTouch(int x, int y) override;
  bool handleBackPress() override;
  void exit() override;
  bool needsRedraw() override;
  bool wantsTouchFeedback(int x, int y) override;

private:
  enum class Difficulty { EASY, MEDIUM, HARD, EXPERT };
  enum class Screen {
    MAIN_MENU,
    DIFFICULTY_SELECT,
    PUZZLE_LIST,
    PLAYING,
    COMPLETE
  };

  struct PuzzleRecord {
    int id = 0;
    std::string difficulty = "Medium";
    std::string given;
    std::string solution;
    std::string state;
    bool complete = false;
    unsigned long elapsedSeconds = 0;
    std::string completedAt;
    std::string createdAt;
  };

  AppManager *appManager;

  Screen _screen;
  std::vector<PuzzleRecord> _puzzles;
  int _nextId;

  int _activePuzzleIndex;
  int _grid[9][9];
  bool _isGiven[9][9];
  bool _errorCells[9][9];
  int _selRow, _selCol;
  std::string _checkMessage;
  bool _cheatRevealed;
  bool _forceFullRedraw;

  unsigned long _sessionStartMillis;
  unsigned long _accumulatedSeconds;

  int _puzzleListPage;

  ui::LabelButton _newGameBtn;
  ui::LabelButton _continueBtn;
  ui::LabelButton _myPuzzlesBtn;

  ui::LabelButton _easyBtn;
  ui::LabelButton _mediumBtn;
  ui::LabelButton _hardBtn;
  ui::LabelButton _expertBtn;

  ui::LabelButton _puzzlePrevBtn;
  ui::LabelButton _puzzleNextBtn;

  std::vector<ui::LabelButton> _numButtons;
  ui::LabelButton _eraseBtn;
  ui::LabelButton _checkBtn;
  ui::LabelButton _cheatBtn;
  ui::LabelButton _revealBtn;

  ui::LabelButton _completeNewGameBtn;
  ui::LabelButton _completeMenuBtn;

  // /sudoku/puzzles.json
  bool ensureSudokuFile();
  bool loadPuzzles();
  bool savePuzzles();

  static int cluesForDifficulty(Difficulty d);
  static std::string difficultyName(Difficulty d);
  static std::string formatTime(unsigned long seconds);
  static std::string nowTimestamp();

  int findMostRecentInProgress() const;
  void loadActiveIntoGrid();
  void startNewGame(Difficulty diff);
  void resumePuzzle(int index);
  void checkCompletion();
  void pauseAndPersist();
  void setCellValue(int r, int c, int value);
  void runCheck();
  void revealSelectedCell();
  void cancelCheatArming();

  // Drawing
  void drawMainMenu();
  void drawDifficultySelect();
  void drawPuzzleList();
  void drawPlaying();
  void drawComplete();
  void drawHeader();
  void drawGrid();
  void drawCellContents(int r, int c);
  void redrawCell(int r, int c);
  void redrawSelection(int newRow, int newCol);
  void redrawCheatButton();
  void drawKeypad();

  // Touch
  void handleMainMenuTouch(int x, int y);
  void handleDifficultySelectTouch(int x, int y);
  void handlePuzzleListTouch(int x, int y);
  void handlePlayingTouch(int x, int y);
  void handleCompleteTouch(int x, int y);
};

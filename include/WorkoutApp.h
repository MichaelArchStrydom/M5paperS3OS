#pragma once
#include "App.h"
#include "ui/Keyboard.h"
#include <map>
#include <string>
#include <vector>

class AppManager;

// Workout tracker. Everything lives on the SD card under /workout:
//   exercises.json     exercise library (seeded with defaults, editable on a PC)
//   last.json          last weight/reps used per exercise, for defaults
//   current.json       the workout in progress, saved after every change
//   history/<id>.json  finished workouts, id = YYYYMMDD-HHMM
//
// Flow: Home -> Start -> Add exercise (search or muscle group) -> Log sets
//       -> back to the workout -> add the next exercise ... -> Finish -> Summary
class WorkoutApp : public App {
public:
  explicit WorkoutApp(AppManager *manager);

  void setup() override;
  void update() override {}
  void draw() override;
  void handleTouch(int x, int y) override;
  bool handleBackPress() override;
  bool handleForwardPress() override;
  bool needsRedraw() override;
  bool wantsTouchFeedback(int x, int y) override { return false; }
  void exit() override;
  unsigned long idleTimeoutMs() override;

private:
  enum class Screen {
    HOME,
    ACTIVE,
    PICK_GROUP,
    PICK_LIST,
    LOGGER,
    SUMMARY,
    HISTORY_LIST,
    HISTORY_DETAIL,
    NO_SD
  };
  enum class PickMode { GROUP, RECENT, ALL, SEARCH };

  enum Action {
    A_START,
    A_RESUME,
    A_REPEAT_LAST,
    A_HISTORY,
    A_OPEN_LOGGED,   // arg: index in session
    A_ADD_EXERCISE,
    A_FINISH,
    A_SEARCH,
    A_PICK_RECENT,
    A_PICK_ALL,
    A_PICK_GROUP,    // arg: group index
    A_ASSIGN_GROUP,  // arg: group index, for a new exercise
    A_PICK_EXERCISE, // arg: index in library
    A_NEW_EXERCISE,
    A_CREATE_FROM_QUERY,
    A_PAGE,          // arg: -1 / +1
    A_WEIGHT_STEP,   // arg: delta in grams
    A_REPS_STEP,     // arg: delta
    A_TYPE_WEIGHT,
    A_TYPE_REPS,
    A_LOG_SET,
    A_SELECT_SET,    // arg: set index
    A_DELETE_SET,
    A_REMOVE_EXERCISE,
    A_TO_WORKOUT,
    A_NEXT_EXERCISE,
    A_OPEN_HISTORY,  // arg: index in history ids
    A_REPEAT_VIEWED,
    A_DELETE_VIEWED,
    A_DONE,
    A_RENAME_EXERCISE
  };

  struct ExerciseDef {
    std::string name;
    std::string group;
    int groupIndex; // -1 when the group isn't one of the built-in ones
  };
  struct SetEntry {
    float weight;
    int reps;
  };
  struct LoggedExercise {
    std::string name;
    std::string group;
    std::vector<SetEntry> sets;
  };
  struct Session {
    std::string id;
    std::string date;  // 2026-10-05
    std::string start; // 18:02
    std::string end;
    long startStamp = 0; // minutes since 1970, for durations
    int durationMin = 0;
    std::vector<LoggedExercise> exercises;
  };
  struct LastUsed {
    float weight;
    int reps;
    long stamp;
  };
  struct HistoryRow {
    std::string title;
    std::string detail;
  };
  struct Hit {
    int x, y, w, h;
    int action;
    int arg;
    int zone;
  };
  struct Line {
    std::string text;
    bool bold;
  };

  AppManager *appManager;
  ui::Keyboard _keyboard{0, 0, 960, 540};

  Screen _screen = Screen::HOME;
  std::vector<Screen> _forwardStack; // screens left via Back, newest last
  bool _fullRedraw = true;
  std::vector<Hit> _hits;
  std::string _message; // shown once on the next full draw

  // Data
  std::vector<ExerciseDef> _library;
  std::map<std::string, LastUsed> _lastUsed;
  bool _hasSession = false;
  Session _session;
  std::vector<std::string> _historyIds; // newest first
  bool _hasLastWorkout = false;
  Session _lastWorkout;

  // Active workout / logger
  int _activePage = 0;
  int _activeExercise = -1;
  float _weight = 0;
  int _reps = 10;
  int _selectedSet = -1;
  bool _finishArmed = false;

  // Exercise picker
  PickMode _pickMode = PickMode::GROUP;
  int _pickGroup = 0;
  std::string _query;
  std::string _pendingNewName; // set while choosing a group for a new exercise
  std::vector<int> _pickList;
  int _pickPage = 0;

  // History / summary
  Session _viewed;
  int _viewedIndex = -1;
  std::vector<HistoryRow> _historyRows;
  int _historyPage = 0;
  int _detailPage = 0;
  bool _deleteArmed = false;

  // Storage
  void loadLibrary();
  bool saveLibrary();
  void loadLastUsed();
  bool saveLastUsed();
  bool loadSession(const std::string &path, Session &session);
  bool saveSession(const std::string &path, const Session &session);
  void saveCurrent();
  void refreshHistory();
  void loadHistoryPage();

  // Actions
  void go(Screen screen, bool keepForward = false);
  bool canShow(Screen screen) const;
  void startSession(const Session *repeatFrom);
  void finishSession();
  void openPicker();
  void showPickList(PickMode mode, int group);
  void buildPickList();
  void openSearchKeyboard();
  void createExercise(const std::string &name, int group);
  void openExercise(const std::string &name, const std::string &group);
  void loadLoggerDefaults();
  void openHistoryEntry(int index);
  void renameExercise(const std::string &oldName, const std::string &newName);
  void dispatch(int action, int arg);

  // Drawing
  void addHit(int x, int y, int w, int h, int action, int arg = 0,
              int zone = 0);
  void drawHome();
  void drawActive();
  void drawFinishButton();
  void drawPickGroup();
  void drawPickList();
  void drawLogger();
  void drawWeightValue();
  void drawRepsValue();
  void drawLoggerLower();
  void drawSummary();
  void drawHistoryList();
  void drawHistoryDetail();
  void drawDeleteButton();
  int drawSessionLines(const Session &session, int top, int bottom);
  void drawPager(int y, int page, int pages);
  void drawNoSd();

  std::vector<Line> sessionLines(const Session &session);
  int setCount(const Session &session) const;
  long volume(const Session &session) const;
};

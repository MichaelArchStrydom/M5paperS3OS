#include "WorkoutApp.h"
#include "AppManager.h"
#include "Storage.h"
#include <SD.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

const char *WORKOUT_DIR = "/workout";
const char *HISTORY_DIR = "/workout/history";
const char *LIBRARY_FILE = "/workout/exercises.json";
const char *LIBRARY_BAD_FILE = "/workout/exercises.bad.json";
const char *LAST_FILE = "/workout/last.json";
const char *CURRENT_FILE = "/workout/current.json";
const char *CURRENT_BAD_FILE = "/workout/current.bad.json";

constexpr size_t LIBRARY_DOC_SIZE = 64 * 1024;
constexpr size_t LAST_DOC_SIZE = 64 * 1024;
constexpr size_t SESSION_DOC_SIZE = 32 * 1024;

// Rests between sets can be long, so stay awake longer mid-workout.
constexpr unsigned long ACTIVE_IDLE_TIMEOUT_MS = 20UL * 60 * 1000;
constexpr unsigned long DEFAULT_IDLE_TIMEOUT_MS = 5UL * 60 * 1000;

constexpr float MAX_WEIGHT = 999.5f;
constexpr int MAX_REPS = 999;

// Layout (portrait 540x960, the footer owns y >= 910)
constexpr int SCREEN_W = 540;
constexpr int CONTENT_BOTTOM = 905;
constexpr int MARGIN = 20;

constexpr int ACTIVE_ROW_Y = 110, ACTIVE_ROW_H = 85, ACTIVE_ROWS = 7;
constexpr int ACTIVE_BTN_Y = 800, ACTIVE_BTN_H = 95;
constexpr int FINISH_X = 340, FINISH_W = 180;

constexpr int TILE_Y = 165, TILE_W = 240, TILE_H = 105, TILE_GAP_Y = 120;

constexpr int LIST_ROW_Y = 95, LIST_ROW_H = 70, LIST_ROWS = 10;
constexpr int PAGER_Y = 815, PAGER_H = 80;

constexpr int LOG_WEIGHT_Y = 160, LOG_REPS_Y = 315, LOG_ROW_H = 100;
constexpr int LOG_WEIGHT_VAL_X = 190, LOG_WEIGHT_VAL_W = 160;
constexpr int LOG_REPS_VAL_X = 170, LOG_REPS_VAL_W = 200;
constexpr int LOG_LOWER_Y = 435; // everything below is redrawn after a set
constexpr int LOG_PRIMARY_Y = 440, LOG_PRIMARY_H = 90;
constexpr int LOG_SETS_Y = 580, LOG_SET_ROW_H = 48, LOG_SET_ROWS = 5;
constexpr int LOG_BOTTOM_Y = 830, LOG_BOTTOM_H = 70;
constexpr int LOGGER_LOWER_ZONE = 1;

constexpr int HISTORY_ROW_Y = 105, HISTORY_ROW_H = 85, HISTORY_ROWS = 8;
constexpr int DETAIL_LINE_H = 36;
constexpr int DETAIL_BTN_Y = 815, DETAIL_BTN_H = 80;

const lgfx::IFont *const FONT_BIG = &fonts::FreeSansBold24pt7b;
const lgfx::IFont *const FONT_TITLE = &fonts::FreeSansBold18pt7b;
const lgfx::IFont *const FONT_BOLD = &fonts::FreeSansBold12pt7b;
const lgfx::IFont *const FONT_TEXT = &fonts::FreeSans12pt7b;
const lgfx::IFont *const FONT_SMALL = &fonts::FreeSans9pt7b;

const char *const MUSCLE_GROUPS[] = {"Chest",   "Back",       "Shoulders",
                                     "Biceps",  "Triceps",    "Quads",
                                     "Hamstrings", "Glutes",  "Calves",
                                     "Core"};
constexpr int NUM_GROUPS = sizeof(MUSCLE_GROUPS) / sizeof(MUSCLE_GROUPS[0]);

enum Group {
  CHEST,
  BACK,
  SHOULDERS,
  BICEPS,
  TRICEPS,
  QUADS,
  HAMSTRINGS,
  GLUTES,
  CALVES,
  CORE
};

struct DefaultExercise {
  const char *name;
  Group group;
};

// Written to exercises.json on first run; edit that file to customise.
const DefaultExercise DEFAULT_LIBRARY[] = {
    {"Bench Press", CHEST},
    {"Incline Bench Press", CHEST},
    {"Decline Bench Press", CHEST},
    {"Dumbbell Bench Press", CHEST},
    {"Incline Dumbbell Press", CHEST},
    {"Machine Chest Press", CHEST},
    {"Dumbbell Fly", CHEST},
    {"Cable Fly", CHEST},
    {"Pec Deck", CHEST},
    {"Chest Dip", CHEST},
    {"Push-Up", CHEST},

    {"Deadlift", BACK},
    {"Pull-Up", BACK},
    {"Chin-Up", BACK},
    {"Lat Pulldown", BACK},
    {"Barbell Row", BACK},
    {"Pendlay Row", BACK},
    {"Dumbbell Row", BACK},
    {"Seated Cable Row", BACK},
    {"T-Bar Row", BACK},
    {"Straight-Arm Pulldown", BACK},
    {"Shrug", BACK},
    {"Back Extension", BACK},

    {"Overhead Press", SHOULDERS},
    {"Dumbbell Shoulder Press", SHOULDERS},
    {"Machine Shoulder Press", SHOULDERS},
    {"Arnold Press", SHOULDERS},
    {"Lateral Raise", SHOULDERS},
    {"Cable Lateral Raise", SHOULDERS},
    {"Front Raise", SHOULDERS},
    {"Rear Delt Fly", SHOULDERS},
    {"Face Pull", SHOULDERS},
    {"Upright Row", SHOULDERS},

    {"Barbell Curl", BICEPS},
    {"EZ-Bar Curl", BICEPS},
    {"Dumbbell Curl", BICEPS},
    {"Hammer Curl", BICEPS},
    {"Incline Dumbbell Curl", BICEPS},
    {"Preacher Curl", BICEPS},
    {"Cable Curl", BICEPS},
    {"Concentration Curl", BICEPS},

    {"Close-Grip Bench Press", TRICEPS},
    {"Tricep Pushdown", TRICEPS},
    {"Rope Pushdown", TRICEPS},
    {"Skull Crusher", TRICEPS},
    {"Overhead Tricep Extension", TRICEPS},
    {"Tricep Dip", TRICEPS},
    {"Tricep Kickback", TRICEPS},

    {"Back Squat", QUADS},
    {"Front Squat", QUADS},
    {"Goblet Squat", QUADS},
    {"Hack Squat", QUADS},
    {"Leg Press", QUADS},
    {"Leg Extension", QUADS},
    {"Bulgarian Split Squat", QUADS},
    {"Lunge", QUADS},
    {"Step-Up", QUADS},

    {"Romanian Deadlift", HAMSTRINGS},
    {"Stiff-Leg Deadlift", HAMSTRINGS},
    {"Lying Leg Curl", HAMSTRINGS},
    {"Seated Leg Curl", HAMSTRINGS},
    {"Good Morning", HAMSTRINGS},
    {"Nordic Curl", HAMSTRINGS},

    {"Hip Thrust", GLUTES},
    {"Glute Bridge", GLUTES},
    {"Sumo Deadlift", GLUTES},
    {"Cable Kickback", GLUTES},
    {"Hip Abduction", GLUTES},

    {"Standing Calf Raise", CALVES},
    {"Seated Calf Raise", CALVES},
    {"Leg Press Calf Raise", CALVES},

    {"Plank", CORE},
    {"Hanging Leg Raise", CORE},
    {"Cable Crunch", CORE},
    {"Crunch", CORE},
    {"Sit-Up", CORE},
    {"Ab Wheel Rollout", CORE},
    {"Russian Twist", CORE},
};

const char *const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *const WEEKDAYS[] = {"Sun", "Mon", "Tue", "Wed",
                                "Thu", "Fri", "Sat"};

// ---------------------------------------------------------------- helpers

int groupIndexOf(const std::string &name) {
  for (int i = 0; i < NUM_GROUPS; i++) {
    if (name == MUSCLE_GROUPS[i])
      return i;
  }
  return -1;
}

// Days since 1970-01-01 (Howard Hinnant's algorithm)
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

struct Clock {
  int year, month, day, hour, minute;
  long stamp; // minutes since 1970
};

Clock now() {
  m5::rtc_date_t date;
  m5::rtc_time_t time;
  M5.Rtc.getDate(&date);
  M5.Rtc.getTime(&time);
  Clock c;
  c.year = date.year;
  c.month = date.month;
  c.day = date.date;
  c.hour = time.hours;
  c.minute = time.minutes;
  c.stamp = daysFromCivil(c.year, c.month, c.day) * 1440L + c.hour * 60L +
            c.minute;
  return c;
}

// "2026-10-05" -> "Mon 05 Oct 2026"
std::string prettyDate(const std::string &iso) {
  int y = 0, m = 0, d = 0;
  if (sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12)
    return iso;
  long days = daysFromCivil(y, m, d);
  int weekday = (int)(((days % 7) + 11) % 7); // 1970-01-01 was a Thursday
  char buf[24];
  snprintf(buf, sizeof(buf), "%s %02d %s %04d", WEEKDAYS[weekday], d,
           MONTHS[m - 1], y);
  return buf;
}

// History ids look like 20261005-1802
bool stampFromId(const std::string &id, long &stamp) {
  int y, mo, d, h, mi;
  if (sscanf(id.c_str(), "%4d%2d%2d-%2d%2d", &y, &mo, &d, &h, &mi) != 5)
    return false;
  stamp = daysFromCivil(y, mo, d) * 1440L + h * 60L + mi;
  return true;
}

std::string formatWeight(float w) {
  char buf[16];
  float rounded = roundf(w * 100) / 100;
  if (fabsf(rounded - roundf(rounded)) < 0.001f)
    snprintf(buf, sizeof(buf), "%.0f", rounded);
  else if (fabsf(rounded * 10 - roundf(rounded * 10)) < 0.01f)
    snprintf(buf, sizeof(buf), "%.1f", rounded);
  else
    snprintf(buf, sizeof(buf), "%.2f", rounded);
  return buf;
}

std::string formatSetShort(float w, int reps) {
  return formatWeight(w) + "x" + std::to_string(reps);
}

std::string formatSetLong(float w, int reps) {
  return formatWeight(w) + " kg x " + std::to_string(reps);
}

std::string formatThousands(long value) {
  std::string digits = std::to_string(value);
  std::string out;
  int count = 0;
  for (int i = (int)digits.size() - 1; i >= 0; i--) {
    out.insert(out.begin(), digits[i]);
    if (++count % 3 == 0 && i > 0 && digits[i - 1] != '-')
      out.insert(out.begin(), ',');
  }
  return out;
}

// 52 -> "52 min", 65 -> "1 h 05 min"
std::string formatDuration(long minutes) {
  if (minutes < 60)
    return std::to_string(minutes) + " min";
  char buf[24];
  snprintf(buf, sizeof(buf), "%ld h %02ld min", minutes / 60, minutes % 60);
  return buf;
}

std::string plural(int n, const char *word) {
  return std::to_string(n) + " " + word + (n == 1 ? "" : "s");
}

// Lowercase letters and digits only, so "pullup" matches "Pull-Up"
std::string normalise(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (isalnum((unsigned char)c))
      out += (char)tolower((unsigned char)c);
  }
  return out;
}

bool lessIgnoreCase(const std::string &a, const std::string &b) {
  return normalise(a) < normalise(b);
}

std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(' ');
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(' ');
  return s.substr(start, end - start + 1);
}

std::string historyPath(const std::string &id) {
  return std::string(HISTORY_DIR) + "/" + id + ".json";
}

void removeWithTmp(const char *path) {
  SD.remove(path);
  SD.remove((std::string(path) + ".tmp").c_str());
}

// Truncates with ".." to fit the current font
std::string fit(const std::string &s, int maxWidth) {
  if (M5.Display.textWidth(s.c_str()) <= maxWidth)
    return s;
  std::string out = s;
  while (!out.empty() && M5.Display.textWidth((out + "..").c_str()) > maxWidth)
    out.pop_back();
  return out + "..";
}

void drawText(const std::string &s, int x, int y, const lgfx::IFont *font,
              int color = BLACK, lgfx::textdatum_t datum = top_left) {
  M5.Display.setFont(font);
  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(datum);
  M5.Display.drawString(s.c_str(), x, y);
  M5.Display.setTextDatum(top_left);
}

void drawButton(int x, int y, int w, int h, const std::string &label,
                bool primary, const lgfx::IFont *font = FONT_BOLD) {
  if (primary) {
    M5.Display.fillRoundRect(x, y, w, h, 10, BLACK);
  } else {
    M5.Display.fillRoundRect(x, y, w, h, 10, WHITE);
    M5.Display.drawRoundRect(x, y, w, h, 10, BLACK);
    M5.Display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 9, BLACK);
  }
  M5.Display.setFont(font);
  drawText(fit(label, w - 16), x + w / 2, y + h / 2, font,
           primary ? WHITE : BLACK, middle_center);
}

// Two-line call-to-action card
void drawCard(int x, int y, int w, int h, const std::string &title,
              const std::string &subtitle, bool primary) {
  drawButton(x, y, w, h, "", primary);
  int fg = primary ? WHITE : BLACK;
  M5.Display.setFont(FONT_TITLE);
  drawText(fit(title, w - 30), x + w / 2, y + h / 2 - 18, FONT_TITLE, fg,
           middle_center);
  M5.Display.setFont(FONT_TEXT);
  drawText(fit(subtitle, w - 30), x + w / 2, y + h / 2 + 24, FONT_TEXT, fg,
           middle_center);
}

void drawHeader(const std::string &title, const std::string &subtitle) {
  M5.Display.setFont(FONT_TITLE);
  drawText(fit(title, SCREEN_W - 2 * MARGIN), MARGIN, 20, FONT_TITLE);
  if (!subtitle.empty()) {
    M5.Display.setFont(FONT_TEXT);
    drawText(fit(subtitle, SCREEN_W - 2 * MARGIN), MARGIN, 62, FONT_TEXT,
             DARKGREY);
  }
}

void drawRule(int y) { M5.Display.drawFastHLine(MARGIN, y, SCREEN_W - 2 * MARGIN, LIGHTGREY); }

int pageCount(int items, int perPage) {
  return items <= 0 ? 1 : (items + perPage - 1) / perPage;
}

} // namespace

// ================================================================ lifecycle

WorkoutApp::WorkoutApp(AppManager *manager) : appManager(manager) {}

void WorkoutApp::setup() {
  _fullRedraw = true;
  _finishArmed = false;
  _deleteArmed = false;
  _pendingNewName.clear();
  _message.clear();
  _forwardStack.clear();

  if (!storage::sdAvailable()) {
    _hasSession = false;
    _screen = Screen::NO_SD;
    return;
  }
  storage::ensureDir(WORKOUT_DIR);
  storage::ensureDir(HISTORY_DIR);

  loadLibrary();
  loadLastUsed();
  refreshHistory();

  _hasSession = loadSession(CURRENT_FILE, _session);
  if (!_hasSession && SD.exists(CURRENT_FILE)) {
    // Unreadable: keep it aside rather than silently overwriting it later
    SD.rename(CURRENT_FILE, CURRENT_BAD_FILE);
    _message = "Saved workout was unreadable (kept as current.bad.json)";
  }

  // Jump straight back into an unfinished workout (e.g. after power loss)
  _activePage = 0;
  _screen = _hasSession ? Screen::ACTIVE : Screen::HOME;
}

void WorkoutApp::exit() {
  if (_hasSession)
    saveCurrent();
}

unsigned long WorkoutApp::idleTimeoutMs() {
  return _hasSession ? ACTIVE_IDLE_TIMEOUT_MS : DEFAULT_IDLE_TIMEOUT_MS;
}

bool WorkoutApp::needsRedraw() {
  bool redraw = _fullRedraw;
  _fullRedraw = false;
  return redraw;
}

// Any fresh navigation discards the forward history, like a browser.
void WorkoutApp::go(Screen screen, bool keepForward) {
  if (!keepForward)
    _forwardStack.clear();
  _screen = screen;
  _finishArmed = false;
  _deleteArmed = false;
  _fullRedraw = true;
}

bool WorkoutApp::handleBackPress() {
  Screen from = _screen;
  switch (_screen) {
  case Screen::HOME:
  case Screen::NO_SD:
    return false; // leave the app
  case Screen::ACTIVE:
  case Screen::SUMMARY:
  case Screen::HISTORY_LIST:
    go(Screen::HOME, true);
    break;
  case Screen::PICK_GROUP:
    if (!_pendingNewName.empty()) {
      _pendingNewName.clear();
      go(Screen::PICK_LIST, true);
    } else {
      go(Screen::ACTIVE, true);
    }
    break;
  case Screen::PICK_LIST:
    go(Screen::PICK_GROUP, true);
    break;
  case Screen::LOGGER:
    go(Screen::ACTIVE, true);
    break;
  case Screen::HISTORY_DETAIL:
    loadHistoryPage();
    go(Screen::HISTORY_LIST, true);
    break;
  }
  _forwardStack.push_back(from);
  return true;
}

// Returns to the screen Back just left. Screens whose data has since gone
// (a removed exercise, a deleted workout) are skipped.
bool WorkoutApp::handleForwardPress() {
  while (!_forwardStack.empty()) {
    Screen next = _forwardStack.back();
    _forwardStack.pop_back();
    if (!canShow(next))
      continue;
    if (next == Screen::HISTORY_LIST)
      loadHistoryPage();
    go(next, true);
    return true;
  }
  return false; // let AppManager move forward between apps
}

bool WorkoutApp::canShow(Screen screen) const {
  switch (screen) {
  case Screen::ACTIVE:
  case Screen::PICK_GROUP:
  case Screen::PICK_LIST:
    return _hasSession;
  case Screen::LOGGER:
    return _hasSession && _activeExercise >= 0 &&
           _activeExercise < (int)_session.exercises.size();
  case Screen::SUMMARY:
    return !_viewed.id.empty();
  case Screen::HISTORY_DETAIL:
    return _viewedIndex >= 0 && _viewedIndex < (int)_historyIds.size() &&
           _historyIds[_viewedIndex] == _viewed.id;
  case Screen::NO_SD:
    return false;
  default:
    return true;
  }
}

// ================================================================ storage

void WorkoutApp::loadLibrary() {
  _library.clear();
  bool exists = SD.exists(LIBRARY_FILE) ||
                SD.exists((std::string(LIBRARY_FILE) + ".tmp").c_str());

  PsramJsonDocument doc(LIBRARY_DOC_SIZE);
  if (exists && storage::readJson(LIBRARY_FILE, doc)) {
    for (JsonObject o : doc["exercises"].as<JsonArray>()) {
      std::string name = trim(o["name"] | "");
      std::string group = o["group"] | "";
      if (!name.empty()) {
        ExerciseDef def = {name, group, groupIndexOf(group)};
        _library.push_back(def);
      }
    }
  } else if (exists) {
    // Don't clobber a hand-edited file that has a typo in it
    SD.remove(LIBRARY_BAD_FILE);
    SD.rename(LIBRARY_FILE, LIBRARY_BAD_FILE);
    _message = "exercises.json was invalid; defaults restored";
  }

  if (_library.empty()) {
    for (const auto &d : DEFAULT_LIBRARY) {
      ExerciseDef def = {d.name, MUSCLE_GROUPS[d.group], (int)d.group};
      _library.push_back(def);
    }
    saveLibrary();
  }

  std::sort(_library.begin(), _library.end(),
            [](const ExerciseDef &a, const ExerciseDef &b) {
              return lessIgnoreCase(a.name, b.name);
            });
}

bool WorkoutApp::saveLibrary() {
  PsramJsonDocument doc(LIBRARY_DOC_SIZE);
  JsonArray arr = doc.createNestedArray("exercises");
  for (const auto &e : _library) {
    JsonObject o = arr.createNestedObject();
    o["name"] = e.name;
    o["group"] = e.group;
  }
  return storage::writeJsonAtomic(LIBRARY_FILE, doc);
}

void WorkoutApp::loadLastUsed() {
  _lastUsed.clear();
  PsramJsonDocument doc(LAST_DOC_SIZE);
  if (!storage::readJson(LAST_FILE, doc))
    return;
  for (JsonPair kv : doc.as<JsonObject>()) {
    JsonObject o = kv.value().as<JsonObject>();
    LastUsed last = {o["w"] | 0.0f, o["r"] | 0, o["t"] | 0L};
    _lastUsed[kv.key().c_str()] = last;
  }
}

bool WorkoutApp::saveLastUsed() {
  PsramJsonDocument doc(LAST_DOC_SIZE);
  for (const auto &kv : _lastUsed) {
    JsonObject o = doc.createNestedObject(kv.first);
    o["w"] = kv.second.weight;
    o["r"] = kv.second.reps;
    o["t"] = kv.second.stamp;
  }
  return storage::writeJsonAtomic(LAST_FILE, doc);
}

bool WorkoutApp::loadSession(const std::string &path, Session &session) {
  PsramJsonDocument doc(SESSION_DOC_SIZE);
  if (!storage::readJson(path.c_str(), doc))
    return false;

  session = Session();
  session.id = doc["id"] | "";
  session.date = doc["date"] | "";
  session.start = doc["start"] | "";
  session.end = doc["end"] | "";
  session.startStamp = doc["startStamp"] | 0L;
  session.durationMin = doc["duration"] | 0;
  for (JsonObject e : doc["exercises"].as<JsonArray>()) {
    LoggedExercise ex;
    ex.name = e["name"] | "";
    ex.group = e["group"] | "";
    for (JsonArray s : e["sets"].as<JsonArray>()) {
      SetEntry set = {s[0] | 0.0f, s[1] | 0};
      ex.sets.push_back(set);
    }
    if (!ex.name.empty())
      session.exercises.push_back(ex);
  }
  return !session.id.empty();
}

// Sets are stored as [weight, reps] pairs; the set count is the array length.
bool WorkoutApp::saveSession(const std::string &path, const Session &session) {
  PsramJsonDocument doc(SESSION_DOC_SIZE);
  doc["id"] = session.id;
  doc["date"] = session.date;
  doc["start"] = session.start;
  if (!session.end.empty()) {
    doc["end"] = session.end;
    doc["duration"] = session.durationMin;
  }
  doc["startStamp"] = session.startStamp;
  JsonArray exercises = doc.createNestedArray("exercises");
  for (const auto &ex : session.exercises) {
    JsonObject o = exercises.createNestedObject();
    o["name"] = ex.name;
    o["group"] = ex.group;
    JsonArray sets = o.createNestedArray("sets");
    for (const auto &s : ex.sets) {
      JsonArray pair = sets.createNestedArray();
      pair.add(roundf(s.weight * 100) / 100);
      pair.add(s.reps);
    }
  }
  return storage::writeJsonAtomic(path.c_str(), doc);
}

void WorkoutApp::saveCurrent() {
  if (!_hasSession)
    return;
  if (!saveSession(CURRENT_FILE, _session)) {
    _message = "Could not save to SD card!";
    _fullRedraw = true;
  }
}

void WorkoutApp::refreshHistory() {
  _historyIds.clear();
  File dir = SD.open(HISTORY_DIR);
  if (dir && dir.isDirectory()) {
    while (true) {
      File f = dir.openNextFile();
      if (!f)
        break;
      std::string name = f.name();
      bool isDir = f.isDirectory();
      f.close();
      size_t slash = name.find_last_of('/');
      if (slash != std::string::npos)
        name = name.substr(slash + 1);
      if (isDir || name.empty() || name[0] == '.' || name.size() <= 5 ||
          name.compare(name.size() - 5, 5, ".json") != 0)
        continue;
      _historyIds.push_back(name.substr(0, name.size() - 5));
    }
  }
  if (dir)
    dir.close();
  std::sort(_historyIds.begin(), _historyIds.end(),
            [](const std::string &a, const std::string &b) { return a > b; });

  _hasLastWorkout =
      !_historyIds.empty() && loadSession(historyPath(_historyIds[0]), _lastWorkout);
}

void WorkoutApp::loadHistoryPage() {
  _historyRows.clear();
  int pages = pageCount(_historyIds.size(), HISTORY_ROWS);
  _historyPage = std::max(0, std::min(_historyPage, pages - 1));
  int first = _historyPage * HISTORY_ROWS;
  for (int i = first;
       i < (int)_historyIds.size() && i < first + HISTORY_ROWS; i++) {
    HistoryRow row;
    Session s;
    if (loadSession(historyPath(_historyIds[i]), s)) {
      row.title = prettyDate(s.date) + "  " + s.start;
      row.detail = plural(s.exercises.size(), "exercise") + " - " +
                   plural(setCount(s), "set") + " - " +
                   formatDuration(s.durationMin);
    } else {
      row.title = _historyIds[i];
      row.detail = "(unreadable)";
    }
    _historyRows.push_back(row);
  }
}

int WorkoutApp::setCount(const Session &session) const {
  int n = 0;
  for (const auto &ex : session.exercises)
    n += ex.sets.size();
  return n;
}

long WorkoutApp::volume(const Session &session) const {
  float total = 0;
  for (const auto &ex : session.exercises)
    for (const auto &s : ex.sets)
      total += s.weight * s.reps;
  return lroundf(total);
}

// ================================================================ actions

void WorkoutApp::startSession(const Session *repeatFrom) {
  Clock c = now();
  char base[20];
  snprintf(base, sizeof(base), "%04d%02d%02d-%02d%02d", c.year, c.month,
           c.day, c.hour, c.minute);
  std::string id = base;
  for (int n = 2; SD.exists(historyPath(id).c_str()); n++)
    id = std::string(base) + "-" + std::to_string(n);

  char date[12], start[6];
  snprintf(date, sizeof(date), "%04d-%02d-%02d", c.year, c.month, c.day);
  snprintf(start, sizeof(start), "%02d:%02d", c.hour, c.minute);

  _session = Session();
  _session.id = id;
  _session.date = date;
  _session.start = start;
  _session.startStamp = c.stamp;
  if (repeatFrom) {
    for (const auto &ex : repeatFrom->exercises) {
      LoggedExercise copy;
      copy.name = ex.name;
      copy.group = ex.group;
      _session.exercises.push_back(copy);
    }
  }
  _hasSession = true;
  _activePage = 0;
  saveCurrent();
}

void WorkoutApp::finishSession() {
  Clock c = now();

  // Exercises that were added but never logged aren't worth keeping
  std::vector<LoggedExercise> kept;
  for (const auto &ex : _session.exercises) {
    if (!ex.sets.empty())
      kept.push_back(ex);
  }

  if (kept.empty()) {
    removeWithTmp(CURRENT_FILE);
    _hasSession = false;
    _message = "Empty workout discarded";
    go(Screen::HOME);
    return;
  }

  Session finished = _session;
  finished.exercises = kept;
  char end[6];
  snprintf(end, sizeof(end), "%02d:%02d", c.hour, c.minute);
  finished.end = end;
  finished.durationMin = (int)std::max(0L, c.stamp - finished.startStamp);

  if (!saveSession(historyPath(finished.id), finished)) {
    _message = "Could not save workout to SD card!";
    go(Screen::ACTIVE);
    return;
  }

  for (const auto &ex : finished.exercises) {
    LastUsed last = {ex.sets.back().weight, ex.sets.back().reps, c.stamp};
    _lastUsed[ex.name] = last;
  }
  saveLastUsed();

  removeWithTmp(CURRENT_FILE);
  _hasSession = false;
  _session = Session();
  refreshHistory();

  _viewed = finished;
  _viewedIndex = -1;
  _detailPage = 0;
  go(Screen::SUMMARY);
}

void WorkoutApp::openPicker() {
  _pendingNewName.clear();
  go(Screen::PICK_GROUP);
}

void WorkoutApp::showPickList(PickMode mode, int group) {
  _pickMode = mode;
  _pickGroup = group;
  _pickPage = 0;
  buildPickList();
  go(Screen::PICK_LIST);
}

void WorkoutApp::buildPickList() {
  _pickList.clear();

  if (_pickMode == PickMode::RECENT) {
    std::vector<std::pair<long, int>> recent;
    for (int i = 0; i < (int)_library.size(); i++) {
      auto it = _lastUsed.find(_library[i].name);
      if (it != _lastUsed.end())
        recent.push_back(std::make_pair(it->second.stamp, i));
    }
    std::sort(recent.begin(), recent.end(),
              [](const std::pair<long, int> &a, const std::pair<long, int> &b) {
                return a.first > b.first;
              });
    for (const auto &r : recent)
      _pickList.push_back(r.second);
    return;
  }

  if (_pickMode == PickMode::SEARCH) {
    // Every word typed must appear in the name or group; prefix hits first
    std::vector<std::string> words;
    size_t pos = 0;
    while (pos < _query.size()) {
      size_t space = _query.find(' ', pos);
      if (space == std::string::npos)
        space = _query.size();
      std::string word = normalise(_query.substr(pos, space - pos));
      if (!word.empty())
        words.push_back(word);
      pos = space + 1;
    }
    std::string whole = normalise(_query);

    std::vector<int> prefix, other;
    for (int i = 0; i < (int)_library.size(); i++) {
      std::string haystack =
          normalise(_library[i].name) + " " + normalise(_library[i].group);
      bool match = !words.empty();
      for (const auto &w : words) {
        if (haystack.find(w) == std::string::npos) {
          match = false;
          break;
        }
      }
      if (!match)
        continue;
      if (normalise(_library[i].name).compare(0, whole.size(), whole) == 0)
        prefix.push_back(i);
      else
        other.push_back(i);
    }
    _pickList = prefix;
    _pickList.insert(_pickList.end(), other.begin(), other.end());
    return;
  }

  for (int i = 0; i < (int)_library.size(); i++) {
    if (_pickMode == PickMode::ALL || _library[i].groupIndex == _pickGroup)
      _pickList.push_back(i);
  }
}

void WorkoutApp::openSearchKeyboard() {
  _keyboard.show("Search exercises", [this](const std::string &text) {
    _query = trim(text);
    if (_query.empty()) {
      go(Screen::PICK_GROUP);
    } else {
      showPickList(PickMode::SEARCH, 0);
    }
  });
}

void WorkoutApp::createExercise(const std::string &rawName, int group) {
  std::string name = trim(rawName);
  if (name.empty() || group < 0 || group >= NUM_GROUPS) {
    _fullRedraw = true;
    return;
  }

  // Reuse an existing exercise with the same name instead of duplicating it
  for (const auto &e : _library) {
    if (normalise(e.name) == normalise(name)) {
      openExercise(e.name, e.group);
      return;
    }
  }

  ExerciseDef def = {name, MUSCLE_GROUPS[group], group};
  _library.push_back(def);
  std::sort(_library.begin(), _library.end(),
            [](const ExerciseDef &a, const ExerciseDef &b) {
              return lessIgnoreCase(a.name, b.name);
            });
  if (!saveLibrary())
    _message = "Could not save the new exercise";
  openExercise(name, MUSCLE_GROUPS[group]);
}

void WorkoutApp::openExercise(const std::string &name,
                              const std::string &group) {
  if (!_hasSession)
    startSession(nullptr);

  _pendingNewName.clear();
  int index = -1;
  for (int i = 0; i < (int)_session.exercises.size(); i++) {
    if (_session.exercises[i].name == name) {
      index = i;
      break;
    }
  }
  if (index < 0) {
    LoggedExercise ex;
    ex.name = name;
    ex.group = group;
    _session.exercises.push_back(ex);
    index = _session.exercises.size() - 1;
    saveCurrent();
  }

  _activeExercise = index;
  _selectedSet = -1;
  loadLoggerDefaults();
  go(Screen::LOGGER);
}

// Pre-fill with this session's last set, else the last time it was done
void WorkoutApp::loadLoggerDefaults() {
  const LoggedExercise &ex = _session.exercises[_activeExercise];
  if (!ex.sets.empty()) {
    _weight = ex.sets.back().weight;
    _reps = ex.sets.back().reps;
    return;
  }
  auto it = _lastUsed.find(ex.name);
  if (it != _lastUsed.end()) {
    _weight = it->second.weight;
    _reps = it->second.reps;
  } else {
    _weight = 0;
    _reps = 10;
  }
}

void WorkoutApp::openHistoryEntry(int index) {
  if (index < 0 || index >= (int)_historyIds.size())
    return;
  if (!loadSession(historyPath(_historyIds[index]), _viewed)) {
    _message = "Could not read that workout";
    _fullRedraw = true;
    return;
  }
  _viewedIndex = index;
  _detailPage = 0;
  go(Screen::HISTORY_DETAIL);
}

// Renames everywhere the name is stored so defaults and history stay linked:
// the library, last-used weights, the current workout and past workouts.
void WorkoutApp::renameExercise(const std::string &oldName,
                                const std::string &rawName) {
  _fullRedraw = true;
  std::string name = trim(rawName);
  if (name.empty() || name == oldName)
    return;

  for (const auto &e : _library) {
    if (e.name != oldName && normalise(e.name) == normalise(name)) {
      _message = "\"" + name + "\" already exists";
      return;
    }
  }
  for (const auto &ex : _session.exercises) {
    if (ex.name != oldName && normalise(ex.name) == normalise(name)) {
      _message = "\"" + name + "\" is already in this workout";
      return;
    }
  }

  // Rewriting history can take a moment on a big card
  M5.Display.startWrite();
  M5.Display.fillRect(0, 862, SCREEN_W, 44, BLACK);
  drawText("Renaming...", SCREEN_W / 2, 884, FONT_BOLD, WHITE, middle_center);
  M5.Display.endWrite();

  bool ok = true;
  for (auto &e : _library) {
    if (e.name == oldName)
      e.name = name;
  }
  std::sort(_library.begin(), _library.end(),
            [](const ExerciseDef &a, const ExerciseDef &b) {
              return lessIgnoreCase(a.name, b.name);
            });
  ok &= saveLibrary();

  auto last = _lastUsed.find(oldName);
  if (last != _lastUsed.end()) {
    LastUsed value = last->second;
    _lastUsed.erase(last);
    _lastUsed[name] = value;
    ok &= saveLastUsed();
  }

  for (auto &ex : _session.exercises) {
    if (ex.name == oldName)
      ex.name = name;
  }
  saveCurrent();

  int updated = 0;
  for (const auto &id : _historyIds) {
    Session past;
    if (!loadSession(historyPath(id), past))
      continue;
    bool changed = false;
    for (auto &ex : past.exercises) {
      if (ex.name == oldName) {
        ex.name = name;
        changed = true;
      }
    }
    if (changed) {
      ok &= saveSession(historyPath(id), past);
      updated++;
    }
  }
  for (auto &ex : _viewed.exercises) {
    if (ex.name == oldName)
      ex.name = name;
  }
  refreshHistory();
  buildPickList(); // library order changed, so stored indices are stale

  if (!ok)
    _message = "Rename partly failed - check the SD card";
  else if (updated > 0)
    _message = "Renamed, updated " + plural(updated, "past workout");
  else
    _message = "Renamed";
}

// ================================================================ touch

void WorkoutApp::addHit(int x, int y, int w, int h, int action, int arg,
                        int zone) {
  Hit hit = {x, y, w, h, action, arg, zone};
  _hits.push_back(hit);
}

void WorkoutApp::handleTouch(int x, int y) {
  if (_keyboard.isVisible()) {
    _keyboard.handleTouch(x, y);
    if (!_keyboard.isVisible())
      _fullRedraw = true; // closed: back to portrait
    return;
  }

  for (int i = (int)_hits.size() - 1; i >= 0; i--) {
    const Hit &h = _hits[i];
    if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) {
      dispatch(h.action, h.arg);
      return;
    }
  }

  // Tapping anywhere else cancels a pending confirmation
  if (_finishArmed) {
    _finishArmed = false;
    M5.Display.startWrite();
    drawFinishButton();
    M5.Display.endWrite();
  }
  if (_deleteArmed) {
    _deleteArmed = false;
    M5.Display.startWrite();
    drawDeleteButton();
    M5.Display.endWrite();
  }
}

void WorkoutApp::dispatch(int action, int arg) {
  if (action != A_FINISH && _finishArmed) {
    _finishArmed = false;
    _fullRedraw = true;
  }
  if (action != A_DELETE_VIEWED && _deleteArmed) {
    _deleteArmed = false;
    _fullRedraw = true;
  }

  switch (action) {
  case A_START:
    startSession(nullptr);
    openPicker();
    break;

  case A_RESUME:
    _activePage = 0;
    go(Screen::ACTIVE);
    break;

  case A_REPEAT_LAST:
    if (_hasLastWorkout) {
      startSession(&_lastWorkout);
      go(Screen::ACTIVE);
    }
    break;

  case A_HISTORY:
    _historyPage = 0;
    loadHistoryPage();
    go(Screen::HISTORY_LIST);
    break;

  case A_OPEN_LOGGED:
    if (arg >= 0 && arg < (int)_session.exercises.size()) {
      openExercise(_session.exercises[arg].name,
                   _session.exercises[arg].group);
    }
    break;

  case A_ADD_EXERCISE:
  case A_NEXT_EXERCISE:
    openPicker();
    break;

  case A_FINISH:
    if (_finishArmed) {
      finishSession();
    } else {
      _finishArmed = true;
      M5.Display.startWrite();
      drawFinishButton();
      M5.Display.endWrite();
    }
    break;

  case A_SEARCH:
    openSearchKeyboard();
    break;

  case A_PICK_RECENT:
    showPickList(PickMode::RECENT, 0);
    break;

  case A_PICK_ALL:
    showPickList(PickMode::ALL, 0);
    break;

  case A_PICK_GROUP:
    showPickList(PickMode::GROUP, arg);
    break;

  case A_ASSIGN_GROUP:
    createExercise(_pendingNewName, arg);
    break;

  case A_PICK_EXERCISE:
    if (arg >= 0 && arg < (int)_library.size())
      openExercise(_library[arg].name, _library[arg].group);
    break;

  case A_NEW_EXERCISE: {
    int group = _pickGroup;
    std::string prompt =
        std::string("New ") + MUSCLE_GROUPS[group] + " exercise name";
    _keyboard.show(prompt, [this, group](const std::string &text) {
      if (trim(text).empty()) {
        _fullRedraw = true;
      } else {
        createExercise(text, group);
      }
    });
    break;
  }

  case A_CREATE_FROM_QUERY:
    // Ask which muscle group the new exercise belongs to
    _pendingNewName = _query;
    go(Screen::PICK_GROUP);
    break;

  case A_PAGE:
    if (_screen == Screen::ACTIVE) {
      _activePage += arg;
    } else if (_screen == Screen::PICK_LIST) {
      _pickPage += arg;
    } else if (_screen == Screen::HISTORY_LIST) {
      _historyPage += arg;
      loadHistoryPage();
    } else {
      _detailPage += arg;
    }
    _fullRedraw = true;
    break;

  case A_WEIGHT_STEP:
    _weight += arg / 1000.0f;
    _weight = std::max(0.0f, std::min(MAX_WEIGHT, _weight));
    _weight = roundf(_weight * 100) / 100;
    M5.Display.startWrite();
    drawWeightValue();
    M5.Display.endWrite();
    break;

  case A_REPS_STEP:
    _reps = std::max(0, std::min(MAX_REPS, _reps + arg));
    M5.Display.startWrite();
    drawRepsValue();
    M5.Display.endWrite();
    break;

  case A_TYPE_WEIGHT:
    _keyboard.show(
        "Weight in kg (now " + formatWeight(_weight) + ")",
        [this](const std::string &text) {
          char *end = nullptr;
          float value = strtof(text.c_str(), &end);
          if (end != text.c_str() && value >= 0 && value <= MAX_WEIGHT)
            _weight = roundf(value * 100) / 100;
          _fullRedraw = true;
        },
        true);
    break;

  case A_TYPE_REPS:
    _keyboard.show(
        "Reps (now " + std::to_string(_reps) + ")",
        [this](const std::string &text) {
          char *end = nullptr;
          long value = strtol(text.c_str(), &end, 10);
          if (end != text.c_str() && value >= 0 && value <= MAX_REPS)
            _reps = (int)value;
          _fullRedraw = true;
        },
        true);
    break;

  case A_LOG_SET: {
    auto &sets = _session.exercises[_activeExercise].sets;
    SetEntry set = {_weight, _reps};
    if (_selectedSet >= 0 && _selectedSet < (int)sets.size()) {
      sets[_selectedSet] = set;
      _selectedSet = -1;
      loadLoggerDefaults();
    } else {
      sets.push_back(set);
    }
    saveCurrent();
    M5.Display.startWrite();
    drawWeightValue();
    drawRepsValue();
    drawLoggerLower();
    M5.Display.endWrite();
    break;
  }

  case A_SELECT_SET: {
    auto &sets = _session.exercises[_activeExercise].sets;
    if (arg == _selectedSet || arg < 0 || arg >= (int)sets.size()) {
      _selectedSet = -1;
      loadLoggerDefaults();
    } else {
      _selectedSet = arg;
      _weight = sets[arg].weight;
      _reps = sets[arg].reps;
    }
    M5.Display.startWrite();
    drawWeightValue();
    drawRepsValue();
    drawLoggerLower();
    M5.Display.endWrite();
    break;
  }

  case A_DELETE_SET: {
    auto &sets = _session.exercises[_activeExercise].sets;
    if (_selectedSet >= 0 && _selectedSet < (int)sets.size()) {
      sets.erase(sets.begin() + _selectedSet);
      saveCurrent();
    }
    _selectedSet = -1;
    loadLoggerDefaults();
    M5.Display.startWrite();
    drawWeightValue();
    drawRepsValue();
    drawLoggerLower();
    M5.Display.endWrite();
    break;
  }

  case A_REMOVE_EXERCISE:
    if (_activeExercise >= 0 &&
        _activeExercise < (int)_session.exercises.size()) {
      _session.exercises.erase(_session.exercises.begin() + _activeExercise);
      _activeExercise = -1;
      saveCurrent();
    }
    go(Screen::ACTIVE);
    break;

  case A_TO_WORKOUT:
    go(Screen::ACTIVE);
    break;

  case A_OPEN_HISTORY:
    openHistoryEntry(arg);
    break;

  case A_REPEAT_VIEWED:
    if (!_hasSession) {
      startSession(&_viewed);
      go(Screen::ACTIVE);
    }
    break;

  case A_DELETE_VIEWED:
    if (!_deleteArmed) {
      _deleteArmed = true;
      M5.Display.startWrite();
      drawDeleteButton();
      M5.Display.endWrite();
    } else if (_viewedIndex >= 0) {
      removeWithTmp(historyPath(_historyIds[_viewedIndex]).c_str());
      refreshHistory();
      loadHistoryPage();
      _message = "Workout deleted";
      go(Screen::HISTORY_LIST);
    }
    break;

  case A_DONE:
    go(Screen::HOME);
    break;

  case A_RENAME_EXERCISE: {
    std::string oldName = _session.exercises[_activeExercise].name;
    _keyboard.show(
        "New name for " + oldName,
        [this, oldName](const std::string &text) {
          renameExercise(oldName, text);
        },
        false, oldName);
    break;
  }
  }
}

// ================================================================ drawing

void WorkoutApp::draw() {
  if (_keyboard.isVisible()) {
    _keyboard.draw();
    return;
  }

  _hits.clear();
  M5.Display.fillScreen(WHITE);

  switch (_screen) {
  case Screen::HOME:
    drawHome();
    break;
  case Screen::ACTIVE:
    drawActive();
    break;
  case Screen::PICK_GROUP:
    drawPickGroup();
    break;
  case Screen::PICK_LIST:
    drawPickList();
    break;
  case Screen::LOGGER:
    if (_activeExercise >= 0 &&
        _activeExercise < (int)_session.exercises.size()) {
      drawLogger();
    } else {
      _screen = Screen::ACTIVE;
      drawActive();
    }
    break;
  case Screen::SUMMARY:
    drawSummary();
    break;
  case Screen::HISTORY_LIST:
    drawHistoryList();
    break;
  case Screen::HISTORY_DETAIL:
    drawHistoryDetail();
    break;
  case Screen::NO_SD:
    drawNoSd();
    break;
  }

  // One-off status message, e.g. a failed save
  if (!_message.empty()) {
    M5.Display.fillRect(0, 862, SCREEN_W, 44, BLACK);
    M5.Display.setFont(FONT_BOLD);
    drawText(fit(_message, SCREEN_W - 2 * MARGIN), SCREEN_W / 2, 884,
             FONT_BOLD, WHITE, middle_center);
    _message.clear();
  }
}

void WorkoutApp::drawNoSd() {
  drawText("Workout", SCREEN_W / 2, 300, FONT_BIG, BLACK, top_center);
  drawText("Insert an SD card to track", SCREEN_W / 2, 400, FONT_TEXT, BLACK,
           top_center);
  drawText("workouts, then reopen the app.", SCREEN_W / 2, 435, FONT_TEXT,
           BLACK, top_center);
}

void WorkoutApp::drawPager(int y, int page, int pages) {
  if (pages <= 1)
    return;
  bool hasPrev = page > 0, hasNext = page < pages - 1;
  if (hasPrev) {
    drawButton(MARGIN, y, 150, PAGER_H, "< Prev", false);
    addHit(MARGIN, y, 150, PAGER_H, A_PAGE, -1);
  }
  if (hasNext) {
    drawButton(SCREEN_W - MARGIN - 150, y, 150, PAGER_H, "Next >", false);
    addHit(SCREEN_W - MARGIN - 150, y, 150, PAGER_H, A_PAGE, 1);
  }
  drawText(std::to_string(page + 1) + " / " + std::to_string(pages),
           SCREEN_W / 2, y + PAGER_H / 2, FONT_TEXT, BLACK, middle_center);
}

// ---------------------------------------------------------------- home

void WorkoutApp::drawHome() {
  drawText("Workout", SCREEN_W / 2, 30, FONT_BIG, BLACK, top_center);

  int y = 120;
  if (_hasSession) {
    std::string detail = "Started " + _session.start + " - " +
                         plural(_session.exercises.size(), "exercise") +
                         " - " + plural(setCount(_session), "set");
    drawCard(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, "Resume workout", detail,
             true);
    addHit(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, A_RESUME);
    y += 150;
  } else {
    drawCard(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, "Start workout",
             "Pick exercises as you go", true);
    addHit(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, A_START);
    y += 150;

    if (_hasLastWorkout) {
      std::string names;
      for (size_t i = 0; i < _lastWorkout.exercises.size() && i < 2; i++)
        names += (i ? ", " : "") + _lastWorkout.exercises[i].name;
      if (_lastWorkout.exercises.size() > 2)
        names += " +" + std::to_string(_lastWorkout.exercises.size() - 2);
      drawCard(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, "Repeat last workout",
               names, false);
      addHit(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, A_REPEAT_LAST);
      y += 150;
    }
  }

  drawCard(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, "History",
           plural(_historyIds.size(), "workout") + " logged", false);
  addHit(MARGIN, y, SCREEN_W - 2 * MARGIN, 130, A_HISTORY);
  y += 175;

  // At-a-glance stats from the history file names
  Clock c = now();
  int thisWeek = 0;
  long lastStamp = -1;
  for (const auto &id : _historyIds) {
    long stamp;
    if (!stampFromId(id, stamp))
      continue;
    if (lastStamp < 0)
      lastStamp = stamp;
    if (c.stamp - stamp < 7L * 1440)
      thisWeek++;
  }
  drawRule(y - 20);
  drawText("Last 7 days: " + plural(thisWeek, "workout"), MARGIN, y,
           FONT_TEXT);
  if (lastStamp >= 0) {
    long days = c.stamp / 1440 - lastStamp / 1440;
    std::string ago = days <= 0   ? "today"
                      : days == 1 ? "yesterday"
                                  : std::to_string(days) + " days ago";
    drawText("Last workout: " + ago, MARGIN, y + 40, FONT_TEXT);
  }
}

// ---------------------------------------------------------------- active

void WorkoutApp::drawActive() {
  Clock c = now();
  long elapsed = std::max(0L, c.stamp - _session.startStamp);
  drawHeader("Workout", "Started " + _session.start + " - " +
                            formatDuration(elapsed) + " - " +
                            plural(setCount(_session), "set"));
  drawRule(100);

  const auto &exercises = _session.exercises;
  int pages = pageCount(exercises.size(), ACTIVE_ROWS);
  _activePage = std::max(0, std::min(_activePage, pages - 1));

  if (exercises.empty()) {
    drawText("No exercises yet.", SCREEN_W / 2, 330, FONT_TEXT, DARKGREY,
             top_center);
    drawText("Tap + Add exercise to begin.", SCREEN_W / 2, 365, FONT_TEXT,
             DARKGREY, top_center);
  }

  int first = _activePage * ACTIVE_ROWS;
  for (int i = first; i < (int)exercises.size() && i < first + ACTIVE_ROWS;
       i++) {
    int y = ACTIVE_ROW_Y + (i - first) * ACTIVE_ROW_H;
    const auto &ex = exercises[i];

    std::string count = ex.sets.empty() ? "-" : plural(ex.sets.size(), "set");
    M5.Display.setFont(FONT_BOLD);
    int countW = M5.Display.textWidth(count.c_str());
    drawText(count, SCREEN_W - MARGIN, y + 14, FONT_BOLD, BLACK, top_right);
    drawText(fit(ex.name, SCREEN_W - 2 * MARGIN - countW - 20), MARGIN,
             y + 14, FONT_BOLD);

    std::string sets;
    for (const auto &s : ex.sets)
      sets += (sets.empty() ? "" : "  ") + formatSetShort(s.weight, s.reps);
    if (sets.empty())
      sets = "Tap to log sets";
    M5.Display.setFont(FONT_TEXT);
    drawText(fit(sets, SCREEN_W - 2 * MARGIN), MARGIN, y + 48, FONT_TEXT,
             DARKGREY);

    drawRule(y + ACTIVE_ROW_H - 1);
    addHit(0, y, SCREEN_W, ACTIVE_ROW_H, A_OPEN_LOGGED, i);
  }

  if (pages > 1) {
    drawPager(ACTIVE_ROW_Y + ACTIVE_ROWS * ACTIVE_ROW_H + 5, _activePage,
              pages);
  }

  drawButton(MARGIN, ACTIVE_BTN_Y, FINISH_X - MARGIN - 15, ACTIVE_BTN_H,
             "+ Add exercise", true, FONT_TITLE);
  addHit(MARGIN, ACTIVE_BTN_Y, FINISH_X - MARGIN - 15, ACTIVE_BTN_H,
         A_ADD_EXERCISE);
  drawFinishButton();
  addHit(FINISH_X, ACTIVE_BTN_Y, FINISH_W, ACTIVE_BTN_H, A_FINISH);
}

void WorkoutApp::drawFinishButton() {
  if (_screen != Screen::ACTIVE)
    return;
  bool empty = setCount(_session) == 0;
  std::string label = _finishArmed ? (empty ? "Discard?" : "Confirm?")
                                   : (empty ? "Discard" : "Finish");
  M5.Display.fillRect(FINISH_X, ACTIVE_BTN_Y, FINISH_W, ACTIVE_BTN_H, WHITE);
  drawButton(FINISH_X, ACTIVE_BTN_Y, FINISH_W, ACTIVE_BTN_H, label,
             _finishArmed, FONT_TITLE);
}

// ---------------------------------------------------------------- picker

void WorkoutApp::drawPickGroup() {
  bool assigning = !_pendingNewName.empty();
  int tileIndex = 0;
  auto tile = [&](const std::string &label, const std::string &sub,
                  int action, int arg) {
    int col = tileIndex % 2, row = tileIndex / 2;
    int x = MARGIN + col * (TILE_W + 20);
    int y = TILE_Y + row * TILE_GAP_Y;
    drawButton(x, y, TILE_W, TILE_H, "", false);
    drawText(label, x + TILE_W / 2, y + 38, FONT_BOLD, BLACK, middle_center);
    drawText(sub, x + TILE_W / 2, y + 74, FONT_SMALL, DARKGREY,
             middle_center);
    addHit(x, y, TILE_W, TILE_H, action, arg);
    tileIndex++;
  };

  int counts[NUM_GROUPS] = {0};
  for (const auto &e : _library) {
    if (e.groupIndex >= 0)
      counts[e.groupIndex]++;
  }

  if (assigning) {
    drawHeader("Choose muscle group", "for \"" + _pendingNewName + "\"");
    for (int g = 0; g < NUM_GROUPS; g++)
      tile(MUSCLE_GROUPS[g], plural(counts[g], "exercise"), A_ASSIGN_GROUP, g);
    return;
  }

  drawHeader("Add exercise", "");

  // Search box
  M5.Display.drawRoundRect(MARGIN, 75, SCREEN_W - 2 * MARGIN, 70, 35, BLACK);
  M5.Display.drawRoundRect(MARGIN + 1, 76, SCREEN_W - 2 * MARGIN - 2, 68, 34,
                           BLACK);
  M5.Display.drawCircle(MARGIN + 42, 104, 11, BLACK);
  M5.Display.drawCircle(MARGIN + 42, 104, 12, BLACK);
  M5.Display.drawLine(MARGIN + 50, 113, MARGIN + 60, 123, BLACK);
  M5.Display.drawLine(MARGIN + 51, 113, MARGIN + 61, 123, BLACK);
  drawText("Search exercises", MARGIN + 80, 110, FONT_TEXT, DARKGREY,
           middle_left);
  addHit(MARGIN, 75, SCREEN_W - 2 * MARGIN, 70, A_SEARCH);

  int recent = 0;
  for (const auto &e : _library) {
    if (_lastUsed.count(e.name))
      recent++;
  }
  tile("Recent", plural(recent, "exercise"), A_PICK_RECENT, 0);
  tile("All", plural(_library.size(), "exercise"), A_PICK_ALL, 0);
  for (int g = 0; g < NUM_GROUPS; g++)
    tile(MUSCLE_GROUPS[g], plural(counts[g], "exercise"), A_PICK_GROUP, g);
}

void WorkoutApp::drawPickList() {
  std::string title;
  switch (_pickMode) {
  case PickMode::GROUP:
    title = MUSCLE_GROUPS[_pickGroup];
    break;
  case PickMode::RECENT:
    title = "Recent";
    break;
  case PickMode::ALL:
    title = "All exercises";
    break;
  case PickMode::SEARCH:
    title = "\"" + _query + "\"";
    break;
  }
  M5.Display.setFont(FONT_TITLE);
  drawText(fit(title, 330), MARGIN, 25, FONT_TITLE);

  if (_pickMode == PickMode::GROUP) {
    drawButton(SCREEN_W - MARGIN - 150, 12, 150, 64, "+ New", false);
    addHit(SCREEN_W - MARGIN - 150, 12, 150, 64, A_NEW_EXERCISE);
  } else if (_pickMode == PickMode::SEARCH) {
    drawButton(SCREEN_W - MARGIN - 150, 12, 150, 64, "Search", false);
    addHit(SCREEN_W - MARGIN - 150, 12, 150, 64, A_SEARCH);
  }

  if (_pickList.empty()) {
    if (_pickMode == PickMode::SEARCH) {
      drawText("No matches.", SCREEN_W / 2, 250, FONT_TEXT, DARKGREY,
               top_center);
      drawButton(60, 320, SCREEN_W - 120, 90, "Create \"" + _query + "\"",
                 true);
      addHit(60, 320, SCREEN_W - 120, 90, A_CREATE_FROM_QUERY);
    } else if (_pickMode == PickMode::RECENT) {
      drawText("Nothing logged yet.", SCREEN_W / 2, 250, FONT_TEXT, DARKGREY,
               top_center);
    } else {
      drawText("No exercises here yet. Tap + New.", SCREEN_W / 2, 250,
               FONT_TEXT, DARKGREY, top_center);
    }
    return;
  }

  int pages = pageCount(_pickList.size(), LIST_ROWS);
  _pickPage = std::max(0, std::min(_pickPage, pages - 1));
  int first = _pickPage * LIST_ROWS;
  for (int i = first; i < (int)_pickList.size() && i < first + LIST_ROWS;
       i++) {
    const ExerciseDef &e = _library[_pickList[i]];
    int y = LIST_ROW_Y + (i - first) * LIST_ROW_H;

    // Right-hand hint: already in this workout, or what was used last time
    std::string hint;
    bool inSession = false;
    if (_hasSession) {
      for (const auto &ex : _session.exercises) {
        if (ex.name == e.name) {
          inSession = true;
          break;
        }
      }
    }
    auto last = _lastUsed.find(e.name);
    if (inSession)
      hint = "added";
    else if (last != _lastUsed.end())
      hint = formatSetLong(last->second.weight, last->second.reps);
    else if (_pickMode != PickMode::GROUP && e.groupIndex >= 0)
      hint = e.group;

    M5.Display.setFont(FONT_TEXT);
    int hintW = hint.empty() ? 0 : M5.Display.textWidth(hint.c_str()) + 20;
    if (!hint.empty()) {
      drawText(hint, SCREEN_W - MARGIN, y + LIST_ROW_H / 2, FONT_TEXT,
               DARKGREY, middle_right);
    }
    M5.Display.setFont(FONT_BOLD);
    drawText(fit(e.name, SCREEN_W - 2 * MARGIN - hintW), MARGIN,
             y + LIST_ROW_H / 2, FONT_BOLD, BLACK, middle_left);
    if (inSession)
      M5.Display.fillCircle(8, y + LIST_ROW_H / 2, 5, BLACK);

    drawRule(y + LIST_ROW_H - 1);
    addHit(0, y, SCREEN_W, LIST_ROW_H, A_PICK_EXERCISE, _pickList[i]);
  }
  drawPager(PAGER_Y, _pickPage, pages);
}

// ---------------------------------------------------------------- logger

void WorkoutApp::drawLogger() {
  const LoggedExercise &ex = _session.exercises[_activeExercise];
  M5.Display.setFont(FONT_TITLE);
  drawText(fit(ex.name, SCREEN_W - 2 * MARGIN - 135), MARGIN, 15, FONT_TITLE);
  drawButton(SCREEN_W - MARGIN - 120, 8, 120, 50, "Rename", false);
  addHit(SCREEN_W - MARGIN - 120, 8, 120, 50, A_RENAME_EXERCISE);

  std::string lastLine;
  auto last = _lastUsed.find(ex.name);
  if (last != _lastUsed.end()) {
    long days = now().stamp / 1440 - last->second.stamp / 1440;
    std::string when = days <= 0   ? "today"
                       : days == 1 ? "yesterday"
                                   : std::to_string(days) + " days ago";
    lastLine = "Last: " +
               formatSetLong(last->second.weight, last->second.reps) + " (" +
               when + ")";
  } else {
    lastLine = "First time - no history yet";
  }
  M5.Display.setFont(FONT_TEXT);
  drawText(fit(ex.group + "  |  " + lastLine, SCREEN_W - 2 * MARGIN), MARGIN,
           60, FONT_TEXT, DARKGREY);

  // Weight: -5 -2.5 [value] +2.5 +5, tap the value to type it
  drawText("Weight (kg)", MARGIN, 125, FONT_BOLD);
  drawText("tap number to type", SCREEN_W - MARGIN, 128, FONT_SMALL, DARKGREY,
           top_right);
  struct Step {
    int x;
    const char *label;
    int grams;
  };
  const Step weightSteps[] = {
      {10, "-5", -5000}, {100, "-2.5", -2500}, {355, "+2.5", 2500}, {445, "+5", 5000}};
  for (const auto &s : weightSteps) {
    drawButton(s.x, LOG_WEIGHT_Y, 85, LOG_ROW_H, s.label, false);
    addHit(s.x, LOG_WEIGHT_Y, 85, LOG_ROW_H, A_WEIGHT_STEP, s.grams);
  }
  addHit(LOG_WEIGHT_VAL_X, LOG_WEIGHT_Y, LOG_WEIGHT_VAL_W, LOG_ROW_H,
         A_TYPE_WEIGHT);
  drawWeightValue();

  drawText("Reps", MARGIN, 280, FONT_BOLD);
  drawButton(MARGIN, LOG_REPS_Y, 140, LOG_ROW_H, "-", false, FONT_BIG);
  addHit(MARGIN, LOG_REPS_Y, 140, LOG_ROW_H, A_REPS_STEP, -1);
  drawButton(SCREEN_W - MARGIN - 140, LOG_REPS_Y, 140, LOG_ROW_H, "+", false,
             FONT_BIG);
  addHit(SCREEN_W - MARGIN - 140, LOG_REPS_Y, 140, LOG_ROW_H, A_REPS_STEP, 1);
  addHit(LOG_REPS_VAL_X, LOG_REPS_Y, LOG_REPS_VAL_W, LOG_ROW_H, A_TYPE_REPS);
  drawRepsValue();

  drawLoggerLower();
}

void WorkoutApp::drawWeightValue() {
  int x = LOG_WEIGHT_VAL_X, y = LOG_WEIGHT_Y;
  M5.Display.fillRect(x, y, LOG_WEIGHT_VAL_W, LOG_ROW_H, WHITE);
  M5.Display.drawRect(x, y, LOG_WEIGHT_VAL_W, LOG_ROW_H, BLACK);
  M5.Display.drawRect(x + 1, y + 1, LOG_WEIGHT_VAL_W - 2, LOG_ROW_H - 2, BLACK);
  drawText(formatWeight(_weight), x + LOG_WEIGHT_VAL_W / 2, y + LOG_ROW_H / 2,
           FONT_BIG, BLACK, middle_center);
}

void WorkoutApp::drawRepsValue() {
  int x = LOG_REPS_VAL_X, y = LOG_REPS_Y;
  M5.Display.fillRect(x, y, LOG_REPS_VAL_W, LOG_ROW_H, WHITE);
  M5.Display.drawRect(x, y, LOG_REPS_VAL_W, LOG_ROW_H, BLACK);
  M5.Display.drawRect(x + 1, y + 1, LOG_REPS_VAL_W - 2, LOG_ROW_H - 2, BLACK);
  drawText(std::to_string(_reps), x + LOG_REPS_VAL_W / 2, y + LOG_ROW_H / 2,
           FONT_BIG, BLACK, middle_center);
}

// Primary action, the logged sets and the bottom buttons. Redrawn on its own
// after each set so the top of the screen doesn't flash.
void WorkoutApp::drawLoggerLower() {
  _hits.erase(std::remove_if(_hits.begin(), _hits.end(),
                             [](const Hit &h) {
                               return h.zone == LOGGER_LOWER_ZONE;
                             }),
              _hits.end());
  M5.Display.fillRect(0, LOG_LOWER_Y, SCREEN_W, CONTENT_BOTTOM - LOG_LOWER_Y,
                      WHITE);

  const auto &sets = _session.exercises[_activeExercise].sets;
  const int zone = LOGGER_LOWER_ZONE;
  int fullW = SCREEN_W - 2 * MARGIN;

  if (_selectedSet >= 0) {
    std::string label = "Update set " + std::to_string(_selectedSet + 1);
    drawButton(MARGIN, LOG_PRIMARY_Y, 320, LOG_PRIMARY_H, label, true,
               FONT_TITLE);
    addHit(MARGIN, LOG_PRIMARY_Y, 320, LOG_PRIMARY_H, A_LOG_SET, 0, zone);
    drawButton(MARGIN + 335, LOG_PRIMARY_Y, fullW - 335, LOG_PRIMARY_H,
               "Delete", false, FONT_TITLE);
    addHit(MARGIN + 335, LOG_PRIMARY_Y, fullW - 335, LOG_PRIMARY_H,
           A_DELETE_SET, 0, zone);
  } else {
    std::string label = "Log set " + std::to_string(sets.size() + 1);
    drawButton(MARGIN, LOG_PRIMARY_Y, fullW, LOG_PRIMARY_H, label, true,
               FONT_TITLE);
    addHit(MARGIN, LOG_PRIMARY_Y, fullW, LOG_PRIMARY_H, A_LOG_SET, 0, zone);
  }

  drawText("Sets", MARGIN, LOG_SETS_Y - 35, FONT_BOLD);
  if (!sets.empty()) {
    drawText(_selectedSet >= 0 ? "tap again to cancel" : "tap a set to edit",
             SCREEN_W - MARGIN, LOG_SETS_Y - 32, FONT_SMALL, DARKGREY,
             top_right);
  }
  drawRule(LOG_SETS_Y - 6);

  if (sets.empty()) {
    drawText("No sets yet", SCREEN_W / 2, LOG_SETS_Y + 60, FONT_TEXT,
             DARKGREY, top_center);
  }

  // Two columns, newest sets visible when there are more than fit
  const int maxVisible = LOG_SET_ROWS * 2;
  int first = std::max(0, (int)sets.size() - maxVisible);
  for (int i = first; i < (int)sets.size(); i++) {
    int slot = i - first;
    int col = slot / LOG_SET_ROWS, row = slot % LOG_SET_ROWS;
    int x = MARGIN + col * 255, y = LOG_SETS_Y + row * LOG_SET_ROW_H;
    bool selected = i == _selectedSet;
    if (selected)
      M5.Display.fillRoundRect(x, y + 2, 245, LOG_SET_ROW_H - 4, 8, BLACK);
    int fg = selected ? WHITE : BLACK;
    drawText(std::to_string(i + 1) + ".", x + 10, y + LOG_SET_ROW_H / 2,
             FONT_TEXT, selected ? WHITE : DARKGREY, middle_left);
    drawText(formatSetLong(sets[i].weight, sets[i].reps), x + 50,
             y + LOG_SET_ROW_H / 2, FONT_BOLD, fg, middle_left);
    addHit(x, y, 245, LOG_SET_ROW_H, A_SELECT_SET, i, zone);
  }

  int half = (fullW - 15) / 2;
  if (sets.empty()) {
    drawButton(MARGIN, LOG_BOTTOM_Y, half, LOG_BOTTOM_H, "Remove exercise",
               false);
    addHit(MARGIN, LOG_BOTTOM_Y, half, LOG_BOTTOM_H, A_REMOVE_EXERCISE, 0,
           zone);
  } else {
    drawButton(MARGIN, LOG_BOTTOM_Y, half, LOG_BOTTOM_H, "Workout", false);
    addHit(MARGIN, LOG_BOTTOM_Y, half, LOG_BOTTOM_H, A_TO_WORKOUT, 0, zone);
  }
  drawButton(MARGIN + half + 15, LOG_BOTTOM_Y, half, LOG_BOTTOM_H,
             "+ Next exercise", false);
  addHit(MARGIN + half + 15, LOG_BOTTOM_Y, half, LOG_BOTTOM_H,
         A_NEXT_EXERCISE, 0, zone);
}

// ---------------------------------------------------------------- summary / history

std::vector<WorkoutApp::Line> WorkoutApp::sessionLines(const Session &session) {
  std::vector<Line> lines;
  const int maxW = SCREEN_W - 2 * MARGIN - 20;
  for (const auto &ex : session.exercises) {
    M5.Display.setFont(FONT_BOLD);
    Line header = {fit(ex.name + "  (" + plural(ex.sets.size(), "set") + ")",
                       SCREEN_W - 2 * MARGIN),
                   true};
    lines.push_back(header);

    // Wrap the sets across as many lines as needed
    M5.Display.setFont(FONT_TEXT);
    std::string current;
    for (const auto &s : ex.sets) {
      std::string item = formatSetShort(s.weight, s.reps);
      std::string candidate = current.empty() ? item : current + "    " + item;
      if (!current.empty() && M5.Display.textWidth(candidate.c_str()) > maxW) {
        Line l = {current, false};
        lines.push_back(l);
        current = item;
      } else {
        current = candidate;
      }
    }
    if (!current.empty()) {
      Line l = {current, false};
      lines.push_back(l);
    }
  }
  return lines;
}

// Returns the number of pages
int WorkoutApp::drawSessionLines(const Session &session, int top, int bottom) {
  std::vector<Line> lines = sessionLines(session);
  int perPage = std::max(1, (bottom - top) / DETAIL_LINE_H);
  int pages = pageCount(lines.size(), perPage);
  _detailPage = std::max(0, std::min(_detailPage, pages - 1));

  int first = _detailPage * perPage;
  for (int i = first; i < (int)lines.size() && i < first + perPage; i++) {
    int y = top + (i - first) * DETAIL_LINE_H;
    if (lines[i].bold) {
      drawText(lines[i].text, MARGIN, y + 6, FONT_BOLD);
    } else {
      drawText(lines[i].text, MARGIN + 20, y + 4, FONT_TEXT, DARKGREY);
    }
  }
  return pages;
}

void WorkoutApp::drawSummary() {
  drawText("Workout complete", SCREEN_W / 2, 20, FONT_TITLE, BLACK,
           top_center);
  std::string stats = formatDuration(_viewed.durationMin) + " - " +
                      plural(_viewed.exercises.size(), "exercise") + " - " +
                      plural(setCount(_viewed), "set");
  drawText(stats, SCREEN_W / 2, 68, FONT_TEXT, BLACK, top_center);
  drawText("Volume " + formatThousands(volume(_viewed)) + " kg", SCREEN_W / 2,
           100, FONT_TEXT, DARKGREY, top_center);
  drawRule(140);

  int pages = drawSessionLines(_viewed, 155, DETAIL_BTN_Y - 10);

  int doneW = pages > 1 ? 300 : SCREEN_W - 2 * MARGIN;
  drawButton(MARGIN, DETAIL_BTN_Y, doneW, DETAIL_BTN_H, "Done", true,
             FONT_TITLE);
  addHit(MARGIN, DETAIL_BTN_Y, doneW, DETAIL_BTN_H, A_DONE);
  if (pages > 1) {
    if (_detailPage > 0) {
      drawButton(340, DETAIL_BTN_Y, 85, DETAIL_BTN_H, "<", false, FONT_TITLE);
      addHit(340, DETAIL_BTN_Y, 85, DETAIL_BTN_H, A_PAGE, -1);
    }
    if (_detailPage < pages - 1) {
      drawButton(435, DETAIL_BTN_Y, 85, DETAIL_BTN_H, ">", false, FONT_TITLE);
      addHit(435, DETAIL_BTN_Y, 85, DETAIL_BTN_H, A_PAGE, 1);
    }
  }
}

void WorkoutApp::drawHistoryList() {
  drawHeader("History", plural(_historyIds.size(), "workout"));
  drawRule(100);

  if (_historyIds.empty()) {
    drawText("No workouts yet.", SCREEN_W / 2, 330, FONT_TEXT, DARKGREY,
             top_center);
    return;
  }

  int first = _historyPage * HISTORY_ROWS;
  for (int i = 0; i < (int)_historyRows.size(); i++) {
    int y = HISTORY_ROW_Y + i * HISTORY_ROW_H;
    M5.Display.setFont(FONT_BOLD);
    drawText(fit(_historyRows[i].title, SCREEN_W - 2 * MARGIN), MARGIN,
             y + 12, FONT_BOLD);
    M5.Display.setFont(FONT_TEXT);
    drawText(fit(_historyRows[i].detail, SCREEN_W - 2 * MARGIN), MARGIN,
             y + 46, FONT_TEXT, DARKGREY);
    drawRule(y + HISTORY_ROW_H - 1);
    addHit(0, y, SCREEN_W, HISTORY_ROW_H, A_OPEN_HISTORY, first + i);
  }
  drawPager(PAGER_Y, _historyPage,
            pageCount(_historyIds.size(), HISTORY_ROWS));
}

void WorkoutApp::drawHistoryDetail() {
  std::string sub = _viewed.start + "-" + _viewed.end + " - " +
                    formatDuration(_viewed.durationMin) + " - " +
                    formatThousands(volume(_viewed)) + " kg";
  drawHeader(prettyDate(_viewed.date), sub);
  drawRule(100);

  int pages = drawSessionLines(_viewed, 115, DETAIL_BTN_Y - 10);

  drawDeleteButton();
  addHit(MARGIN, DETAIL_BTN_Y, 150, DETAIL_BTN_H, A_DELETE_VIEWED);
  if (!_hasSession) {
    drawButton(185, DETAIL_BTN_Y, 150, DETAIL_BTN_H, "Repeat", true);
    addHit(185, DETAIL_BTN_Y, 150, DETAIL_BTN_H, A_REPEAT_VIEWED);
  }
  if (pages > 1) {
    if (_detailPage > 0) {
      drawButton(350, DETAIL_BTN_Y, 80, DETAIL_BTN_H, "<", false, FONT_TITLE);
      addHit(350, DETAIL_BTN_Y, 80, DETAIL_BTN_H, A_PAGE, -1);
    }
    if (_detailPage < pages - 1) {
      drawButton(440, DETAIL_BTN_Y, 80, DETAIL_BTN_H, ">", false, FONT_TITLE);
      addHit(440, DETAIL_BTN_Y, 80, DETAIL_BTN_H, A_PAGE, 1);
    }
  }
}

void WorkoutApp::drawDeleteButton() {
  if (_screen != Screen::HISTORY_DETAIL)
    return;
  M5.Display.fillRect(MARGIN, DETAIL_BTN_Y, 150, DETAIL_BTN_H, WHITE);
  drawButton(MARGIN, DETAIL_BTN_Y, 150, DETAIL_BTN_H,
             _deleteArmed ? "Sure?" : "Delete", _deleteArmed);
}

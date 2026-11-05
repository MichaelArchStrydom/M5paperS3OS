#include "Storage.h"
#include <SD.h>

namespace storage {

bool sdAvailable() { return SD.cardType() != CARD_NONE; }

bool ensureDir(const char *path) {
  if (SD.exists(path))
    return true;
  return SD.mkdir(path);
}

bool readJson(const char *path, JsonDocument &doc) {
  String source = path;
  if (!SD.exists(path)) {
    String tmp = String(path) + ".tmp";
    if (!SD.exists(tmp))
      return false;
    source = tmp;
  }

  File f = SD.open(source, FILE_READ);
  if (!f)
    return false;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("storage: failed to parse %s: %s\n", source.c_str(),
                  err.c_str());
    return false;
  }
  return true;
}

bool writeJsonAtomic(const char *path, const JsonDocument &doc) {
  if (doc.overflowed()) {
    Serial.printf("storage: refusing to write %s, document overflowed\n", path);
    return false;
  }

  String tmp = String(path) + ".tmp";
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) {
    Serial.printf("storage: cannot open %s for writing\n", tmp.c_str());
    return false;
  }
  size_t expected = measureJson(doc);
  size_t written = serializeJson(doc, f);
  f.close();
  if (written != expected) {
    Serial.printf("storage: short write on %s (%u/%u)\n", tmp.c_str(),
                  (unsigned)written, (unsigned)expected);
    SD.remove(tmp);
    return false;
  }

  SD.remove(path);
  return SD.rename(tmp, path);
}

} // namespace storage

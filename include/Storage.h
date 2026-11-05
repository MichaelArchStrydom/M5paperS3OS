#pragma once
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// Keeps large JSON documents in PSRAM instead of the 320KB internal heap.
// Falls back to the regular heap if PSRAM is unavailable.
struct SpiRamAllocator {
  void *allocate(size_t size) {
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    return p ? p : malloc(size);
  }
  void deallocate(void *pointer) { heap_caps_free(pointer); }
  void *reallocate(void *ptr, size_t new_size) {
    void *p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    return p ? p : realloc(ptr, new_size);
  }
};

using PsramJsonDocument = BasicJsonDocument<SpiRamAllocator>;

namespace storage {

bool sdAvailable();
bool ensureDir(const char *path);

// Reads a JSON file. If the file is missing but a leftover "<path>.tmp" from
// an interrupted write exists, that is used instead.
bool readJson(const char *path, JsonDocument &doc);

// Writes to "<path>.tmp" first and only replaces the original once the full
// document is on the card, so a power loss mid-write never corrupts data.
// Refuses to write a document that overflowed its capacity.
bool writeJsonAtomic(const char *path, const JsonDocument &doc);

} // namespace storage

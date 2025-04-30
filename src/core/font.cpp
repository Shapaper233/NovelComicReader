#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <fcntl.h>
// #include <FS.h> // Already included via font.h
#include <map>      // Included via font.h now
#include <list>     // Included via font.h now
#include <string>   // Included via font.h now
#include <utility>  // Included via font.h now
#include <cstring>  // For memcpy
#include "font.h"
#include "display.h" // Include Display header for progress bar

// Define a reasonable cache size (e.g., 50KB). Adjust as needed for your device's RAM.
#define FONT_CACHE_MAX_SIZE_BYTES (25 * 1024)
//for 50kb:1600 characters of 16x16 size 711 characters of 24x24 size 400 characters of 32x32 size(or a mix thereof).This should significantly improve performance by reducing SD card access.

Font *Font::instance = nullptr;

// --- Fast Cache File Paths (Definition) ---
const char* Font::FAST_CACHE_BIN_PATH = "/font_data/fast.font";
const char* Font::FAST_CACHE_JSON_PATH = "/font_data/fast.json";
// --- End Fast Cache File Paths ---

Font::Font() : maxCacheSizeInBytes(FONT_CACHE_MAX_SIZE_BYTES), currentCacheSizeInBytes(0) {
    fontBuffer = nullptr;
    currentSize = 0;
    bufferSize = 0;
    // Cache map and list are initialized automatically
}

Font::~Font() {
    clearBuffer();
    clearMemoryCache(); // Ensure memory cache is cleared on destruction
}

// --- In-Memory Cache Implementation ---

void Font::clearMemoryCache() {
    for (auto const& [key, val] : cacheMap) {
        free(val.first.bitmap); // Free the allocated bitmap memory
    }
    cacheMap.clear();
    cacheLRUList.clear();
    currentCacheSizeInBytes = 0;
}

// Tries to retrieve an entry from the cache. Returns pointer if found, nullptr otherwise.
// Moves the found item to the front of the LRU list.
Font::CacheEntry* Font::cacheGet(const CacheKey& key) {
    auto it = cacheMap.find(key);
    if (it == cacheMap.end()) {
        return nullptr; // Not in cache
    }

    // Move the accessed item to the front of the LRU list
    cacheLRUList.splice(cacheLRUList.begin(), cacheLRUList, it->second.second);
    
    // Return pointer to the CacheEntry data
    return &(it->second.first);
}

// Evicts the least recently used item(s) until the cache is below its max size
void Font::cacheEvict() {
    while (currentCacheSizeInBytes > maxCacheSizeInBytes && !cacheLRUList.empty()) {
        // Get the least recently used key (back of the list)
        CacheKey lruKey = cacheLRUList.back();
        
        // Find the corresponding entry in the map
        auto it = cacheMap.find(lruKey);
        if (it != cacheMap.end()) {
            // Free the bitmap memory
            free(it->second.first.bitmap);
            // Update current cache size
            currentCacheSizeInBytes -= it->second.first.dataSize;
            // Remove from map
            cacheMap.erase(it);
        }
        // Remove from LRU list
        cacheLRUList.pop_back();
    }
}

// Adds a new bitmap to the cache
void Font::cachePut(const CacheKey& key, const uint8_t* data, uint16_t charSize, size_t dataSize) {
    // If the item is already in the cache, do nothing (or update if necessary, but get handles LRU update)
    if (cacheMap.count(key)) {
        // Optional: Could update LRU position here too, but cacheGet already does it.
        return; 
    }

    // Check if adding this item exceeds the cache size limit
    if (currentCacheSizeInBytes + dataSize > maxCacheSizeInBytes) {
        cacheEvict(); // Evict old items to make space
        // Re-check if eviction made enough space
        if (currentCacheSizeInBytes + dataSize > maxCacheSizeInBytes) {
             // Still not enough space even after eviction (maybe item is too large?)
             // In this case, we simply don't cache it.
             return; 
        }
    }

    // Allocate memory for the bitmap copy in the cache
    uint8_t* cachedBitmap = (uint8_t*)malloc(dataSize);
    if (!cachedBitmap) {
        return; // Allocation failed
    }
    memcpy(cachedBitmap, data, dataSize); // Copy the bitmap data

    // Add the new item to the front of the LRU list
    cacheLRUList.push_front(key);
    
    // Create the cache entry
    CacheEntry entry;
    entry.bitmap = cachedBitmap;
    entry.size = charSize;
    entry.dataSize = dataSize;

    // Add to the map, storing the entry and an iterator to its position in the LRU list
    cacheMap[key] = {entry, cacheLRUList.begin()};

    // Update the current cache size
    currentCacheSizeInBytes += dataSize;
}

// --- End In-Memory Cache Implementation ---


// --- Fast Cache Implementation ---

bool Font::saveFastFontCache() {
    Serial.println("Saving fast font cache..."); // Debug message

    // Ensure /font_data exists
    if (!SD.exists("/font_data")) {
        if (!SD.mkdir("/font_data")) {
             Serial.println("Failed to create /font_data directory.");
             return false;
        }
    }

    // 1. Open JSON file for writing metadata
    File jsonFile = SD.open(FAST_CACHE_JSON_PATH, FILE_WRITE);
    if (!jsonFile) {
        Serial.println("Failed to open fast.json for writing.");
        return false;
    }

    // 2. Open binary file for writing bitmap data
    File binFile = SD.open(FAST_CACHE_BIN_PATH, FILE_WRITE);
     if (!binFile) {
        Serial.println("Failed to open fast.font for writing.");
        jsonFile.close(); // Close the already opened json file
        return false;
    }

    // 3. Prepare JSON document
    // Estimate JSON size: ~50 bytes per entry (char, size, offset, dataSize, overhead)
    // Max entries could be maxCacheSizeInBytes / smallest_char_data_size
    // Example: 25KB cache / 32 bytes (16x16) = ~800 entries. 800 * 50 = 40KB JSON.
    // Use a dynamic document or increase static size if needed. Let's try 16KB for now.
    StaticJsonDocument<16384> doc;
    JsonArray entries = doc.to<JsonArray>();
    size_t currentOffset = 0;

    // 4. Iterate through the in-memory cache map
    for (const auto& [key, valuePair] : cacheMap) {
        const std::string& character = key.first;
        uint16_t size = key.second;
        const CacheEntry& entry = valuePair.first;

        // Write bitmap data to binary file
        size_t written = binFile.write(entry.bitmap, entry.dataSize);
        if (written != entry.dataSize) {
            Serial.printf("Error writing bitmap for %s (%d) to fast.font\n", character.c_str(), size);
            binFile.close();
            jsonFile.close();
            SD.remove(FAST_CACHE_BIN_PATH); // Clean up potentially corrupted files
            SD.remove(FAST_CACHE_JSON_PATH);
            return false;
        }

        // Add metadata entry to JSON
        JsonObject metaEntry = entries.createNestedObject();
        metaEntry["char"] = character; // ArduinoJson handles std::string
        metaEntry["size"] = size;
        metaEntry["offset"] = currentOffset;
        metaEntry["dataSize"] = entry.dataSize;

        currentOffset += entry.dataSize; // Update offset for the next entry
    }

    // 5. Serialize JSON to file
    if (serializeJson(doc, jsonFile) == 0) {
        Serial.println("Failed to write to fast.json.");
        binFile.close();
        jsonFile.close();
        SD.remove(FAST_CACHE_BIN_PATH); // Clean up potentially corrupted files
        SD.remove(FAST_CACHE_JSON_PATH);
        return false;
    }

    // 6. Close files
    jsonFile.close();
    binFile.close();

    Serial.printf("Fast font cache saved successfully. %d entries, %d bytes.\n", cacheMap.size(), currentOffset);
    return true;
}


bool Font::loadFastFontCache() {
    Serial.println("Loading fast font cache...");

    // 1. Check if cache files exist
    if (!SD.exists(FAST_CACHE_JSON_PATH) || !SD.exists(FAST_CACHE_BIN_PATH)) {
        Serial.println("Fast cache files not found.");
        return false; // Not an error, just no cache to load
    }

    // 2. Open JSON file for reading
    File jsonFile = SD.open(FAST_CACHE_JSON_PATH, FILE_READ);
    if (!jsonFile) {
        Serial.println("Failed to open fast.json for reading.");
        return false;
    }

    // 3. Deserialize JSON
    // Use DynamicJsonDocument for flexibility, or ensure static size matches save size.
    // Let's stick with static for now, assuming 16KB is enough.
    StaticJsonDocument<16384> doc;
    DeserializationError error = deserializeJson(doc, jsonFile);
    jsonFile.close(); // Close JSON file after reading

    if (error) {
        Serial.print("Failed to parse fast.json: ");
        Serial.println(error.c_str());
        SD.remove(FAST_CACHE_BIN_PATH); // Clean up potentially corrupt cache
        SD.remove(FAST_CACHE_JSON_PATH);
        return false;
    }

    // 4. Open binary file for reading bitmap data
    File binFile = SD.open(FAST_CACHE_BIN_PATH, FILE_READ);
     if (!binFile) {
        Serial.println("Failed to open fast.font for reading.");
        SD.remove(FAST_CACHE_JSON_PATH); // JSON was ok, but bin file missing/corrupt
        return false;
    }

    // 5. Clear existing memory cache before loading
    clearMemoryCache();

    // 6. Iterate through JSON entries and load data
    JsonArrayConst entries = doc.as<JsonArrayConst>(); // Use const view
    size_t totalEntries = entries.size();
    if (totalEntries == 0) {
        Serial.println("Fast cache JSON is empty.");
        binFile.close();
        return true; // Not an error, just nothing to load
    }

    size_t loadedCount = 0;
    size_t totalBytesRead = 0;
    uint8_t currentProgress = 0;
    uint8_t lastProgress = 0;

    // Get display instance and dimensions for progress bar
    Display& display = Display::getInstance();
    uint16_t barX = 20;
    uint16_t barY = display.height() - 30;
    uint16_t barW = display.width() - 40;
    uint16_t barH = 15;
    // Define area for status text, higher above the progress bar
    uint16_t statusX = barX;
    uint16_t statusY = barY - 20; // Place it 20 pixels above the bar's top edge
    uint16_t statusW = barW / 2; // Make status text area smaller
    uint16_t statusH = 16;       // Height for clearing (font size 1 uses 16px height)

    // Define area for the glyph, positioned above the status text, near the right
    uint16_t glyphSizeMax = 32; // Assume max glyph size is 32x32 for clearing/positioning
    uint16_t glyphX = barX + barW - glyphSizeMax - 5; // Position near the right edge
    uint16_t glyphY = statusY - glyphSizeMax - 5; // Place glyph 5px above status text area
    uint16_t glyphW = glyphSizeMax;
    uint16_t glyphH = glyphSizeMax;


    // Draw initial progress bar (0%)
    display.drawProgressBar(barX, barY, barW, barH, 0);
    // Clear status area initially
    display.getTFT()->fillRect(statusX, statusY, statusW, statusH, TFT_BLACK);
    // Clear glyph area initially
    display.getTFT()->fillRect(glyphX, glyphY, glyphW, glyphH, TFT_BLACK);

    // Time tracking for display updates
    unsigned long lastUpdateTime = 0; // Initialize to 0 to force first update
    const unsigned long UPDATE_INTERVAL_MS = 20;


    for (JsonObjectConst metaEntry : entries) {
        const char* character_cstr = metaEntry["char"];
        uint16_t size = metaEntry["size"];
        size_t offset = metaEntry["offset"];
        size_t dataSize = metaEntry["dataSize"];

        if (!character_cstr || size == 0 || dataSize == 0) {
            Serial.println("Skipping invalid entry in fast.json");
            continue;
        }

        std::string character(character_cstr);
        CacheKey key = {character, size};

        // Check if already loaded (shouldn't happen with clearMemoryCache)
        if (cacheMap.count(key)) {
            continue;
        }

        // Check if adding this item would exceed cache limits
        if (currentCacheSizeInBytes + dataSize > maxCacheSizeInBytes) {
             Serial.printf("Fast cache load exceeds memory limit (%d + %d > %d). Stopping load.\n",
                           currentCacheSizeInBytes, dataSize, maxCacheSizeInBytes);
             break; // Stop loading further entries
        }

        // Allocate memory for the bitmap
        uint8_t* bitmapData = (uint8_t*)malloc(dataSize);
        if (!bitmapData) {
            Serial.printf("Memory allocation failed for %s (%d) during fast cache load.\n", character.c_str(), size);
            continue; // Skip this entry, try others
        }

        // Seek and read bitmap data from binary file
        if (!binFile.seek(offset)) {
             Serial.printf("Seek failed in fast.font for %s (%d) at offset %d.\n", character.c_str(), size, offset);
             free(bitmapData);
             continue; // Skip this entry
        }
        size_t bytesRead = binFile.read(bitmapData, dataSize);
        if (bytesRead != dataSize) {
            Serial.printf("Read failed in fast.font for %s (%d). Expected %d, got %d.\n", character.c_str(), size, dataSize, bytesRead);
            free(bitmapData);
            continue; // Skip this entry
        }

        // Add to memory cache (manually, similar to cachePut but avoids checks/eviction)
        cacheLRUList.push_front(key); // Add to front (most recently used)
        CacheEntry entry;
        entry.bitmap = bitmapData;
        entry.size = size;
        entry.dataSize = dataSize;
        cacheMap[key] = {entry, cacheLRUList.begin()};
        currentCacheSizeInBytes += dataSize;
        loadedCount++;
        totalBytesRead += dataSize;

        // --- Conditional Display Update (every UPDATE_INTERVAL_MS) ---
        unsigned long currentTime = millis();
        if (currentTime - lastUpdateTime >= UPDATE_INTERVAL_MS) {

            // --- Draw the actual glyph using Display::drawCharacter ---
            // Clear previous glyph area (using max size for safety)
            display.getTFT()->fillRect(glyphX, glyphY, glyphW, glyphH, TFT_BLACK);

            // Calculate size multiplier relative to 16px base. Map 16->1, 24->2, 32->2.
            uint8_t sizeMultiplier = (size > 16) ? 2 : 1; // Simplified mapping for visibility
            uint16_t drawnGlyphSize = sizeMultiplier * 16; // Actual drawn width/height

            // Calculate dynamic position based on drawn size for consistent alignment
            uint16_t currentGlyphX = barX + barW - drawnGlyphSize - 5; // Right-align drawn glyph
            uint16_t currentGlyphY = statusY - drawnGlyphSize - 5; // Position drawn glyph above status

            // Draw the character using the standard method at the calculated dynamic position
            // This might re-read from cache, but ensures correct drawing logic
            display.drawCharacter(character.c_str(), currentGlyphX, currentGlyphY, sizeMultiplier, true);
            // --- End glyph drawing ---


            // Update status text (showing char, size, and count)
            // Clear previous status text
            display.getTFT()->fillRect(statusX, statusY, statusW, statusH, TFT_BLACK);
            // Prepare status string (showing Unicode, size, and count)
            uint32_t unicodeValue = utf8ToUnicode(character.c_str()); // Get Unicode code point
            char statusBuffer[70]; // Slightly larger buffer for "U+XXXX" format
            snprintf(statusBuffer, sizeof(statusBuffer), "Load: U+%04X(%u) (%zu/%zu)",
                     unicodeValue, size, loadedCount, totalEntries); // Show U+Code(size) (count/total) - Use loadedCount directly
            // Draw status text using small built-in font
            display.getTFT()->setTextColor(TFT_WHITE, TFT_BLACK);
            display.drawText(statusBuffer, statusX, statusY, 1, false); // size 1, useCustomFont=false


            // Update progress bar
            currentProgress = (loadedCount * 100) / totalEntries;
            if (currentProgress > lastProgress) { // Only redraw if percentage changed
                 display.drawProgressBar(barX, barY, barW, barH, currentProgress);
                 lastProgress = currentProgress;
            }

            lastUpdateTime = currentTime; // Update time of last display refresh
            // yield(); // Consider adding yield() here if updates cause blocking
        }
        // --- End Conditional Display Update ---

    } // End loop through JSON entries

    // Ensure progress bar shows 100% at the end
    display.drawProgressBar(barX, barY, barW, barH, 100);

    // Clear status and glyph areas after finishing
    display.getTFT()->fillRect(statusX, statusY, statusW, statusH, TFT_BLACK);
    display.getTFT()->fillRect(glyphX, glyphY, glyphW, glyphH, TFT_BLACK); // Use max size for final clear


    // 7. Close binary file
    binFile.close();

    Serial.printf("Fast font cache loaded successfully. %d entries, %d bytes.\n", loadedCount, totalBytesRead);
    return true;
}

// --- End Fast Cache Implementation ---


Font& Font::getInstance() {
    if (!instance) {
        instance = new Font();
    }
    return *instance;
}

// Clears the single temporary buffer used for SD loading
void Font::clearBuffer() {
    if (fontBuffer) {
        free(fontBuffer);
        fontBuffer = nullptr;
    }
    if (currentFontFile) {
        currentFontFile.close();
    }
}

bool Font::begin() {
    return true; // SD卡初始化已在其他地方完成
}

String Font::findIndexFile(const char* character) {
    // 在所有索引文件中查找字符
    File root = SD.open("/font_data");
    if (!root || !root.isDirectory()) {
        return "";
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory() && String(file.name()).startsWith("index_")) {
            // 读取并解析JSON
            StaticJsonDocument<4096> doc;
            DeserializationError error = deserializeJson(doc, file);
            
            if (!error && doc.containsKey(character)) {
                String filename = String(file.name());
                file.close();
                root.close();
                return filename;
            }
        }
        file = root.openNextFile();
    }
    
    root.close();
    return "";
}

bool Font::loadCharacter(const char* character, uint16_t size) {
    clearBuffer();

    // 尝试从缓存加载 (直接尝试打开)
    String cacheFilename = "/font_data/cache/" + String(character) + "_" + String(size) + ".font";
    File cacheFile = SD.open(cacheFilename); // Attempt to open the cache file directly

    if (cacheFile) { // Check if the file opened successfully (exists and is readable)
        // 从缓存加载字体数据
        bufferSize = (size * size + 7) / 8;
        fontBuffer = (uint8_t*)malloc(bufferSize);
        if (!fontBuffer) {
            cacheFile.close();
            return false; // Allocation failed
        }

        if (cacheFile.read(fontBuffer, bufferSize) == bufferSize) {
            // Successfully read from cache
            cacheFile.close();
            currentSize = size;
            return true; // Character loaded from cache
        } else {
            // Read failed (e.g., file corrupted or incomplete)
            clearBuffer(); // Clean up allocated buffer
            cacheFile.close();
            // Proceed to load from main font file below
        }
    }
    // If cacheFile is false here, it means the file didn't exist or couldn't be opened.
    // Continue to load from the main index/font files.
    
    // 查找包含该字符的索引文件
    String indexFile = findIndexFile(character);
    if (indexFile.length() == 0) {
        // 尝试加载 "☐" 的字体文件
        character = "☐";
        indexFile = findIndexFile(character);
        if (indexFile.length() == 0) {
            return false;
        }
    }
    
    // 读取索引文件
    File file = SD.open("/font_data/" + indexFile);
    if (!file) {
        return false;
    }
    
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    
    if (error) {
        file.close();
        return false;
    }
    file.close();
    
    if (!doc.containsKey(character) || !doc[character].containsKey(String(size))) {
        // 尝试加载 "☐" 的字体文件
        character = "☐";
        indexFile = findIndexFile(character);
        if (indexFile.length() == 0) {
            return false;
        }
        
        // 读取索引文件
        file = SD.open("/font_data/" + indexFile);
        if (!file) {
            return false;
        }
        
        DeserializationError error = deserializeJson(doc, file);
        if (error) {
            file.close();
            return false;
        }
        file.close();
        
        if (!doc.containsKey(character) || !doc[character].containsKey(String(size))) {
            return false;
        }
    }
    
    // 获取字体文件信息
    const char* fontFile = doc[character][String(size)]["file"];
    size_t offset = doc[character][String(size)]["offset"];
    
    // 打开字体文件
    currentFontFile = SD.open("/font_data/" + String(fontFile));
    if (!currentFontFile) {
        return false;
    }
    
    // 计算所需缓冲区大小
    bufferSize = (size * size + 7) / 8;  // 向上取整到字节
    fontBuffer = (uint8_t*)malloc(bufferSize);
    if (!fontBuffer) {
        currentFontFile.close();
        return false;
    }
    
    // 读取字体数据
    currentFontFile.seek(offset);
    if (currentFontFile.read(fontBuffer, bufferSize) != bufferSize) {
        clearBuffer();
        return false;
    }

    currentFontFile.close();
    currentSize = size;

   // 创建缓存目录（如果不存在）
    if (!SD.exists("/font_data/cache")) {
        SD.mkdir("/font_data/cache");
    }

    // 将字体数据保存到缓存
    // Re-open the file for writing. Note: cacheFile was declared earlier for reading.
    // We need to close it if it was opened for reading but failed (e.g., read error).
    // However, the logic flow ensures cacheFile is closed if reading fails.
    // If reading succeeded, it was closed after reading.
    // If the file didn't exist initially, cacheFile is already invalid/closed.
    // So, we can safely attempt to open for writing here.
    cacheFile = SD.open(cacheFilename, FILE_WRITE); // Re-assign to the existing cacheFile variable

    if (cacheFile) { // Check if opening for writing succeeded
        cacheFile.write(fontBuffer, bufferSize);
        cacheFile.close();
    }

    return true;
}

// Main function to get character bitmap. Checks memory cache first, then SD.
uint8_t* Font::getCharacterBitmap(const char* character, uint16_t size) {
    // Create the key for the cache lookup
    CacheKey key = {std::string(character), size};

    // 1. Check in-memory cache first
    CacheEntry* cachedEntry = cacheGet(key);
    if (cachedEntry) {
        // Memory cache hit! Return the cached bitmap directly.
        return cachedEntry->bitmap;
    }

    // 2. Memory cache miss: Load from SD card (using existing logic)
    //    loadCharacter handles SD cache check and loading from main font file.
    //    It loads the data into the temporary 'fontBuffer'.
    if (!loadCharacter(character, size)) {
        // Failed to load from SD (neither cache nor main file)
        return nullptr;
    }

    // --- Fast Cache Trigger ---
    // Increment counter since we loaded from SD (either cache file or main file)
    fontsReadFromSDCounter++;
    if (fontsReadFromSDCounter >= SAVE_CACHE_INTERVAL) {
        saveFastFontCache(); // Attempt to save the cache
        fontsReadFromSDCounter = 0; // Reset counter regardless of save success
    }
    // --- End Fast Cache Trigger ---


    // 3. Successfully loaded from SD into fontBuffer. Now add it to the memory cache.
    //    'bufferSize' holds the size of the data loaded into fontBuffer.
    if (fontBuffer && bufferSize > 0) {
        cachePut(key, fontBuffer, size, bufferSize);
    }

    // 4. Return the pointer to the data in fontBuffer (which was just loaded).
    //    Note: The data is ALSO now in the memory cache, but we return the
    //    pointer from the temporary buffer for consistency with the old logic flow.
    //    Subsequent calls for the same char/size should hit the memory cache.
    return fontBuffer; 
}

// Helper function to convert a UTF-8 character string to its Unicode code point
// Returns 0 if the input is null or invalid UTF-8 start byte
uint32_t Font::utf8ToUnicode(const char* utf8_char) {
    if (!utf8_char || *utf8_char == '\0') {
        return 0; // Invalid input
    }

    uint32_t unicode_val = 0;
    unsigned char c1 = utf8_char[0];

    if (c1 < 0x80) { // 1-byte sequence (ASCII)
        unicode_val = c1;
    } else if ((c1 & 0xE0) == 0xC0) { // 2-byte sequence
        if ((utf8_char[1] & 0xC0) == 0x80) {
            unicode_val = ((c1 & 0x1F) << 6) | (utf8_char[1] & 0x3F);
        } else return 0; // Invalid sequence
    } else if ((c1 & 0xF0) == 0xE0) { // 3-byte sequence
        if (((utf8_char[1] & 0xC0) == 0x80) && ((utf8_char[2] & 0xC0) == 0x80)) {
            unicode_val = ((c1 & 0x0F) << 12) | ((utf8_char[1] & 0x3F) << 6) | (utf8_char[2] & 0x3F);
        } else return 0; // Invalid sequence
    } else if ((c1 & 0xF8) == 0xF0) { // 4-byte sequence
        if (((utf8_char[1] & 0xC0) == 0x80) && ((utf8_char[2] & 0xC0) == 0x80) && ((utf8_char[3] & 0xC0) == 0x80)) {
            unicode_val = ((c1 & 0x07) << 18) | ((utf8_char[1] & 0x3F) << 12) | ((utf8_char[2] & 0x3F) << 6) | (utf8_char[3] & 0x3F);
        } else return 0; // Invalid sequence
    } else {
        return 0; // Invalid start byte
    }

    return unicode_val;
}


size_t Font::utf8Length(const char* str) {
    size_t len = 0;
    while (*str) {
        if ((*str & 0xC0) != 0x80) {
            len++;
        }
        str++;
    }
    return len;
}

String Font::getNextCharacter(const char* str, size_t& offset) {
    if (!str[offset]) {
        return "";
    }
    
    size_t start = offset;
    uint8_t first = str[offset];
    
    if (first < 0x80) {
        offset += 1;
    } else if ((first & 0xE0) == 0xC0) {
        offset += 2;
    } else if ((first & 0xF0) == 0xE0) {
        offset += 3;
    } else if ((first & 0xF8) == 0xF0) {
        offset += 4;
    }
    
    return String(str).substring(start, offset);
}

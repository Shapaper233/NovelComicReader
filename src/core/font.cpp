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

// Define a reasonable cache size (e.g., 50KB). Adjust as needed for your device's RAM.
#define FONT_CACHE_MAX_SIZE_BYTES (25 * 1024)
//for 50kb:1600 characters of 16x16 size 711 characters of 24x24 size 400 characters of 32x32 size(or a mix thereof).This should significantly improve performance by reducing SD card access.

Font *Font::instance = nullptr;

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

    // 尝试从缓存加载
    String cacheFilename = "/font_data/cache/" + String(character) + "_" + String(size) + ".font";
    if (SD.exists(cacheFilename)) {
        // 从缓存加载字体数据
        File cacheFile = SD.open(cacheFilename);
        if (!cacheFile) {
            return false;
        }

        bufferSize = (size * size + 7) / 8;
        fontBuffer = (uint8_t*)malloc(bufferSize);
        if (!fontBuffer) {
            cacheFile.close();
            return false;
        }

        if (cacheFile.read(fontBuffer, bufferSize) != bufferSize) {
            clearBuffer();
            cacheFile.close();
            return false;
        }

        cacheFile.close();
        currentSize = size;
        return true;
    }
    
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
    File cacheFile;
    if (!SD.exists(cacheFilename)) {
      cacheFile = SD.open(cacheFilename, FILE_WRITE);
    } else {
      cacheFile = SD.open(cacheFilename, FILE_WRITE);
    }
    
    if (cacheFile) {
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

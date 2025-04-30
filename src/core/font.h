#ifndef FONT_H
#define FONT_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <map>                // For in-memory cache map
#include <list>               // For LRU tracking list
#include <string>             // Using std::string for map keys (character part)
#include <utility>            // For std::pair used in map key
#include "../config/config.h" // Adjusted path

class Font
{
private:
    static Font *instance;
    File currentFontFile;
    uint8_t *fontBuffer;
    uint16_t currentSize;
    uint16_t bufferSize; // Size of the single 'fontBuffer' used for loading from SD

    // --- Fast Cache ---
    static const int SAVE_CACHE_INTERVAL = 10; // Save every 10 SD reads
    static const char* FAST_CACHE_BIN_PATH; // Defined in .cpp
    static const char* FAST_CACHE_JSON_PATH; // Defined in .cpp
    int fontsReadFromSDCounter = 0;
    // --- End Fast Cache ---

    // --- In-Memory Cache ---
    struct CacheEntry
    {
        uint8_t *bitmap; // Pointer to the bitmap data in RAM
        uint16_t size;   // Size of the character (e.g., 16 for 16x16)
        size_t dataSize; // Size of the bitmap data in bytes
    };

    // Key for the cache: pair of (UTF8 character string, character size)
    using CacheKey = std::pair<std::string, uint16_t>;
    // Iterator for the LRU list
    using CacheIterator = std::list<CacheKey>::iterator;

    // Map: Key -> {CacheEntry data, Iterator to LRU list}
    std::map<CacheKey, std::pair<CacheEntry, CacheIterator>> cacheMap;
    // List storing keys in LRU order (most recent at front)
    std::list<CacheKey> cacheLRUList;

    size_t maxCacheSizeInBytes;     // Max total size of bitmaps allowed in cache
    size_t currentCacheSizeInBytes; // Current total size of bitmaps in cache

    // Cache helper methods
    void cachePut(const CacheKey &key, const uint8_t *data, uint16_t charSize, size_t dataSize);
    CacheEntry *cacheGet(const CacheKey &key); // Returns pointer to entry if found, nullptr otherwise
    void cacheEvict();                         // Evicts the least recently used item(s) if cache is full
    void clearMemoryCache();                   // Clears the entire in-memory cache
    // --- End In-Memory Cache ---

    // --- Fast Cache Methods ---
    bool saveFastFontCache(); // Saves the current in-memory cache to SD
    // --- End Fast Cache Methods ---


    Font();  // Constructor: Initialize cache settings
    ~Font(); // Destructor: Clean up cache memory

    bool loadCharacter(const char *character, uint16_t size); // Loads from SD (cache or file) into fontBuffer
    void clearBuffer();                                       // Clears the single fontBuffer (doesn't affect memory cache directly)
    String findIndexFile(const char *character);              // Finds index file on SD

public:
    static Font &getInstance();

    // 初始化字体系统
    bool begin();

    // 加载快速字体缓存 (需要在 begin() 之后, SD卡初始化之后调用)
    bool loadFastFontCache(); // Loads the fast cache from SD into memory

    // 获取字符点阵数据
    uint8_t *getCharacterBitmap(const char *character, uint16_t size);

    // 获取字符大小（像素）
    uint16_t getCharacterWidth(uint16_t size) const { return size; }
    uint16_t getCharacterHeight(uint16_t size) const { return size; }

    // 判断是否为ASCII字符
    static bool isAscii(const char *character)
    {
        return (uint8_t)*character < 128;
    }

    // 计算UTF8字符串中的实际字符数
    static size_t utf8Length(const char *str);

    // 获取UTF8字符串中的下一个字符
    static String getNextCharacter(const char *str, size_t &offset);

    // Helper function to convert UTF-8 char string to Unicode code point
    static uint32_t utf8ToUnicode(const char* utf8_char);
};

#endif // FONT_H

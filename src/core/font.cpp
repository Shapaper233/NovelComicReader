#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <fcntl.h>
#include <FS.h>
#include "font.h"

Font* Font::instance = nullptr;

Font::Font() {
    fontBuffer = nullptr;
    currentSize = 0;
    bufferSize = 0;
}

Font::~Font() {
    clearBuffer();
}

Font& Font::getInstance() {
    if (!instance) {
        instance = new Font();
    }
    return *instance;
}

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

uint8_t* Font::getCharacterBitmap(const char* character, uint16_t size) {
    if (!loadCharacter(character, size)) {
        return nullptr;
    }
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

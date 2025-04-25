#ifndef FONT_H
#define FONT_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "../config/config.h" // Adjusted path

class Font {
private:
    static Font* instance;
    File currentFontFile;
    uint8_t* fontBuffer;
    uint16_t currentSize;
    uint16_t bufferSize;
    
    Font();
    ~Font();
    
    bool loadCharacter(const char* character, uint16_t size);
    void clearBuffer();
    String findIndexFile(const char* character);
    
public:
    static Font& getInstance();
    
    // 初始化字体系统
    bool begin();
    
    // 获取字符点阵数据
    uint8_t* getCharacterBitmap(const char* character, uint16_t size);
    
    // 获取字符大小（像素）
    uint16_t getCharacterWidth(uint16_t size) const { return size; }
    uint16_t getCharacterHeight(uint16_t size) const { return size; }
    
    // 判断是否为ASCII字符
    static bool isAscii(const char* character) {
        return (uint8_t)*character < 128;
    }
    
    // 计算UTF8字符串中的实际字符数
    static size_t utf8Length(const char* str);
    
    // 获取UTF8字符串中的下一个字符
    static String getNextCharacter(const char* str, size_t& offset);
};

#endif // FONT_H

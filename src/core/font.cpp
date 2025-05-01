#include <Arduino.h>      // 包含 Arduino 核心库
#include <SD.h>           // 包含 SD 卡库
#include <ArduinoJson.h>  // 包含 ArduinoJson 库，用于解析和生成 JSON
#include <fcntl.h>        // 文件控制定义 (可能在此处未使用)
// #include <FS.h> // 已通过 font.h 包含
#include <map>      // 已通过 font.h 包含
#include <list>     // 已通过 font.h 包含
#include <string>   // 已通过 font.h 包含
#include <utility>  // 已通过 font.h 包含
#include <cstring>  // 包含 C 字符串函数，如 memcpy
#include "font.h"   // 包含 Font 类的头文件
#include "display.h" // 包含 Display 头文件，用于显示加载进度条

// 定义内存缓存的最大大小 (例如 25KB)。根据设备的 RAM 进行调整。
#define FONT_CACHE_MAX_SIZE_BYTES (40 * 1024)
// 备注：50KB 缓存大约可存储: 1600 个 16x16 字符, 711 个 24x24 字符, 400 个 32x32 字符 (或混合)。
// 这应能通过减少 SD 卡访问显著提高性能。

// 初始化 Font 类的静态单例实例指针为空
Font *Font::instance = nullptr;

// --- 快速缓存文件路径 (定义) ---
const char* Font::FAST_CACHE_BIN_PATH = "/font_data/fast.font"; // 快速缓存二进制数据文件
const char* Font::FAST_CACHE_JSON_PATH = "/font_data/fast.json"; // 快速缓存 JSON 索引文件
// --- 快速缓存文件路径结束 ---

// Font 类构造函数
Font::Font() : maxCacheSizeInBytes(FONT_CACHE_MAX_SIZE_BYTES), currentCacheSizeInBytes(0) {
    fontBuffer = nullptr; // 初始化临时字体缓冲区指针为空
    currentSize = 0;      // 初始化当前缓冲区字体大小为 0
    bufferSize = 0;       // 初始化缓冲区大小为 0
    // cacheMap 和 cacheLRUList 会自动初始化
}

// Font 类析构函数
Font::~Font() {
    clearBuffer();      // 清理临时缓冲区
    clearMemoryCache(); // 确保在销毁时清理内存缓存
}

// --- 内存缓存实现 ---

// 清空内存缓存，释放所有分配的位图内存
void Font::clearMemoryCache() {
    // 遍历缓存 map
    for (auto const& [key, val] : cacheMap) {
        free(val.first.bitmap); // 释放缓存条目中位图数据占用的内存
    }
    cacheMap.clear();       // 清空 map
    cacheLRUList.clear();   // 清空 LRU 链表
    currentCacheSizeInBytes = 0; // 重置当前缓存大小计数
}

// 尝试从缓存中检索条目。如果找到则返回指针，否则返回 nullptr。
// 将找到的条目移动到 LRU 链表的前端。
Font::CacheEntry* Font::cacheGet(const CacheKey& key) {
    // 在 map 中查找键
    auto it = cacheMap.find(key);
    // 如果未找到
    if (it == cacheMap.end()) {
        return nullptr; // 不在缓存中
    }

    // 将访问到的条目移动到 LRU 链表的前端 (表示最近使用)
    // splice 操作将元素从其当前位置移动到链表的开头
    cacheLRUList.splice(cacheLRUList.begin(), cacheLRUList, it->second.second);

    // 返回指向缓存条目数据的指针
    return &(it->second.first);
}

// 根据 LRU 策略淘汰最少使用的条目，直到缓存大小低于最大限制
void Font::cacheEvict() {
    // 当缓存大小超过限制且 LRU 链表不为空时循环
    while (currentCacheSizeInBytes > maxCacheSizeInBytes && !cacheLRUList.empty()) {
        // 获取 LRU 链表末尾的键 (最少使用的)
        CacheKey lruKey = cacheLRUList.back();

        // 在 map 中查找对应的条目
        auto it = cacheMap.find(lruKey);
        if (it != cacheMap.end()) {
            // 释放位图内存
            free(it->second.first.bitmap);
            // 更新当前缓存大小
            currentCacheSizeInBytes -= it->second.first.dataSize;
            // 从 map 中移除
            cacheMap.erase(it);
        }
        // 从 LRU 链表中移除 (无论是否在 map 中找到，都应移除)
        cacheLRUList.pop_back();
    }
}

// 将新的位图数据添加到缓存中
void Font::cachePut(const CacheKey& key, const uint8_t* data, uint16_t charSize, size_t dataSize) {
    // 如果条目已存在于缓存中，则不执行任何操作 (或者可以更新，但 cacheGet 已处理 LRU 更新)
    if (cacheMap.count(key)) {
        // 可选：也可以在此处更新 LRU 位置，但 cacheGet 已经做了。
        return;
    }

    // 检查添加此条目是否会超过缓存大小限制
    if (currentCacheSizeInBytes + dataSize > maxCacheSizeInBytes) {
        cacheEvict(); // 淘汰旧条目以腾出空间
        // 再次检查淘汰后是否有足够空间
        if (currentCacheSizeInBytes + dataSize > maxCacheSizeInBytes) {
             // 即使淘汰后空间仍然不足 (可能条目本身太大?)
             // 在这种情况下，我们不缓存此条目。
             return;
        }
    }

    // 为缓存中的位图副本分配内存
    uint8_t* cachedBitmap = (uint8_t*)malloc(dataSize);
    if (!cachedBitmap) {
        return; // 内存分配失败
    }
    memcpy(cachedBitmap, data, dataSize); // 复制位图数据

    // 将新条目的键添加到 LRU 链表的前端
    cacheLRUList.push_front(key);

    // 创建缓存条目
    CacheEntry entry;
    entry.bitmap = cachedBitmap; // 指向新分配的内存
    entry.size = charSize;
    entry.dataSize = dataSize;

    // 将条目添加到 map 中，存储条目数据和指向其在 LRU 链表中位置的迭代器
    cacheMap[key] = {entry, cacheLRUList.begin()};

    // 更新当前缓存大小
    currentCacheSizeInBytes += dataSize;
}

// --- 内存缓存实现结束 ---


// --- 快速缓存实现 ---

// 将当前内存缓存保存到 SD 卡上的快速缓存文件
bool Font::saveFastFontCache() {
    Serial.println("正在保存快速字体缓存..."); // 调试信息

    // 确保 /font_data 目录存在
    if (!SD.exists("/font_data")) {
        if (!SD.mkdir("/font_data")) {
             Serial.println("创建 /font_data 目录失败。");
             return false;
        }
    }

    // 1. 打开 JSON 文件用于写入元数据
    File jsonFile = SD.open(FAST_CACHE_JSON_PATH, FILE_WRITE);
    if (!jsonFile) {
        Serial.println("打开 fast.json 进行写入失败。");
        return false;
    }

    // 2. 打开二进制文件用于写入位图数据
    File binFile = SD.open(FAST_CACHE_BIN_PATH, FILE_WRITE);
     if (!binFile) {
        Serial.println("打开 fast.font 进行写入失败。");
        jsonFile.close(); // 关闭已打开的 json 文件
        return false;
    }

    // 3. 准备 JSON 文档
    // 估计 JSON 大小：每个条目约 50 字节 (字符, 大小, 偏移量, 数据大小, 开销)
    // 最大条目数可能是 maxCacheSizeInBytes / 最小字符数据大小
    // 示例：25KB 缓存 / 32 字节 (16x16) = 约 800 条目。800 * 50 = 40KB JSON。
    // 如果需要，使用动态文档或增加静态大小。暂时尝试 16KB。
    StaticJsonDocument<16384> doc; // 使用 16KB 的静态 JSON 文档
    JsonArray entries = doc.to<JsonArray>(); // 创建 JSON 数组
    size_t currentOffset = 0; // 当前在二进制文件中的偏移量

    // 4. 遍历内存缓存 map
    for (const auto& [key, valuePair] : cacheMap) {
        const std::string& character = key.first; // 获取字符 (std::string)
        uint16_t size = key.second;               // 获取大小
        const CacheEntry& entry = valuePair.first; // 获取缓存条目数据

        // 将位图数据写入二进制文件
        size_t written = binFile.write(entry.bitmap, entry.dataSize);
        if (written != entry.dataSize) {
            Serial.printf("将 %s (%d) 的位图写入 fast.font 时出错\n", character.c_str(), size);
            binFile.close();
            jsonFile.close();
            SD.remove(FAST_CACHE_BIN_PATH); // 清理可能损坏的文件
            SD.remove(FAST_CACHE_JSON_PATH);
            return false;
        }

        // 将元数据条目添加到 JSON 数组
        JsonObject metaEntry = entries.createNestedObject();
        metaEntry["char"] = character; // ArduinoJson 可以处理 std::string
        metaEntry["size"] = size;
        metaEntry["offset"] = currentOffset; // 记录在二进制文件中的偏移量
        metaEntry["dataSize"] = entry.dataSize; // 记录数据大小

        currentOffset += entry.dataSize; // 更新下一个条目的偏移量
    }

    // 5. 将 JSON 文档序列化到文件
    if (serializeJson(doc, jsonFile) == 0) {
        Serial.println("写入 fast.json 失败。");
        binFile.close();
        jsonFile.close();
        SD.remove(FAST_CACHE_BIN_PATH); // 清理可能损坏的文件
        SD.remove(FAST_CACHE_JSON_PATH);
        return false;
    }

    // 6. 关闭文件
    jsonFile.close();
    binFile.close();

    Serial.printf("快速字体缓存保存成功。%d 个条目，%d 字节。\n", cacheMap.size(), currentOffset);
    return true;
}


// 从 SD 卡加载快速字体缓存到内存缓存
bool Font::loadFastFontCache() {
    Serial.println("正在加载快速字体缓存...");

    // 1. 检查缓存文件是否存在
    if (!SD.exists(FAST_CACHE_JSON_PATH) || !SD.exists(FAST_CACHE_BIN_PATH)) {
        Serial.println("未找到快速缓存文件。");
        return false; // 不是错误，只是没有缓存可加载
    }

    // 2. 打开 JSON 文件进行读取
    File jsonFile = SD.open(FAST_CACHE_JSON_PATH, FILE_READ);
    if (!jsonFile) {
        Serial.println("打开 fast.json 进行读取失败。");
        return false;
    }

    // 3. 反序列化 JSON
    // 使用 DynamicJsonDocument 以获得灵活性，或确保静态大小与保存时匹配。
    // 暂时坚持使用静态，假设 16KB 足够。
    StaticJsonDocument<16384> doc;
    DeserializationError error = deserializeJson(doc, jsonFile);
    jsonFile.close(); // 读取后关闭 JSON 文件

    if (error) {
        Serial.print("解析 fast.json 失败: ");
        Serial.println(error.c_str());
        SD.remove(FAST_CACHE_BIN_PATH); // 清理可能损坏的缓存
        SD.remove(FAST_CACHE_JSON_PATH);
        return false;
    }

    // 4. 打开二进制文件以读取位图数据
    File binFile = SD.open(FAST_CACHE_BIN_PATH, FILE_READ);
     if (!binFile) {
        Serial.println("打开 fast.font 进行读取失败。");
        SD.remove(FAST_CACHE_JSON_PATH); // JSON 正常，但 bin 文件丢失/损坏
        return false;
    }

    // 5. 加载前清除现有的内存缓存
    clearMemoryCache();

    // 6. 遍历 JSON 条目并加载数据
    JsonArrayConst entries = doc.as<JsonArrayConst>(); // 使用 const 视图
    size_t totalEntries = entries.size(); // 获取总条目数
    if (totalEntries == 0) {
        Serial.println("快速缓存 JSON 为空。");
        binFile.close();
        return true; // 不是错误，只是没有内容可加载
    }

    size_t loadedCount = 0;     // 已加载条目计数
    size_t totalBytesRead = 0;  // 已读取总字节数
    uint8_t currentProgress = 0;// 当前进度百分比
    uint8_t lastProgress = 0;   // 上次绘制的进度百分比

    // 获取显示实例和尺寸用于进度条
    Display& display = Display::getInstance();
    uint16_t barX = 20;                         // 进度条 X 坐标
    uint16_t barY = display.height() - 30;      // 进度条 Y 坐标 (屏幕底部)
    uint16_t barW = display.width() - 40;       // 进度条宽度
    uint16_t barH = 15;                         // 进度条高度
    // 定义状态文本区域，位于进度条上方
    uint16_t statusX = barX;                    // 状态文本 X
    uint16_t statusY = barY - 20;               // 状态文本 Y (进度条上方 20 像素)
    uint16_t statusW = barW / 2;                // 状态文本区域宽度 (较小)
    uint16_t statusH = 16;                      // 状态文本区域高度 (字体大小 1 对应 16px 高)

    // 定义字形显示区域，位于状态文本上方，靠近右侧
    uint16_t glyphSizeMax = 32;                 // 假设最大字形为 32x32，用于清除和定位
    uint16_t glyphX = barX + barW - glyphSizeMax - 5; // 靠近右边缘定位
    uint16_t glyphY = statusY - glyphSizeMax - 5; // 字形位于状态文本上方 5px
    uint16_t glyphW = glyphSizeMax;             // 字形区域宽度
    uint16_t glyphH = glyphSizeMax;             // 字形区域高度


    // 绘制初始进度条 (0%)
    display.drawProgressBar(barX, barY, barW, barH, 0);
    // 初始清除状态区域
    display.getTFT()->fillRect(statusX, statusY, statusW, statusH, TFT_BLACK);
    // 初始清除字形区域
    display.getTFT()->fillRect(glyphX, glyphY, glyphW, glyphH, TFT_BLACK);

    // 用于显示更新的时间跟踪
    unsigned long lastUpdateTime = 0;             // 初始化为 0 以强制首次更新
    const unsigned long UPDATE_INTERVAL_MS = 20; // 显示更新间隔 (毫秒)


    // 遍历 JSON 中的每个元数据条目
    for (JsonObjectConst metaEntry : entries) {
        const char* character_cstr = metaEntry["char"]; // 获取字符 C 字符串
        uint16_t size = metaEntry["size"];             // 获取大小
        size_t offset = metaEntry["offset"];           // 获取偏移量
        size_t dataSize = metaEntry["dataSize"];       // 获取数据大小

        // 检查条目有效性
        if (!character_cstr || size == 0 || dataSize == 0) {
            Serial.println("跳过 fast.json 中的无效条目");
            continue;
        }

        std::string character(character_cstr); // 创建 std::string
        CacheKey key = {character, size};      // 创建缓存键

        // 检查是否已加载 (理论上不应发生，因为 clearMemoryCache 已调用)
        if (cacheMap.count(key)) {
            continue;
        }

        // 检查添加此条目是否会超出缓存限制
        if (currentCacheSizeInBytes + dataSize > maxCacheSizeInBytes) {
             Serial.printf("快速缓存加载超出内存限制 (%d + %d > %d)。停止加载。\n",
                           currentCacheSizeInBytes, dataSize, maxCacheSizeInBytes);
             break; // 停止加载更多条目
        }

        // 为位图分配内存
        uint8_t* bitmapData = (uint8_t*)malloc(dataSize);
        if (!bitmapData) {
            Serial.printf("在快速缓存加载期间为 %s (%d) 分配内存失败。\n", character.c_str(), size);
            continue; // 跳过此条目，尝试其他条目
        }

        // 在二进制文件中定位并读取位图数据
        if (!binFile.seek(offset)) {
             Serial.printf("在 fast.font 中为 %s (%d) 定位偏移量 %d 失败。\n", character.c_str(), size, offset);
             free(bitmapData); // 释放已分配的内存
             continue; // 跳过此条目
        }
        size_t bytesRead = binFile.read(bitmapData, dataSize); // 读取数据
        if (bytesRead != dataSize) {
            Serial.printf("在 fast.font 中为 %s (%d) 读取失败。预期 %d，得到 %d。\n", character.c_str(), size, dataSize, bytesRead);
            free(bitmapData); // 释放已分配的内存
            continue; // 跳过此条目
        }

        // 手动添加到内存缓存 (类似于 cachePut 但避免检查/淘汰)
        cacheLRUList.push_front(key); // 添加到 LRU 链表前端 (最近使用)
        CacheEntry entry;
        entry.bitmap = bitmapData;
        entry.size = size;
        entry.dataSize = dataSize;
        cacheMap[key] = {entry, cacheLRUList.begin()}; // 添加到 map
        currentCacheSizeInBytes += dataSize; // 更新缓存大小
        loadedCount++; // 增加已加载计数
        totalBytesRead += dataSize; // 增加总读取字节数

        // --- 条件性显示更新 (每隔 UPDATE_INTERVAL_MS) ---
        unsigned long currentTime = millis(); // 获取当前时间
        if (currentTime - lastUpdateTime >= UPDATE_INTERVAL_MS) {

            // --- 使用 Display::drawCharacter 绘制实际字形 ---
            // 清除之前的字形区域 (使用最大尺寸以确保安全)
            display.getTFT()->fillRect(glyphX, glyphY, glyphW, glyphH, TFT_BLACK);

            // 计算相对于 16px 基础的大小乘数。映射 16->1, 24->2, 32->2。
            uint8_t sizeMultiplier = (size > 16) ? 2 : 1; // 简化的映射以保证可见性
            uint16_t drawnGlyphSize = sizeMultiplier * 16; // 实际绘制的宽度/高度

            // 根据绘制大小计算动态位置以实现一致对齐
            uint16_t currentGlyphX = barX + barW - drawnGlyphSize - 5; // 右对齐绘制的字形
            uint16_t currentGlyphY = statusY - drawnGlyphSize - 5; // 将绘制的字形置于状态文本之上

            // 使用标准方法在计算出的动态位置绘制字符
            // 这可能会从缓存中重新读取，但确保了正确的绘制逻辑
            display.drawCharacter(character.c_str(), currentGlyphX, currentGlyphY, sizeMultiplier, true);
            // --- 字形绘制结束 ---


            // 更新状态文本 (显示字符、大小和计数)
            // 清除之前的状态文本
            display.getTFT()->fillRect(statusX, statusY, statusW, statusH, TFT_BLACK);
            // 准备状态字符串 (显示 Unicode、大小和计数)
            uint32_t unicodeValue = utf8ToUnicode(character.c_str()); // 获取 Unicode 码点
            char statusBuffer[70]; // 稍大的缓冲区用于 "U+XXXX" 格式
            snprintf(statusBuffer, sizeof(statusBuffer), "Load: U+%04X(%u) (%zu/%zu)",
                     unicodeValue, size, loadedCount, totalEntries); // 显示 U+码点(大小) (计数/总数) - 直接使用 loadedCount
            // 使用小的内建字体绘制状态文本
            display.getTFT()->setTextColor(TFT_WHITE, TFT_BLACK);
            display.drawText(statusBuffer, statusX, statusY, 1, false); // 大小 1, useCustomFont=false


            // 更新进度条
            currentProgress = (loadedCount * 100) / totalEntries; // 计算当前百分比
            if (currentProgress > lastProgress) { // 仅当百分比变化时才重绘
                 display.drawProgressBar(barX, barY, barW, barH, currentProgress);
                 lastProgress = currentProgress; // 更新上次绘制的百分比
            }

            lastUpdateTime = currentTime; // 更新上次显示刷新的时间
            // yield(); // 如果更新导致阻塞，考虑在此处添加 yield()
        }
        // --- 条件性显示更新结束 ---

    } // 结束遍历 JSON 条目

    // 确保进度条最后显示 100%
    display.drawProgressBar(barX, barY, barW, barH, 100);

    // 完成后清除状态和字形区域
    display.getTFT()->fillRect(statusX, statusY, statusW, statusH, TFT_BLACK);
    display.getTFT()->fillRect(glyphX, glyphY, glyphW, glyphH, TFT_BLACK); // 使用最大尺寸进行最终清除


    // 7. 关闭二进制文件
    binFile.close();

    Serial.printf("快速字体缓存加载成功。%d 个条目，%d 字节。\n", loadedCount, totalBytesRead);
    return true;
}

// --- 快速缓存实现结束 ---


// 获取 Font 类的单例实例
Font& Font::getInstance() {
    if (!instance) {
        instance = new Font();
    }
    return *instance;
}

// 清理用于 SD 加载的单个临时缓冲区
void Font::clearBuffer() {
    if (fontBuffer) {
        free(fontBuffer); // 释放缓冲区内存
        fontBuffer = nullptr; // 将指针置空
    }
    if (currentFontFile) { // 如果当前字体文件已打开
        currentFontFile.close(); // 关闭文件
    }
}

// 初始化字体系统 (目前仅返回 true，假设 SD 卡已在别处初始化)
bool Font::begin() {
    return true;
}

// 在 SD 卡上查找包含指定字符的索引文件 (.idx)
String Font::findIndexFile(const char* character) {
    // 打开字体数据根目录
    File root = SD.open("/font_data");
    if (!root || !root.isDirectory()) {
        Serial.println("无法打开 /font_data 目录");
        return ""; // 无法打开目录或不是目录
    }

    File file = root.openNextFile(); // 打开目录中的第一个文件/子目录
    while (file) { // 循环直到没有更多文件
        // 如果不是目录且文件名以 "index_" 开头
        if (!file.isDirectory() && String(file.name()).startsWith("index_")) {
            // 读取并解析 JSON 索引文件
            // 增加 JSON 文档大小以容纳更多索引条目
            StaticJsonDocument<8192> doc; // 尝试 8KB
            DeserializationError error = deserializeJson(doc, file); // 从文件流解析

            // 如果解析成功且 JSON 包含该字符的键
            if (!error && doc.containsKey(character)) {
                String filename = String(file.name()); // 获取文件名
                file.close(); // 关闭当前索引文件
                root.close(); // 关闭根目录
                return filename; // 返回找到的索引文件名
            }
            // 如果解析失败或不包含键，继续查找下一个文件
        }
        file.close(); // 关闭当前文件（无论是否是索引文件）
        file = root.openNextFile(); // 打开下一个文件
    }

    root.close(); // 关闭根目录
    return ""; // 未找到包含该字符的索引文件
}


// 从 SD 卡加载字符数据 (先检查 SD 缓存，再检查主索引/字体文件)
bool Font::loadCharacter(const char* character, uint16_t size) {
    clearBuffer(); // 清理之前的临时缓冲区和文件句柄

    // --- 步骤 1: 尝试从 SD 卡上的单个字符缓存文件加载 ---
    // 构建缓存文件名: /font_data/cache/字符_大小.font
    String cacheFilename = "/font_data/cache/" + String(character) + "_" + String(size) + ".font";
    File cacheFile = SD.open(cacheFilename, FILE_READ); // 尝试直接以读取模式打开缓存文件

    if (cacheFile) { // 如果文件成功打开 (存在且可读)
        // 计算所需的缓冲区大小 (字节) = (宽度 * 高度 + 7) / 8
        bufferSize = (size * size + 7) / 8;
        fontBuffer = (uint8_t*)malloc(bufferSize); // 分配内存
        if (!fontBuffer) {
            cacheFile.close(); // 关闭文件
            Serial.println("为 SD 缓存分配内存失败");
            return false; // 内存分配失败
        }

        // 从缓存文件读取数据到缓冲区
        if (cacheFile.read(fontBuffer, bufferSize) == bufferSize) {
            // 成功从缓存读取
            cacheFile.close(); // 关闭文件
            currentSize = size; // 更新当前缓冲区大小
            // Serial.printf("从 SD 缓存加载: %s (%d)\n", character, size); // 调试信息
            return true; // 字符已从 SD 缓存加载
        } else {
            // 读取失败 (例如，文件损坏或不完整)
            Serial.printf("从 SD 缓存文件 %s 读取失败\n", cacheFilename.c_str());
            clearBuffer(); // 清理分配的缓冲区
            cacheFile.close(); // 关闭文件
            // 继续尝试从主字体文件加载 (下面)
        }
    }
    // 如果 cacheFile 为 false，表示文件不存在或无法打开。
    // 继续从主索引/字体文件加载。

    // --- 步骤 2: 从主索引和字体文件加载 ---
    // 查找包含该字符的索引文件 (.idx)
    String indexFileName = findIndexFile(character);
    if (indexFileName.length() == 0) {
        // 如果找不到原始字符，尝试加载备用字符 "☐"
        Serial.printf("未找到字符 '%s' 的索引，尝试 '☐'\n", character);
        character = "☐"; // 使用备用字符
        indexFileName = findIndexFile(character); // 再次查找索引
        if (indexFileName.length() == 0) {
            Serial.println("也未找到备用字符 '☐' 的索引");
            return false; // 如果连备用字符都找不到，则失败
        }
    }

    // 读取找到的索引文件
    File indexFile = SD.open("/font_data/" + indexFileName, FILE_READ);
    if (!indexFile) {
        Serial.printf("无法打开索引文件: %s\n", indexFileName.c_str());
        return false;
    }

    // 解析索引文件的 JSON 内容
    StaticJsonDocument<8192> doc; // 使用与 findIndexFile 中相同的大小
    DeserializationError error = deserializeJson(doc, indexFile);
    indexFile.close(); // 解析后关闭索引文件

    if (error) {
        Serial.printf("解析索引文件 %s 失败: %s\n", indexFileName.c_str(), error.c_str());
        return false;
    }

    // 检查索引中是否存在该字符和指定大小的条目
    if (!doc.containsKey(character) || !doc[character].containsKey(String(size))) {
        // 如果原始字符或大小不存在，再次尝试备用字符 "☐"
        if (strcmp(character, "☐") != 0) { // 避免无限递归
            Serial.printf("索引 %s 中未找到 '%s' (%d)，尝试 '☐'\n", indexFileName.c_str(), character, size);
            character = "☐"; // 使用备用字符
            // 重新检查备用字符是否存在于已加载的 doc 中
            if (!doc.containsKey(character) || !doc[character].containsKey(String(size))) {
                 Serial.printf("索引 %s 中也未找到 '☐' (%d)\n", indexFileName.c_str(), size);
                 return false; // 如果备用字符和大小也不存在，则失败
            }
            // 如果备用字符存在，则继续使用备用字符的信息
        } else {
             Serial.printf("索引 %s 中未找到 '☐' (%d)\n", indexFileName.c_str(), size);
             return false; // 如果连备用字符都找不到，则失败
        }
    }

    // 从 JSON 中获取字体文件名和偏移量
    const char* fontFileName = doc[character][String(size)]["file"];
    size_t offset = doc[character][String(size)]["offset"];

    if (!fontFileName) {
        Serial.printf("索引 %s 中 '%s' (%d) 条目缺少 'file'\n", indexFileName.c_str(), character, size);
        return false;
    }

    // 打开对应的字体数据文件 (.font)
    currentFontFile = SD.open("/font_data/" + String(fontFileName), FILE_READ);
    if (!currentFontFile) {
        Serial.printf("无法打开字体文件: %s\n", fontFileName);
        return false;
    }

    // 计算并分配临时缓冲区大小
    bufferSize = (size * size + 7) / 8;  // 向上取整到字节
    fontBuffer = (uint8_t*)malloc(bufferSize);
    if (!fontBuffer) {
        Serial.println("为字体数据分配缓冲区失败");
        currentFontFile.close();
        return false;
    }

    // 定位到文件中的偏移量并读取字体数据
    if (!currentFontFile.seek(offset)) {
        Serial.printf("在字体文件 %s 中定位偏移量 %d 失败\n", fontFileName, offset);
        clearBuffer(); // 清理缓冲区和文件句柄
        return false;
    }
    if (currentFontFile.read(fontBuffer, bufferSize) != bufferSize) {
        Serial.printf("从字体文件 %s 读取数据失败\n", fontFileName);
        clearBuffer(); // 清理缓冲区和文件句柄
        return false;
    }

    // 成功从主文件加载
    currentFontFile.close(); // 关闭字体文件
    currentSize = size; // 更新当前缓冲区大小
    // Serial.printf("从主文件加载: %s (%d)\n", character, size); // 调试信息

    // --- 步骤 3: 将从主文件加载的数据写入 SD 缓存 ---
    // 创建缓存目录（如果不存在）
    if (!SD.exists("/font_data/cache")) {
        SD.mkdir("/font_data/cache");
    }

    // 尝试以写入模式打开（或创建）缓存文件
    // 注意：cacheFile 变量在函数开头声明，用于读取。这里重新赋值。
    // 之前的逻辑确保如果读取失败或文件不存在，cacheFile 已关闭或无效。
    cacheFile = SD.open(cacheFilename, FILE_WRITE); // 重新赋值给现有的 cacheFile 变量

    if (cacheFile) { // 检查写入模式打开是否成功
        cacheFile.write(fontBuffer, bufferSize); // 将缓冲区内容写入缓存文件
        cacheFile.close(); // 关闭缓存文件
        // Serial.printf("已写入 SD 缓存: %s\n", cacheFilename.c_str()); // 调试信息
    } else {
        Serial.printf("无法打开 SD 缓存文件 %s 进行写入\n", cacheFilename.c_str());
        // 即使无法写入缓存，加载仍然成功，所以继续
    }

    return true; // 字符已从主文件加载（并尝试了缓存）
}

// 获取字符位图的主函数。首先检查内存缓存，然后检查 SD 卡。
uint8_t* Font::getCharacterBitmap(const char* character, uint16_t size) {
    // 创建用于缓存查找的键
    CacheKey key = {std::string(character), size};

    // 1. 首先检查内存缓存
    CacheEntry* cachedEntry = cacheGet(key);
    if (cachedEntry) {
        // 内存缓存命中！直接返回缓存的位图。
        // Serial.printf("内存缓存命中: %s (%d)\n", character, size); // 调试信息
        return cachedEntry->bitmap;
    }

    // 2. 内存缓存未命中：从 SD 卡加载 (使用现有逻辑)
    //    loadCharacter 函数处理 SD 缓存检查和从主字体文件加载。
    //    它将数据加载到临时的 'fontBuffer' 中。
    // Serial.printf("内存缓存未命中，尝试从 SD 加载: %s (%d)\n", character, size); // 调试信息
    if (!loadCharacter(character, size)) {
        // 从 SD 加载失败 (缓存和主文件都失败)
        Serial.printf("从 SD 加载 %s (%d) 失败\n", character, size);
        return nullptr;
    }

    // --- 触发快速缓存保存 ---
    // 因为我们从 SD 加载了数据 (无论是 SD 缓存文件还是主文件)，增加计数器
    fontsReadFromSDCounter++;
    // 如果计数器达到阈值
    if (fontsReadFromSDCounter >= SAVE_CACHE_INTERVAL) {
        Serial.printf("达到 SD 读取阈值 (%d)，尝试保存快速缓存...\n", SAVE_CACHE_INTERVAL);
        saveFastFontCache(); // 尝试保存快速缓存
        fontsReadFromSDCounter = 0; // 无论保存是否成功，都重置计数器
    }
    // --- 触发快速缓存保存结束 ---


    // 3. 成功从 SD 加载到 fontBuffer。现在将其添加到内存缓存。
    //    'bufferSize' 保存了加载到 fontBuffer 的数据大小。
    if (fontBuffer && bufferSize > 0) {
        // Serial.printf("将从 SD 加载的 %s (%d) 添加到内存缓存\n", character, size); // 调试信息
        cachePut(key, fontBuffer, size, bufferSize);
    } else {
         Serial.printf("警告：从 SD 加载后 fontBuffer 为空或 bufferSize 为 0 (%s, %d)\n", character, size);
    }

    // 4. 返回指向 fontBuffer 中数据的指针 (刚刚加载的)。
    //    注意：数据现在也存在于内存缓存中，但我们返回临时缓冲区的指针
    //    以保持与旧逻辑流程的一致性。
    //    后续对相同字符/大小的调用应该会命中内存缓存。
    return fontBuffer;
}

// 辅助函数：将 UTF-8 字符字符串转换为其 Unicode 码点
// 如果输入为 null 或无效的 UTF-8 起始字节，则返回 0
uint32_t Font::utf8ToUnicode(const char* utf8_char) {
    if (!utf8_char || *utf8_char == '\0') {
        return 0; // 无效输入
    }

    uint32_t unicode_val = 0; // 初始化 Unicode 值
    unsigned char c1 = utf8_char[0]; // 获取第一个字节

    if (c1 < 0x80) { // 1 字节序列 (ASCII)
        unicode_val = c1;
    } else if ((c1 & 0xE0) == 0xC0) { // 2 字节序列 (以 110xxxxx 开头)
        // 检查第二个字节是否为 10xxxxxx
        if ((utf8_char[1] & 0xC0) == 0x80) {
            // 计算 Unicode 值
            unicode_val = ((c1 & 0x1F) << 6) | (utf8_char[1] & 0x3F);
        } else return 0; // 无效序列
    } else if ((c1 & 0xF0) == 0xE0) { // 3 字节序列 (以 1110xxxx 开头)
        // 检查第二和第三个字节是否为 10xxxxxx
        if (((utf8_char[1] & 0xC0) == 0x80) && ((utf8_char[2] & 0xC0) == 0x80)) {
            // 计算 Unicode 值
            unicode_val = ((c1 & 0x0F) << 12) | ((utf8_char[1] & 0x3F) << 6) | (utf8_char[2] & 0x3F);
        } else return 0; // 无效序列
    } else if ((c1 & 0xF8) == 0xF0) { // 4 字节序列 (以 11110xxx 开头)
        // 检查第二、三、四个字节是否为 10xxxxxx
        if (((utf8_char[1] & 0xC0) == 0x80) && ((utf8_char[2] & 0xC0) == 0x80) && ((utf8_char[3] & 0xC0) == 0x80)) {
            // 计算 Unicode 值
            unicode_val = ((c1 & 0x07) << 18) | ((utf8_char[1] & 0x3F) << 12) | ((utf8_char[2] & 0x3F) << 6) | (utf8_char[3] & 0x3F);
        } else return 0; // 无效序列
    } else {
        return 0; // 无效的 UTF-8 起始字节
    }

    return unicode_val; // 返回计算出的 Unicode 码点
}


// 计算 UTF-8 字符串的实际字符长度 (而不是字节长度)
size_t Font::utf8Length(const char* str) {
    size_t len = 0; // 初始化字符计数
    while (*str) { // 遍历字符串直到空终止符
        // UTF-8 字符的后续字节以 10xxxxxx 开头 (& 0xC0 结果为 0x80)
        // 只有字符的第一个字节不满足这个条件
        if ((*str & 0xC0) != 0x80) {
            len++; // 遇到非后续字节，增加字符计数
        }
        str++; // 移动到下一个字节
    }
    return len; // 返回字符总数
}

// 从 UTF-8 字符串中获取下一个字符，并更新偏移量
String Font::getNextCharacter(const char* str, size_t& offset) {
    // 如果当前偏移量指向字符串末尾 (空终止符)
    if (!str[offset]) {
        return ""; // 返回空字符串表示结束
    }

    size_t start = offset; // 记录当前字符的起始字节位置
    uint8_t first = str[offset]; // 获取第一个字节

    // 根据第一个字节判断 UTF-8 字符的字节数
    if (first < 0x80) { // ASCII 字符 (1 字节)
        offset += 1;
    } else if ((first & 0xE0) == 0xC0) { // 2 字节字符
        offset += 2;
    } else if ((first & 0xF0) == 0xE0) { // 3 字节字符
        offset += 3;
    } else if ((first & 0xF8) == 0xF0) { // 4 字节字符
        offset += 4;
    } else {
        // 无效的 UTF-8 起始字节，当作单字节处理以避免死循环
        offset += 1;
    }

    // 从原始字符串中提取从 start 到新 offset 的子字符串
    return String(str).substring(start, offset);
}

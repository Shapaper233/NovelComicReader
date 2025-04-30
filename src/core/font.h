#ifndef FONT_H // 防止头文件被重复包含
#define FONT_H

#include <Arduino.h>      // 包含 Arduino 核心库
#include <SD.h>           // 包含 SD 卡库
#include <FS.h>           // 包含文件系统库 (SD 库依赖)
#include <ArduinoJson.h>  // 包含 ArduinoJson 库，用于处理 JSON 格式的快速缓存索引
#include <map>            // 包含 C++ 标准库 map，用于内存缓存
#include <list>           // 包含 C++ 标准库 list，用于 LRU (最近最少使用) 缓存淘汰策略
#include <string>         // 包含 C++ 标准库 string，用于 map 的键 (字符部分)
#include <utility>        // 包含 C++ 标准库 utility，用于 std::pair (map 的键)
#include "../config/config.h" // 包含项目配置文件 (调整了路径)

/**
 * @brief 管理字体加载和缓存的单例类。
 * 支持从 SD 卡加载自定义点阵字体，并使用内存缓存和快速缓存 (SD 卡) 提高性能。
 */
class Font
{
private:
    static Font *instance;      // 指向 Font 类单例实例的指针
    File currentFontFile;       // 当前打开的字体文件对象 (用于从 SD 卡读取)
    uint8_t *fontBuffer;        // 用于从 SD 卡读取字体数据的临时缓冲区
    uint16_t currentSize;       // 当前加载到 fontBuffer 中的字体大小 (像素)
    uint16_t bufferSize;        // fontBuffer 的大小 (字节)

    // --- 快速缓存 (持久化到 SD 卡) ---
    static const int SAVE_CACHE_INTERVAL = 10; // 每从 SD 读取 10 次字体后，保存一次快速缓存
    static const char* FAST_CACHE_BIN_PATH;    // 快速缓存二进制数据文件路径 (在 .cpp 文件中定义)
    static const char* FAST_CACHE_JSON_PATH;   // 快速缓存 JSON 索引文件路径 (在 .cpp 文件中定义)
    int fontsReadFromSDCounter = 0;            // 从 SD 卡读取字体的计数器，用于触发快速缓存保存
    // --- 快速缓存结束 ---

    // --- 内存缓存 (LRU) ---
    /**
     * @brief 内存缓存条目结构体。
     */
    struct CacheEntry
    {
        uint8_t *bitmap; // 指向 RAM 中缓存的字体位图数据的指针
        uint16_t size;   // 字符的像素大小 (例如 16 表示 16x16)
        size_t dataSize; // 位图数据占用的字节数
    };

    // 内存缓存的键类型：一个包含 (UTF8 字符的 std::string, 字符像素大小 uint16_t) 的 pair
    using CacheKey = std::pair<std::string, uint16_t>;
    // LRU 链表的迭代器类型
    using CacheIterator = std::list<CacheKey>::iterator;

    // 内存缓存 map：键 (CacheKey) 映射到 {缓存条目数据 (CacheEntry), 指向 LRU 链表中对应键的迭代器}
    std::map<CacheKey, std::pair<CacheEntry, CacheIterator>> cacheMap;
    // LRU 链表：存储缓存键，按照最近最少使用的顺序排列 (最近使用的在链表前端)
    std::list<CacheKey> cacheLRUList;

    size_t maxCacheSizeInBytes;     // 内存缓存允许占用的最大总字节数
    size_t currentCacheSizeInBytes; // 当前内存缓存已占用的总字节数

    // 内存缓存辅助方法
    /**
     * @brief 将字体数据放入内存缓存。
     * @param key 缓存键 (字符和大小)。
     * @param data 指向字体位图数据的指针。
     * @param charSize 字符的像素大小。
     * @param dataSize 位图数据的字节大小。
     */
    void cachePut(const CacheKey &key, const uint8_t *data, uint16_t charSize, size_t dataSize);

    /**
     * @brief 从内存缓存中获取字体数据。
     * @param key 缓存键 (字符和大小)。
     * @return 如果找到，返回指向 CacheEntry 的指针；否则返回 nullptr。
     */
    CacheEntry *cacheGet(const CacheKey &key); // Returns pointer to entry if found, nullptr otherwise

    /**
     * @brief 当缓存满时，根据 LRU 策略淘汰最少使用的条目。
     */
    void cacheEvict();                         // Evicts the least recently used item(s) if cache is full

    /**
     * @brief 清空整个内存缓存，释放所有缓存的位图数据。
     */
    void clearMemoryCache();                   // Clears the entire in-memory cache
    // --- 内存缓存结束 ---

    // --- 快速缓存方法 ---
    /**
     * @brief 将当前的内存缓存内容保存到 SD 卡上的快速缓存文件 (二进制 + JSON 索引)。
     * @return 如果保存成功返回 true，否则返回 false。
     */
    bool saveFastFontCache(); // Saves the current in-memory cache to SD
    // --- 快速缓存方法结束 ---


    /**
     * @brief 私有构造函数。初始化缓存设置和缓冲区。
     */
    Font();  // Constructor: Initialize cache settings
    /**
     * @brief 析构函数。清理内存缓存和缓冲区。
     */
    ~Font(); // Destructor: Clean up cache memory

    /**
     * @brief 从 SD 卡加载指定字符和大小的字体数据到 fontBuffer。
     * 会先尝试从快速缓存加载，如果失败则从原始字体文件加载。
     * @param character 要加载的 UTF-8 字符。
     * @param size 字体像素大小。
     * @return 如果加载成功返回 true，否则返回 false。
     */
    bool loadCharacter(const char *character, uint16_t size); // Loads from SD (cache or file) into fontBuffer

    /**
     * @brief 清空临时的 fontBuffer。不直接影响内存缓存。
     */
    void clearBuffer();                                       // Clears the single fontBuffer (doesn't affect memory cache directly)

    /**
     * @brief 在 SD 卡上查找包含指定字符的字体索引文件 (.idx)。
     * @param character 要查找的 UTF-8 字符。
     * @return 如果找到，返回索引文件的完整路径 (String)；否则返回空 String。
     */
    String findIndexFile(const char *character);              // Finds index file on SD

public:
    // 禁止拷贝构造和赋值操作，确保单例唯一性
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    /**
     * @brief 获取 Font 类的单例实例。
     * @return Font 类的引用。
     */
    static Font &getInstance();

    /**
     * @brief 初始化字体系统。
     * 分配缓冲区，设置缓存大小限制。需要在 SD 卡初始化之后调用。
     * @return 如果初始化成功返回 true，否则返回 false。
     */
    bool begin();

    /**
     * @brief 从 SD 卡加载快速字体缓存到内存缓存中。
     * 应在 begin() 之后、SD 卡初始化之后调用。
     * @return 如果加载成功返回 true，否则返回 false。
     */
    bool loadFastFontCache(); // Loads the fast cache from SD into memory

    /**
     * @brief 获取指定字符和大小的点阵位图数据。
     * 优先从内存缓存获取，其次尝试从 SD 卡加载 (可能触发快速缓存加载或原始文件加载)。
     * @param character 要获取位图的 UTF-8 字符。
     * @param size 字体像素大小。
     * @return 如果成功获取，返回指向位图数据的指针 (可能指向内存缓存或临时 fontBuffer)；否则返回 nullptr。
     * @note 返回的指针指向的数据可能在下次调用或缓存淘汰时失效，特别是当它指向 fontBuffer 时。
     *       如果需要持久保留，应复制数据。
     */
    uint8_t *getCharacterBitmap(const char *character, uint16_t size);

    /**
     * @brief 获取指定像素大小的字符宽度。
     * @param size 字体像素大小。
     * @return 字符宽度 (像素)。当前实现假设宽度等于大小。
     */
    uint16_t getCharacterWidth(uint16_t size) const { return size; }

    /**
     * @brief 获取指定像素大小的字符高度。
     * @param size 字体像素大小。
     * @return 字符高度 (像素)。当前实现假设高度等于大小。
     */
    uint16_t getCharacterHeight(uint16_t size) const { return size; }

    /**
     * @brief 判断一个 UTF-8 字符是否为 ASCII 字符 (码点 < 128)。
     * @param character 指向 UTF-8 字符的指针。
     * @return 如果是 ASCII 字符返回 true，否则返回 false。
     */
    static bool isAscii(const char *character)
    {
        return (uint8_t)*character < 128;
    }

    /**
     * @brief 计算一个 UTF-8 编码字符串中包含的实际字符数量。
     * @param str 指向 UTF-8 字符串的指针。
     * @return 字符串中的字符数量。
     */
    static size_t utf8Length(const char *str);

    /**
     * @brief 从 UTF-8 字符串中提取下一个字符。
     * @param str 指向 UTF-8 字符串的指针。
     * @param offset 输入/输出参数，表示当前处理到字符串的字节偏移量。函数会更新此值。
     * @return 包含下一个 UTF-8 字符的 String 对象。如果到达字符串末尾，返回空 String。
     */
    static String getNextCharacter(const char *str, size_t &offset);

    /**
     * @brief 将 UTF-8 编码的字符转换为其 Unicode 码点。
     * @param utf8_char 指向 UTF-8 字符的指针。
     * @return 对应的 Unicode 码点 (uint32_t)。如果输入无效，可能返回错误码或 0。
     */
    static uint32_t utf8ToUnicode(const char* utf8_char);
};

#endif // FONT_H

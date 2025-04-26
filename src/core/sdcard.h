#ifndef SDCARD_H
#define SDCARD_H

#include <SD.h>
#include <vector>
#include <string>
#include "../config/config.h" // Adjusted path

struct FileItem {
    String name;          // 文件/文件夹名称
    bool isDirectory;     // 是否是目录
    bool isComic;         // 是否是漫画目录（包含.info文件）
    bool isText;          // 是否是文本文件 (.txt)
};

class SDCard {
private:
    static SDCard* instance;
    bool initialized;
    String currentPath;
    std::vector<FileItem> currentItems;
    size_t currentPage;
    size_t totalPages;
    
    SDCard();
    
    // 检查目录是否是漫画目录
    bool checkIsComic(const String& path);
    
    // 计算总页数
    void updatePageInfo();

public:
    static SDCard& getInstance();
    
    // 初始化SD卡
    bool begin();
    
    // 文件浏览功能
    bool enterDirectory(const String& dirName);
    bool goBack();
    const String& getCurrentPath() const { return currentPath; }
    
    // 获取当前页的文件列表
    const std::vector<FileItem>& getCurrentItems() const { return currentItems; }
    
    // 分页控制
    void nextPage();
    void prevPage();
    size_t getCurrentPage() const { return currentPage; }
    size_t getTotalPages() const { return totalPages; }
    
    // 加载当前目录内容
    bool loadDirectory(const String& path = "/");
    
    // 文件操作
    bool exists(const String& path);
    File openFile(const String& path, const char* mode = FILE_READ);
};

#endif // SDCARD_H

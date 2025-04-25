#include "sdcard.h"
#include <algorithm>

// 初始化静态成员
SDCard* SDCard::instance = nullptr;

SDCard& SDCard::getInstance() {
    if (!instance) {
        instance = new SDCard();
    }
    return *instance;
}

SDCard::SDCard() : initialized(false), currentPath("/"), currentPage(0), totalPages(0) {}

extern SPIClass sdSPI;

bool SDCard::begin() {
    if (!SD.begin(SD_CS, sdSPI)) {
        return false;
    }
    initialized = true;
    return loadDirectory("/");
}

bool SDCard::checkIsComic(const String& path) {
    // 检查目录下是否存在.info文件
    String infoPath = path + "/" + INFO_FILE;
    if (!SD.exists(infoPath)) {
        return false;
    }

    // 读取.info文件
    File infoFile = SD.open(infoPath);
    if (!infoFile) {
        return false;
    }

    // 读取文件内容
    String content = infoFile.readString();
    infoFile.close();

    // 简单解析JSON，查找"type":"comic"
    int typePos = content.indexOf("\"type\"");
    if (typePos == -1) {
        return false;
    }

    int colonPos = content.indexOf(":", typePos);
    if (colonPos == -1) {
        return false;
    }

    int quotePos = content.indexOf("\"", colonPos);
    if (quotePos == -1) {
        return false;
    }

    int endQuotePos = content.indexOf("\"", quotePos + 1);
    if (endQuotePos == -1) {
        return false;
    }

    String type = content.substring(quotePos + 1, endQuotePos);
    return type == "comic";
}

void SDCard::updatePageInfo() {
    // 计算总页数
    totalPages = (currentItems.size() + MAX_ITEMS_PER_PAGE - 1) / MAX_ITEMS_PER_PAGE;
    if (totalPages == 0) totalPages = 1;
    
    // 确保当前页在有效范围内
    if (currentPage >= totalPages) {
        currentPage = totalPages - 1;
    }
}

bool SDCard::loadDirectory(const String& path) {
    if (!initialized) return false;
    
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        return false;
    }
    
    // 清空当前列表
    currentItems.clear();
    currentPath = path;
    
    // 读取目录内容
    File entry;
    while (entry = dir.openNextFile()) {
        FileItem item;
        item.name = String(entry.name());
        item.isDirectory = entry.isDirectory();
        
        // 如果是目录，检查是否是漫画目录
        if (item.isDirectory) {
            item.isComic = checkIsComic(entry.path());
        } else {
            item.isComic = false;
        }
        
        currentItems.push_back(item);
        entry.close();
    }
    
    // 按名称排序
    std::sort(currentItems.begin(), currentItems.end(), 
              [](const FileItem& a, const FileItem& b) {
                  return a.name < b.name;
              });
    
    // 更新分页信息
    currentPage = 0;
    updatePageInfo();
    
    dir.close();
    return true;
}

bool SDCard::enterDirectory(const String& dirName) {
    String newPath;
    if (currentPath == "/") {
        newPath = "/" + dirName;
    } else {
        newPath = currentPath + "/" + dirName;
    }
    return loadDirectory(newPath);
}

bool SDCard::goBack() {
    if (currentPath == "/") return false;
    
    // 找到最后一个斜杠的位置
    int lastSlash = currentPath.lastIndexOf('/');
    if (lastSlash <= 0) {
        // 返回根目录
        return loadDirectory("/");
    }
    
    // 获取上一级目录路径
    String parentPath = currentPath.substring(0, lastSlash);
    return loadDirectory(parentPath);
}

void SDCard::nextPage() {
    if (currentPage < totalPages - 1) {
        currentPage++;
    }
}

void SDCard::prevPage() {
    if (currentPage > 0) {
        currentPage--;
    }
}

bool SDCard::exists(const String& path) {
    return SD.exists(path);
}

File SDCard::openFile(const String& path, const char* mode) {
    return SD.open(path, mode);
}

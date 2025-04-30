#include "sdcard.h"  // 包含 SDCard 类的头文件
#include <algorithm> // 包含 C++ 标准库的算法头文件，用于排序 (std::sort)

// 初始化静态单例实例指针
SDCard* SDCard::instance = nullptr;

// 获取 SDCard 单例实例的静态方法
SDCard& SDCard::getInstance() {
    // 如果实例尚未创建
    if (!instance) {
        // 创建一个新的 SDCard 实例
        instance = new SDCard();
    }
    // 返回实例的引用
    return *instance;
}

// SDCard 类的构造函数
// 初始化成员变量：initialized 为 false，currentPath 为根目录 "/"，页码和总页数都为 0
SDCard::SDCard() : initialized(false), currentPath("/"), currentPage(0), totalPages(0) {}

// 声明外部 SPIClass 对象，用于 SD 卡通信 (假设在其他地方定义)
extern SPIClass sdSPI;

// 初始化 SD 卡
bool SDCard::begin() {
    // 调用 SD 库的 begin 方法，传入 CS 引脚和 SPI 对象
    // 如果初始化失败
    if (!SD.begin(SD_CS, sdSPI)) {
        return false; // 返回 false 表示失败
    }
    // 标记 SD 卡已成功初始化
    initialized = true;
    // 加载根目录的内容并返回加载结果
    return loadDirectory("/");
}

// 检查指定路径是否是一个漫画目录
bool SDCard::checkIsComic(const String& path) {
    // 构建 .info 文件的完整路径
    String infoPath = path + "/" + INFO_FILE; // INFO_FILE 在 config.h 中定义
    // 检查 .info 文件是否存在
    if (!SD.exists(infoPath)) {
        return false; // 文件不存在，不是漫画目录
    }

    // 打开 .info 文件进行读取
    File infoFile = SD.open(infoPath);
    // 如果文件打开失败
    if (!infoFile) {
        return false; // 无法打开文件，判定为非漫画目录
    }

    // 读取文件的全部内容到一个 String 对象
    String content = infoFile.readString();
    // 关闭文件
    infoFile.close();

    // --- 简单的 JSON 解析逻辑 ---
    // 查找 "type" 键的位置
    int typePos = content.indexOf("\"type\"");
    if (typePos == -1) {
        return false; // 未找到 "type" 键
    }

    // 查找 "type" 键后面的冒号位置
    int colonPos = content.indexOf(":", typePos);
    if (colonPos == -1) {
        return false; // 格式错误
    }

    // 查找冒号后面的第一个引号 (值的开始)
    int quotePos = content.indexOf("\"", colonPos);
    if (quotePos == -1) {
        return false; // 格式错误
    }

    // 查找值的结束引号
    int endQuotePos = content.indexOf("\"", quotePos + 1);
    if (endQuotePos == -1) {
        return false; // 格式错误
    }

    // 提取引号之间的值
    String type = content.substring(quotePos + 1, endQuotePos);
    // 比较提取出的类型值是否为 "comic"
    return type == "comic";
}

// 更新分页信息
void SDCard::updatePageInfo() {
    // 计算总页数：(总项目数 + 每页最大项目数 - 1) / 每页最大项目数 (向上取整)
    // MAX_ITEMS_PER_PAGE 在 config.h 中定义
    totalPages = (currentItems.size() + MAX_ITEMS_PER_PAGE - 1) / MAX_ITEMS_PER_PAGE;
    // 如果计算结果为 0 (例如目录为空)，则至少有 1 页
    if (totalPages == 0) totalPages = 1;

    // 确保当前页码不会超出总页数范围
    // 注意：页码是从 0 开始计数的，所以最大有效页码是 totalPages - 1
    if (currentPage >= totalPages) {
        currentPage = totalPages - 1;
    }
}

// 加载指定目录的内容
bool SDCard::loadDirectory(const String& path) {
    // 如果 SD 卡未初始化，则无法加载
    if (!initialized) return false;

    // 打开指定路径的目录
    File dir = SD.open(path);
    // 如果打开失败或打开的不是一个目录
    if (!dir || !dir.isDirectory()) {
        return false; // 返回 false 表示加载失败
    }

    // 清空之前存储的文件/目录项列表
    currentItems.clear();
    // 更新当前路径
    currentPath = path;

    // 循环读取目录中的每一个条目 (文件或子目录)
    File entry;
    while (entry = dir.openNextFile()) {
        FileItem item; // 创建一个新的 FileItem 对象
        item.name = String(entry.name()); // 获取条目名称
        item.isDirectory = entry.isDirectory(); // 判断是否为目录

        item.isText = false; // 默认不是文本文件
        // 如果是目录
        if (item.isDirectory) {
            // 检查该目录是否为漫画目录
            item.isComic = checkIsComic(entry.path()); // 使用完整路径检查
        } else { // 如果是文件
            item.isComic = false; // 文件肯定不是漫画目录
            // 检查文件名是否以 .txt 或 .TXT 结尾
            if (item.name.endsWith(".txt") || item.name.endsWith(".TXT")) {
                item.isText = true; // 标记为文本文件
            }
        }

        // 将创建的文件项添加到列表中
        currentItems.push_back(item);
        // 关闭当前条目文件句柄，准备读取下一个
        entry.close();
    }

    // 使用 C++ 标准库的 sort 函数对列表进行排序
    // 排序规则：按名称的字母顺序升序排列
    std::sort(currentItems.begin(), currentItems.end(),
              [](const FileItem& a, const FileItem& b) {
                  return a.name < b.name; // 比较两个 FileItem 的 name 成员
              });

    // 重置当前页码为第一页 (索引 0)
    currentPage = 0;
    // 更新总页数等分页信息
    updatePageInfo();

    // 关闭目录文件句柄
    dir.close();
    // 返回 true 表示加载成功
    return true;
}

// 进入指定的子目录
bool SDCard::enterDirectory(const String& dirName) {
    String newPath; // 用于存储新的完整路径
    // 如果当前在根目录 "/"
    if (currentPath == "/") {
        // 新路径直接是 "/" + 目录名
        newPath = "/" + dirName;
    } else {
        // 否则，新路径是当前路径 + "/" + 目录名
        newPath = currentPath + "/" + dirName;
    }
    // 加载新路径的内容
    return loadDirectory(newPath);
}

// 返回上一级目录
bool SDCard::goBack() {
    // 如果当前已在根目录，无法再返回
    if (currentPath == "/") return false;

    // 查找当前路径中最后一个 '/' 的位置
    int lastSlash = currentPath.lastIndexOf('/');
    // 如果找不到 '/' 或者 '/' 就在第一个位置 (理论上对于非根目录不会是这种情况)
    if (lastSlash <= 0) {
        // 直接返回根目录
        return loadDirectory("/");
    }

    // 截取从开头到最后一个 '/' 之前的子字符串，即为父目录路径
    String parentPath = currentPath.substring(0, lastSlash);
    // 加载父目录的内容
    return loadDirectory(parentPath);
}

// 切换到下一页
void SDCard::nextPage() {
    // 检查当前页是否小于总页数减 1 (因为页码从 0 开始)
    if (currentPage < totalPages - 1) {
        // 页码加 1
        currentPage++;
        // 注意：这里没有重新加载数据，因为 loadDirectory 已经加载了所有项。
        // 实际显示哪一页的数据是在 UI 层面处理的。
    }
}

// 切换到上一页
void SDCard::prevPage() {
    // 检查当前页是否大于 0
    if (currentPage > 0) {
        // 页码减 1
        currentPage--;
        // 同样，这里只改变页码，不重新加载数据。
    }
}

// 检查指定路径的文件或目录是否存在
bool SDCard::exists(const String& path) {
    // 直接调用 SD 库的 exists 方法
    return SD.exists(path);
}

// 打开指定路径的文件
File SDCard::openFile(const String& path, const char* mode) {
    // 直接调用 SD 库的 open 方法，传入路径和打开模式
    return SD.open(path, mode);
}

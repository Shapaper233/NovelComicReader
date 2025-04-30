#ifndef SDCARD_H // 防止头文件被重复包含
#define SDCARD_H

#include <SD.h>           // 包含 Arduino SD 卡库
#include <vector>         // 包含 std::vector，用于存储文件项列表
#include <string>         // 包含 std::string (虽然这里主要用 Arduino String，但包含以备不时之需)
#include "../config/config.h" // 包含项目配置文件 (调整了路径)

/**
 * @brief 文件项结构体。
 * 用于表示 SD 卡上的一个文件或目录及其属性。
 */
struct FileItem {
    String name;          // 文件或文件夹的名称
    bool isDirectory;     // 标记此项是否为目录
    bool isComic;         // 标记此项是否为漫画目录 (特征是包含 .info 文件)
    bool isText;          // 标记此项是否为文本文件 (扩展名为 .txt)
};

/**
 * @brief SD 卡管理单例类。
 * 负责初始化 SD 卡、浏览目录、获取文件列表、分页显示以及基本文件操作。
 */
class SDCard {
private:
    static SDCard* instance; // 指向 SDCard 类单例实例的指针
    bool initialized;        // 标记 SD 卡是否已成功初始化
    String currentPath;      // 当前浏览的目录路径
    std::vector<FileItem> currentItems; // 当前目录下（当前页）的文件/目录项列表
    size_t currentPage;      // 当前显示的页码 (从 1 开始)
    size_t totalPages;       // 当前目录下内容的总页数

    /**
     * @brief 私有构造函数。
     * 防止外部直接创建实例，强制使用 getInstance()。
     */
    SDCard();

    /**
     * @brief 检查指定路径是否是一个漫画目录。
     * 通过查找目录下是否存在 ".info" 文件来判断。
     * @param path 要检查的目录路径。
     * @return 如果是漫画目录返回 true，否则返回 false。
     */
    bool checkIsComic(const String& path);

    /**
     * @brief 更新分页信息。
     * 根据当前目录下的项目总数和每页显示数量计算总页数。
     */
    void updatePageInfo();

public:
    /**
     * @brief 获取 SDCard 类的单例实例。
     * @return SDCard 类的引用。
     */
    static SDCard& getInstance();

    /**
     * @brief 初始化 SD 卡。
     * 尝试挂载 SD 卡并设置初始状态。
     * @return 如果初始化成功返回 true，否则返回 false。
     */
    bool begin();

    /**
     * @brief 进入指定的子目录。
     * 更新当前路径并重新加载目录内容。
     * @param dirName 要进入的子目录名称。
     * @return 如果成功进入目录返回 true，否则返回 false。
     */
    bool enterDirectory(const String& dirName);

    /**
     * @brief 返回上一级目录。
     * 更新当前路径到父目录并重新加载内容。
     * @return 如果成功返回上一级 (不是根目录) 返回 true，否则返回 false。
     */
    bool goBack();

    /**
     * @brief 获取当前浏览的路径。
     * @return 当前路径的常量引用。
     */
    const String& getCurrentPath() const { return currentPath; }

    /**
     * @brief 获取当前页的文件/目录项列表。
     * @return 当前项列表的常量引用。
     */
    const std::vector<FileItem>& getCurrentItems() const { return currentItems; }

    /**
     * @brief 切换到下一页。
     * 如果当前不是最后一页，则增加页码并重新加载该页内容。
     */
    void nextPage();

    /**
     * @brief 切换到上一页。
     * 如果当前不是第一页，则减少页码并重新加载该页内容。
     */
    void prevPage();

    /**
     * @brief 获取当前页码。
     * @return 当前页码 (从 1 开始)。
     */
    size_t getCurrentPage() const { return currentPage; }

    /**
     * @brief 获取总页数。
     * @return 当前目录下的总页数。
     */
    size_t getTotalPages() const { return totalPages; }

    /**
     * @brief 加载指定目录的内容。
     * 读取目录下的文件和子目录，填充 currentItems 列表，并更新分页信息。
     * @param path (可选) 要加载的目录路径。默认为根目录 "/"。
     * @return 如果加载成功返回 true，否则返回 false。
     */
    bool loadDirectory(const String& path = "/");

    /**
     * @brief 检查指定路径的文件或目录是否存在。
     * @param path 要检查的完整路径。
     * @return 如果存在返回 true，否则返回 false。
     */
    bool exists(const String& path);

    /**
     * @brief 打开指定路径的文件。
     * @param path 要打开的文件的完整路径。
     * @param mode (可选) 打开文件的模式 (例如 FILE_READ, FILE_WRITE)。默认为 FILE_READ。
     * @return 返回一个 File 对象。如果打开失败，该对象的布尔值评估为 false。
     */
    File openFile(const String& path, const char* mode = FILE_READ);
};

#endif // SDCARD_H

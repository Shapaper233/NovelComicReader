#ifndef ROUTER_H // 防止头文件被重复包含
#define ROUTER_H

#include <Arduino.h>      // 包含 Arduino 核心库 (为了 String 类型等)
#include <functional>     // 包含 std::function，用于存储页面创建函数
#include <vector>         // 包含 std::vector，用于存储路由历史记录
#include <string>         // 包含 std::string，用于路由名称和历史记录
#include <unordered_map>  // 包含 std::unordered_map，用于存储路由名称到创建函数的映射
#include <cstdint>        // 包含标准整数类型定义 (如 uintptr_t, 虽然这里没直接用，但 void* 转换时可能相关)
#include "../config/config.h" // 包含项目配置文件 (调整了路径)
// #include "pages.h" // 移除此包含，使用前向声明解决循环依赖

// 前向声明各个 Page 类，以打破头文件之间的循环依赖
class Page;
class FileBrowserPage;
class ImageViewerPage;
class TextViewerPage;
class ComicViewerPage;

/**
 * @brief 路由历史记录项结构体。
 * 用于存储导航历史中的页面名称和传递给该页面的参数。
 */
struct RouteHistoryItem
{
    std::string name; // 页面的注册名称
    void *params;     // 导航到该页面时传递的参数指针 (类型由页面自行解释)
};

/**
 * @brief 页面路由管理单例类。
 * 负责注册页面、导航到不同页面以及管理导航历史记录。
 */
class Router
{
private:
    /**
     * @brief 私有默认构造函数。
     * 防止外部直接创建实例，强制使用 getInstance()。
     */
    Router() = default;

    std::vector<RouteHistoryItem> history; // 存储导航历史的向量
    // 存储已注册路由的 map：页面名称 (string) -> 创建该页面实例的函数 (std::function)
    std::unordered_map<std::string, std::function<Page *()>> routes;
    Page *currentPage = nullptr;         // 指向当前活动页面的指针
    std::string currentPageName;         // 当前活动页面的注册名称
    void* currentPageParams = nullptr;   // 导航到当前页面时使用的参数

    static Router *instance; // 指向 Router 类单例实例的指针

public:
    // 禁止拷贝构造函数
    Router(const Router&) = delete;
    // 禁止赋值操作符
    Router& operator=(const Router&) = delete;

    /**
     * @brief 获取 Router 类的单例实例。
     * @return Router 类的引用。
     */
    static Router &getInstance();

    /**
     * @brief 注册一个页面。
     * 将页面名称与一个用于创建该页面实例的 lambda 函数或函数指针关联起来。
     * @param name 页面的唯一注册名称。
     * @param creator 一个无参数并返回 Page* 的函数 (通常是 lambda 表达式)。
     */
    void registerPage(const std::string &name, std::function<Page *()> creator);

    /**
     * @brief 导航到指定名称的页面。
     * 如果页面已注册，则创建新页面实例，销毁旧页面，更新当前页面指针，并将旧页面添加到历史记录。
     * @param name 要导航到的页面的注册名称。
     * @param params (可选) 传递给新页面的参数指针。默认为 nullptr。页面需要知道如何解释这些参数。
     */
    void navigateTo(const std::string &name, void *params = nullptr);

    /**
     * @brief 返回到导航历史中的上一个页面。
     * 如果历史记录不为空，则销毁当前页面，从历史记录中恢复上一个页面及其参数，并将其设为当前页面。
     * @return 如果成功返回 (历史记录非空) 返回 true，否则返回 false。
     */
    bool goBack();

    /**
     * @brief 获取当前活动的页面指针。
     * @return 指向当前 Page 对象的指针，如果当前没有活动页面则为 nullptr。
     */
    Page *getCurrentPage();

    /**
     * @brief Router 类的析构函数。
     * 负责清理当前页面和可能存在的单例实例（如果动态分配）。
     * 注意：这里的实现可能需要根据 instance 的分配方式调整。
     */
    ~Router();
};

#endif // ROUTER_H

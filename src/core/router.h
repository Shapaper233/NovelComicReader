#ifndef ROUTER_H
#define ROUTER_H

#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "../config/config.h" // Adjusted path
// #include "pages.h" // Remove this include

// Forward declarations to break circular dependency
class Page;
class FileBrowserPage;
class ImageViewerPage;
class TextViewerPage;
class ComicViewerPage;

// 页面基类
class Page
{
public:
    virtual void display() = 0;
    virtual void handleTouch(uint16_t x, uint16_t y) = 0;
    virtual ~Page() = default;

    void *params = nullptr; // 页面参数
};

// 路由历史记录项
struct RouteHistoryItem
{
    std::string name;
    void *params;
};

// 路由处理类
class Router
{
private:
    Router() = default;
    std::vector<RouteHistoryItem> history;
    std::unordered_map<std::string, std::function<Page *()>> routes;
    Page *currentPage = nullptr;

    static Router *instance;

public:
    static Router &getInstance()
    {
        if (!instance)
        {
            instance = new Router();
        }
        return *instance;
    }

    // 注册页面
    void registerPage(const std::string &name, std::function<Page *()> creator)
    {
        routes[name] = creator;
    }

    // 导航到指定页面
    void navigateTo(const std::string &name, void *params = nullptr)
    {
        auto it = routes.find(name);
        if (it != routes.end())
        {
            // 创建新页面
            Page *newPage = it->second();
            newPage->params = params;

            // Special handling for ComicViewerPage removed - loading is handled in setComicPath

            // 保存当前页面到历史记录
            if (currentPage)
            {
                history.push_back({name, params});
            }

            // 删除旧页面并切换到新页面
            delete currentPage;
            currentPage = newPage;
            currentPage->display();
        }
    }

    // 返回上一页
    bool goBack()
    {
        if (history.empty())
        {
            return false;
        }

        // 获取上一页信息
        RouteHistoryItem lastPage = history.back();
        history.pop_back();

        // 切换到上一页
        auto it = routes.find(lastPage.name);
        if (it != routes.end())
        {
            delete currentPage;
            currentPage = it->second();
            currentPage->params = lastPage.params;
            currentPage->display();
            return true;
        }
        return false;
    }

    // 获取当前页面
    Page *getCurrentPage()
    {
        return currentPage;
    }

    ~Router()
    {
        delete currentPage;
        delete instance;
    }
};

#endif // ROUTER_H

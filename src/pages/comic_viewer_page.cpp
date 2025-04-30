#include <Arduino.h>
#include <FS.h>        // 用于文件系统操作
#include <algorithm>   // 用于 std::min 和 std::max
#include <vector>      // 用于 std::vector

#include "pages.h"     // 包含页面基类和相关定义 (Correct path)
#include "../core/display.h"   // 包含显示管理类 (Adjusted path)
#include "../core/sdcard.h"    // 包含 SD 卡管理类 (Adjusted path)
#include "../core/router.h"    // 包含页面路由类 (Adjusted path)
#include "../config/config.h"    // 包含配置常量 (Adjusted path)

// ComicViewerPage 类实现

/**
 * @brief ComicViewerPage 构造函数
 * 初始化显示管理器、SD 卡管理器、触摸管理器引用，并将滚动偏移和总高度缓存清零。
 */
ComicViewerPage::ComicViewerPage()
    : displayManager(Display::getInstance()), sdManager(SDCard::getInstance()), touchManager(Touch::getInstance()), scrollOffset(0), totalComicHeight(0)
{
    Serial.println("ComicViewerPage constructor called");
}

// Implementation of the virtual setParams method
void ComicViewerPage::setParams(void* params) {
    if (params) {
        // Cast the void pointer back to a String pointer
        String* pathPtr = static_cast<String*>(params);
        // Call the existing setComicPath method with the actual path String
        setComicPath(*pathPtr);
    } else {
        // Handle case where no parameters were passed (optional)
        Serial.println("ComicViewerPage::setParams received null params.");
        // Set a default state or show an error?
        setComicPath(""); // Set empty path to indicate an issue
    }
}


/**
 * @brief 加载指定漫画路径下的所有图片文件。
 * 按照 "1.bmp", "2.bmp", ... 的顺序查找连续的 BMP 文件。
 * 同时计算并缓存每张图片的高度以及所有图片的总高度。
 *
 * @details
 * - 清空现有的图片文件列表 (imageFiles) 和高度缓存 (imageHeights)。
 * - 重置总漫画高度 (totalComicHeight)。
 * - 遍历数字索引，构建预期的图片文件名 (e.g., "/path/to/comic/1.bmp")。
 * - 使用 sdManager.exists() 检查文件是否存在，直到找不到连续的文件为止。
 * - 对于找到的每个文件：
 *   - 将其路径添加到 imageFiles 向量中。
 *   - 打开文件，读取 BMP 文件头以获取图片高度。
 *   - 将高度添加到 imageHeights 向量中。
 *   - 累加高度到 totalComicHeight。
 *   - 关闭文件。
 * - 如果计算出的 totalComicHeight 为 0 但找到了图片，设置一个默认高度（屏幕高度）作为回退。
 */
void ComicViewerPage::loadImages()
{
    imageFiles.clear();   // 清空图片文件列表
    imageHeights.clear(); // 清空缓存的高度
    totalComicHeight = 0; // 重置总高度
    Serial.print("Loading comic images from path: ");
    Serial.println(currentPath);

    // --- Display Loading Message ---
    displayManager.getTFT()->fillScreen(TFT_WHITE); // Clear screen
    displayManager.drawCenteredText("Loading Comic...", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.println("Displayed loading message.");
    // --- End Loading Message ---

    // 寻找从 1.bmp 开始的连续图片文件
    int index = 1;
    while (true)
    {
        String imagePath = currentPath + "/" + String(index) + ".bmp";
        if (!sdManager.exists(imagePath))
        {
            Serial.print("No more images found after index ");
            Serial.println(index - 1);
            break; // 找不到文件，停止加载
        }
        Serial.print("Found image: ");
        Serial.println(imagePath);
        imageFiles.push_back(imagePath); // 添加到文件列表

        // --- 计算并缓存高度 ---
        File file = sdManager.openFile(imagePath);
        int height = SCREEN_HEIGHT; // 默认高度，以防读取失败
        if (file)
        {
            uint8_t header[54];
            // 读取 BMP 文件头的前 54 字节
            if (file.read(header, 54) == 54 && header[0] == 'B' && header[1] == 'M')
            {
                // BMP 文件头中，高度信息位于偏移量 22 的位置 (4 字节整数)
                height = *(int *)&header[22];
            }
            file.close(); // 关闭文件
        }
        imageHeights.push_back(height); // 缓存图片高度
        totalComicHeight += height;     // 累加到总高度
        Serial.print("  Image height: ");
        Serial.println(height);
        // --- 结束高度计算 ---

        index++; // 准备查找下一个文件
    }
    Serial.print("Total images loaded: ");
    Serial.println(imageFiles.size());
    Serial.print("Total comic height calculated: ");
    Serial.println(totalComicHeight);
    // 如果计算出的总高度为 0 但确实加载了图片（可能因为文件头读取失败），
    // 提供一个基于屏幕高度的回退值，避免完全无法滚动。
    if (totalComicHeight == 0 && !imageFiles.empty()) {
        totalComicHeight = SCREEN_HEIGHT; // 使用屏幕高度作为回退
        Serial.println("Warning: Total height was 0, using fallback.");
    }
}

/**
 * @brief 绘制漫画内容区域。
 * 根据当前的滚动偏移 (scrollOffset) 和缓存的图片高度，
 * 计算需要显示哪些图片以及它们在屏幕上的位置，并进行绘制。
 *
 * @details
 * - 使用白色填充屏幕背景。
 * - 如果没有加载任何图片，显示提示信息并返回。
 * - 使用缓存的总高度 (totalComicHeight) 来约束滚动偏移量 (scrollOffset)，确保其在有效范围内 [0, totalComicHeight - SCREEN_HEIGHT]。
 * - 根据缓存的每张图片的高度 (imageHeights)，计算出每张图片在整个漫画长条中的起始 Y 坐标 (startPositions)。
 * - 根据当前的 scrollOffset，确定从哪张图片 (startImage) 开始绘制，以及该图片顶部相对于屏幕顶部的偏移量 (yOffset)。
 *   原理：屏幕显示的是漫画长条从 scrollOffset 开始的部分。如果图片 i 的起始绝对坐标是 startPositions[i]，
 *   那么它在屏幕上的起始 Y 坐标就是 startPositions[i] - scrollOffset。
 * - 定义用于读取和处理 BMP 数据的缓冲区：
 *   - rawBuffer: 存储从文件读取的原始 BGR 数据（带行填充）。
 *   - pixelBuffer: 存储转换后的一行 RGB565 像素数据，用于 pushImage。
 * - 从 startImage 开始遍历图片列表：
 *   - 获取当前图片的缓存高度。
 *   - 打开图片文件。
 *   - 读取并验证 BMP 文件头，获取宽度和位深度（必须是 24 位）。
 *   - 计算图片中实际需要读取和绘制的行范围 (startY, readHeight)，这取决于图片本身在屏幕上的可见部分。
 *   - 计算 BMP 行数据的实际存储大小 (actualRowSize)，包含 4 字节对齐的填充。
 *   - 计算像素数据在文件中的起始偏移量 (dataOffset)。
 *   - 分块读取图片数据（每次读取 BUFFER_ROWS 行）：
 *     - 计算当前块在文件中的位置 (pos)，注意 BMP 是从下往上存储的。
 *     - 使用 file.seek() 定位。
 *     - 使用 file.read() 将原始 BGR 数据读入 rawBuffer。
 *     - 逐行处理 rawBuffer 中的数据（从缓冲区底部开始，对应图片中较低的行号）：
 *       - 获取指向当前行数据的指针 (currentRowPtr)。
 *       - 计算该行在屏幕上的目标 Y 坐标 (screenRowY)。
 *       - 如果 screenRowY 在屏幕范围内：
 *         - 将当前行的 BGR 数据转换为 RGB565 格式，存入 pixelBuffer。
 *         - 使用 tft->pushImage() 将 pixelBuffer 中的一行像素推送到屏幕的 (0, screenRowY) 位置。
 *   - 关闭文件。
 *   - 更新 yOffset，准备绘制下一张图片。
 * - 循环直到绘制完所有可见图片或超出屏幕底部。
 * @return true if drawing was interrupted by touch, false otherwise.
 */
bool ComicViewerPage::drawContent()
{
    // 用白色填充背景
    displayManager.getTFT()->fillScreen(TFT_WHITE);
    Serial.println("Drawing comic content");

    // 如果没有图片，显示提示信息
    if (imageFiles.empty())
    {
        displayManager.drawCenteredText("No images found", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        return false; // Return false as drawing didn't complete (no images)
    }

    // 使用缓存的总高度
    Serial.print("Using cached total height: ");
    Serial.println(totalComicHeight);

    // 确保滚动偏移量在有效范围内 [0, max_scroll_offset]
    // 最大滚动偏移 = 总高度 - 屏幕高度 (如果总高度小于屏幕高度，则为 0)
    if (totalComicHeight > SCREEN_HEIGHT) {
        scrollOffset = std::max(0, std::min(scrollOffset, totalComicHeight - SCREEN_HEIGHT));
    } else {
        scrollOffset = 0; // 内容不足一屏，无法滚动
    }
    Serial.print("Scroll offset: ");
    Serial.println(scrollOffset);

    // --- 计算每张图片在漫画长条中的起始 Y 坐标 ---
    std::vector<int> startPositions; // 存储每张图片顶部的绝对 Y 坐标
    int currentHeightSum = 0;
    for (int height : imageHeights) { // 遍历缓存的高度
        startPositions.push_back(currentHeightSum);
        currentHeightSum += height;
    }
    // --- 结束计算起始 Y 坐标 ---

    // --- 确定从哪张图片开始绘制，以及绘制的起始屏幕 Y 坐标 ---
    int startImage = 0; // 第一个需要绘制的图片的索引
    int yOffset = -scrollOffset; // 屏幕上绘制区域的起始 Y 坐标，相对于屏幕顶部
                                 // 初始化为负的滚动偏移，因为屏幕顶部对应漫画的 scrollOffset 位置

    // 遍历计算好的起始位置，找到第一个完全或部分可见的图片
    for (size_t i = 0; i < startPositions.size(); i++)
    {
        // 检查当前的滚动偏移量 scrollOffset 是否落入图片 i 的垂直范围 [startPositions[i], startPositions[i+1])
        // 或者图片 i 是最后一张图片
        if (scrollOffset >= startPositions[i] &&
            (i == startPositions.size() - 1 || scrollOffset < startPositions[i + 1]))
        {
            startImage = i; // 找到了起始图片
            // 计算这张图片顶部在屏幕上的 Y 坐标
            // 绝对位置 startPositions[i] 减去屏幕顶部的绝对位置 scrollOffset
            yOffset = startPositions[i] - scrollOffset;
            break;
        }
    }
    // --- 结束确定起始图片和 Y 坐标 ---

    Serial.print("Starting from image ");
    Serial.print(startImage);
    Serial.print(" at yOffset ");
    Serial.println(yOffset);

    // --- 定义缓冲区 ---
    // 缓冲区大小，减少以降低单次 heap 分配大小，缓解碎片问题
    const int BUFFER_ROWS = 8; // Reduced from 16
    const int BPP = 3; // Bytes per pixel (24-bit BMP)
    // 计算存储原始 BMP 行数据（包括填充）所需的最大缓冲区大小
    // BMP 行数据需要填充到 4 字节的倍数: ((width * BPP + 3) & ~3)
    // 这里假设最大宽度为屏幕宽度来分配缓冲区
    const int MAX_RAW_ROW_SIZE = ((SCREEN_WIDTH * BPP + 3) & ~3);
    const int RAW_BUFFER_SIZE = MAX_RAW_ROW_SIZE * BUFFER_ROWS;

    // Dynamically allocate buffers on the heap
    uint8_t* rawBuffer = new (std::nothrow) uint8_t[RAW_BUFFER_SIZE];
    uint16_t* pixelBuffer = new (std::nothrow) uint16_t[SCREEN_WIDTH];

    // Check if allocation succeeded
    if (!rawBuffer || !pixelBuffer) {
        Serial.println("ERROR: Failed to allocate drawing buffers in drawContent!");
        delete[] rawBuffer; // Safe even if nullptr
        delete[] pixelBuffer; // Safe even if nullptr
        displayManager.drawCenteredText("Memory Error", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        return false; // Cannot proceed, drawing did not complete (but wasn't interrupted by touch)
    }

    bool touchDetected = false; // Flag to track if touch interrupted drawing

    // --- 绘制可见图片 ---
    // 从 startImage 开始遍历，直到图片底部超出屏幕底部 (yOffset >= SCREEN_HEIGHT)
    for (int i = startImage; i < imageFiles.size() && yOffset < SCREEN_HEIGHT; i++)
    {
        // --- Check for touch interrupt before processing each image ---
        if (touchManager.isTouched()) {
            Serial.println("Touch detected during drawContent (image loop), stopping draw.");
            touchDetected = true;
            break;
        }
        // --- End touch check ---

        Serial.print("Loading image: ");
        Serial.println(imageFiles[i]);

        // 使用缓存的高度
        int height = (i < imageHeights.size()) ? imageHeights[i] : SCREEN_HEIGHT; // 获取当前图片的缓存高度

        // 打开图片文件
        File file = sdManager.openFile(imageFiles[i]);

        if (!file)
        {
            Serial.println("Failed to open file!");
            yOffset += height; // 即使文件打开失败，也要增加 yOffset，跳过这张图片的高度
            continue;
        }

        // --- 读取并验证 BMP 文件头 ---
        uint8_t header[54];
        size_t bytesRead = file.read(header, 54);
        if (bytesRead != 54)
        {
            Serial.println("Failed to read BMP header!");
            file.close();
            yOffset += height;
            continue;
        }
        // 验证 BMP 签名 ('BM')
        if (header[0] != 'B' || header[1] != 'M')
        {
            Serial.println("Invalid BMP file!");
            file.close();
            yOffset += height;
            continue;
        }
        // --- 结束 BMP 文件头处理 ---

        // 从文件头获取宽度和位深度 (高度使用缓存值)
        int width = *(int *)&header[18];
        // height = *(int *)&header[22]; // 使用缓存的高度
        int bitsPerPixel = *(short *)&header[28];

        Serial.print("Image dimensions (WxH): ");
        Serial.print(width);
        Serial.print("x");
        Serial.print(height); // 打印缓存的高度
        Serial.print(" @");
        Serial.print(bitsPerPixel);
        Serial.println("bpp");

        // 检查宽度和位深度是否支持
        if (width > SCREEN_WIDTH)
        {
            Serial.println("Image too wide!");
            file.close();
            yOffset += height;
            continue;
        }
        if (bitsPerPixel != 24)
        {
            Serial.println("Only 24-bit BMP supported!");
            file.close();
            yOffset += height;
            continue;
        }

        // --- 计算需要读取和绘制的行范围 ---
        // startY: 图片内部需要开始读取的行号 (0-based)
        //         如果图片顶部在屏幕上方 (yOffset < 0)，则从 -yOffset 行开始读
        // readHeight: 需要读取的总行数
        //             等于图片在屏幕可见区域的高度
        int startY = std::max(0, -yOffset); // 图片内开始读取的行
        // 可见区域的起始 Y = max(0, yOffset)
        // 可见区域的结束 Y = min(SCREEN_HEIGHT, yOffset + height)
        // 可见高度 = 可见结束 Y - 可见起始 Y
        // 需要读取的高度 = min(可见高度, 图片剩余高度)
        int readHeight = std::min(SCREEN_HEIGHT - std::max(0, yOffset), height - startY);

        Serial.print("Reading from y=");
        Serial.print(startY);
        Serial.print(" height=");
        Serial.println(readHeight);

        // --- 分块读取并绘制图片数据 ---
        if (readHeight > 0)
        {
            // 计算当前图片 BMP 行数据的实际大小（包括填充）
            int actualRowSize = ((width * BPP + 3) & ~3);
            // 获取像素数据在文件中的起始偏移量
            long dataOffset = (*(int *)&header[10]); // bfOffBits

            // 循环读取，每次读取 BUFFER_ROWS 行，或者剩余行数（如果不足）
            for (int row = startY; row < startY + readHeight; )
            {
                // --- Check for touch interrupt before reading each chunk ---
                if (touchManager.isTouched()) {
                    Serial.println("Touch detected during drawContent (chunk read loop), stopping draw.");
                    touchDetected = true;
                    break; // Exit the chunk reading loop for this image
                }
                // --- End touch check ---

                // 计算本次要读取的行数
                int rowsToRead = std::min(BUFFER_ROWS, (startY + readHeight) - row);
                int bufferBytesToRead = rowsToRead * actualRowSize; // 本次要读取的总字节数

                // 计算文件中要读取的第一行的位置
                // BMP 数据是倒置存储的（从下往上）
                // 文件中的行号 = (图片总高度 - 1 - 图片内行号)
                // 要读取的块中，图片内行号范围是 [firstRowInChunk, firstRowInChunk + rowsToRead - 1]
                // 对应文件中的行号范围是 [height - 1 - (firstRowInChunk + rowsToRead - 1), height - 1 - firstRowInChunk]
                // 我们需要定位到这个范围在文件中的起始位置，即对应图片行 (firstRowInChunk + rowsToRead - 1) 的数据起始位置
                int firstRowInChunk = row; // 当前块在图片中起始行号
                long pos = dataOffset + (height - 1 - (firstRowInChunk + rowsToRead - 1)) * actualRowSize;

                // 定位文件指针
                if (!file.seek(pos)) {
                    Serial.println("Seek failed!");
                    row += rowsToRead; // 跳过这个块
                    continue;
                }

                // 读取 BGR 数据块到 rawBuffer
                size_t actualBytesRead = file.read(rawBuffer, bufferBytesToRead);
                if (actualBytesRead != bufferBytesToRead) {
                    Serial.print("Chunk read failed! Expected ");
                    Serial.print(bufferBytesToRead);
                    Serial.print(", got ");
                    Serial.println(actualBytesRead);
                    row += rowsToRead; // 跳过这个块
                    continue;
                }

                // --- 逐行处理缓冲区中的数据 ---
                // 因为我们读取的是文件中的连续块，对应图片中从下往上的行
                // 所以处理缓冲区时，需要从缓冲区尾部向前处理，或者调整指针计算
                for (int chunkRowIndex = 0; chunkRowIndex < rowsToRead; ++chunkRowIndex)
                {
                    // --- Check for touch interrupt before processing each row ---
                    if (touchManager.isTouched()) {
                        Serial.println("Touch detected during drawContent (row process loop), stopping draw.");
                        touchDetected = true;
                        break; // Exit the row processing loop for this chunk
                    }
                    // --- End touch check ---

                    // 当前处理的行在图片中的实际行号
                    int currentRowInImage = firstRowInChunk + chunkRowIndex;
                    // 获取当前行数据在 rawBuffer 中的起始指针
                    // rawBuffer[0] 对应图片行 firstRowInChunk + rowsToRead - 1
                    // rawBuffer[actualRowSize] 对应图片行 firstRowInChunk + rowsToRead - 2
                    // ...
                    // rawBuffer[(rowsToRead - 1 - chunkRowIndex) * actualRowSize] 对应图片行 firstRowInChunk + chunkRowIndex
                    uint8_t* currentRowPtr = rawBuffer + (rowsToRead - 1 - chunkRowIndex) * actualRowSize;

                    // 计算该行在屏幕上的 Y 坐标
                    // screenRowY = 图片顶部屏幕坐标 + 图片内行号 - 图片内起始读取行号
                    int screenRowY = yOffset + currentRowInImage - startY;

                    // 检查是否在屏幕范围内
                    if (screenRowY >= 0 && screenRowY < SCREEN_HEIGHT)
                    {
                        // --- 转换 BGR 到 RGB565 ---
                        for (int col = 0; col < width; col++)
                        {
                            pixelBuffer[col] = displayManager.getTFT()->color565(
                                currentRowPtr[col * BPP + 2], // R
                                currentRowPtr[col * BPP + 1], // G
                                currentRowPtr[col * BPP + 0]);// B
                        }
                        // --- 推送一行像素到屏幕 ---
                        // TFT_eSPI 的 pushImage 需要 uint16_t* 数据
                        // 可能需要设置字节交换，具体取决于 TFT_eSPI 配置和目标硬件
                        displayManager.getTFT()->setSwapBytes(true);
                        displayManager.getTFT()->pushImage(0, screenRowY, width, 1, pixelBuffer);
                    }
                }
                // --- 结束处理缓冲区 ---
                if (touchDetected) break; // Exit chunk loop if touch detected in row loop

                // 移动到下一个块的起始行
                row += rowsToRead;
            } // End chunk reading loop
        }
        // --- 结束分块读取和绘制 ---

        file.close(); // 关闭当前图片文件

        if (touchDetected) break; // Exit image loop if touch detected in chunk/row loop

        yOffset += height; // 更新屏幕 Y 坐标，准备绘制下一张图片
    } // End image loop
    // --- 结束绘制所有可见图片 ---

    // --- Clean up dynamically allocated buffers ---
    delete[] rawBuffer;
    delete[] pixelBuffer;

    if (touchDetected) {
        Serial.println("DrawContent interrupted by touch.");
        return true; // Indicate interruption
    }

    return false; // Drawing completed without interruption
}


/**
 * @brief 处理屏幕滚动（优化版本）。
 * 通过复制屏幕上未改变的部分，并只重绘新暴露的区域，来提高滚动性能。
 *
 * @param scrollDelta 滚动的像素距离。正值表示向下滚动（内容向上移动），负值表示向上滚动（内容向下移动）。
 *
 * @details (当前实现为临时测试，直接全屏重绘)
 * - 获取 TFT_eSPI 实例。
 * - 如果滚动距离为 0，直接返回。
 * - (原始逻辑 - 已注释掉) 计算需要复制的区域高度 (copyHeight) 和源/目标 Y 坐标 (srcY, dstY)。
 * - (原始逻辑 - 已注释掉) 使用 tft->readRect() 和 tft->pushImage() 逐行复制屏幕内容。
 * - (原始逻辑 - 已注释掉) 计算新暴露区域的位置 (newAreaY) 和高度 (newAreaHeight)。
 * - (原始逻辑 - 已注释掉) 使用 tft->fillRect() 清空新暴露的区域。
 * - (原始逻辑 - 已注释掉) 调用 drawNewArea() 绘制新暴露区域的内容。
 * - (当前临时逻辑) 直接清屏并调用 drawNewArea() 重绘整个屏幕。这是为了调试 drawNewArea，后续应恢复原始逻辑。
 */
void ComicViewerPage::scrollDisplay(int scrollDelta)
{
    TFT_eSPI *tft = displayManager.getTFT();
    Serial.print("Scrolling display by delta: ");
    Serial.println(scrollDelta);

    if (scrollDelta == 0) return; // 无需滚动

    // --- 临时测试：跳过屏幕复制，直接清屏并重绘整个可见区域 ---
    // 这有助于验证 drawNewArea 是否能正确绘制任意区域
    Serial.println("TEMPORARY TEST: Clearing screen and redrawing full area.");
    tft->fillScreen(TFT_WHITE); // 清空屏幕
    // 调用 drawNewArea 绘制整个屏幕 (y=0, h=SCREEN_HEIGHT)
    // drawNewArea 内部会根据当前的 scrollOffset 来绘制正确的内容
    if (drawNewArea(0, SCREEN_HEIGHT)) {
        // If drawing was interrupted, handle the touch immediately
        handleTouchInterrupt();
    }
    // --- 结束临时测试 ---

    /* --- 原始优化滚动逻辑 (注释掉以进行测试) ---
    ... [omitted original scroll logic for brevity] ...
    // --- 绘制新暴露区域的内容 ---
    Serial.println("Drawing new area content...");
    if (drawNewArea(newAreaY, newAreaHeight)) {
        // If drawing was interrupted, handle the touch immediately
        handleTouchInterrupt();
    }
    // 计算屏幕复制操作的参数
    int copyHeight = SCREEN_HEIGHT - abs(scrollDelta); // 需要复制的高度
    int srcY, dstY; // 源 Y 坐标, 目标 Y 坐标

    if (scrollDelta > 0) { // 向下滚动 (内容上移)
        srcY = scrollDelta; // 从 scrollDelta 行开始读
        dstY = 0;           // 写到第 0 行
    } else { // 向上滚动 (内容下移)
        srcY = 0;           // 从第 0 行开始读
        dstY = -scrollDelta; // 写到 -scrollDelta 行 (scrollDelta 是负数)
    }

    // 执行屏幕复制 (使用行缓冲)
    Serial.println("Performing screen copy...");
    uint16_t rowBuffer[SCREEN_WIDTH]; // 用于读写一行的缓冲区

    if (scrollDelta > 0) { // 向下滚动，从下往上复制，避免覆盖未读数据
        for (int y = copyHeight - 1; y >= 0; --y) {
            tft->readRect(0, srcY + y, SCREEN_WIDTH, 1, rowBuffer); // 读取源行
            tft->pushImage(0, dstY + y, SCREEN_WIDTH, 1, rowBuffer); // 写入目标行
        }
    } else { // 向上滚动，从上往下复制
        for (int y = 0; y < copyHeight; ++y) {
            tft->readRect(0, srcY + y, SCREEN_WIDTH, 1, rowBuffer);
            tft->pushImage(0, dstY + y, SCREEN_WIDTH, 1, rowBuffer);
        }
    }
    Serial.println("Screen copy finished.");

    // --- 清空新暴露的区域 ---
    int newAreaY, newAreaHeight;
    if (scrollDelta > 0) { // 新区域在底部
        newAreaY = SCREEN_HEIGHT - scrollDelta;
        newAreaHeight = scrollDelta;
    } else { // 新区域在顶部
        newAreaY = 0;
        newAreaHeight = -scrollDelta;
    }
    tft->fillRect(0, newAreaY, SCREEN_WIDTH, newAreaHeight, TFT_WHITE); // 用白色填充
    Serial.print("Cleared new area at Y=");
    Serial.print(newAreaY);
    Serial.print(", Height=");
    Serial.println(newAreaHeight);

    // --- 绘制新暴露区域的内容 ---
    Serial.println("Drawing new area content...");
    drawNewArea(newAreaY, newAreaHeight);
    --- 结束原始优化滚动逻辑 --- */
}


/**
 * @brief 绘制屏幕上指定矩形区域内的漫画内容。
 * 用于优化滚动，只绘制滚动后新暴露出来的区域。
 *
 * @param y 要绘制区域的屏幕起始 Y 坐标。
 * @param h 要绘制区域的屏幕高度。
 *
 * @details
 * - 如果没有图片，直接返回。
 * - 计算需要绘制的区域在整个漫画长条中的绝对 Y 坐标范围 [startAbsoluteY, endAbsoluteY)。
 *   startAbsoluteY = scrollOffset + y
 *   endAbsoluteY = startAbsoluteY + h
 * - (如果需要) 重新计算每张图片的起始绝对 Y 坐标 (startPositions)，基于缓存的高度 (imageHeights)。
 * - 遍历图片列表 (imageFiles) 和它们的起始位置 (startPositions)：
 *   - 获取当前图片的缓存高度 (imgHeight) 和起始绝对 Y 坐标 (imgStartY)。
 *   - 计算图片的结束绝对 Y 坐标 (imgEndY = imgStartY + imgHeight)。
 *   - 检查当前图片 [imgStartY, imgEndY) 是否与需要绘制的绝对范围 [startAbsoluteY, endAbsoluteY) 有重叠。
 *   - 如果有重叠：
 *     - 打开图片文件。
 *     - 读取并验证 BMP 头，获取宽度、位深度等信息。
 *     - 计算这张图片内部需要被绘制的行范围 [drawStartRowInImage, drawEndRowInImage)。
 *       drawStartRowInImage = max(0, startAbsoluteY - imgStartY)
 *       drawEndRowInImage = min(imgHeight, endAbsoluteY - imgStartY)
 *     - 计算绘制起始行 (drawStartRowInImage) 在屏幕上的目标 Y 坐标 (screenY)。
 *       screenY = y + (imgStartY + drawStartRowInImage) - startAbsoluteY
 *     - 使用与 drawContent() 类似的分块读取和绘制逻辑，但只处理 [drawStartRowInImage, drawEndRowInImage) 范围内的行。
 *     - 在将像素行推送到屏幕前，检查计算出的当前屏幕 Y 坐标 (currentScreenY) 是否在目标绘制区域 [y, y + h) 内。
 *     - 关闭文件。
 *   - 如果当前图片的起始 Y 坐标 (imgStartY) 已经超出了需要绘制的范围 (endAbsoluteY)，则可以提前结束遍历。
 * @return true if drawing was interrupted by touch, false otherwise.
 */
bool ComicViewerPage::drawNewArea(int y, int h)
{
    Serial.print("Drawing new area: y=");
    Serial.print(y);
    Serial.print(", h=");
    Serial.println(h);

    if (imageFiles.empty()) return false; // 没有图片可绘制

    // --- 计算绝对 Y 坐标范围 ---
    // 需要绘制的区域在整个漫画长条中的起始和结束 Y 坐标
    int startAbsoluteY = scrollOffset + y;
    int endAbsoluteY = startAbsoluteY + h;

    Serial.print("Absolute Y range to draw: ");
    Serial.print(startAbsoluteY);
    Serial.print(" to ");
    Serial.println(endAbsoluteY);

    // --- 计算图片起始位置 (如果需要) ---
    // 如果 startPositions 不是成员变量或在调用前未计算，需要在这里计算
    // (假设它在 drawContent 或 loadImages 中已计算并可用，或者重新计算)
    std::vector<int> startPositions;
    int currentHeightSum = 0;
    for (int h_cached : imageHeights) { // 使用缓存的高度
        startPositions.push_back(currentHeightSum);
        currentHeightSum += h_cached;
    }
    // --- 结束计算图片起始位置 ---

    // --- 定义缓冲区 (与 drawContent 相同) ---
    // 缓冲区大小，减少以降低单次 heap 分配大小，缓解碎片问题
    const int BUFFER_ROWS = 8; // Reduced from 16
    const int BPP = 3;
    const int MAX_RAW_ROW_SIZE = ((SCREEN_WIDTH * BPP + 3) & ~3);
    const int RAW_BUFFER_SIZE = MAX_RAW_ROW_SIZE * BUFFER_ROWS;
    // Dynamically allocate buffers on the heap
    uint8_t* rawBuffer = new (std::nothrow) uint8_t[RAW_BUFFER_SIZE];
    uint16_t* pixelBuffer = new (std::nothrow) uint16_t[SCREEN_WIDTH];

    // Check if allocation succeeded
    if (!rawBuffer || !pixelBuffer) {
        Serial.println("ERROR: Failed to allocate drawing buffers in drawNewArea!");
        delete[] rawBuffer; // Safe even if nullptr
        delete[] pixelBuffer; // Safe even if nullptr
        // Don't return here, maybe just skip drawing the new area or draw an error?
        // For now, just log and continue (might result in visual glitches)
    }
    // --- 结束定义缓冲区 ---

    bool touchDetected = false; // Flag for touch interruption

    // --- 遍历图片，绘制与目标区域重叠的部分 ---
    for (size_t i = 0; i < imageFiles.size(); ++i) {
        // --- Check for touch interrupt before processing each image ---
        if (touchManager.isTouched()) {
            Serial.println("Touch detected during drawNewArea (image loop), stopping draw.");
            touchDetected = true;
            break;
        }
        // --- End touch check ---

        // 获取图片信息
        int imgHeight = (i < imageHeights.size()) ? imageHeights[i] : SCREEN_HEIGHT; // 缓存高度
        int imgStartY = (i < startPositions.size()) ? startPositions[i] : -1; // 起始绝对 Y
        if (imgStartY == -1) continue; // 数据不一致，跳过
        int imgEndY = imgStartY + imgHeight; // 结束绝对 Y

        // --- 检查图片是否与目标绘制区域 [startAbsoluteY, endAbsoluteY) 重叠 ---
        if (imgEndY > startAbsoluteY && imgStartY < endAbsoluteY) {
            Serial.print("Image ");
            Serial.print(i);
            Serial.print(" ("); Serial.print(imgStartY); Serial.print("-"); Serial.print(imgEndY);
            Serial.print(") overlaps with target range (");
            Serial.print(startAbsoluteY); Serial.print("-"); Serial.print(endAbsoluteY); Serial.println(")");

            // 打开文件并读取头信息 (与 drawContent 类似)
            File file = sdManager.openFile(imageFiles[i]);
            if (!file) continue;

            uint8_t header[54];
            if (file.read(header, 54) != 54 || header[0] != 'B' || header[1] != 'M') {
                file.close(); continue;
            }
            int width = *(int *)&header[18];
            int bitsPerPixel = *(short *)&header[28];
            if (width > SCREEN_WIDTH || bitsPerPixel != 24) {
                 file.close(); continue;
            }
            int rowSize = ((width * BPP + 3) & ~3); // 当前图片的行大小
            long dataOffset = (*(int *)&header[10]);

            // --- 计算图片内部需要绘制的行范围 ---
            // drawStartRowInImage: 图片内开始绘制的行号 (0-based)
            // drawEndRowInImage: 图片内结束绘制的行号 (exclusive)
            int drawStartRowInImage = std::max(0, startAbsoluteY - imgStartY);
            int drawEndRowInImage = std::min(imgHeight, endAbsoluteY - imgStartY);

            Serial.print("  Drawing rows ");
            Serial.print(drawStartRowInImage);
            Serial.print(" to ");
            Serial.print(drawEndRowInImage);
            Serial.print(" of this image");

            // --- 计算绘制起始行在屏幕上的 Y 坐标 ---
            // screenY = 目标区域起始Y + (图片内绘制起始行绝对坐标 - 目标区域绝对起始坐标)
            //         = y + (imgStartY + drawStartRowInImage) - startAbsoluteY
            int screenY = y + (imgStartY + drawStartRowInImage) - startAbsoluteY;
            Serial.print(" starting at screen Y: ");
            Serial.println(screenY);

            // --- 分块读取并绘制相关行 ---
            if (drawStartRowInImage < drawEndRowInImage) // 确保有行需要绘制
            {
                for (int row = drawStartRowInImage; row < drawEndRowInImage; )
                {
                    // --- Check for touch interrupt before reading each chunk ---
                    if (touchManager.isTouched()) {
                        Serial.println("Touch detected during drawNewArea (chunk read loop), stopping draw.");
                        touchDetected = true;
                        break; // Exit chunk loop for this image
                    }
                    // --- End touch check ---

                    int rowsToRead = std::min(BUFFER_ROWS, drawEndRowInImage - row);
                    int bufferBytesToRead = rowsToRead * rowSize;
                    int firstRowInChunk = row;
                    long pos = dataOffset + (imgHeight - 1 - (firstRowInChunk + rowsToRead - 1)) * rowSize;

                    if (!file.seek(pos)) {
                        Serial.println("Seek failed in drawNewArea!");
                        row += rowsToRead; continue;
                    }

                    size_t actualBytesRead = file.read(rawBuffer, bufferBytesToRead);
                    if (actualBytesRead != bufferBytesToRead) {
                        Serial.print("Chunk read failed in drawNewArea! Expected ");
                        Serial.print(bufferBytesToRead); Serial.print(", got "); Serial.println(actualBytesRead);
                        row += rowsToRead; continue;
                    }

                    // 处理缓冲区中的每一行
                    for (int chunkRowIndex = 0; chunkRowIndex < rowsToRead; ++chunkRowIndex)
                    {
                        // --- Check for touch interrupt before processing each row ---
                        if (touchManager.isTouched()) {
                            Serial.println("Touch detected during drawNewArea (row process loop), stopping draw.");
                            touchDetected = true;
                            break; // Exit row loop for this chunk
                        }
                        // --- End touch check ---

                        int currentRowInImage = firstRowInChunk + chunkRowIndex;
                        uint8_t* currentRowPtr = rawBuffer + (rowsToRead - 1 - chunkRowIndex) * rowSize;

                        // 计算当前行在屏幕上的目标 Y 坐标
                        int currentScreenY = screenY + (currentRowInImage - drawStartRowInImage);

                        // --- 检查是否在指定的绘制区域 [y, y + h) 内 ---
                        if (currentScreenY >= y && currentScreenY < y + h) {
                            // 转换 BGR 到 RGB565
                            for (int col = 0; col < width; col++) {
                                pixelBuffer[col] = displayManager.getTFT()->color565(
                                    currentRowPtr[col * BPP + 2], // R
                                    currentRowPtr[col * BPP + 1], // G
                                    currentRowPtr[col * BPP + 0]);// B
                            }
                            // 推送像素行
                            displayManager.getTFT()->setSwapBytes(true);
                            displayManager.getTFT()->pushImage(0, currentScreenY, width, 1, pixelBuffer);
                        }
                    } // End row processing loop

                    if (touchDetected) break; // Exit chunk loop if touch detected in row loop

                    row += rowsToRead; // 移动到下一个块
                } // End chunk reading loop
            }
            // --- 结束分块绘制 ---
            file.close(); // 关闭文件
        }
        // --- 结束处理重叠图片 ---

        if (touchDetected) break; // Exit image loop if touch detected

        // --- 优化：如果当前图片已经完全在目标绘制区域下方，停止遍历 ---
        if (imgStartY >= endAbsoluteY) {
             Serial.println("Image is past the target range, stopping.");
             break;
        }
    } // End image loop
    // --- 结束遍历所有图片 ---

    // --- Clean up dynamically allocated buffers ---
    // Important: Only delete if allocation succeeded!
    delete[] rawBuffer;
    delete[] pixelBuffer;

    if (touchDetected) {
        Serial.println("DrawNewArea interrupted by touch.");
        return true; // Indicate interruption
    } else {
        Serial.println("Finished drawing new area.");
        return false; // Indicate successful completion
    }
}


/**
 * @brief 处理漫画阅读器中的触摸事件，主要用于滚动和返回。
 *
 * @param x 触摸点的 X 坐标。
 * @param y 触摸点的 Y 坐标。
 * @return bool 如果触摸事件被处理（用于滚动或返回），则返回 true；否则返回 false。
 *
 * @details
 * - 实现简单的双击检测：如果在 500ms 内连续点击，则触发返回操作。
 * - 根据触摸点的 Y 坐标判断滚动方向：
 *   - 触摸屏幕底部 1/4 区域：向下滚动 (scrollDelta = SCREEN_HEIGHT / 4)。
 *   - 触摸屏幕顶部 1/4 区域：向上滚动 (scrollDelta = -SCREEN_HEIGHT / 4)。
 * - 如果确定了滚动方向 (scrollDelta != 0)：
 *   - 记录旧的滚动偏移 (oldScrollOffset)。
 *   - 计算最大允许的滚动偏移 (maxScrollOffset)，基于缓存的总高度。
 *   - 更新滚动偏移 (scrollOffset)，并使用 std::max 和 std::min 将其限制在 [0, maxScrollOffset] 范围内。
 *   - 计算实际发生的滚动距离 (actualScrollDelta = scrollOffset - oldScrollOffset)。
 *   - 如果实际发生了滚动 (actualScrollDelta != 0)，调用优化后的 scrollDisplay() 函数来更新屏幕。
 *   - 返回 true，表示触摸事件已处理。
 * - 如果触摸点不在滚动区域内，则返回 false（将在 handleTouch 中处理为返回操作）。
 */
bool ComicViewerPage::handleScrollGesture(uint16_t x, uint16_t y)
{
    // --- 双击检测 ---
    static uint32_t lastTapTime = 0; // 记录上次点击时间
    uint32_t currentTime = millis(); // 获取当前时间

    // 如果两次点击间隔小于 500 毫秒，视为双击
    if (currentTime - lastTapTime < 500)
    {
        Serial.println("Double tap detected, returning to browser");
        //Router::getInstance().goBack(); // 返回到上一个页面（文件浏览器）
        //不返回了，不然太难用了
        lastTapTime = 0; // 重置时间，避免连续触发
        return true; // 事件已处理
    }
    lastTapTime = currentTime; // 更新上次点击时间
    // --- 结束双击检测 ---

    // --- 滚动逻辑 ---
    int scrollDelta = 0; // 请求的滚动距离
    Serial.print("Touch position in Comic Viewer: (");
    Serial.print(x); Serial.print(", "); Serial.print(y); Serial.println(")");

    // 判断触摸区域
    if (y > (SCREEN_HEIGHT * 3) / 4) { // 触摸屏幕底部 1/4
        Serial.println("Scrolling down request");
        scrollDelta = SCREEN_HEIGHT / 4; // 请求向下滚动四分之一屏幕
    } else if (y < SCREEN_HEIGHT / 4) { // 触摸屏幕顶部 1/4
        Serial.println("Scrolling up request");
        scrollDelta = -SCREEN_HEIGHT / 4; // 请求向上滚动四分之一屏幕
    }

    // 如果请求了滚动
    if (scrollDelta != 0) {
        int oldScrollOffset = scrollOffset; // 保存当前偏移量
        // 计算最大滚动偏移量
        int maxScrollOffset = (totalComicHeight > SCREEN_HEIGHT) ? (totalComicHeight - SCREEN_HEIGHT) : 0;

        // 更新滚动偏移量，并确保不越界
        scrollOffset = std::max(0, std::min(scrollOffset + scrollDelta, maxScrollOffset));

        // 计算实际滚动了多少
        int actualScrollDelta = scrollOffset - oldScrollOffset;

        Serial.print("Old offset: "); Serial.print(oldScrollOffset);
        Serial.print(", New offset: "); Serial.print(scrollOffset);
        Serial.print(", Max offset: "); Serial.print(maxScrollOffset);
        Serial.print(", Actual delta: "); Serial.println(actualScrollDelta);

        // 如果实际发生了滚动
        if (actualScrollDelta != 0) {
             // 调用优化后的滚动函数来更新显示
             scrollDisplay(actualScrollDelta);
        } else {
            Serial.println("Scroll hit boundary, no change.");
        }
        return true; // 触摸事件已处理（即使未滚动，也视为处理了滚动尝试）
    }
    // --- 结束滚动逻辑 ---

    return false; // 触摸不在滚动区域，未处理
}


/**
 * @brief Helper function to process a touch event that interrupted drawing.
 * Reads the touch point and calls the main handleTouch logic.
 */
void ComicViewerPage::handleTouchInterrupt() {
    uint16_t tx, ty;
    // Attempt to get the touch point that caused the interrupt
    if (touchManager.getPoint(tx, ty)) {
        Serial.print("Handling touch interrupt at: (");
        Serial.print(tx); Serial.print(", "); Serial.print(ty); Serial.println(")");
        handleTouch(tx, ty); // Process the touch using the standard handler
    } else {
        Serial.println("Could not get touch point after interrupt.");
        // Optionally, trigger a redraw or other recovery action if needed
    }
}


/**
 * @brief 设置当前要显示的漫画所在的目录路径。
 * 会重置滚动偏移，并调用 loadImages() 重新加载该目录下的图片。
 *
 * @param path 漫画目录的完整路径。
 */
void ComicViewerPage::setComicPath(const String &path)
{
    Serial.print("Setting comic path to: ");
    Serial.println(path);
    currentPath = path;   // 更新当前路径
    scrollOffset = 0;     // 重置滚动偏移到顶部
    loadImages();         // 加载新路径下的图片并计算高度
    Serial.print("Image count after loading: ");
    Serial.println(imageFiles.size());
}

/**
 * @brief 显示漫画阅读器页面的主函数。
 * 调用 drawContent() 来绘制当前视口的漫画内容。
 */
void ComicViewerPage::display()
{
    Serial.print("Display called, image count: ");
    Serial.println(imageFiles.size());
    // 绘制内容（会根据当前的 scrollOffset 绘制）
    if (drawContent()) {
        // If drawing was interrupted, handle the touch immediately
        handleTouchInterrupt();
    }
}

/**
 * @brief 处理页面上的所有触摸事件。
 *
 * @param x 触摸点的 X 坐标。
 * @param y 触摸点的 Y 坐标。
 *
 * @details
 * - 首先尝试调用 handleScrollGesture() 处理滚动或双击返回事件。
 * - 如果 handleScrollGesture() 返回 false（表示触摸事件未被处理），
 *   则认为用户点击了屏幕中间区域，触发返回操作。
 */
void ComicViewerPage::handleTouch(uint16_t x, uint16_t y)
{
    // 优先处理滚动和双击返回
    if (!handleScrollGesture(x, y))
    {
        // 如果不是滚动/双击，则认为是点击中间区域返回
        Serial.println("Middle area touched, returning to browser");
        Router::getInstance().goBack(); // 返回到上一个页面
    }
}

/**
 * @brief 清理页面资源，特别是释放 vector 占用的内存。
 */
void ComicViewerPage::cleanup() {
    Serial.println("ComicViewerPage::cleanup() called.");
    imageFiles.clear();
    imageFiles.shrink_to_fit(); // Attempt to release vector memory
    imageHeights.clear();
    imageHeights.shrink_to_fit(); // Attempt to release vector memory
    // Reset other state if necessary
    currentPath = "";
    scrollOffset = 0;
    totalComicHeight = 0;
    Serial.println("ComicViewerPage resources cleaned up.");
}

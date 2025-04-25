# 中文字体系统使用说明

## 概述
本系统实现了ESP32上的中文字体显示支持，使用点阵字体并通过SD卡动态加载，以解决内存限制问题。

## 文件结构
- `tools/font_generator.py`: 点阵字体生成工具
- `tools/sample_chars.txt`: 示例中文字符集
- `font.h/cpp`: 字体加载和管理类
- `display.h/cpp`: 显示控制类（已集成中文支持）

## 使用步骤

### 1. 生成字体文件

#### 准备工作
1. 安装Python依赖：
```bash
pip install pillow
```

2. 准备中文TTF字体文件（建议使用开源字体如文泉驿）

#### 生成字体数据
```bash
cd tools
python font_generator.py your_font.ttf sample_chars.txt
```

这将在`font_data`目录下生成：
- 点阵字体文件 (*.font)
- 索引文件 (index_*.json)

### 2. 部署到SD卡

1. 在SD卡根目录创建`font_data`文件夹
2. 将生成的所有.font和.json文件复制到此文件夹

### 3. 代码中使用

```cpp
// 初始化字体系统
Font::getInstance().begin();

// 显示中文文本
display.drawText("你好世界", 10, 10, 2); // size=2对应32x32点阵

// 居中显示
display.drawCenteredText("加载中", x, y, width, height, 2);
```

## 字体大小说明
- size=1: 16x16点阵
- size=2: 32x32点阵（推荐用于标准文本）
- size=3: 48x48点阵（适用于标题）

## 内存优化
- 字体数据按需从SD卡加载
- 索引文件分片存储
- 使用固定大小缓冲区

## 注意事项
1. 确保SD卡正确初始化
2. 使用前检查font_data目录存在
3. ASCII字符仍使用内置字体，无需加载点阵
4. 建议将常用字符添加到sample_chars.txt以生成点阵数据

#!/usr/bin/env python3
import os
from PIL import Image, ImageDraw, ImageFont
import struct
import argparse
import json
import numpy as np

class FontGenerator:
    def __init__(self, ttf_path, sizes=[16, 24, 32], chars_per_file=100):
        self.ttf_path = ttf_path
        self.sizes = sizes
        self.chars_per_file = chars_per_file
        self.output_dir = "font_data"
        self.debug_dir = os.path.join(self.output_dir, "debug")
        os.makedirs(self.output_dir, exist_ok=True)
        os.makedirs(self.debug_dir, exist_ok=True)

    def char_to_bitmap(self, char, size):
        # 创建更大的画布以确保完整捕获字符
        canvas_size = int(size * 2)  # 调整画布大小
        img = Image.new('L', (canvas_size, canvas_size), 'white')
        draw = ImageDraw.Draw(img)
        font = ImageFont.truetype(self.ttf_path, size)
        
        # 计算字符边界框
        bbox = draw.textbbox((0, 0), char, font=font)
        char_width = bbox[2] - bbox[0]
        char_height = bbox[3] - bbox[1]
        
        # 在画布中心绘制字符
        x = (canvas_size - char_width) // 2
        y = (canvas_size - char_height) // 2 - bbox[1]  # 调整垂直位置
        draw.text((x, y), char, font=font, fill='black')
        
        # 计算裁剪框，确保字符完整
        left = (canvas_size - size) // 2
        top = (canvas_size - size) // 2
        right = left + size
        bottom = top + size
        crop_box = (left, top, right, bottom)
        img = img.crop(crop_box)
        
        # 增强对比度并二值化
        img = img.point(lambda x: 0 if x < 200 else 255)
        
        # 将图像数据转换为位图数据
        bitmap = []
        pixels = np.array(img)
        for row in pixels:
            for pixel in row:
                bitmap.append(1 if pixel < 128 else 0)
        
        try:
            # 保存调试图片
            debug_path = os.path.join(self.debug_dir, f"{char}_{size}.png")
            os.makedirs(self.debug_dir, exist_ok=True)
            img.save(debug_path)
        except Exception as e:
            print(f"Error saving debug image for char '{char}': {e}")

        return bitmap

    def generate_font_data(self, chars):
        index_data = {}
        for size in self.sizes:
            # 按块处理字符
            for chunk_idx, i in enumerate(range(0, len(chars), self.chars_per_file)):
                chunk_chars = chars[i:i + self.chars_per_file]
                font_data = []
                
                # 为每个字符生成位图数据
                for char in chunk_chars:
                    bitmap = self.char_to_bitmap(char, size)
                    font_data.extend(bitmap)
                
                # 保存字体数据
                font_filename = f"{size}x{size}_{chunk_idx + 1}.font"
                font_path = os.path.join(self.output_dir, font_filename)
                
                # 将位数据转换为字节（每8位一个字节）
                font_bytes = bytearray()
                for i in range(0, len(font_data), 8):
                    byte = 0
                    for j in range(8):
                        if i + j < len(font_data) and font_data[i + j]:
                            byte |= (1 << j)
                    font_bytes.append(byte)
                
                with open(font_path, 'wb') as f:
                    f.write(font_bytes)
                
                # 更新索引数据
                for idx, char in enumerate(chunk_chars):
                    if char not in index_data:
                        index_data[char] = {}
                    index_data[char][str(size)] = {
                        'file': font_filename,
                        'offset': idx * (size * size) // 8
                    }
        
        # 保存索引数据（按块）
        chars_list = list(index_data.keys())
        for chunk_idx, i in enumerate(range(0, len(chars_list), self.chars_per_file)):
            chunk_chars = chars_list[i:i + self.chars_per_file]
            chunk_data = {char: index_data[char] for char in chunk_chars}
            
            index_filename = f"index_{chunk_idx + 1}.json"
            index_path = os.path.join(self.output_dir, index_filename)
            with open(index_path, 'w', encoding='utf-8') as f:
                json.dump(chunk_data, f, ensure_ascii=False, indent=2)

def main():
    parser = argparse.ArgumentParser(description='Generate dot matrix font data')
    parser.add_argument('ttf_path', help='Path to TTF font file')
    parser.add_argument('chars_file', help='File containing characters to convert')
    parser.add_argument('--sizes', nargs='+', type=int, default=[16, 24, 32],
                      help='Font sizes to generate')
    args = parser.parse_args()

    # 读取字符
    with open(args.chars_file, 'r', encoding='utf-8') as f:
        chars = f.read().strip()

    # 生成字体数据
    generator = FontGenerator(args.ttf_path, args.sizes)
    generator.generate_font_data(chars)

if __name__ == '__main__':
    main()

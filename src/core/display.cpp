#include "display.h"

// Initialize static instance pointer for Singleton
Display *Display::instance = nullptr;

/**
 * @brief Private constructor for Singleton pattern.
 * Initializes the TFT display, sets rotation, clears the screen,
 * configures default text settings, and enables the backlight.
 */
Display::Display()
{
    // Initialize the TFT object (pins are defined in User_Setup.h)
    tft.init();
    tft.setRotation(1); // Set landscape mode (adjust if needed)
    tft.fillScreen(TFT_BLACK); // Start with a black screen

    // Configure default text rendering settings
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);         // 基础字体大小
    tft.setTextFont(2);         // 使用Font2 (16像素高度)
    tft.setTextDatum(MC_DATUM); // Default to Middle Center datum for centered text

    // Enable the backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH); // Turn backlight on
}

/**
 * @brief Gets the singleton instance of the Display class.
 * Creates the instance if it doesn't exist yet.
 * @return Display& Reference to the singleton instance.
 */
Display &Display::getInstance()
{
    if (!instance)
    {
        instance = new Display();
    }
    return *instance;
}

/**
 * @brief Performs initial setup tasks after object creation (currently just clears screen).
 */
void Display::begin()
{
    clear(); // Ensure screen is clear on begin
}

/**
 * @brief Clears the entire display to black.
 */
void Display::clear()
{
    tft.fillScreen(TFT_BLACK);
}

/**
 * @brief Draws a single character at the specified coordinates.
 * Uses either the built-in Adafruit GFX font or the custom bitmap font
 * managed by the Font class, based on the character and useCustomFont flag.
 *
 * @param character The character string to draw (should be a single UTF-8 character).
 * @param x The x-coordinate of the top-left corner.
 * @param y The y-coordinate of the top-left corner.
 * @param size The font size multiplier (applies differently to built-in vs custom).
 * @param useCustomFont If true, attempts to use the custom font for non-ASCII chars.
 */
void Display::drawCharacter(const char *character, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont)
{
    if (!useCustomFont || Font::isAscii(character))
    {
        // Use built-in Adafruit GFX font for ASCII or when custom font is disabled
        tft.setTextFont(TEXT_FONT);
        tft.setTextSize(size);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextFont(TEXT_FONT); // Select the default built-in font (defined in config.h)
        tft.setTextSize(size);      // Set the size multiplier
        tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color (white on black)
        tft.setTextDatum(TL_DATUM); // IMPORTANT: Set datum to Top-Left for accurate positioning with drawString
        tft.drawString(character, x, y); // Draw the character
        return; // Done with built-in font
    }

    // Attempt to use custom bitmap font for non-ASCII characters
    Font &font = Font::getInstance(); // Get Font manager instance
    // Request bitmap data. Size is multiplied by 16 assuming custom font sizes are multiples of 16px.
    uint8_t *bitmap = font.getCharacterBitmap(character, size * 16);
    if (!bitmap)
    {
        // Fallback: Draw a placeholder if custom font data is missing?
        // Or just return silently.
        return; // Failed to load custom font data
    }

    // Calculate dimensions for the custom font character
    // Assuming custom fonts are square (width = height = size * 16)
    uint16_t fontWidth = size * 16;
    uint16_t fontHeight = size * 16;
    // Calculate the number of bytes per row in the bitmap data (packed bits)
    uint16_t byteWidth = (fontWidth + 7) / 8;

    // Draw the character pixel by pixel from the bitmap data
    for (uint16_t py = 0; py < fontHeight; py++)
    {
        for (uint16_t px = 0; px < fontWidth; px++)
        {
            // Calculate the byte index and bit index within that byte for the current pixel
            uint16_t byteIndex = (py * byteWidth) + (px / 8);
            // Bit order might depend on how the font was generated. Assuming LSB first.
            // Adjust (7 - (px % 8)) or similar if MSB first.
            uint8_t bitIndex = px % 8;
            // Check if the bit corresponding to the pixel is set
            if (bitmap[byteIndex] & (1 << bitIndex))
            {
                // Draw the pixel if the bit is set
                tft.drawPixel(x + px, y + py, TFT_WHITE);
            }
            // else: Background is assumed transparent or already drawn
        }
    }
}

/**
 * @brief Draws a string of text at the specified coordinates.
 * Handles both ASCII (using built-in font) and multi-byte UTF-8 characters
 * (using custom bitmap font if enabled and available). Iterates through
 * characters using Font::getNextCharacter.
 *
 * @param text The null-terminated string to draw.
 * @param x The x-coordinate of the top-left corner of the first character.
 * @param y The y-coordinate of the top-left corner of the first character.
 * @param size The font size multiplier.
 * @param useCustomFont If true, attempts to use the custom font for non-ASCII chars.
 */
void Display::drawText(const char *text, uint16_t x, uint16_t y, uint8_t size, bool useCustomFont)
{
    size_t offset = 0; // Tracks the current position in the input string
    uint16_t curX = x; // Tracks the x-coordinate for the next character

    while (true)
    {
        // Get the next UTF-8 character from the string
        String character = Font::getNextCharacter(text, offset);
        if (character.length() == 0)
            break; // End of string reached

        // Draw the current character
        drawCharacter(character.c_str(), curX, y, size, useCustomFont);

        // Calculate the width of the drawn character to advance curX
        uint16_t charWidth;
        if (!useCustomFont || Font::isAscii(character.c_str()))
        if (!useCustomFont || Font::isAscii(character.c_str()))
        {
            // Built-in font character width (Font 2 is 8 pixels wide base)
            charWidth = 8 * size;
        }
        else
        {
            // Custom font character width (assuming square)
            charWidth = size * 16;
        }
        // Advance the x-coordinate for the next character.
        // Apply kerning/spacing adjustment if desired (e.g., subtract a small value).
        curX += charWidth - size / 4; // Example: Reduce spacing slightly based on size
    }
}

/**
 * @brief Draws text centered within a specified rectangular area.
 * Calculates the total width of the text first, then determines the
 * starting coordinates to center it horizontally and vertically.
 *
 * @param text The null-terminated string to draw.
 * @param x The x-coordinate of the bounding box.
 * @param y The y-coordinate of the bounding box.
 * @param w The width of the bounding box.
 * @param h The height of the bounding box.
 * @param size The font size multiplier.
 * @param useCustomFont If true, attempts to use the custom font for non-ASCII chars.
 */
void Display::drawCenteredText(const char *text, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t size, bool useCustomFont)
{
    // --- Calculate total text width ---
    size_t offset = 0;
    uint16_t totalWidth = 0;
    // String temp = text; // Not needed

    while (true)
    {
        String character = Font::getNextCharacter(text, offset);
        if (character.length() == 0)
            break;

        uint16_t charWidth;
        if (!useCustomFont || Font::isAscii(character.c_str()))
        {
            charWidth = 8 * size; // ASCII width
        }
        else
        {
            charWidth = size * 16; // Custom font width
        }
        totalWidth += charWidth - size / 4; // Apply same spacing adjustment as drawText
    }
    // --- End Calculate total text width ---

    // --- Calculate starting coordinates for centering ---
    // Assuming text height is based on the custom font size (16px base)
    uint16_t charHeight = size * 16;
    // Center horizontally: start_x = box_x + (box_width - text_width) / 2
    uint16_t startX = x + (w - totalWidth) / 2;
    // Center vertically: start_y = box_y + (box_height - text_height) / 2
    uint16_t startY = y + (h - charHeight) / 2;

    // Draw the text at the calculated centered position
    // Note: drawText uses Top-Left alignment internally via drawCharacter
    drawText(text, startX, startY, size, useCustomFont);
}

// Removed drawFolder implementation
// Removed drawButton implementation

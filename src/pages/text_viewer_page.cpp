#include <Arduino.h>
#include <FS.h>
#include <algorithm>  // For std::min, std::max
#include <TFT_eSPI.h> // Include for color constants like TFT_LIGHTGREY

#include "text_viewer_page.h"
#include "../core/router.h"
#include "../core/sdcard.h" // Needed for file operations

// --- Constants ---
const int TEXT_FONT_SIZE = 1;   // Use font size 1 (e.g., 16x16)
const int TEXT_MARGIN_X = 5;    // Left/right margin for text
const int TEXT_MARGIN_Y = 5;    // Top/bottom margin for text
const int SCROLLBAR_WIDTH = 10; // Width of the scrollbar
const int SCROLLBAR_MARGIN = 2; // Margin between text and scrollbar
const int BACK_BUTTON_WIDTH = 60;
const int BACK_BUTTON_HEIGHT = 30;
const int BACK_BUTTON_X = 5;
const int BACK_BUTTON_Y = 5;

// Define content area based on button/header
const int CONTENT_Y = BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y * 2;
const int CONTENT_HEIGHT = SCREEN_HEIGHT - CONTENT_Y - TEXT_MARGIN_Y;

// --- Constructor ---
TextViewerPage::TextViewerPage()
    : displayManager(Display::getInstance()),
      fontManager(Font::getInstance()),
      currentScrollLine(0),
      totalLines(0),
      linesPerPage(0),
      lineHeight(0),
      fileLoaded(false) {}

// --- Public Methods ---

void TextViewerPage::setFilePath(const String &path)
{
    Serial.print("TextViewerPage::setFilePath received path: ");
    Serial.println(path); // Log the path as soon as it's received
    filePath = path;
    Serial.print("TextViewerPage::filePath assigned: ");
    Serial.println(filePath); // Log the path after assignment
    fileLoaded = false;       // Reset loaded flag when path changes
    currentScrollLine = 0;    // Reset scroll position
    lines.clear();            // Clear previous content
}

// Implementation of the virtual setParams method
void TextViewerPage::setParams(void *params)
{
    if (params)
    {
        // Cast the void pointer back to a String pointer
        String *pathPtr = static_cast<String *>(params);
        // Call the existing setFilePath method with the actual path String
        setFilePath(*pathPtr);
    }
    else
    {
        // Handle case where no parameters were passed (optional)
        Serial.println("TextViewerPage::setParams received null params.");
        // Maybe set a default state or show an error?
        setFilePath(""); // Set empty path to indicate an issue
    }
}

void TextViewerPage::display()
{
    if (!fileLoaded)
    {
        calculateLayout(); // Calculate layout based on font size
        loadContent();     // Load and wrap text content
    }

    displayManager.clear();

    // Draw Back Button (similar to FileBrowserPage)
    displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
    displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false); // Use built-in font for button

    // Draw Content Area Separator
    displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);

    if (fileLoaded)
    {
        drawContent();
        drawScrollbar();
    }
    else
    {
        // Display loading or error message
        displayManager.drawCenteredText("Loading...", 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, 2);
    }
}

void TextViewerPage::handleTouch(uint16_t x, uint16_t y)
{
    // Handle Back Button Touch
    if (x >= BACK_BUTTON_X && x < BACK_BUTTON_X + BACK_BUTTON_WIDTH &&
        y >= BACK_BUTTON_Y && y < BACK_BUTTON_Y + BACK_BUTTON_HEIGHT)
    {
        Router::getInstance().goBack(); // Navigate back
        return;
    }

    // Handle Scrolling Touch (only if content is scrollable)
    if (totalLines > linesPerPage)
    {
        // Check if touch is within the main content area (excluding header/button)
        if (y > BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y)
        {
            handleScroll(y);
        }
    }
}

// --- Private Helper Methods ---

void TextViewerPage::calculateLayout()
{
    // Use font size 1 (16x16) for calculations
    lineHeight = fontManager.getCharacterHeight(TEXT_FONT_SIZE * 16); // Assuming size 1 maps to 16px font
    if (lineHeight == 0)
        lineHeight = 16; // Fallback if font not loaded yet

    int availableHeight = SCREEN_HEIGHT - (BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y) - TEXT_MARGIN_Y; // Height below button
    linesPerPage = availableHeight / lineHeight;
    if (linesPerPage < 1)
        linesPerPage = 1; // Ensure at least one line fits
}

void TextViewerPage::loadContent()
{
    lines.clear();
    currentScrollLine = 0;
    fileLoaded = false;

    const char *pathCStr = filePath.c_str(); // Convert String to const char*

    Serial.print("Attempting to load content for (pathCStr): '");
    Serial.print(pathCStr); // Try printing pathCStr directly
    Serial.println("'");

    // Print raw bytes of the path string
    Serial.print("Raw bytes of pathCStr: ");
    for (int i = 0; pathCStr[i] != '\0'; ++i)
    {
        Serial.print(pathCStr[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Check if file exists first using const char*
    if (!SDCard::getInstance().exists(pathCStr))
    {
        Serial.print("File existence check failed for pathCStr: '");
        Serial.print(pathCStr);
        Serial.println("'");
        lines.push_back("Error: File not found.");
        totalLines = lines.size();
        fileLoaded = true;
        return;
    }
    else
    {
        Serial.println("File existence check passed. Attempting to open...");
    }

    // Open file using const char*
    File file = SDCard::getInstance().openFile(pathCStr);
    if (!file)
    {
        Serial.print("Failed to open file with SDCard::openFile() using pathCStr: '"); // Clarify which open failed
        Serial.print(pathCStr);
        Serial.println("'");
        lines.push_back("Error: Could not open file.");
        totalLines = lines.size();
        fileLoaded = true; // Mark as loaded even on error to show message
        return;
    }

    Serial.print("Loading file (pathCStr): '");
    Serial.print(pathCStr);
    Serial.println("'");

    // Calculate available width for text (screen width - margins - scrollbar)
    int availableWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    if (availableWidth <= 0)
    {
        Serial.println("Error: Screen too narrow for text.");
        lines.push_back("Error: Screen too narrow.");
        totalLines = lines.size();
        fileLoaded = true;
        file.close();
        return;
    }

    String currentLine = "";
    String wordBuffer = "";

    while (file.available())
    {
        char c = file.read();

        // Handle line breaks
        if (c == '\n' || c == '\r')
        {
            if (c == '\r' && file.peek() == '\n')
            {                // Handle CRLF
                file.read(); // Consume the '\n'
            }
            // Process the completed line (including the last word)
            if (wordBuffer.length() > 0)
            {
                currentLine += wordBuffer;
                wordBuffer = "";
            }
            lines.push_back(currentLine);
            currentLine = "";
            continue; // Move to next character
        }

        // Add character to word buffer
        wordBuffer += c;

        // Check if adding the current word exceeds the line width
        String testLine = currentLine + wordBuffer;
        int currentLineWidth = 0;
        size_t offset = 0;
        while (offset < testLine.length())
        {
            String nextChar = fontManager.getNextCharacter(testLine.c_str(), offset);
            currentLineWidth += fontManager.getCharacterWidth(TEXT_FONT_SIZE * 16); // Use configured font size
        }

        if (currentLineWidth > availableWidth)
        {
            // Word itself is too long or line is full
            if (currentLine.length() > 0)
            {
                // Add the line *without* the current word
                lines.push_back(currentLine);
                // Start new line with the current word
                currentLine = wordBuffer;
                wordBuffer = "";
            }
            else
            {
                // The word itself is longer than the line width
                // Break the word (simple approach: add what fits, carry over rest)
                String fittedPart = "";
                String remainingPart = "";
                int fittedWidth = 0;
                size_t wordOffset = 0;
                while (wordOffset < wordBuffer.length())
                {
                    String nextChar = fontManager.getNextCharacter(wordBuffer.c_str(), wordOffset);
                    int charWidth = fontManager.getCharacterWidth(TEXT_FONT_SIZE * 16);
                    if (fittedWidth + charWidth <= availableWidth)
                    {
                        fittedPart += nextChar;
                        fittedWidth += charWidth;
                    }
                    else
                    {
                        remainingPart += nextChar; // Add the rest to remaining
                    }
                }
                lines.push_back(fittedPart); // Add the part that fits
                currentLine = "";            // Start fresh line
                wordBuffer = remainingPart;  // Carry over the rest of the word
            }
        }
        else if (c == ' ' || c == '\t')
        {                              // Word boundary
            currentLine += wordBuffer; // Add word and the space/tab
            wordBuffer = "";           // Clear word buffer
        }
        // else: continue accumulating characters into wordBuffer
    }

    // Add any remaining content from the buffers
    if (wordBuffer.length() > 0)
    {
        currentLine += wordBuffer;
    }
    if (currentLine.length() > 0)
    {
        lines.push_back(currentLine);
    }

    file.close();
    totalLines = lines.size();
    fileLoaded = true;
    Serial.printf("File loaded. Total wrapped lines: %d\n", totalLines);
}

void TextViewerPage::drawContent()
{
    int y = BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y * 2; // Start drawing below button/header area
    int endLine = std::min(currentScrollLine + linesPerPage, totalLines);

    for (int i = currentScrollLine; i < endLine; ++i)
    {
        if (i >= 0 && i < lines.size())
        { // Bounds check
            displayManager.drawText(lines[i].c_str(), TEXT_MARGIN_X, y, TEXT_FONT_SIZE);
        }
        y += lineHeight;
    }
}

void TextViewerPage::drawScrollbar()
{
    if (totalLines <= linesPerPage)
        return; // No scrollbar needed

    int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    int scrollbarY = BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y; // Align with text area top
    int scrollbarHeight = SCREEN_HEIGHT - scrollbarY - TEXT_MARGIN_Y;    // Align with text area bottom

    // Draw scrollbar track
    displayManager.getTFT()->drawRect(scrollbarX, scrollbarY, SCROLLBAR_WIDTH, scrollbarHeight, TFT_DARKGREY);

    // Calculate handle size and position
    int handleHeight = std::max(10, (int)((float)linesPerPage / totalLines * scrollbarHeight));
    int handleY = scrollbarY + (int)((float)currentScrollLine / totalLines * scrollbarHeight);
    handleHeight = std::min(handleHeight, scrollbarHeight);                   // Ensure handle doesn't exceed track
    handleY = std::min(handleY, scrollbarY + scrollbarHeight - handleHeight); // Ensure handle stays within track

    // Draw scrollbar handle
    displayManager.getTFT()->fillRect(scrollbarX + 1, handleY, SCROLLBAR_WIDTH - 2, handleHeight, TFT_LIGHTGREY);
}

void TextViewerPage::handleScroll(int touchY)
{
    // Simple scroll: tapping top half scrolls up, bottom half scrolls down
    int contentTopY = BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y;
    int contentHeight = SCREEN_HEIGHT - contentTopY - TEXT_MARGIN_Y;
    int midPointY = contentTopY + contentHeight / 2;

    int scrollAmount = linesPerPage / 2; // Scroll by half a page
    if (scrollAmount < 1)
        scrollAmount = 1;

    if (touchY < midPointY)
    {
        // Scroll Up
        currentScrollLine -= scrollAmount;
    }
    else
    {
        // Scroll Down
        currentScrollLine += scrollAmount;
    }

    // Clamp scroll position
    currentScrollLine = std::max(0, currentScrollLine);
    currentScrollLine = std::min(currentScrollLine, totalLines - linesPerPage);
    // Ensure scroll position is valid if totalLines < linesPerPage
    if (totalLines <= linesPerPage)
    {
        currentScrollLine = 0;
    }

    // Redraw content and scrollbar
    displayManager.clear(); // Need to clear for redraw
    // Re-draw button and separator
    displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
    displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
    displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);
    // Draw scrolled content
    drawContent();
    drawScrollbar();
}

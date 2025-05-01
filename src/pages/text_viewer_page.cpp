#include <Arduino.h>
#include <FS.h>
#include <algorithm>  // For std::min, std::max
#include <TFT_eSPI.h> // Include for color constants like TFT_LIGHTGREY
#include <cstring>    // For strpbrk
#include <vector>     // Make sure vector is included for lineIndex

#include "text_viewer_page.h"
#include "../core/router.h"
#include "../core/sdcard.h" // Needed for file operations
#include "../core/font.h"   // Ensure Font class methods are available
#include "../config/config.h" // Include for SCREEN_WIDTH

// --- Constants ---
const int TEXT_FONT_SIZE = 1;   // Use font size 1 (e.g., 16x16) - Font size might need adjustment based on font file
const int TEXT_MARGIN_X = 5;    // Left/right margin for text
const int TEXT_MARGIN_Y = 5;    // Top/bottom margin for text
const int SCROLLBAR_WIDTH = 10; // Width of the scrollbar
const int SCROLLBAR_MARGIN = 2; // Margin between text and scrollbar
const int BACK_BUTTON_WIDTH = 60;
const int BACK_BUTTON_HEIGHT = 30;
const int BACK_BUTTON_X = 5;
const int BACK_BUTTON_Y = 5;
const int LINE_COUNT_UPDATE_INTERVAL = 500; // Update display every X lines indexed

// Define content area based on button/header
const int CONTENT_Y = BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y * 2;
const int CONTENT_HEIGHT = SCREEN_HEIGHT - CONTENT_Y - TEXT_MARGIN_Y;
// Calculate available width for text wrapping
const int AVAILABLE_TEXT_WIDTH = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;


// --- Constructor ---
TextViewerPage::TextViewerPage()
    : displayManager(Display::getInstance()),
      fontManager(Font::getInstance()),
      currentScrollLine(0),
      totalLines(0),
      linesPerPage(0),
      lineHeight(0),
      fileLoaded(false),
      fileSize(0),
      originalLineCount(0)
{
}

// --- Destructor ---
// (No changes needed)

// --- Public Methods ---

void TextViewerPage::setFilePath(const String &path)
{
    Serial.print("TextViewerPage::setFilePath received path: ");
    Serial.println(path);
    closeFile();
    filePath = path;
    Serial.print("TextViewerPage::filePath assigned: ");
    Serial.println(filePath);
    fileLoaded = false;
    currentScrollLine = 0;
    lineIndex.clear();
    totalLines = 0;
    originalLineCount = 0;
    fileSize = 0;
}

void TextViewerPage::setParams(void *params)
{
    if (params)
    {
        String *pathPtr = static_cast<String *>(params);
        setFilePath(*pathPtr);
    }
    else
    {
        Serial.println("TextViewerPage::setParams received null params.");
        setFilePath("");
    }
}

void TextViewerPage::display()
{
    if (!fileLoaded)
    {
        calculateLayout(); // Calculate lineHeight first
        loadContent(); // Handles loading UI and drawing final content/error
    }
    else
    {
        // Redraw current view if needed
        displayManager.clear();
        displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
        displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
        displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
        displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);
        drawContent();
        drawScrollbar();
    }
}

void TextViewerPage::handleTouch(uint16_t x, uint16_t y)
{
    if (x >= BACK_BUTTON_X && x < BACK_BUTTON_X + BACK_BUTTON_WIDTH &&
        y >= BACK_BUTTON_Y && y < BACK_BUTTON_Y + BACK_BUTTON_HEIGHT)
    {
        closeFile();
        Router::getInstance().goBack();
        return;
    }

    if (totalLines > linesPerPage)
    {
        if (y > BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y)
        {
            handleScroll(y);
        }
    }
}

// --- Private Helper Methods ---

void TextViewerPage::closeFile()
{
    if (currentFile)
    {
        currentFile.close();
        Serial.println("Closed file.");
    }
}

void TextViewerPage::calculateLayout()
{
    // Use the actual font height provided by the font manager
    // Ensure font is loaded before calling this, or handle fallback
    // fontManager.loadFont(); // Removed incorrect call - Font should be initialized earlier
    lineHeight = fontManager.getCharacterHeight(TEXT_FONT_SIZE * 16); // Assuming size 1 maps to 16px font height
    if (lineHeight == 0)
    {
        lineHeight = 16; // Fallback if font not loaded yet
        Serial.println("Warning: Font height is 0, using fallback 16.");
    }

    int availableHeight = SCREEN_HEIGHT - (BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y * 2);
    linesPerPage = availableHeight / lineHeight;
    if (linesPerPage < 1)
    {
        linesPerPage = 1;
    }
    Serial.printf("Layout calculated: lineHeight=%d, linesPerPage=%d, availableWidth=%d\n", lineHeight, linesPerPage, AVAILABLE_TEXT_WIDTH);

    // Set TFT font size for width calculations in loadContent
    displayManager.getTFT()->setTextSize(TEXT_FONT_SIZE);
    // Set the correct font if your displayManager or fontManager handles it
    // Example: displayManager.getTFT()->setFreeFont(YOUR_FONT);
    // Or rely on fontManager to set it if it controls the TFT font globally
}

// loadContent with text wrapping logic
void TextViewerPage::loadContent()
{
    lineIndex.clear();
    currentScrollLine = 0;
    fileLoaded = false;
    fileSize = 0;
    totalLines = 0;

    const char *pathCStr = filePath.c_str();
    Serial.print("Attempting to load content (with wrapping) for: '");
    Serial.print(pathCStr);
    Serial.println("'");

    // --- 1. Open File, Get Size ---
    currentFile = SDCard::getInstance().openFile(pathCStr);
    if (!currentFile)
    {
        Serial.println("Failed to open file initially.");
        displayManager.clear();
        displayManager.drawCenteredText("Error: Could not open file.", 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, TEXT_FONT_SIZE);
        displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
        displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
        displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
        fileLoaded = true; // Error state
        return;
    }
    fileSize = currentFile.size();
    if (fileSize == 0)
    {
        Serial.println("File is empty.");
        displayManager.clear();
        displayManager.drawCenteredText("Empty File", 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, TEXT_FONT_SIZE);
        displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
        displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
        displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
        fileLoaded = true;
        totalLines = 1;
        lineIndex.push_back(0);
        return;
    }

    // --- 2. Display Loading UI ---
    displayManager.clear();
    displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_DARKGREY);
    displayManager.drawCenteredText("Loading", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
    displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);

    int infoY = CONTENT_Y;
    String displayName = filePath;
    if (displayName.length() > 35) {
        displayName = "..." + displayName.substring(displayName.length() - 32);
    }
    displayManager.drawText(("File: " + displayName).c_str(), TEXT_MARGIN_X, infoY, TEXT_FONT_SIZE);
    infoY += lineHeight;
    displayManager.drawText(("Size: " + String(fileSize) + " bytes").c_str(), TEXT_MARGIN_X, infoY, TEXT_FONT_SIZE);
    infoY += lineHeight;
    int lineCountY = infoY;
    displayManager.drawText("Lines: 0", TEXT_MARGIN_X, lineCountY, TEXT_FONT_SIZE);
    infoY += lineHeight;
    displayManager.drawText("Processing...", TEXT_MARGIN_X, infoY, TEXT_FONT_SIZE);
    Serial.println("Processing file for line index (with wrapping)...");

    // --- 3. Build Line Index with Wrapping ---
    lineIndex.clear();
    lineIndex.push_back(0); // First line starts at byte 0

    currentFile.seek(0); // Ensure we start from the beginning

    // Buffer for reading file chunks
    const size_t bufferSize = 256;
    char fileBuffer[bufferSize];
    size_t bufferOffset = 0;  // Current position within the fileBuffer
    size_t bytesInBuffer = 0; // Number of valid bytes currently in fileBuffer

    size_t currentLineStartPos = 0;       // File position where the current rendered line started
    size_t lastPotentialWrapPos = 0;      // File position *after* the last whitespace seen on the current line segment.
    String currentLineStr = "";           // Accumulate current line segment for width check
    unsigned long lastUpdateTime = millis(); // For periodic UI update

    while (true) { // Loop until EOF is processed
        // Refill buffer if needed
        if (bufferOffset >= bytesInBuffer) {
            bytesInBuffer = currentFile.readBytes(fileBuffer, bufferSize);
            bufferOffset = 0;
            if (bytesInBuffer == 0) {
                break; // End of file
            }
        }

        // Process one character from the buffer
        size_t charByteStartPos = currentFile.position() - bytesInBuffer + bufferOffset; // Calculate original file position
        size_t previousBufferOffset = bufferOffset;
        String currentCharStr = fontManager.getNextCharacter(fileBuffer, bufferOffset);
        size_t charByteLength = bufferOffset - previousBufferOffset;

        if (charByteLength == 0 || currentCharStr.length() == 0) {
             // This might happen with invalid UTF-8 sequences or end of buffer issues
             Serial.printf("Warning: getNextCharacter consumed 0 bytes or returned empty string at file pos %u. Skipping 1 byte.\n", charByteStartPos);
             // Manually advance buffer offset if possible, otherwise break if stuck
             if (bufferOffset < bytesInBuffer) {
                 bufferOffset++;
             } else {
                 // If we consumed 0 bytes at the very end of the buffer, reading again might help,
                 // but if bytesInBuffer is 0, we are truly at EOF or error.
                 if (bytesInBuffer == 0) break; // Break if already at EOF
                 // Otherwise, the outer loop will try to refill the buffer.
                 // We might risk an infinite loop if the file has invalid bytes.
                 // Let's just break to be safe if we can't advance bufferOffset.
                 break;
             }
             continue; // Skip processing this byte/position
        }

        size_t currentPos = charByteStartPos + charByteLength; // Position *after* the character
        const char *currentCharCStr = currentCharStr.c_str();
        char firstByte = currentCharCStr[0];
        bool isNewline = (firstByte == '\n');
        bool isWhitespace = (firstByte == ' ' || firstByte == '\t' || firstByte == '\r'); // Exclude newline

        if (isNewline) {
            // Explicit newline: End the current line here
            if (currentPos > lineIndex.back()) {
                lineIndex.push_back(currentPos);
            }
            currentLineStr = "";
            currentLineStartPos = currentPos;
            lastPotentialWrapPos = currentPos;
        } else {
            // Not a newline, check for wrapping
            String testLineStr = currentLineStr + currentCharStr;
            int testLineWidth = displayManager.getTFT()->textWidth(testLineStr.c_str());

            if (testLineWidth > AVAILABLE_TEXT_WIDTH && currentLineStr.length() > 0) {
                // Wrap needed, and the line isn't empty (prevents wrapping just because first char is too wide)
                size_t wrapPos;
                if (lastPotentialWrapPos > currentLineStartPos) {
                    // Wrap at the position *after* the last whitespace
                    wrapPos = lastPotentialWrapPos;
                } else {
                    // No whitespace found on this line segment, force wrap *before* the current character
                    wrapPos = charByteStartPos;
                    // Edge case: If the first character itself is too wide
                    if (wrapPos == currentLineStartPos) {
                         // Wrap *after* this single character if it's the only one
                         wrapPos = currentPos;
                    }
                }

                // Add the wrap point if it's valid and new
                if (wrapPos > lineIndex.back()) {
                    lineIndex.push_back(wrapPos);
                } else {
                    // This might happen if forced wrap (wrapPos=charByteStartPos) coincides with previous index.
                    // Or if wrapPos == currentLineStartPos and we decided to wrap after (wrapPos=currentPos)
                    // and currentPos coincides with previous index.
                    // Avoid adding duplicate or earlier indices.
                    // If wrapPos needs to be currentPos due to single char overflow, check again.
                    if (wrapPos == charByteStartPos && wrapPos == currentLineStartPos && currentPos > lineIndex.back()) {
                         lineIndex.push_back(currentPos); // Wrap after the single long char
                         wrapPos = currentPos; // Update wrapPos for line start calculation
                    } else {
                         Serial.printf("Skipping redundant wrap point: %u (last index: %u)\n", wrapPos, lineIndex.back());
                    }
                }


                // Start new line segment calculation from wrapPos
                currentLineStartPos = wrapPos;

                // Read the content from wrapPos up to currentPos to form the start of the new line
                // Need to handle potential multi-byte characters correctly if seeking back.
                // Simpler: just use the current character as the start of the new line string.
                // If we wrapped at whitespace, the current char is the start of the next word.
                // If we force-wrapped, the current char is the one that didn't fit.
                currentLineStr = currentCharStr; // Start new line string with the current char
                lastPotentialWrapPos = currentLineStartPos; // Reset wrap pos for the new line

                // If the character that starts the new line *is* whitespace, update potential wrap pos
                if (isWhitespace) {
                     lastPotentialWrapPos = currentPos;
                }

            } else {
                // Character fits, add it to the current line string
                currentLineStr += currentCharStr;
                if (isWhitespace) {
                    lastPotentialWrapPos = currentPos; // Update potential wrap position
                }
            }
        }

        // --- Periodic UI Update ---
        if (millis() - lastUpdateTime > LINE_COUNT_UPDATE_INTERVAL) {
             displayManager.getTFT()->fillRect(TEXT_MARGIN_X + 40, lineCountY, 100, lineHeight, TFT_BLACK); // Clear old count
             displayManager.drawText(String(lineIndex.size()).c_str(), TEXT_MARGIN_X + 40, lineCountY, TEXT_FONT_SIZE);
             lastUpdateTime = millis();
        }

    } // End while loop

    // --- Finalize Loading ---
    totalLines = lineIndex.size();
    Serial.printf("File processing complete. Total rendered lines indexed: %d\n", totalLines);
    // Final update for line count display
    displayManager.getTFT()->fillRect(TEXT_MARGIN_X + 40, lineCountY, 100, lineHeight, TFT_BLACK);
    displayManager.drawText(String(totalLines).c_str(), TEXT_MARGIN_X + 40, lineCountY, TEXT_FONT_SIZE);
    // Clear "Processing..." message
    displayManager.getTFT()->fillRect(TEXT_MARGIN_X, infoY, SCREEN_WIDTH - TEXT_MARGIN_X * 2, lineHeight, TFT_BLACK);

    fileLoaded = true;

    // --- Draw Initial Content View ---
    displayManager.clear(); // Clear loading screen fully
    displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
    displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
    displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);
    drawContent();
    drawScrollbar();
}


// drawContent reads segments based on index and draws them
// (No changes needed in drawContent for wrapping, as lineIndex now contains wrap points)
void TextViewerPage::drawContent()
{
    if (!currentFile)
    { // Check if file is valid
        displayManager.drawCenteredText("Error: File not open", 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, TEXT_FONT_SIZE);
        return;
    }
    if (lineIndex.empty())
    { // Check if index is empty
        displayManager.drawCenteredText("Empty File", 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, TEXT_FONT_SIZE);
        return;
    }

    int y = CONTENT_Y;
    int endLine = std::min(currentScrollLine + linesPerPage, totalLines);

    // Buffer for reading line segments
    const size_t lineBufferSize = 512; // Adjust as needed
    char lineBuffer[lineBufferSize];

    for (int i = currentScrollLine; i < endLine; ++i)
    {
        if (i >= 0 && i < lineIndex.size())
        {
            size_t startOffset = lineIndex[i];
            // Determine endOffset carefully: it's the start of the *next* indexed line, or EOF
            size_t endOffset = (i + 1 < lineIndex.size()) ? lineIndex[i + 1] : fileSize;
            size_t segmentLength = endOffset - startOffset;

            if (segmentLength == 0) // Handle empty lines correctly
            {
                // Draw nothing, just advance y
            }
            else if (segmentLength < lineBufferSize) // Ensure segment fits in buffer
            {
                currentFile.seek(startOffset);
                size_t bytesRead = currentFile.readBytes(lineBuffer, segmentLength);

                if (bytesRead == segmentLength)
                {
                    lineBuffer[bytesRead] = '\0'; // Null-terminate

                    // Remove trailing newline/CR characters before drawing
                    // These might exist if the segment ends exactly on a file's newline
                    size_t actualLength = bytesRead;
                    while (actualLength > 0 && (lineBuffer[actualLength - 1] == '\n' || lineBuffer[actualLength - 1] == '\r')) {
                        lineBuffer[actualLength - 1] = '\0';
                        actualLength--;
                    }

                    // Draw the segment if it's not empty after stripping newlines
                    if (actualLength > 0) {
                         // Trim leading whitespace *only for display* if the line started due to wrapping whitespace
                         char* drawPtr = lineBuffer;
                         if (startOffset > 0) { // Don't trim the very first line
                             bool wrappedAtSpace = false;
                             // Heuristic: If the previous char in the file was whitespace, we likely wrapped there.
                             if (startOffset > 0) {
                                 currentFile.seek(startOffset - 1);
                                 char prevChar = currentFile.read();
                                 if (prevChar == ' ' || prevChar == '\t' || prevChar == '\r') {
                                     wrappedAtSpace = true;
                                 }
                             }
                             // If wrapped at space, skip leading spaces in the buffer for drawing
                             if (wrappedAtSpace) {
                                 while (*drawPtr == ' ' || *drawPtr == '\t') {
                                     drawPtr++;
                                 }
                             }
                         }
                         // Only draw if there's something left after potential trimming
                         if (*drawPtr != '\0') {
                            displayManager.drawText(drawPtr, TEXT_MARGIN_X, y, TEXT_FONT_SIZE);
                         }
                    }
                }
                else
                {
                    Serial.printf("Error reading segment %d: Expected %u, got %u\n", i, segmentLength, bytesRead);
                    // Optionally draw an error indicator for this line
                }
            }
            else
            {
                 Serial.printf("Error: Segment %d too long (%u bytes) for buffer (%u bytes)\n", i, segmentLength, lineBufferSize);
                 // Draw a placeholder or error message for this line
                 displayManager.drawText("...", TEXT_MARGIN_X, y, TEXT_FONT_SIZE);
            }
        }
        y += lineHeight; // Advance Y position for the next line
    }
}


void TextViewerPage::drawScrollbar()
{
    if (totalLines <= linesPerPage)
        return;

    int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    int scrollbarY = CONTENT_Y;
    int scrollbarHeight = CONTENT_HEIGHT;

    displayManager.getTFT()->drawRect(scrollbarX, scrollbarY, SCROLLBAR_WIDTH, scrollbarHeight, TFT_DARKGREY);

    float ratio = (totalLines > 0) ? (float)linesPerPage / totalLines : 1.0f;
    int handleHeight = std::max(10, (int)(ratio * scrollbarHeight));
    handleHeight = std::min(handleHeight, scrollbarHeight);

    float scrollRange = (totalLines > linesPerPage) ? (float)(totalLines - linesPerPage) : 1.0f;
    float scrollRatio = (totalLines > linesPerPage) ? (float)currentScrollLine / scrollRange : 0.0f;

    int handleY = scrollbarY + (int)(scrollRatio * (scrollbarHeight - handleHeight));
    handleY = std::max(scrollbarY, handleY);
    handleY = std::min(handleY, scrollbarY + scrollbarHeight - handleHeight);

// Use TFT_LIGHTGREY definition if available, otherwise a fallback
#ifndef TFT_LIGHTGREY
#define TFT_LIGHTGREY 0xD69A // Define a fallback color if not defined by TFT_eSPI
#endif
    displayManager.getTFT()->fillRect(scrollbarX + 1, handleY, SCROLLBAR_WIDTH - 2, handleHeight, TFT_LIGHTGREY);
}

void TextViewerPage::handleScroll(int touchY)
{
    int contentTopY = CONTENT_Y;
    int contentBottomY = SCREEN_HEIGHT - TEXT_MARGIN_Y;
    int contentClickHeight = contentBottomY - contentTopY;
    int midPointY = contentTopY + contentClickHeight / 2;

    if (contentClickHeight <= 0)
        return;

    int scrollAmount = linesPerPage / 2;
    if (scrollAmount < 1)
    {
        scrollAmount = 1;
    }

    if (touchY < midPointY)
    {
        currentScrollLine -= scrollAmount;
    }
    else
    {
        currentScrollLine += scrollAmount;
    }

    int maxScrollLine = 0;
    if (totalLines > linesPerPage)
    {
        maxScrollLine = totalLines - linesPerPage;
    }
    currentScrollLine = std::max(0, currentScrollLine);
    currentScrollLine = std::min(currentScrollLine, maxScrollLine);

    // Redraw screen after scroll
    displayManager.clear();
    displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
    displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
    displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);
    drawContent();
    drawScrollbar();
}

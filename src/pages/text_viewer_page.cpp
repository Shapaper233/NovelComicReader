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
      touchManager(Touch::getInstance()), // Initialize touchManager reference
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
    errorMessage = "";        // Clear any previous error message
    // lineStartPositions.clear(); // No longer needed
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
    if (!fileLoaded) // fileLoaded now means "metadata calculated"
    {
        calculateLayout(); // Calculate layout based on font size
        calculateFileMetadata(); // Calculate size and total lines
    }

    // If metadata calculation failed, calculateFileMetadata would have set fileLoaded=true
    // and potentially stored an error message in lines[0].
    // So, we clear *after* loadContent potentially displayed progress.
    displayManager.clear();

    // Draw Back Button (similar to FileBrowserPage)
    displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
    displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
    displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false); // Use built-in font for button

    // Draw Content Area Separator
    displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);

    if (fileLoaded) // Check if metadata calculation is done
    {
        // Check if an error occurred during metadata calculation
        if (this->errorMessage.length() > 0) { // Use this->errorMessage
             displayManager.drawCenteredText(this->errorMessage.c_str(), 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, 2);
        } else if (totalLines > 0) { // Check if metadata seems valid (just check lines now)
            // Only draw content and scrollbar if no error and metadata loaded
            drawContent();
            drawScrollbar();
        } else {
            // Handle case where fileLoaded is true but no lines/positions (e.g., empty file)
             displayManager.drawCenteredText("File is empty.", 0, CONTENT_Y, SCREEN_WIDTH, CONTENT_HEIGHT, 2);
        }
    }
    // No explicit else needed here, as loadContent handles the initial "Loading..." or error display.
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
    if (fileLoaded && totalLines > linesPerPage) // Ensure file is loaded before allowing scroll
    {
        // Check if touch is within the main content area (excluding header/button)
        if (y > BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y)
        {
            handleScroll(y);
        }
    }
}

// --- Private Helper Methods ---

// Helper function to format bytes into KB/MB
String formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return String(bytes / 1024.0, 1) + " KB";
    } else {
        return String(bytes / (1024.0 * 1024.0), 1) + " MB";
    }
}

// Helper function to format seconds into MM:SS
String formatETC(unsigned long seconds) {
    if (seconds == 0) return "--:--";
    unsigned long minutes = seconds / 60;
    seconds %= 60;
    char buffer[6]; // MM:SS + null terminator
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu", minutes, seconds);
    return String(buffer);
}


void TextViewerPage::updateLoadingProgress(size_t currentBytes, size_t totalBytes, int currentLineCount, unsigned long elapsedMillis)
{
    // Define progress bar dimensions and position (centered)
    const int barWidth = SCREEN_WIDTH * 0.8;
    const int barHeight = 20;
    const int barX = (SCREEN_WIDTH - barWidth) / 2;
    const int barY = (SCREEN_HEIGHT - barHeight) / 2; // Center vertically

    // Calculate percentage
    int percentage = (totalBytes > 0) ? (int)(((float)currentBytes / totalBytes) * 100) : 0;
    percentage = std::max(0, std::min(100, percentage)); // Clamp between 0 and 100

    // --- Clear relevant areas ---
    // Clear area for top-left info text
    displayManager.getTFT()->fillRect(0, 0, SCREEN_WIDTH, 20, TFT_BLACK); // Clear top area for info text
    // Clear area for progress bar and percentage text below it
    int clearBarAreaY = barY; // Start clearing at the bar's top
    int clearBarAreaHeight = barHeight + 25; // Height to clear (bar + percentage text area below)
    displayManager.getTFT()->fillRect(barX - 5, clearBarAreaY, barWidth + 10, clearBarAreaHeight, TFT_BLACK);

    // --- Calculate ETC ---
    unsigned long etcSeconds = 0;
    if (currentBytes > 0 && elapsedMillis > 100) { // Avoid division by zero and unstable early values
        float bytesPerSecond = (float)currentBytes / (elapsedMillis / 1000.0f);
        if (bytesPerSecond > 0) {
            size_t remainingBytes = totalBytes - currentBytes;
            etcSeconds = (unsigned long)(remainingBytes / bytesPerSecond);
        }
    }
    String etcText = "ETC: " + formatETC(etcSeconds);


    // --- Draw Top-Left Info Text (Size, Lines, ETC) ---
    String sizeText = "Size: " + formatBytes(totalBytes);
    String lineText = "Lines: " + String(currentLineCount);
    // Combine info text, potentially wrapping if too long (simple concatenation for now)
    String infoText = sizeText + "  " + lineText + "  " + etcText;
    // Use displayManager.drawText for custom font rendering at top-left
    displayManager.drawText(infoText.c_str(), 5, 5, TEXT_FONT_SIZE, true); // Use font size 1 at (5, 5)

    // --- Draw Progress Bar ---
    // Draw progress bar outline
    displayManager.getTFT()->drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);

    // Draw filled portion of the progress bar
    int fillWidth = (int)(((float)percentage / 100) * (barWidth - 4)); // Inner width, leave border
    if (fillWidth > 0) {
        displayManager.getTFT()->fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, TFT_BLUE);
    }

    // Draw percentage text further below the bar
    String progressText = String(percentage) + "%";
    int textY = barY + barHeight + 8; // Increased spacing below bar (was 5)
    displayManager.drawCenteredText(progressText.c_str(), 0, textY, SCREEN_WIDTH, 20, 2, false); // Use font size 2, built-in font

    // Optional: Force display update if needed, though TFT_eSPI usually updates automatically
    // displayManager.getTFT()->display(); // Or similar command if available/needed
}

void TextViewerPage::calculateLayout()
{
    // Use font size 1 (16x16) for calculations
    lineHeight = fontManager.getCharacterHeight(TEXT_FONT_SIZE * 16); // Assuming size 1 maps to 16px font
    if (lineHeight <= 0) // Check if lineHeight is valid
        lineHeight = 16; // Fallback if font not loaded yet or invalid size

    int availableHeight = CONTENT_HEIGHT; // Use defined content height
    if (availableHeight <= 0) availableHeight = 1; // Prevent division by zero

    linesPerPage = availableHeight / lineHeight;
    if (linesPerPage < 1)
        linesPerPage = 1; // Ensure at least one line fits
}

// Renamed from loadContent. Calculates metadata (size, total wrapped lines)
// and populates the partial line index. Updates progress display.
void TextViewerPage::calculateFileMetadata()
{
    lineIndex.clear(); // Clear previous index
    currentScrollLine = 0;
    totalLines = 0; // Reset total lines count
    fileLoaded = false; // Set to false initially
    startTimeMillis = millis(); // Record start time for ETC

    const char *pathCStr = filePath.c_str();

    // Don't log path here, might be sensitive or long. Log errors instead.

    // Clear previous error message
    this->errorMessage = ""; // Ensure member variable is cleared

    if (!SDCard::getInstance().exists(pathCStr))
    {
        Serial.print("Error: File not found: "); Serial.println(pathCStr);
        this->errorMessage = "Error: File not found."; // Store in member variable
        fileLoaded = true; // Mark as loaded (with error)
        // lineStartPositions.clear(); // No longer needed
        totalLines = 0; // No lines calculated
        return;
    }

    File file = SDCard::getInstance().openFile(pathCStr);
    if (!file)
    {
        Serial.print("Error: Could not open file: "); Serial.println(pathCStr);
        this->errorMessage = "Error: Could not open file."; // Store in member variable
        fileLoaded = true; // Mark as loaded (with error)
        // lineStartPositions.clear(); // No longer needed
        totalLines = 0;
        return;
    }

    // --- Get total file size for progress ---
    size_t totalSize = file.size();
    // int lastPercentage = -1; // No longer using percentage for update trigger
    unsigned long lastProgressUpdateMillis = 0; // Track time of last progress update

    // --- Local line counter for this pass ---
    int calculatedLines = 0;
    lineIndex[0] = 0; // First line always starts at position 0

    // --- Initial progress display ---
    displayManager.clear(); // Clear screen for loading progress
    updateLoadingProgress(0, totalSize, calculatedLines, 0); // Show 0 lines, 0 elapsed initially
    yield(); // Allow display to update

    Serial.print("Loading file: "); Serial.println(pathCStr); // Log start of loading

    int availableWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    if (availableWidth <= 0)
    {
        Serial.println("Error: Screen too narrow for text.");
        this->errorMessage = "Error: Screen too narrow."; // Store in member variable
        fileLoaded = true; // Mark as loaded (with error)
        // lineStartPositions.clear(); // No longer needed
        totalLines = 0;
        file.close();
        return;
    }

    String currentLine = "";
    String wordBuffer = "";

    // Define the width calculation lambda function *before* the loop
    auto calculateStringWidth = [&](const String& s) -> int {
        int totalWidth = 0;
        size_t offset = 0;
        uint16_t baseSize = TEXT_FONT_SIZE * 16; // Base size (e.g., 16)
        while (offset < s.length()) {
            String nextCharStr = fontManager.getNextCharacter(s.c_str(), offset);
            if (nextCharStr.length() > 0) {
                // Estimate width: ASCII ~ half, Non-ASCII ~ full
                if (fontManager.isAscii(nextCharStr.c_str())) {
                    totalWidth += baseSize / 2; // Estimate ASCII width as half
                } else {
                    totalWidth += baseSize;     // Estimate Non-ASCII width as full
                }
            }
        }
        return totalWidth;
    };

    while (file.available())
    {
        char c = file.read();

        // --- Periodic Progress Update (Time-based) ---
        unsigned long currentMillis = millis();
        if (currentMillis - lastProgressUpdateMillis >= 100) { // Update every 20ms
            updateLoadingProgress(file.position(), totalSize, calculatedLines, currentMillis - startTimeMillis);
            lastProgressUpdateMillis = currentMillis; // Record time of this update
            yield(); // Allow other tasks (like display updates) to run
        }
        // --- End Progress Update ---


        // Handle line breaks first (CRLF, LF, CR)
        if (c == '\n' || c == '\r')
        {
            if (c == '\r' && file.peek() == '\n') {
                file.read(); // Consume the '\n' for CRLF
            }

            // Process the completed line (including the last word buffer content)
            // size_t lineStartPosition = file.position(); // No longer storing positions

            if (wordBuffer.length() > 0) {
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer;
                } else {
                    // Final word doesn't fit, count the line without it
                    if (currentLine.length() > 0) {
                        calculatedLines++;
                        // Position for the *next* line (the wordBuffer) is tricky, estimate based on current char pos?
                        // For simplicity, we'll store the position *after* the newline for now.
                        // The seek in drawContent will need refinement if exact word start is needed.
                    }
                    currentLine = wordBuffer; // Word becomes the new line
                }
                wordBuffer = ""; // Clear buffer after processing
            }
            calculatedLines++; // Count the completed line
            // Store index point if interval is reached
            if (calculatedLines % INDEX_INTERVAL == 0) {
                // Store the position *after* the newline, which is the start of the next line
                lineIndex[calculatedLines] = file.position();
            }
            currentLine = "";             // Reset for the next line from the file
            continue; // Move to next character from file
        }

        // If it's not a newline, process the character for word wrapping

        // Check width *before* adding the character to the word buffer
        // This helps in deciding whether to break the line *before* the current word
        int currentLinePixelWidth = calculateStringWidth(currentLine);
        // Test width with potential new char added to word buffer
        int wordBufferPlusCharWidth = calculateStringWidth(wordBuffer + c);

        // Case 1: Adding the current character to the word buffer makes it exceed the line width
        if (currentLinePixelWidth + wordBufferPlusCharWidth > availableWidth)
        {
            // Subcase 1.1: The current line already has content.
            if (currentLine.length() > 0) {
                // Count the current line (without the word buffer)
                calculatedLines++;
                // Store index point if interval is reached
                if (calculatedLines % INDEX_INTERVAL == 0) {
                    // Position is tricky here. It's the start of the wordBuffer line.
                    // Estimate based on current file position minus the buffer length + char we just read.
                    lineIndex[calculatedLines] = file.position() - (wordBuffer.length() + 1);
                }
                // Start the new line with the word buffer (which doesn't include 'c' yet)
                currentLine = wordBuffer;
                wordBuffer = ""; // Clear word buffer
                // Start new word buffer with the current character 'c' (unless it's leading whitespace)
                if (c != ' ' && c != '\t') {
                     wordBuffer += c;
                }
            }
            // Subcase 1.2: The current line is empty, meaning the word buffer itself (plus 'c') is too long.
            else {
                // We need to break the word buffer itself.
                String fittedPart = "";
                String remainingPart = "";
                int fittedWidth = 0;
                size_t wordOffset = 0;
                uint16_t baseSize = TEXT_FONT_SIZE * 16;

                // Iterate through the *existing* wordBuffer to see what fits
                while(wordOffset < wordBuffer.length()) {
                    String nextCharStr = fontManager.getNextCharacter(wordBuffer.c_str(), wordOffset);
                    if (nextCharStr.length() > 0) {
                        int charWidth = fontManager.isAscii(nextCharStr.c_str()) ? (baseSize / 2) : baseSize;
                        if (fittedWidth + charWidth <= availableWidth) {
                            fittedPart += nextCharStr;
                            fittedWidth += charWidth;
                        } else {
                            // Character doesn't fit, the rest goes to remainingPart
                            // Correctly get the substring from the *start* of the character that didn't fit
                            remainingPart = wordBuffer.substring(wordOffset - nextCharStr.length());
                            break;
                        }
                    } else {
                        break; // End of word buffer string
                    }
                }

                // Add the part that fits (if any)
                if (fittedPart.length() > 0) {
                    calculatedLines++; // Count the fitted part as a line
                    // Store index point if interval is reached
                    if (calculatedLines % INDEX_INTERVAL == 0) {
                        // Position is start of remainingPart line. Estimate.
                         lineIndex[calculatedLines] = file.position() - (remainingPart.length() + 1);
                    }
                }
                // The remaining part becomes the new word buffer, starting with the current char 'c' (unless whitespace)
                wordBuffer = remainingPart;
                if (c != ' ' && c != '\t') {
                     wordBuffer += c;
                }
                currentLine = ""; // Ensure current line is empty
            }
        }
        // Case 2: Adding the character doesn't exceed the width
        else {
            // Add the character to the word buffer
            wordBuffer += c;

            // If the character is a word separator (space/tab), add the word buffer to the current line
            if (c == ' ' || c == '\t') {
                // Check if adding the word buffer (including space) fits
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer; // Add word buffer (including the space/tab)
                    wordBuffer = "";           // Clear word buffer for the next word
                } else {
                    // Word buffer with space doesn't fit. Count current line.
                    if (currentLine.length() > 0) {
                         calculatedLines++;
                         // Store index point if interval is reached
                         if (calculatedLines % INDEX_INTERVAL == 0) {
                             // Position is start of wordBuffer line. Estimate.
                             lineIndex[calculatedLines] = file.position() - (wordBuffer.length() + 1);
                         }
                    }
                    // Start new line with the word buffer (including space)
                    currentLine = wordBuffer;
                    wordBuffer = "";
                }
            }
            // else: it's a normal character within a word, just keep accumulating in wordBuffer
        }
    } // End of while(file.available()) loop

    // --- Final progress update ---
    // Ensure 100% is shown, along with the final line count and final time
    updateLoadingProgress(totalSize, totalSize, calculatedLines, millis() - startTimeMillis);
    // A small delay might be needed here if the screen clears too quickly
    // delay(100); // Optional: delay 100ms to ensure 100% is visible

    // Add any remaining content from the buffers after the loop finishes
    if (wordBuffer.length() > 0) {
        // Check if the final word fits on the last line
        if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
            currentLine += wordBuffer;
        } else {
            // Doesn't fit, count the current line (if any)
            if (currentLine.length() > 0) {
                calculatedLines++;
                 // Store index point if interval is reached
                 if (calculatedLines % INDEX_INTERVAL == 0) {
                     // Position is start of wordBuffer line. Estimate.
                     lineIndex[calculatedLines] = file.position() - wordBuffer.length();
                 }
            }
            currentLine = wordBuffer; // Word buffer becomes the last line
        }
    }
    // Count the very last line if it has content
    if (currentLine.length() > 0) {
        calculatedLines++;
         // Store index point if interval is reached (unlikely for the very last line, but check)
         if (calculatedLines % INDEX_INTERVAL == 0) {
             // Position is tricky, maybe store end of file? Or just rely on previous index.
             // Let's not store an index for the very last partial line.
         }
    }

    file.close();
    totalLines = calculatedLines; // Store the final calculated count
    fileLoaded = true; // Mark metadata as calculated *after* processing
    Serial.printf("File metadata calculated. Total wrapped lines: %d. Index points: %d\n", totalLines, lineIndex.size());
    // errorMessage should be empty if we reached here successfully.
}

// Refactored drawContent: Reads file on demand, wraps, and draws visible lines.
void TextViewerPage::drawContent()
{
    int y = CONTENT_Y; // Starting Y position for drawing text
    int linesDrawn = 0; // Counter for lines drawn on the current screen

    // Clear the content area before drawing new text
    displayManager.getTFT()->fillRect(TEXT_MARGIN_X, y,
                                      SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN,
                                      CONTENT_HEIGHT, // Use calculated content height
                                      TFT_BLACK); // Assuming black background

    // --- File Reading and On-the-Fly Wrapping/Drawing ---
    const char *pathCStr = filePath.c_str();
    File file = SDCard::getInstance().openFile(pathCStr);
    if (!file) {
        // Should not happen if calculateFileMetadata succeeded, but handle defensively
        displayManager.drawText("Error: Cannot reopen file.", TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        return;
    }

    int availableWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    if (availableWidth <= 0) {
        displayManager.drawText("Error: Screen too narrow.", TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        file.close();
        return;
    }

    // --- Check if metadata is loaded ---
    // Also check if an error occurred during loading and if scroll line is valid
    if (!fileLoaded || this->errorMessage.length() > 0 || currentScrollLine < 0 || (totalLines > 0 && currentScrollLine >= totalLines)) {
        String errorToShow = (this->errorMessage.length() > 0) ? this->errorMessage : "Error: Invalid state or scroll position.";
        displayManager.drawText(errorToShow.c_str(), TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        if (file) file.close();
        return;
    }

    // --- Use partial index to find starting point ---
    int seekLine = 0;
    size_t seekPos = 0;
    // Find the largest index key less than or equal to currentScrollLine
    auto it = lineIndex.upper_bound(currentScrollLine); // Find first element *greater* than currentScrollLine
    if (it != lineIndex.begin()) {
        --it; // Go back to the element less than or equal to currentScrollLine
        seekLine = it->first;
        seekPos = it->second;
    }
    // else: currentScrollLine is before the first index point (or index is empty), so start from beginning (seekLine=0, seekPos=0)

    Serial.printf("DrawContent: Target line %d. Seeking to index line %d at pos %u\n", currentScrollLine, seekLine, seekPos);

    if (!file.seek(seekPos)) {
        Serial.printf("Error: Failed to seek to index position %u\n", seekPos);
        displayManager.drawText("Error: Seek failed.", TEXT_MARGIN_X, y, TEXT_FONT_SIZE, true);
        file.close();
        return;
    }

    // --- Read from seeked position, count lines until currentScrollLine ---
    String currentLine = "";
    String wordBuffer = "";
    int linesProcessedSoFar = seekLine; // Start counting from the line we seeked to

    // Re-use the width calculation lambda
    auto calculateStringWidth = [&](const String& s) -> int {
        int totalWidth = 0;
        size_t offset = 0;
        uint16_t baseSize = TEXT_FONT_SIZE * 16;
        while (offset < s.length()) {
            String nextCharStr = fontManager.getNextCharacter(s.c_str(), offset);
            if (nextCharStr.length() > 0) {
                if (fontManager.isAscii(nextCharStr.c_str())) {
                    totalWidth += baseSize / 2;
                } else {
                    totalWidth += baseSize;
                }
            }
        }
        return totalWidth;
    };

    // Lambda to draw a line *if* it's within the visible range
    auto drawVisibleLine = [&](const String& lineToDraw) {
        if (linesProcessedSoFar >= currentScrollLine && linesDrawn < linesPerPage) {
            displayManager.drawText(lineToDraw.c_str(), TEXT_MARGIN_X, y + (linesDrawn * lineHeight), TEXT_FONT_SIZE, true);
            linesDrawn++;
        }
        linesProcessedSoFar++; // Increment *after* checking visibility
    };

    bool touchDetected = false; // Flag for touch interruption

    // Read from the seeked position until enough lines are drawn or EOF
    while (file.available() && linesDrawn < linesPerPage) // Stop drawing if screen is full
    {
        // --- Check for touch interrupt ---
        if (touchManager.isTouched()) { // Use the touchManager member
             Serial.println("Touch detected during TextViewerPage::drawContent, stopping draw.");
             touchDetected = true;
             // We need to handle the touch *after* closing the file and exiting the loop.
             break; // Exit the while loop
        }
        // --- End touch check ---

        // Read character by character from the seeked position
        char c = file.read();

        // Handle line breaks first (CRLF, LF, CR)
        if (c == '\n' || c == '\r') {
            if (c == '\r' && file.peek() == '\n') {
                file.read(); // Consume the '\n' for CRLF
            }

            // Process the completed line (including the last word buffer content)
            if (wordBuffer.length() > 0) {
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer;
                } else {
                    // Word doesn't fit after newline
                    if (currentLine.length() > 0) drawVisibleLine(currentLine); // Draw previous line if not empty
                    currentLine = wordBuffer; // Word becomes the new line
                }
                wordBuffer = "";
            }
            // Draw the completed line (which might be empty if file ended with newline)
            drawVisibleLine(currentLine);
            currentLine = "";
            continue; // Move to next char
        }

        // If it's not a newline, process the character for word wrapping
        int currentLinePixelWidth = calculateStringWidth(currentLine);
        int wordBufferPlusCharWidth = calculateStringWidth(wordBuffer + c);

        // Case 1: Adding the current character makes the word buffer exceed the line width
        if (currentLinePixelWidth + wordBufferPlusCharWidth > availableWidth) {
            // Subcase 1.1: The current line already has content.
            if (currentLine.length() > 0) {
                drawVisibleLine(currentLine); // Draw the current line
                currentLine = wordBuffer;     // Start new line with word buffer
                wordBuffer = "";
                if (c != ' ' && c != '\t') wordBuffer += c; // Start new word buffer unless whitespace
            }
            // Subcase 1.2: The current line is empty (long word needs breaking).
            else {
                String fittedPart = "";
                String remainingPart = "";
                int fittedWidth = 0;
                size_t wordOffset = 0;
                uint16_t baseSize = TEXT_FONT_SIZE * 16;
                while(wordOffset < wordBuffer.length()) {
                     String nextCharStr = fontManager.getNextCharacter(wordBuffer.c_str(), wordOffset);
                     if (nextCharStr.length() > 0) {
                        int charWidth = fontManager.isAscii(nextCharStr.c_str()) ? (baseSize / 2) : baseSize;
                        if (fittedWidth + charWidth <= availableWidth) {
                            fittedPart += nextCharStr;
                            fittedWidth += charWidth;
                        } else {
                            remainingPart = wordBuffer.substring(wordOffset - nextCharStr.length());
                            break;
                        }
                    } else break;
                }
                if (fittedPart.length() > 0) drawVisibleLine(fittedPart); // Draw the fitted part
                wordBuffer = remainingPart;
                if (c != ' ' && c != '\t') wordBuffer += c; // Start new word buffer unless whitespace
                currentLine = "";
            }
        }
        // Case 2: Adding the character doesn't exceed the width
        else {
            wordBuffer += c;
            if (c == ' ' || c == '\t') {
                if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
                    currentLine += wordBuffer;
                    wordBuffer = "";
                } else {
                    // Word buffer with space doesn't fit. Count current line.
                    if (currentLine.length() > 0) drawVisibleLine(currentLine); // Draw current line
                    currentLine = wordBuffer; // Start new line with word buffer (incl. space)
                    wordBuffer = "";
                }
            }
        }
         // Optimization: If we've already processed enough lines to get past the visible area,
         // we could potentially break early, but the file.available() check handles this.
    } // End while

    // Draw any remaining content from the buffers after the loop finishes
    if (wordBuffer.length() > 0) {
        if (calculateStringWidth(currentLine) + calculateStringWidth(wordBuffer) <= availableWidth) {
            currentLine += wordBuffer;
        } else {
            // Doesn't fit, count the current line (if any) and add the word buffer as a new line
            if (currentLine.length() > 0) drawVisibleLine(currentLine);
            currentLine = wordBuffer; // Word buffer becomes the last line
        }
    }
    // Draw the very last line if it has content and we haven't filled the page yet
    if (currentLine.length() > 0) {
        drawVisibleLine(currentLine); // Draw the very last line if needed
    }

    file.close(); // Close the file regardless of how the loop exited

    // --- Handle touch if detected ---
    if (touchDetected) {
        uint16_t touchX, touchY;
        // Attempt to get the point, might have changed but worth trying
        if (touchManager.getPoint(touchX, touchY)) { // Use the touchManager member
             handleTouch(touchX, touchY); // Process the touch event
        }
        // Do not proceed to draw scrollbar if interrupted
        return;
    }
    // --- End touch handling ---

    // If not interrupted, proceed to draw the scrollbar (if needed)
    // Note: drawScrollbar is now called from display(), so this call might be redundant
    // Let's remove it from here and rely on display() calling it after drawContent finishes.
    // drawScrollbar(); // Removed from here
}


void TextViewerPage::drawScrollbar()
{
    if (totalLines <= linesPerPage)
        return; // No scrollbar needed

    int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    int scrollbarY = CONTENT_Y; // Align with text content area top
    int scrollbarHeight = CONTENT_HEIGHT; // Align with text content area height

    // Draw scrollbar track (clear previous first)
    displayManager.getTFT()->fillRect(scrollbarX, scrollbarY, SCROLLBAR_WIDTH, scrollbarHeight, TFT_BLACK); // Clear area
    displayManager.getTFT()->drawRect(scrollbarX, scrollbarY, SCROLLBAR_WIDTH, scrollbarHeight, TFT_DARKGREY);

    // Calculate handle size and position
    // Ensure totalLines is not zero to avoid division by zero
    float linesRatio = (totalLines > 0) ? (float)linesPerPage / totalLines : 1.0f;
    int handleHeight = std::max(10, (int)(linesRatio * scrollbarHeight));
    handleHeight = std::min(handleHeight, scrollbarHeight); // Ensure handle doesn't exceed track

    // Ensure totalLines > linesPerPage before calculating scroll position ratio
    // Correct calculation for handle position based on scroll range
    float scrollRatio = (totalLines > linesPerPage) ? (float)currentScrollLine / (totalLines - linesPerPage) : 0.0f;
    int handleY = scrollbarY + (int)(scrollRatio * (scrollbarHeight - handleHeight)); // Position relative to available space

    // Clamp handle Y position just in case
    handleY = std::max(scrollbarY, handleY);
    handleY = std::min(handleY, scrollbarY + scrollbarHeight - handleHeight);


    // Draw scrollbar handle
    displayManager.getTFT()->fillRect(scrollbarX + 1, handleY, SCROLLBAR_WIDTH - 2, handleHeight, TFT_LIGHTGREY);
}

void TextViewerPage::handleScroll(int touchY)
{
    // Simple scroll: tapping top half scrolls up, bottom half scrolls down
    int contentTopY = CONTENT_Y; // Use constant
    int contentHeight = CONTENT_HEIGHT; // Use constant
    int midPointY = contentTopY + contentHeight / 2;

    int scrollAmount = linesPerPage / 2; // Scroll by half a page
    if (scrollAmount < 1)
        scrollAmount = 1;

    int previousScrollLine = currentScrollLine; // Store previous position

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
    // Adjust clamping: max scroll should be totalLines - linesPerPage
    if (totalLines > linesPerPage) {
         currentScrollLine = std::min(currentScrollLine, totalLines - linesPerPage);
    } else {
         currentScrollLine = 0; // Cannot scroll if content fits
    }


    // Only redraw if scroll position actually changed
    if (currentScrollLine != previousScrollLine) {
        // --- Clear only the content area, not the whole screen ---
        int contentClearX = TEXT_MARGIN_X;
        int contentClearY = CONTENT_Y;
        int contentClearWidth = SCREEN_WIDTH - TEXT_MARGIN_X * 2 - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
        int contentClearHeight = CONTENT_HEIGHT;
        displayManager.getTFT()->fillRect(contentClearX, contentClearY, contentClearWidth, contentClearHeight, TFT_BLACK);

        // Clear the scrollbar area separately
        int scrollbarX = SCREEN_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
        displayManager.getTFT()->fillRect(scrollbarX, contentClearY, SCROLLBAR_WIDTH, contentClearHeight, TFT_BLACK); // Clear scrollbar track area

        // --- No need to redraw static elements ---
        // displayManager.getTFT()->fillRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_BLUE);
        // displayManager.getTFT()->drawRoundRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 5, TFT_WHITE);
        // displayManager.drawCenteredText("Back", BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_WIDTH, BACK_BUTTON_HEIGHT, 1, false);
        // displayManager.getTFT()->drawFastHLine(0, BACK_BUTTON_Y + BACK_BUTTON_HEIGHT + TEXT_MARGIN_Y - 1, SCREEN_WIDTH, TFT_DARKGREY);

        // Now draw the updated content and scrollbar directly into the cleared areas
        drawContent();   // This function clears its own area, but clearing above ensures no artifacts
        drawScrollbar(); // This function clears and redraws its area
    }
}

/**
 * @brief Cleans up resources used by the TextViewerPage.
 * Called when navigating away from the page to free memory and reset state.
 */
void TextViewerPage::cleanup() {
    Serial.println("TextViewerPage::cleanup() called.");
    lineIndex.clear(); // Clear the partial index map
    // Reset state variables
    filePath = "";
    currentScrollLine = 0;
    totalLines = 0;
    fileLoaded = false;
    errorMessage = "";
    // Note: std::map doesn't have shrink_to_fit like std::vector.
    // Clearing it is usually sufficient to release nodes.
    Serial.println("TextViewerPage resources cleaned up.");
}

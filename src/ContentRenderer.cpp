#include "ContentRenderer.h"

void ContentRenderer::renderPage(
    const std::vector<String>& paragraphs,
    const std::vector<int>& pageBreaks,
    int currentPage,
    int totalPages,
    TextLayout& textLayout,
    PopupMenu& popupMenu
) {
    int availableHeight = SCREEN_HEIGHT - _config.tabBarHeight;
    int contentY = _config.headerHeight + _config.padY;
    int contentH = availableHeight - _config.headerHeight - _config.navHeight;

    // Clear content area
    M5.Display.fillRect(0, _config.headerHeight, SCREEN_WIDTH, contentH, TFT_WHITE);

    // Clear text layout for new page
    textLayout.clear();

    if (currentPage >= totalPages || pageBreaks.empty()) {
        return;
    }

    FontManager& fm = FontManager::instance();
    bool useCustomFont = fm.hasCustomFont();

    int fontH;
    int contentW = SCREEN_WIDTH - _config.padX * 2;

    if (useCustomFont) {
        fm.setFontSize(_config.fontSizePt);
        fontH = _config.fontSizePt + 4;
    } else {
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextSize(1.0);
        fontH = M5.Display.fontHeight();
    }
    M5.Display.setTextColor(TFT_BLACK);

    int maxY = availableHeight - _config.navHeight - _config.padY;

    int startPara = pageBreaks[currentPage];
    int endPara = (currentPage + 1 < totalPages) ? pageBreaks[currentPage + 1] : paragraphs.size();

    int y = contentY;

    for (int i = startPara; i < endPara && y < maxY; i++) {
        const String& para = paragraphs[i];

        int bytePos = 0;
        int byteLen = para.length();

        while (bytePos < byteLen && y < maxY) {
            int lineEnd = bytePos;

            // Find line break point
            while (lineEnd < byteLen) {
                int charLen = getCharLen(para[lineEnd]);
                String sub = para.substring(bytePos, lineEnd + charLen);
                int textW = getTextWidth(sub, useCustomFont, fm);
                if (textW > contentW) break;
                lineEnd += charLen;
            }

            if (lineEnd == bytePos) lineEnd += 1;

            String lineText = para.substring(bytePos, lineEnd);
            int lineWidth = getTextWidth(lineText, useCustomFont, fm);

            // Record line position for text selection
            textLayout.addLine(i, bytePos, lineEnd, _config.padX, y,
                              lineWidth, fontH + _config.lineSpacing, lineText);

            // Draw text
            drawText(lineText, _config.padX, y, useCustomFont, fm);

            bytePos = lineEnd;
            y += fontH + _config.lineSpacing;
        }

        y += _config.paragraphSpacing - _config.lineSpacing;
    }

    // Draw selection highlight
    if (textLayout.hasSelection()) {
        textLayout.drawHighlight();
    }

    // Draw popup menu if visible
    if (popupMenu.isVisible()) {
        popupMenu.draw();
    }
}

int ContentRenderer::calculatePages(
    const std::vector<String>& paragraphs,
    std::vector<int>& pageBreaks
) {
    pageBreaks.clear();
    if (paragraphs.empty()) return 0;

    FontManager& fm = FontManager::instance();
    bool useCustomFont = fm.hasCustomFont();

    int fontH;
    int contentW = SCREEN_WIDTH - _config.padX * 2;

    if (useCustomFont) {
        fontH = _config.fontSizePt + 4;
    } else {
        M5.Display.setFont(&fonts::efontJA_24);
        fontH = M5.Display.fontHeight();
    }

    int availableHeight = SCREEN_HEIGHT - _config.tabBarHeight;
    int contentH = availableHeight - _config.headerHeight - _config.navHeight - _config.padY * 2;

    pageBreaks.push_back(0);
    int y = 0;

    for (int i = 0; i < (int)paragraphs.size(); i++) {
        const String& para = paragraphs[i];
        int bytePos = 0;
        int byteLen = para.length();

        while (bytePos < byteLen) {
            int lineEnd = bytePos;

            while (lineEnd < byteLen) {
                int charLen = getCharLen(para[lineEnd]);
                String sub = para.substring(bytePos, lineEnd + charLen);
                int textW = getTextWidth(sub, useCustomFont, fm);
                if (textW > contentW) break;
                lineEnd += charLen;
            }

            if (lineEnd == bytePos) lineEnd += 1;

            y += fontH + _config.lineSpacing;
            bytePos = lineEnd;

            if (y >= contentH) {
                pageBreaks.push_back(i);
                y = 0;
            }
        }

        y += _config.paragraphSpacing - _config.lineSpacing;

        if (y >= contentH && i < (int)paragraphs.size() - 1) {
            pageBreaks.push_back(i + 1);
            y = 0;
        }
    }

    return pageBreaks.size();
}

#pragma once

#include <Arduino.h>
#include <vector>
#include <SD.h>

// Chapter info structure
struct EpubChapter {
    String id;
    String href;      // Path within epub (e.g., "OEBPS/chapter1.xhtml")
    String title;     // Chapter title (from TOC or heading)
};

// Book metadata
struct EpubMetadata {
    String title;
    String author;
    String language;
    String coverPath;  // Path to cover image within epub
};

class EpubParser {
public:
    EpubParser();
    ~EpubParser();

    // Open and parse epub file
    bool open(const String& path);

    // Close and cleanup
    void close();

    // Check if epub is open
    bool isOpen() const { return _isOpen; }

    // Get metadata
    const EpubMetadata& getMetadata() const { return _metadata; }

    // Get chapter list
    const std::vector<EpubChapter>& getChapters() const { return _chapters; }

    // Get chapter count
    int getChapterCount() const { return _chapters.size(); }

    // Get chapter content as plain text (strips HTML)
    String getChapterText(int chapterIndex);

    // Get raw chapter HTML
    String getChapterHtml(int chapterIndex);

private:
    bool _isOpen;
    String _epubPath;
    EpubMetadata _metadata;
    std::vector<EpubChapter> _chapters;

    // Internal paths
    String _rootPath;      // Root folder path (usually OEBPS or OPS)
    String _opfPath;       // Path to content.opf

    // Helper methods
    bool findContainerXml();
    bool parseContentOpf();
    String extractFile(const String& innerPath);
    String stripHtml(const String& html);
    String decodeHtmlEntities(const String& text);
};

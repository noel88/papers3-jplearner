#include "EpubParser.h"
#include <unzipLIB.h>
#include <tinyxml2.h>

using namespace tinyxml2;

// Global unzip instance and file handle
static UNZIP _zip;
static File _epubFileHandle;

// Callback functions for unzipLIB
static void* myOpen(const char* filename, int32_t* size) {
    _epubFileHandle = SD.open(filename, FILE_READ);
    if (!_epubFileHandle) return nullptr;
    *size = _epubFileHandle.size();
    return (void*)&_epubFileHandle;
}

static void myClose(void* handle) {
    if (_epubFileHandle) {
        _epubFileHandle.close();
    }
}

static int32_t myRead(void* handle, uint8_t* buffer, int32_t length) {
    return _epubFileHandle.read(buffer, length);
}

static int32_t mySeek(void* handle, int32_t position, int type) {
    if (type == SEEK_SET) {
        return _epubFileHandle.seek(position) ? 0 : -1;
    } else if (type == SEEK_CUR) {
        return _epubFileHandle.seek(_epubFileHandle.position() + position) ? 0 : -1;
    } else if (type == SEEK_END) {
        return _epubFileHandle.seek(_epubFileHandle.size() + position) ? 0 : -1;
    }
    return -1;
}

EpubParser::EpubParser()
    : _isOpen(false) {
}

EpubParser::~EpubParser() {
    close();
}

void EpubParser::close() {
    _isOpen = false;
    _epubPath = "";
    _rootPath = "";
    _opfPath = "";
    _metadata = EpubMetadata();
    _chapters.clear();
}

bool EpubParser::open(const String& path) {
    close();

    if (!SD.exists(path.c_str())) {
        Serial.printf("EpubParser: File not found: %s\n", path.c_str());
        return false;
    }

    _epubPath = path;
    Serial.printf("EpubParser: Opening %s\n", path.c_str());

    // Step 1: Find and parse META-INF/container.xml
    if (!findContainerXml()) {
        Serial.println("EpubParser: Failed to find container.xml");
        return false;
    }

    // Step 2: Parse content.opf
    if (!parseContentOpf()) {
        Serial.println("EpubParser: Failed to parse content.opf");
        return false;
    }

    _isOpen = true;
    Serial.printf("EpubParser: Opened successfully. Title: %s, Chapters: %d\n",
                  _metadata.title.c_str(), _chapters.size());

    return true;
}

String EpubParser::extractFile(const String& innerPath) {
    int rc = _zip.openZIP(_epubPath.c_str(), myOpen, myClose, myRead, mySeek);
    if (rc != UNZ_OK) {
        Serial.printf("EpubParser: Failed to open zip: %d\n", rc);
        return "";
    }

    rc = _zip.locateFile(innerPath.c_str());
    if (rc != UNZ_OK) {
        Serial.printf("EpubParser: File not found in zip: %s\n", innerPath.c_str());
        _zip.closeZIP();
        return "";
    }

    // Get file info to know uncompressed size
    unz_file_info fileInfo;
    char filename[256];
    rc = _zip.getFileInfo(&fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0);
    if (rc != UNZ_OK) {
        Serial.println("EpubParser: Failed to get file info");
        _zip.closeZIP();
        return "";
    }

    uint32_t uncompSize = fileInfo.uncompressed_size;

    rc = _zip.openCurrentFile();
    if (rc != UNZ_OK) {
        Serial.println("EpubParser: Failed to open current file");
        _zip.closeZIP();
        return "";
    }

    // Allocate buffer in PSRAM
    uint8_t* buffer = (uint8_t*)ps_malloc(uncompSize + 1);
    if (!buffer) {
        Serial.println("EpubParser: Failed to allocate buffer");
        _zip.closeCurrentFile();
        _zip.closeZIP();
        return "";
    }

    // Read data
    int bytesRead = _zip.readCurrentFile(buffer, uncompSize);
    _zip.closeCurrentFile();
    _zip.closeZIP();

    if (bytesRead <= 0) {
        Serial.println("EpubParser: Failed to read file data");
        free(buffer);
        return "";
    }

    buffer[bytesRead] = '\0';
    String result = String((char*)buffer);
    free(buffer);

    return result;
}

bool EpubParser::findContainerXml() {
    String containerXml = extractFile("META-INF/container.xml");
    if (containerXml.length() == 0) {
        return false;
    }

    XMLDocument doc;
    if (doc.Parse(containerXml.c_str()) != XML_SUCCESS) {
        Serial.println("EpubParser: Failed to parse container.xml");
        return false;
    }

    // Find rootfile element
    XMLElement* container = doc.FirstChildElement("container");
    if (!container) return false;

    XMLElement* rootfiles = container->FirstChildElement("rootfiles");
    if (!rootfiles) return false;

    XMLElement* rootfile = rootfiles->FirstChildElement("rootfile");
    if (!rootfile) return false;

    const char* fullPath = rootfile->Attribute("full-path");
    if (!fullPath) return false;

    _opfPath = String(fullPath);

    // Extract root path (directory containing content.opf)
    int lastSlash = _opfPath.lastIndexOf('/');
    if (lastSlash > 0) {
        _rootPath = _opfPath.substring(0, lastSlash + 1);
    } else {
        _rootPath = "";
    }

    Serial.printf("EpubParser: OPF path: %s, Root: %s\n", _opfPath.c_str(), _rootPath.c_str());
    return true;
}

bool EpubParser::parseContentOpf() {
    String opfContent = extractFile(_opfPath);
    if (opfContent.length() == 0) {
        return false;
    }

    XMLDocument doc;
    if (doc.Parse(opfContent.c_str()) != XML_SUCCESS) {
        Serial.println("EpubParser: Failed to parse content.opf");
        return false;
    }

    XMLElement* package = doc.FirstChildElement("package");
    if (!package) {
        // Try with namespace prefix
        package = doc.FirstChildElement("opf:package");
    }
    if (!package) return false;

    // Parse metadata
    XMLElement* metadata = package->FirstChildElement("metadata");
    if (!metadata) {
        metadata = package->FirstChildElement("opf:metadata");
    }

    if (metadata) {
        // Title
        XMLElement* titleEl = metadata->FirstChildElement("dc:title");
        if (titleEl && titleEl->GetText()) {
            _metadata.title = String(titleEl->GetText());
        }

        // Author
        XMLElement* creatorEl = metadata->FirstChildElement("dc:creator");
        if (creatorEl && creatorEl->GetText()) {
            _metadata.author = String(creatorEl->GetText());
        }

        // Language
        XMLElement* langEl = metadata->FirstChildElement("dc:language");
        if (langEl && langEl->GetText()) {
            _metadata.language = String(langEl->GetText());
        }
    }

    // Build manifest map (id -> href)
    std::vector<std::pair<String, String>> manifestItems;

    XMLElement* manifest = package->FirstChildElement("manifest");
    if (manifest) {
        for (XMLElement* item = manifest->FirstChildElement("item");
             item != nullptr;
             item = item->NextSiblingElement("item")) {

            const char* id = item->Attribute("id");
            const char* href = item->Attribute("href");
            const char* mediaType = item->Attribute("media-type");

            if (id && href) {
                manifestItems.push_back({String(id), String(href)});

                // Check for cover image
                if (mediaType && strstr(mediaType, "image") != nullptr) {
                    const char* properties = item->Attribute("properties");
                    if (properties && strstr(properties, "cover-image") != nullptr) {
                        _metadata.coverPath = _rootPath + String(href);
                    }
                }
            }
        }
    }

    // Parse spine (reading order)
    XMLElement* spine = package->FirstChildElement("spine");
    if (spine) {
        for (XMLElement* itemref = spine->FirstChildElement("itemref");
             itemref != nullptr;
             itemref = itemref->NextSiblingElement("itemref")) {

            const char* idref = itemref->Attribute("idref");
            if (!idref) continue;

            // Find href in manifest
            String href = "";
            for (const auto& item : manifestItems) {
                if (item.first == String(idref)) {
                    href = item.second;
                    break;
                }
            }

            if (href.length() > 0) {
                EpubChapter chapter;
                chapter.id = String(idref);
                chapter.href = _rootPath + href;
                chapter.title = "Chapter " + String(_chapters.size() + 1);
                _chapters.push_back(chapter);
            }
        }
    }

    Serial.printf("EpubParser: Parsed %d chapters\n", _chapters.size());
    return _chapters.size() > 0;
}

String EpubParser::getChapterHtml(int chapterIndex) {
    if (chapterIndex < 0 || chapterIndex >= (int)_chapters.size()) {
        return "";
    }

    return extractFile(_chapters[chapterIndex].href);
}

String EpubParser::getChapterText(int chapterIndex) {
    String html = getChapterHtml(chapterIndex);
    if (html.length() == 0) {
        return "";
    }

    return stripHtml(html);
}

String EpubParser::stripHtml(const String& html) {
    String result;
    result.reserve(html.length());

    bool inTag = false;
    bool inScript = false;
    bool inStyle = false;
    bool lastWasSpace = false;

    for (size_t i = 0; i < html.length(); i++) {
        char c = html[i];

        if (c == '<') {
            inTag = true;

            // Check for script/style tags
            if (html.substring(i, i + 7).equalsIgnoreCase("<script")) {
                inScript = true;
            } else if (html.substring(i, i + 6).equalsIgnoreCase("<style")) {
                inStyle = true;
            } else if (html.substring(i, i + 9).equalsIgnoreCase("</script>")) {
                inScript = false;
            } else if (html.substring(i, i + 8).equalsIgnoreCase("</style>")) {
                inStyle = false;
            }

            // Add newline for block elements
            String tagCheck = html.substring(i, i + 4);
            tagCheck.toLowerCase();
            if (tagCheck.startsWith("<p>") || tagCheck.startsWith("<p ") ||
                tagCheck.startsWith("<br") || tagCheck.startsWith("<di") ||
                tagCheck.startsWith("<h1") || tagCheck.startsWith("<h2") ||
                tagCheck.startsWith("<h3") || tagCheck.startsWith("<li")) {
                if (result.length() > 0 && result[result.length() - 1] != '\n') {
                    result += '\n';
                    lastWasSpace = true;
                }
            }
            continue;
        }

        if (c == '>') {
            inTag = false;
            continue;
        }

        if (inTag || inScript || inStyle) {
            continue;
        }

        // Handle whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!lastWasSpace && result.length() > 0) {
                result += ' ';
                lastWasSpace = true;
            }
            continue;
        }

        // Handle HTML entities
        if (c == '&') {
            int semiPos = html.indexOf(';', i);
            if (semiPos > 0 && semiPos - (int)i < 10) {
                String entity = html.substring(i, semiPos + 1);
                String decoded = decodeHtmlEntities(entity);
                result += decoded;
                i = semiPos;
                lastWasSpace = false;
                continue;
            }
        }

        result += c;
        lastWasSpace = false;
    }

    // Trim
    result.trim();
    return result;
}

String EpubParser::decodeHtmlEntities(const String& text) {
    if (text == "&nbsp;") return " ";
    if (text == "&lt;") return "<";
    if (text == "&gt;") return ">";
    if (text == "&amp;") return "&";
    if (text == "&quot;") return "\"";
    if (text == "&apos;") return "'";
    if (text == "&#x3000;") return "\u3000";  // Full-width space
    if (text == "&#8230;") return "...";
    if (text == "&#8212;") return "—";
    if (text == "&#8211;") return "–";

    // Numeric entity
    if (text.startsWith("&#")) {
        int val = 0;
        if (text[2] == 'x' || text[2] == 'X') {
            // Hex
            val = strtol(text.substring(3).c_str(), nullptr, 16);
        } else {
            // Decimal
            val = text.substring(2).toInt();
        }
        if (val > 0 && val < 0x10000) {
            char buf[5];
            if (val < 0x80) {
                buf[0] = val;
                buf[1] = 0;
            } else if (val < 0x800) {
                buf[0] = 0xC0 | (val >> 6);
                buf[1] = 0x80 | (val & 0x3F);
                buf[2] = 0;
            } else {
                buf[0] = 0xE0 | (val >> 12);
                buf[1] = 0x80 | ((val >> 6) & 0x3F);
                buf[2] = 0x80 | (val & 0x3F);
                buf[3] = 0;
            }
            return String(buf);
        }
    }

    return text;  // Return as-is if unknown
}

#include "PsramUtils.hpp"

// Implementierungen der in PsramUtils.hpp deklarierten Funktionen

size_t psramFree() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

char* psram_strdup(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str) + 1;
    char* new_str = (char*)ps_malloc(len);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}

PsramString readFromStream(Stream& stream) {
    size_t size = stream.available();
    if (size == 0) return "";

    char* buf = (char*)ps_malloc(size + 1);
    if (!buf) {
        Serial.println("[PsramUtils] FEHLER: Konnte Puffer für Stream-Lesen nicht allozieren.");
        return "";
    }

    stream.readBytes(buf, size);
    buf[size] = '\0';
    
    PsramString result(buf);
    free(buf);
    
    return result;
}

int indexOf(const PsramString& str, const char* substring, size_t fromIndex) {
    if (fromIndex >= str.length()) return -1;
    const char* found = strstr(str.c_str() + fromIndex, substring);
    if (found == nullptr) return -1;
    return found - str.c_str();
}

int indexOf(const PsramString& str, const String& substring, size_t fromIndex) {
    return indexOf(str, substring.c_str(), fromIndex);
}

void replaceAll(PsramString& str, const PsramString& from, const PsramString& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != PsramString::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

PsramString escapeJsonString(const PsramString& input) {
    PsramString output;
    output.reserve(input.length());
    for (char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b";  break;
            case '\f': output += "\\f";  break;
            case '\n': output += "\\n";  break;
            case '\r': output += "\\r";  break;
            case '\t': output += "\\t";  break;
            default:
                if (c >= 0 && c < 32) {
                    // Andere Steuerzeichen ignorieren
                } else {
                    output += c;
                }
                break;
        }
    }
    return output;
}


#if __has_include(<WString.h>)
PsramString& operator+=(PsramString& s, const __FlashStringHelper* f) {
  s += String(f).c_str();
  return s;
}

PsramString operator+(const PsramString& s, const __FlashStringHelper* f) {
    return s + String(f).c_str();
}

PsramString operator+(const __FlashStringHelper* f, const PsramString& s) {
    return String(f).c_str() + s;
}
#endif
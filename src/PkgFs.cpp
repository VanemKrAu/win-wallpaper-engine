#include "PkgFs.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <algorithm>

PkgFs::PkgFs() : m_handle(nullptr) {}
PkgFs::~PkgFs() { Close(); }

// Read a sized string from file: int32 length prefix then raw bytes
static bool ReadSizedString(HANDLE h, std::string& out) {
    int32_t len;
    DWORD read = 0;
    if (!::ReadFile(h, &len, 4, &read, nullptr) || read < 4) return false;
    if (len < 0 || len > 4096) return false;
    out.resize(len);
    if (len > 0) {
        if (!::ReadFile(h, &out[0], len, &read, nullptr) || (int32_t)read < len) return false;
    }
    return true;
}

// Path normalization: lowercase + always start with /
static std::string NormalizePath(const std::string& raw) {
    std::string s;
    s.reserve(raw.size() + 1);
    s += '/';
    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];
        // convert backslash to forward slash
        if (c == '\\') c = '/';
        // lowercase
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        // strip drive letter (e.g. "C:" -> "")
        if (c == ':' && i + 1 < raw.size() && raw[i+1] == '/') {
            s.clear();
            s += '/';
            continue;
        }
        s += c;
    }
    return s;
}

bool PkgFs::Open(const wchar_t* path) {
    Close();

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    // Read version stamp: sized string
    if (!ReadSizedString(h, m_version)) { CloseHandle(h); return false; }

    // Validate: must start with "PKGV"
    if (m_version.size() < 5 || m_version.substr(0, 4) != "PKGV") {
        CloseHandle(h);
        return false;
    }

    // Read directory entry count
    int32_t entryCount;
    DWORD read = 0;
    if (!::ReadFile(h, &entryCount, 4, &read, nullptr) || read < 4 || entryCount < 0) {
        CloseHandle(h);
        return false;
    }

    // Read directory entries
    for (int32_t i = 0; i < entryCount; i++) {
        std::string rawPath;
        if (!ReadSizedString(h, rawPath)) { CloseHandle(h); return false; }

        PkgEntry entry;
        entry.name = NormalizePath(rawPath);

        int32_t relOffset;
        if (!::ReadFile(h, &relOffset, 4, &read, nullptr) || read < 4) { CloseHandle(h); return false; }
        entry.offset = relOffset;

        int32_t fileLen;
        if (!::ReadFile(h, &fileLen, 4, &read, nullptr) || read < 4) { CloseHandle(h); return false; }
        entry.length = fileLen;

        m_entries.push_back(entry);
    }

    // Adjust offsets: all offsets are relative to the end of directory table
    LARGE_INTEGER li;
    li.QuadPart = 0;
    SetFilePointerEx(h, li, &li, FILE_CURRENT);
    uint64_t headerSize = li.QuadPart;
    for (auto& e : m_entries) {
        e.offset += headerSize;
    }

    m_handle = h;
    return true;
}

void PkgFs::Close() {
    if (m_handle) {
        CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
    m_entries.clear();
    m_version.clear();
}

const PkgEntry* PkgFs::Find(const std::string& pkgPath) const {
    std::string key = NormalizePath(pkgPath);
    for (size_t i = 0; i < m_entries.size(); i++) {
        if (m_entries[i].name == key) return &m_entries[i];
    }
    return nullptr;
}

std::vector<uint8_t> PkgFs::ReadEntry(const PkgEntry& entry) const {
    if (!m_handle || entry.length == 0) return {};

    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(entry.offset);
    if (!SetFilePointerEx(static_cast<HANDLE>(m_handle), li, nullptr, FILE_BEGIN))
        return {};

    std::vector<uint8_t> buf(static_cast<size_t>(entry.length));
    DWORD read = 0;
    if (!::ReadFile(static_cast<HANDLE>(m_handle), buf.data(),
                  static_cast<DWORD>(entry.length), &read, nullptr))
        return {};

    if (read < entry.length) buf.resize(read);
    return buf;
}

std::vector<uint8_t> PkgFs::ReadFile(const std::string& pkgPath) const {
    const PkgEntry* entry = Find(pkgPath);
    if (!entry) return {};
    return ReadEntry(*entry);
}

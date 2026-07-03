#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct PkgEntry {
    std::string name;
    uint64_t    offset;
    uint64_t    length;
};

class PkgFs {
public:
    PkgFs();
    ~PkgFs();
    bool Open(const wchar_t* path);
    void Close();
    bool IsOpen() const { return m_handle != nullptr; }
    const std::vector<PkgEntry>& Entries() const { return m_entries; }
    const PkgEntry* Find(const std::string& name) const;
    std::vector<uint8_t> ReadEntry(const PkgEntry& entry) const;
    std::vector<uint8_t> ReadFile(const std::string& pkgPath) const;
    const std::string& Version() const { return m_version; }

private:
    void*   m_handle;
    std::vector<PkgEntry> m_entries;
    std::string m_version;
};

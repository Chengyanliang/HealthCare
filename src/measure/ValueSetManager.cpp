// ValueSetManager — Value set loading and membership lookup

#include "measure/ValueSetManager.h"
#include <fstream>
#include <sstream>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

namespace hedis {

const std::vector<ValueSetEntry> ValueSetManager::EMPTY_SET;

ValueSetManager::ValueSetManager()  = default;
ValueSetManager::~ValueSetManager() = default;

// ---------------------------------------------------------------------------
// loadFromDatabase — load from SMA_CQL_VALUESETS table
// ---------------------------------------------------------------------------
void ValueSetManager::loadFromDatabase(OCI_Connection* conn, const std::string& version) {
    if (!conn) return;

#ifdef HAS_OCILIB
    OCI_Statement* stmt = OCI_StatementCreate(conn);
    OCI_ExecuteStmtFmt(stmt,
        "SELECT VALUESET_NAME, CODE, CODE_SYSTEM, DISPLAY "
        "FROM SMA_CQL_VALUESETS WHERE VERSION = '%s' ORDER BY VALUESET_NAME",
        version.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        std::string vsName = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        ValueSetEntry entry;
        entry.code       = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        entry.codeSystem = OCI_GetString(rs, 3) ? OCI_GetString(rs, 3) : "";
        entry.display    = OCI_GetString(rs, 4) ? OCI_GetString(rs, 4) : "";
        m_valueSets[vsName].push_back(entry);
    }
    OCI_StatementFree(stmt);
#else
    (void)conn;
    (void)version;
#endif

    rebuildIndex();
}

// ---------------------------------------------------------------------------
// loadFromFile — load from JSON file
// Format: { "ValueSetName": [ { "code": "...", "system": "...", "display": "..." }, ... ] }
// ---------------------------------------------------------------------------
void ValueSetManager::loadFromFile(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    // Minimal JSON parser for the simple value set format
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());

    // State machine: look for "name": [ { "code": ..., "system": ..., "display": ... } ]
    size_t pos = 0;
    auto skipWS = [&]() {
        while (pos < content.size() && isspace(content[pos])) ++pos;
    };
    auto readString = [&]() -> std::string {
        skipWS();
        if (pos >= content.size() || content[pos] != '"') return "";
        ++pos;
        std::string s;
        while (pos < content.size() && content[pos] != '"') {
            if (content[pos] == '\\' && pos + 1 < content.size()) {
                ++pos; s += content[pos]; ++pos;
            } else {
                s += content[pos]; ++pos;
            }
        }
        if (pos < content.size()) ++pos;  // closing quote
        return s;
    };
    auto expect = [&](char c) {
        skipWS();
        if (pos < content.size() && content[pos] == c) ++pos;
    };

    expect('{');
    while (pos < content.size() && content[pos] != '}') {
        std::string vsName = readString();
        expect(':');
        expect('[');

        while (pos < content.size() && content[pos] != ']') {
            expect('{');
            ValueSetEntry entry;
            while (pos < content.size() && content[pos] != '}') {
                std::string key = readString();
                expect(':');
                std::string val = readString();
                if (key == "code")    entry.code = val;
                else if (key == "system" || key == "codeSystem") entry.codeSystem = val;
                else if (key == "display") entry.display = val;
                skipWS();
                if (pos < content.size() && content[pos] == ',') ++pos;
            }
            expect('}');
            m_valueSets[vsName].push_back(entry);
            skipWS();
            if (pos < content.size() && content[pos] == ',') ++pos;
        }
        expect(']');
        skipWS();
        if (pos < content.size() && content[pos] == ',') ++pos;
    }

    rebuildIndex();
}

// ---------------------------------------------------------------------------
// isMember — O(1) lookup via membership index
// ---------------------------------------------------------------------------
bool ValueSetManager::isMember(const std::string& valueSetName,
                                const std::string& code,
                                const std::string& codeSystem) const {
    return m_membershipIndex.find(memberKey(valueSetName, code, codeSystem))
           != m_membershipIndex.end();
}

const std::vector<ValueSetEntry>& ValueSetManager::getValueSet(const std::string& name) const {
    auto it = m_valueSets.find(name);
    return (it != m_valueSets.end()) ? it->second : EMPTY_SET;
}

size_t ValueSetManager::totalCodeCount() const {
    size_t total = 0;
    for (const auto& [name, entries] : m_valueSets)
        total += entries.size();
    return total;
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
void ValueSetManager::rebuildIndex() {
    m_membershipIndex.clear();
    for (const auto& [name, entries] : m_valueSets) {
        for (const auto& e : entries) {
            m_membershipIndex[memberKey(name, e.code, e.codeSystem)] = true;
        }
    }
}

std::string ValueSetManager::memberKey(const std::string& vs,
                                        const std::string& code,
                                        const std::string& cs) {
    return vs + "|" + code + "|" + cs;
}

}  // namespace hedis

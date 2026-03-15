#pragma once
// ValueSetManager — Loads and manages HEDIS value sets for CQL evaluation

#include <map>
#include <string>
#include <vector>

// Forward declare OCI_Connection (from OCILIB)
typedef struct OCI_Connection OCI_Connection;

namespace hedis {

struct ValueSetEntry {
    std::string code;
    std::string codeSystem;  // "ICD10", "CPT", "LOINC", "NDC", "SNOMED", "HCPCS"
    std::string display;
};

class ValueSetManager {
public:
    ValueSetManager();
    ~ValueSetManager();

    // Load value sets from Oracle (SMA_CQL_VALUESETS table)
    void loadFromDatabase(OCI_Connection* conn, const std::string& version);

    // Load value sets from JSON file on filesystem
    void loadFromFile(const std::string& filepath);

    // Check if a code is a member of a named value set
    bool isMember(const std::string& valueSetName,
                  const std::string& code,
                  const std::string& codeSystem) const;

    // Get all entries for a value set
    const std::vector<ValueSetEntry>& getValueSet(const std::string& name) const;

    // Number of loaded value sets
    size_t valueSetCount() const { return m_valueSets.size(); }

    // Total code count across all value sets
    size_t totalCodeCount() const;

private:
    // name → entries
    std::map<std::string, std::vector<ValueSetEntry>> m_valueSets;

    // Fast lookup: "valueSetName|code|codeSystem" → true
    std::map<std::string, bool> m_membershipIndex;

    void rebuildIndex();
    static std::string memberKey(const std::string& vs, const std::string& code,
                                  const std::string& cs);

    static const std::vector<ValueSetEntry> EMPTY_SET;
};

}  // namespace hedis

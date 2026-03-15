#pragma once
// MeasureStore — Oracle-backed storage for CQL measure definitions

#include "cql/CQLParser.h"
#include "measure/MeasureDefinition.h"
#include <string>
#include <vector>

typedef struct OCI_Connection OCI_Connection;

namespace hedis {

class MeasureStore {
public:
    explicit MeasureStore(OCI_Connection* conn);
    ~MeasureStore();

    // Load all active measures for a measurement year
    std::vector<MeasureDefinition> loadActiveMeasures(const std::string& measurementYear);

    // Load single measure by ID and version
    MeasureDefinition loadMeasure(const std::string& measureId,
                                   const std::string& version);

    // Store/update a measure in the database
    void saveMeasure(const MeasureDefinition& measure);

    // Load measures from filesystem directory
    std::vector<MeasureDefinition> loadFromDirectory(const std::string& dirPath);

    // Load a single measure from a .cql file
    MeasureDefinition loadFromFile(const std::string& filepath,
                                    const std::string& measureId);

private:
    OCI_Connection* m_conn;
    CQLMeasureParser m_parser;
};

}  // namespace hedis

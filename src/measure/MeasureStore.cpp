// MeasureStore — Load/save CQL measures from Oracle or filesystem

#include "measure/MeasureStore.h"
#include <dirent.h>
#include <fstream>
#include <sstream>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

namespace hedis {

MeasureStore::MeasureStore(OCI_Connection* conn) : m_conn(conn) {}
MeasureStore::~MeasureStore() = default;

// ---------------------------------------------------------------------------
// loadActiveMeasures — load all active measures for a measurement year
// ---------------------------------------------------------------------------
std::vector<MeasureDefinition> MeasureStore::loadActiveMeasures(
        const std::string& measurementYear) {
    std::vector<MeasureDefinition> measures;

#ifdef HAS_OCILIB
    if (m_conn) {
        OCI_Statement* stmt = OCI_StatementCreate(m_conn);

        // Use bind variable instead of format string for LIKE pattern
        std::string versionPattern = "%" + measurementYear + "%";
        OCI_Prepare(stmt,
            "SELECT MEASURE_ID, VERSION, TO_CHAR(CQL_TEXT) FROM SMA_CQL_MEASURES "
            "WHERE STATUS = 'A' AND VERSION LIKE :ver ORDER BY MEASURE_ID");
        OCI_BindString(stmt, ":ver", &versionPattern[0], versionPattern.size());

        if (!OCI_Execute(stmt)) {
            OCI_Error* err = OCI_GetLastError();
            fprintf(stderr, "MeasureStore: query failed: %s\n",
                    err ? OCI_ErrorGetString(err) : "unknown");
            OCI_StatementFree(stmt);
            return measures;
        }

        OCI_Resultset* rs = OCI_GetResultset(stmt);
        while (rs && OCI_FetchNext(rs)) {
            MeasureDefinition def;
            def.measureId = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
            def.version   = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
            def.cqlText   = OCI_GetString(rs, 3) ? OCI_GetString(rs, 3) : "";

            if (def.cqlText.empty()) {
                fprintf(stderr, "MeasureStore: %s — empty CQL text (CLOB read failed?)\n",
                        def.measureId.c_str());
                continue;
            }

            // Parse CQL
            auto ast = m_parser.parse(def.cqlText);
            if (ast) {
                def.ast = std::move(ast);
                def.resolvePopulations();
                measures.push_back(std::move(def));
            } else {
                fprintf(stderr, "MeasureStore: %s — CQL parse failed\n",
                        def.measureId.c_str());
            }
        }
        OCI_StatementFree(stmt);
    }
#else
    (void)measurementYear;
#endif

    return measures;
}

// ---------------------------------------------------------------------------
// loadMeasure — load a single measure by ID and version
// ---------------------------------------------------------------------------
MeasureDefinition MeasureStore::loadMeasure(const std::string& measureId,
                                             const std::string& version) {
    MeasureDefinition def;
    def.measureId = measureId;
    def.version = version;

#ifdef HAS_OCILIB
    if (m_conn) {
        OCI_Statement* stmt = OCI_StatementCreate(m_conn);
        OCI_ExecuteStmtFmt(stmt,
            "SELECT CQL_TEXT FROM SMA_CQL_MEASURES "
            "WHERE MEASURE_ID = '%s' AND VERSION = '%s'",
            measureId.c_str(), version.c_str());

        OCI_Resultset* rs = OCI_GetResultset(stmt);
        if (OCI_FetchNext(rs)) {
            def.cqlText = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
            auto ast = m_parser.parse(def.cqlText);
            if (ast) {
                def.ast = std::move(ast);
                def.resolvePopulations();
            }
        }
        OCI_StatementFree(stmt);
    }
#endif

    return def;
}

// ---------------------------------------------------------------------------
// saveMeasure — insert or update a measure in the database
// ---------------------------------------------------------------------------
void MeasureStore::saveMeasure(const MeasureDefinition& measure) {
#ifdef HAS_OCILIB
    if (!m_conn) return;

    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "MERGE INTO SMA_CQL_MEASURES t "
        "USING (SELECT '%s' AS MEASURE_ID, '%s' AS VERSION FROM DUAL) s "
        "ON (t.MEASURE_ID = s.MEASURE_ID AND t.VERSION = s.VERSION) "
        "WHEN MATCHED THEN UPDATE SET CQL_TEXT = :cql, MODIFIED_DATE = SYSDATE "
        "WHEN NOT MATCHED THEN INSERT (MEASURE_ID, VERSION, CQL_TEXT) "
        "VALUES (s.MEASURE_ID, s.VERSION, :cql)",
        measure.measureId.c_str(), measure.version.c_str());

    std::string cqlBuf = measure.cqlText;
    OCI_BindString(stmt, ":cql", &cqlBuf[0], cqlBuf.size());
    OCI_Execute(stmt);
    OCI_Commit(m_conn);
    OCI_StatementFree(stmt);
#else
    (void)measure;
#endif
}

// ---------------------------------------------------------------------------
// loadFromDirectory — load all .cql files from a directory
// ---------------------------------------------------------------------------
std::vector<MeasureDefinition> MeasureStore::loadFromDirectory(const std::string& dirPath) {
    std::vector<MeasureDefinition> measures;

    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return measures;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fname = entry->d_name;
        if (fname.size() < 5 || fname.substr(fname.size() - 4) != ".cql")
            continue;

        // Derive measure ID from filename: CCS_CervicalCancerScreening.cql → CCS
        std::string measureId = fname.substr(0, fname.find('_'));
        if (measureId.empty()) measureId = fname.substr(0, fname.size() - 4);

        std::string fullPath = dirPath + "/" + fname;
        MeasureDefinition def = loadFromFile(fullPath, measureId);
        if (def.ast)
            measures.push_back(std::move(def));
    }
    closedir(dir);

    return measures;
}

// ---------------------------------------------------------------------------
// loadFromFile — load a single .cql file
// ---------------------------------------------------------------------------
MeasureDefinition MeasureStore::loadFromFile(const std::string& filepath,
                                              const std::string& measureId) {
    MeasureDefinition def;
    def.measureId = measureId;

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return def;

    std::ostringstream oss;
    oss << ifs.rdbuf();
    def.cqlText = oss.str();

    auto ast = m_parser.parse(def.cqlText);
    if (ast) {
        def.version = ast->version;
        def.ast = std::move(ast);
        def.resolvePopulations();
    }

    return def;
}

}  // namespace hedis

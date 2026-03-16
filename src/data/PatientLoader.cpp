// PatientLoader — Load patient records from Oracle

#include "data/PatientLoader.h"
#include <cstdio>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

namespace hedis {

PatientLoader::PatientLoader(OCI_Connection* conn)
    : m_conn(conn) {}

PatientLoader::~PatientLoader() = default;

// ---------------------------------------------------------------------------
// Helper: read date as 3 integers from EXTRACT(YEAR/MONTH/DAY) columns
// OCILIB's date handling (both OCI_DateGetDate and TO_CHAR) is broken on
// Oracle Cloud — single-digit months/days get multiplied by 10.
// Using EXTRACT + OCI_GetInt bypasses all OCILIB date conversion.
// ---------------------------------------------------------------------------
static HADate extractDate(OCI_Resultset* rs, int yearCol, int monthCol, int dayCol) {
    HADate dt = {0, 0, 0};
    if (!rs) return dt;
    dt.year  = OCI_GetInt(rs, yearCol);
    dt.month = OCI_GetInt(rs, monthCol);
    dt.day   = OCI_GetInt(rs, dayCol);
    return dt;
}

// ---------------------------------------------------------------------------
// loadPartition — prepare to iterate over patient ID range
// ---------------------------------------------------------------------------
void PatientLoader::loadPartition(int64_t startId, int64_t endId) {
    m_startId = startId;
    m_endId = endId;
    m_currentId = startId;
    m_loadedCount = 0;
    m_patientIndex = 0;
    fetchPatientIds();
}

// ---------------------------------------------------------------------------
// fetchPatientIds — get all patient IDs in the range
// ---------------------------------------------------------------------------
void PatientLoader::fetchPatientIds() {
    m_patientIds.clear();

#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT MEMBER_ID FROM SMA_MEMBER_MASTER "
        "WHERE ROWNUM_ID BETWEEN :start_id AND :end_id "
        "ORDER BY MEMBER_ID");
    int startInt = static_cast<int>(m_startId);
    int endInt   = static_cast<int>(m_endId);
    OCI_BindInt(stmt, ":start_id", &startInt);
    OCI_BindInt(stmt, ":end_id",   &endInt);

    if (!OCI_Execute(stmt)) {
        OCI_Error* err = OCI_GetLastError();
        fprintf(stderr, "PatientLoader: fetchPatientIds failed: %s\n",
                err ? OCI_ErrorGetString(err) : "unknown");
        OCI_StatementFree(stmt);
        return;
    }

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (rs && OCI_FetchNext(rs)) {
        const char* id = OCI_GetString(rs, 1);
        if (id) m_patientIds.push_back(id);
    }
    OCI_StatementFree(stmt);
#endif
}

// ---------------------------------------------------------------------------
// loadNextPatient — load the next patient and all data arrays
// ---------------------------------------------------------------------------
bool PatientLoader::loadNextPatient(HAPatient& patient,
                                     std::vector<HAClaim>& claims,
                                     std::vector<HACoverage>& coverages,
                                     std::vector<HADiagnosis>& diagnoses,
                                     std::vector<HAProcedure>& procedures,
                                     std::vector<HADrug>& drugs,
                                     std::vector<HALabResult>& labs) {
    if (m_patientIndex >= m_patientIds.size()) return false;

    const std::string& patientId = m_patientIds[m_patientIndex++];

    // Clear output
    claims.clear();
    coverages.clear();
    diagnoses.clear();
    procedures.clear();
    drugs.clear();
    labs.clear();

    loadPatientDemographics(patientId, patient);
    loadClaims(patientId, claims);
    loadCoverages(patientId, coverages);
    loadDiagnoses(patientId, diagnoses);
    loadProcedures(patientId, procedures);
    loadDrugs(patientId, drugs);
    loadLabResults(patientId, labs);

    ++m_loadedCount;
    return true;
}

// ---------------------------------------------------------------------------
// Individual data loaders — use TO_CHAR for all dates to avoid OCI_DateGetDate bug
// ---------------------------------------------------------------------------

void PatientLoader::loadPatientDemographics(const std::string& patientId,
                                             HAPatient& patient) {
    patient.patientId = patientId;

#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT GENDER, "
        "       EXTRACT(YEAR FROM BIRTH_DATE), "
        "       EXTRACT(MONTH FROM BIRTH_DATE), "
        "       EXTRACT(DAY FROM BIRTH_DATE), "
        "       RACE, ETHNICITY "
        "FROM SMA_MEMBER_MASTER WHERE MEMBER_ID = :mid");
    std::string midBuf = patientId;
    OCI_BindString(stmt, ":mid", &midBuf[0], midBuf.size());
    OCI_Execute(stmt);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    if (rs && OCI_FetchNext(rs)) {
        patient.gender    = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        patient.birthDate = extractDate(rs, 2, 3, 4);
        patient.race      = OCI_GetString(rs, 5) ? OCI_GetString(rs, 5) : "";
        patient.ethnicity = OCI_GetString(rs, 6) ? OCI_GetString(rs, 6) : "";
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)patient;
#endif
}

void PatientLoader::loadClaims(const std::string& patientId,
                                std::vector<HAClaim>& claims) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT CLAIM_ID, CLAIM_TYPE, "
        "       EXTRACT(YEAR FROM SERVICE_DATE), "
        "       EXTRACT(MONTH FROM SERVICE_DATE), "
        "       EXTRACT(DAY FROM SERVICE_DATE), "
        "       EXTRACT(YEAR FROM END_DATE), "
        "       EXTRACT(MONTH FROM END_DATE), "
        "       EXTRACT(DAY FROM END_DATE), "
        "       PRIMARY_DX, PLACE_OF_SERVICE "
        "FROM SMA_CLAIMS WHERE MEMBER_ID = :mid "
        "ORDER BY SERVICE_DATE");
    std::string midBuf = patientId;
    OCI_BindString(stmt, ":mid", &midBuf[0], midBuf.size());
    OCI_Execute(stmt);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (rs && OCI_FetchNext(rs)) {
        HAClaim c;
        c.claimId        = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        c.type           = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        c.serviceDate    = extractDate(rs, 3, 4, 5);
        c.endDate        = extractDate(rs, 6, 7, 8);
        c.primaryDiagnosis = OCI_GetString(rs, 9) ? OCI_GetString(rs, 9) : "";
        c.placeOfService   = OCI_GetString(rs, 10) ? OCI_GetString(rs, 10) : "";
        claims.push_back(std::move(c));
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)claims;
#endif
}

void PatientLoader::loadCoverages(const std::string& patientId,
                                   std::vector<HACoverage>& coverages) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT COVERAGE_TYPE, "
        "       EXTRACT(YEAR FROM START_DATE), "
        "       EXTRACT(MONTH FROM START_DATE), "
        "       EXTRACT(DAY FROM START_DATE), "
        "       EXTRACT(YEAR FROM END_DATE), "
        "       EXTRACT(MONTH FROM END_DATE), "
        "       EXTRACT(DAY FROM END_DATE) "
        "FROM SMA_COVERAGE WHERE MEMBER_ID = :mid "
        "ORDER BY START_DATE");
    std::string midBuf = patientId;
    OCI_BindString(stmt, ":mid", &midBuf[0], midBuf.size());
    OCI_Execute(stmt);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (rs && OCI_FetchNext(rs)) {
        HACoverage cov;
        cov.type      = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        cov.startDate = extractDate(rs, 2, 3, 4);
        cov.endDate   = extractDate(rs, 5, 6, 7);
        coverages.push_back(std::move(cov));
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)coverages;
#endif
}

void PatientLoader::loadDiagnoses(const std::string& patientId,
                                   std::vector<HADiagnosis>& diagnoses) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT DX_CODE, CODE_SYSTEM, "
        "       EXTRACT(YEAR FROM SERVICE_DATE), "
        "       EXTRACT(MONTH FROM SERVICE_DATE), "
        "       EXTRACT(DAY FROM SERVICE_DATE), "
        "       STATUS "
        "FROM SMA_DIAGNOSIS WHERE MEMBER_ID = :mid");
    std::string midBuf = patientId;
    OCI_BindString(stmt, ":mid", &midBuf[0], midBuf.size());
    OCI_Execute(stmt);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (rs && OCI_FetchNext(rs)) {
        HADiagnosis dx;
        dx.code       = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        dx.codeSystem = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        dx.date       = extractDate(rs, 3, 4, 5);
        dx.status     = OCI_GetString(rs, 6) ? OCI_GetString(rs, 6) : "";
        diagnoses.push_back(std::move(dx));
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)diagnoses;
#endif
}

void PatientLoader::loadProcedures(const std::string& patientId,
                                    std::vector<HAProcedure>& procedures) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT PROC_CODE, CODE_SYSTEM, "
        "       EXTRACT(YEAR FROM SERVICE_DATE), "
        "       EXTRACT(MONTH FROM SERVICE_DATE), "
        "       EXTRACT(DAY FROM SERVICE_DATE), "
        "       STATUS "
        "FROM SMA_PROCEDURE WHERE MEMBER_ID = :mid");
    std::string midBuf = patientId;
    OCI_BindString(stmt, ":mid", &midBuf[0], midBuf.size());
    OCI_Execute(stmt);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (rs && OCI_FetchNext(rs)) {
        HAProcedure px;
        px.code       = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        px.codeSystem = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        px.date       = extractDate(rs, 3, 4, 5);
        px.status     = OCI_GetString(rs, 6) ? OCI_GetString(rs, 6) : "";
        procedures.push_back(std::move(px));
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)procedures;
#endif
}

void PatientLoader::loadDrugs(const std::string& patientId,
                               std::vector<HADrug>& drugs) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT NDC_CODE, GPI_CODE, "
        "       EXTRACT(YEAR FROM DISPENSED_DATE), "
        "       EXTRACT(MONTH FROM DISPENSED_DATE), "
        "       EXTRACT(DAY FROM DISPENSED_DATE), "
        "       DAYS_SUPPLY, QUANTITY "
        "FROM SMA_PHARMACY WHERE MEMBER_ID = :mid");
    std::string midBuf = patientId;
    OCI_BindString(stmt, ":mid", &midBuf[0], midBuf.size());
    OCI_Execute(stmt);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (rs && OCI_FetchNext(rs)) {
        HADrug drug;
        drug.ndcCode      = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        drug.gpiCode      = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        drug.dispensedDate = extractDate(rs, 3, 4, 5);
        drug.daysSupply   = OCI_GetInt(rs, 6);
        drug.quantity     = OCI_GetDouble(rs, 7);
        drugs.push_back(std::move(drug));
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)drugs;
#endif
}

void PatientLoader::loadLabResults(const std::string& patientId,
                                    std::vector<HALabResult>& labs) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_Prepare(stmt,
        "SELECT LOINC_CODE, TEST_NAME, "
        "       EXTRACT(YEAR FROM RESULT_DATE), "
        "       EXTRACT(MONTH FROM RESULT_DATE), "
        "       EXTRACT(DAY FROM RESULT_DATE), "
        "       NUMERIC_VALUE, STRING_VALUE, UNITS, STATUS "
        "FROM SMA_LAB_RESULTS WHERE MEMBER_ID = :mid");
    std::string midBuf = patientId;
    OCI_BindString(stmt, ":mid", &midBuf[0], midBuf.size());
    OCI_Execute(stmt);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (rs && OCI_FetchNext(rs)) {
        HALabResult lab;
        lab.loincCode    = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        lab.name         = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        lab.resultDate   = extractDate(rs, 3, 4, 5);
        lab.numericValue = OCI_GetDouble(rs, 6);
        lab.stringValue  = OCI_GetString(rs, 7) ? OCI_GetString(rs, 7) : "";
        lab.units        = OCI_GetString(rs, 8) ? OCI_GetString(rs, 8) : "";
        lab.status       = OCI_GetString(rs, 9) ? OCI_GetString(rs, 9) : "";
        labs.push_back(std::move(lab));
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)labs;
#endif
}

}  // namespace hedis

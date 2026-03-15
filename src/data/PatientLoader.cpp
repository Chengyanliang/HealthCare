// PatientLoader — Load patient records from Oracle

#include "data/PatientLoader.h"

// #include <ocilib.h>

namespace hedis {

PatientLoader::PatientLoader(OCI_Connection* conn)
    : m_conn(conn) {}

PatientLoader::~PatientLoader() = default;

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
    OCI_ExecuteStmtFmt(stmt,
        "SELECT MEMBER_ID FROM SMA_MEMBER_MASTER "
        "WHERE ROWNUM_ID BETWEEN %lld AND %lld "
        "ORDER BY MEMBER_ID",
        m_startId, m_endId);

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
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
// Individual data loaders (Oracle SQL)
// ---------------------------------------------------------------------------

void PatientLoader::loadPatientDemographics(const std::string& patientId,
                                             HAPatient& patient) {
    patient.patientId = patientId;

#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "SELECT GENDER, BIRTH_DATE, RACE, ETHNICITY "
        "FROM SMA_MEMBER_MASTER WHERE MEMBER_ID = '%s'",
        patientId.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    if (OCI_FetchNext(rs)) {
        patient.gender = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        OCI_Date* dt = OCI_GetDate(rs, 2);
        if (dt) {
            int y, m, d;
            OCI_DateGetDate(dt, &y, &m, &d);
            patient.birthDate = {y, m, d};
        }
        patient.race      = OCI_GetString(rs, 3) ? OCI_GetString(rs, 3) : "";
        patient.ethnicity  = OCI_GetString(rs, 4) ? OCI_GetString(rs, 4) : "";
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
    OCI_ExecuteStmtFmt(stmt,
        "SELECT CLAIM_ID, CLAIM_TYPE, SERVICE_DATE, END_DATE, "
        "       PRIMARY_DX, PLACE_OF_SERVICE "
        "FROM SMA_CLAIMS WHERE MEMBER_ID = '%s' "
        "ORDER BY SERVICE_DATE",
        patientId.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        HAClaim c;
        c.claimId = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        c.type    = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";

        OCI_Date* sd = OCI_GetDate(rs, 3);
        if (sd) { int y,m,d; OCI_DateGetDate(sd,&y,&m,&d); c.serviceDate = {y,m,d}; }
        OCI_Date* ed = OCI_GetDate(rs, 4);
        if (ed) { int y,m,d; OCI_DateGetDate(ed,&y,&m,&d); c.endDate = {y,m,d}; }

        c.primaryDiagnosis = OCI_GetString(rs, 5) ? OCI_GetString(rs, 5) : "";
        c.placeOfService   = OCI_GetString(rs, 6) ? OCI_GetString(rs, 6) : "";
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
    OCI_ExecuteStmtFmt(stmt,
        "SELECT COVERAGE_TYPE, START_DATE, END_DATE "
        "FROM SMA_COVERAGE WHERE MEMBER_ID = '%s' "
        "ORDER BY START_DATE",
        patientId.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        HACoverage cov;
        cov.type = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        OCI_Date* sd = OCI_GetDate(rs, 2);
        if (sd) { int y,m,d; OCI_DateGetDate(sd,&y,&m,&d); cov.startDate = {y,m,d}; }
        OCI_Date* ed = OCI_GetDate(rs, 3);
        if (ed) { int y,m,d; OCI_DateGetDate(ed,&y,&m,&d); cov.endDate = {y,m,d}; }
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
    OCI_ExecuteStmtFmt(stmt,
        "SELECT DX_CODE, CODE_SYSTEM, SERVICE_DATE, STATUS "
        "FROM SMA_DIAGNOSIS WHERE MEMBER_ID = '%s'",
        patientId.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        HADiagnosis dx;
        dx.code       = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        dx.codeSystem = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        OCI_Date* dt = OCI_GetDate(rs, 3);
        if (dt) { int y,m,d; OCI_DateGetDate(dt,&y,&m,&d); dx.date = {y,m,d}; }
        dx.status = OCI_GetString(rs, 4) ? OCI_GetString(rs, 4) : "";
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
    OCI_ExecuteStmtFmt(stmt,
        "SELECT PROC_CODE, CODE_SYSTEM, SERVICE_DATE, STATUS "
        "FROM SMA_PROCEDURE WHERE MEMBER_ID = '%s'",
        patientId.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        HAProcedure px;
        px.code       = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        px.codeSystem = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        OCI_Date* dt = OCI_GetDate(rs, 3);
        if (dt) { int y,m,d; OCI_DateGetDate(dt,&y,&m,&d); px.date = {y,m,d}; }
        px.status = OCI_GetString(rs, 4) ? OCI_GetString(rs, 4) : "";
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
    OCI_ExecuteStmtFmt(stmt,
        "SELECT NDC_CODE, GPI_CODE, DISPENSED_DATE, DAYS_SUPPLY, QUANTITY "
        "FROM SMA_PHARMACY WHERE MEMBER_ID = '%s'",
        patientId.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        HADrug drug;
        drug.ndcCode = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        drug.gpiCode = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        OCI_Date* dt = OCI_GetDate(rs, 3);
        if (dt) { int y,m,d; OCI_DateGetDate(dt,&y,&m,&d); drug.dispensedDate = {y,m,d}; }
        drug.daysSupply = OCI_GetInt(rs, 4);
        drug.quantity   = OCI_GetDouble(rs, 5);
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
    OCI_ExecuteStmtFmt(stmt,
        "SELECT LOINC_CODE, TEST_NAME, RESULT_DATE, "
        "       NUMERIC_VALUE, STRING_VALUE, UNITS, STATUS "
        "FROM SMA_LAB_RESULTS WHERE MEMBER_ID = '%s'",
        patientId.c_str());

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        HALabResult lab;
        lab.loincCode    = OCI_GetString(rs, 1) ? OCI_GetString(rs, 1) : "";
        lab.name         = OCI_GetString(rs, 2) ? OCI_GetString(rs, 2) : "";
        OCI_Date* dt = OCI_GetDate(rs, 3);
        if (dt) { int y,m,d; OCI_DateGetDate(dt,&y,&m,&d); lab.resultDate = {y,m,d}; }
        lab.numericValue = OCI_GetDouble(rs, 4);
        lab.stringValue  = OCI_GetString(rs, 5) ? OCI_GetString(rs, 5) : "";
        lab.units        = OCI_GetString(rs, 6) ? OCI_GetString(rs, 6) : "";
        lab.status       = OCI_GetString(rs, 7) ? OCI_GetString(rs, 7) : "";
        labs.push_back(std::move(lab));
    }
    OCI_StatementFree(stmt);
#else
    (void)patientId;
    (void)labs;
#endif
}

}  // namespace hedis

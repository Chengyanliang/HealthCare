#!/usr/bin/env python3
"""Load FHIR-modeled test patients into Oracle for end-to-end CQL measure testing.

Creates clinical tables (SMA_MEMBER_MASTER, SMA_CLAIMS, SMA_COVERAGE,
SMA_DIAGNOSIS, SMA_PROCEDURE, SMA_PHARMACY, SMA_LAB_RESULTS) and populates
them with 10 synthetic patients designed to produce known HEDIS outcomes.

Patient scenarios (measurement year 2026):
  P001 - 40F, compliant CCS (pap smear 2024), compliant COL (colonoscopy 2020)
  P002 - 55F, compliant BCS (mammogram 2025), compliant CCS (pap+HPV 2023)
  P003 - 50M, compliant COL (FOBT 2026), diabetic CDC compliant (HbA1c=6.8)
  P004 - 60F, excluded CCS (hysterectomy), compliant BCS (mammogram 2026)
  P005 - 70M, hypertensive CBP compliant (BP 128/78), compliant COL (colonoscopy 2018)
  P006 - 30F, non-compliant CCS (no pap in 3 years)
  P007 - 52M, diabetic CDC non-compliant (HbA1c=9.5)
  P008 - 45M, hypertensive CBP non-compliant (BP 155/95)
  P009 - 22F, too young for CCS (age 22 < 24), no measures apply
  P010 - 80M, excluded COL (colorectal cancer history), hypertensive CBP compliant

Environment variables:
  HEDIS_DB_PASSWORD       - ADMIN password (required)
  HEDIS_WALLET_PASSWORD   - Wallet password (required)
"""
import oracledb
import os
import getpass
import sys
from datetime import date

WALLET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'wallet')

db_password = os.environ.get('HEDIS_DB_PASSWORD')
if not db_password:
    db_password = getpass.getpass('ADMIN password: ')

wallet_password = os.environ.get('HEDIS_WALLET_PASSWORD')
if not wallet_password:
    wallet_password = getpass.getpass('Wallet password: ')

print('Connecting...')
sys.stdout.flush()

conn = oracledb.connect(
    user='ADMIN',
    password=db_password,
    dsn='hediscql_tp',
    config_dir=WALLET_DIR,
    wallet_location=WALLET_DIR,
    wallet_password=wallet_password,
    tcp_connect_timeout=10
)
cursor = conn.cursor()
print(f'Connected to Oracle {conn.version}')

# ============================================================================
# Step 1: Create clinical data tables
# ============================================================================
print('\n=== Creating clinical data tables ===')

tables = [
    """CREATE TABLE SMA_MEMBER_MASTER (
        MEMBER_ID   VARCHAR2(30) NOT NULL PRIMARY KEY,
        ROWNUM_ID   NUMBER(15),
        GENDER      VARCHAR2(10),
        BIRTH_DATE  DATE,
        RACE        VARCHAR2(50),
        ETHNICITY   VARCHAR2(50)
    )""",
    """CREATE TABLE SMA_CLAIMS (
        CLAIM_ID         VARCHAR2(30),
        MEMBER_ID        VARCHAR2(30) NOT NULL,
        CLAIM_TYPE       VARCHAR2(20),
        SERVICE_DATE     DATE,
        END_DATE         DATE,
        PRIMARY_DX       VARCHAR2(20),
        PLACE_OF_SERVICE VARCHAR2(10)
    )""",
    """CREATE TABLE SMA_COVERAGE (
        MEMBER_ID     VARCHAR2(30) NOT NULL,
        COVERAGE_TYPE VARCHAR2(20),
        START_DATE    DATE,
        END_DATE      DATE
    )""",
    """CREATE TABLE SMA_DIAGNOSIS (
        MEMBER_ID    VARCHAR2(30) NOT NULL,
        DX_CODE      VARCHAR2(20),
        CODE_SYSTEM  VARCHAR2(20),
        SERVICE_DATE DATE,
        STATUS       VARCHAR2(20) DEFAULT 'active'
    )""",
    """CREATE TABLE SMA_PROCEDURE (
        MEMBER_ID    VARCHAR2(30) NOT NULL,
        PROC_CODE    VARCHAR2(20),
        CODE_SYSTEM  VARCHAR2(20),
        SERVICE_DATE DATE,
        STATUS       VARCHAR2(20) DEFAULT 'completed'
    )""",
    """CREATE TABLE SMA_PHARMACY (
        MEMBER_ID      VARCHAR2(30) NOT NULL,
        NDC_CODE       VARCHAR2(20),
        GPI_CODE       VARCHAR2(20),
        DISPENSED_DATE DATE,
        DAYS_SUPPLY    NUMBER(5),
        QUANTITY       NUMBER(10,2)
    )""",
    """CREATE TABLE SMA_LAB_RESULTS (
        MEMBER_ID     VARCHAR2(30) NOT NULL,
        LOINC_CODE    VARCHAR2(20),
        TEST_NAME     VARCHAR2(200),
        RESULT_DATE   DATE,
        NUMERIC_VALUE NUMBER(10,4),
        STRING_VALUE  VARCHAR2(200),
        UNITS         VARCHAR2(50),
        STATUS        VARCHAR2(20) DEFAULT 'final'
    )""",
]

for sql in tables:
    name = sql.split('(')[0].strip().split()[-1]
    try:
        cursor.execute(sql)
        print(f'  CREATED: {name}')
    except oracledb.DatabaseError as e:
        if 'ORA-00955' in str(e):
            # Table exists — truncate it for fresh data
            cursor.execute(f'TRUNCATE TABLE {name}')
            print(f'  EXISTS (truncated): {name}')
        else:
            print(f'  ERROR: {name}: {e}')

# Add indexes
for idx_sql in [
    "CREATE INDEX IDX_CLAIMS_MEMBER ON SMA_CLAIMS (MEMBER_ID)",
    "CREATE INDEX IDX_COVERAGE_MEMBER ON SMA_COVERAGE (MEMBER_ID)",
    "CREATE INDEX IDX_DX_MEMBER ON SMA_DIAGNOSIS (MEMBER_ID)",
    "CREATE INDEX IDX_PROC_MEMBER ON SMA_PROCEDURE (MEMBER_ID)",
    "CREATE INDEX IDX_PHARM_MEMBER ON SMA_PHARMACY (MEMBER_ID)",
    "CREATE INDEX IDX_LAB_MEMBER ON SMA_LAB_RESULTS (MEMBER_ID)",
    "CREATE INDEX IDX_MEMBER_ROWNUM ON SMA_MEMBER_MASTER (ROWNUM_ID)",
]:
    try:
        cursor.execute(idx_sql)
    except oracledb.DatabaseError:
        pass  # Index already exists

conn.commit()

# ============================================================================
# Step 2: Load FHIR-modeled test patients
# ============================================================================
print('\n=== Loading test patients ===')

# -- Members (FHIR Patient resources) --
members = [
    # (MEMBER_ID, ROWNUM_ID, GENDER, BIRTH_DATE, RACE, ETHNICITY)
    ('P001', 1, 'female', date(1986, 3, 15), 'White', 'Not Hispanic'),
    ('P002', 2, 'female', date(1971, 7, 22), 'Black', 'Not Hispanic'),
    ('P003', 3, 'male',   date(1976, 1, 10), 'White', 'Hispanic'),
    ('P004', 4, 'female', date(1966, 11, 5), 'Asian', 'Not Hispanic'),
    ('P005', 5, 'male',   date(1956, 4, 20), 'White', 'Not Hispanic'),
    ('P006', 6, 'female', date(1996, 8, 1),  'Black', 'Not Hispanic'),
    ('P007', 7, 'male',   date(1974, 5, 18), 'White', 'Hispanic'),
    ('P008', 8, 'male',   date(1981, 12, 3), 'White', 'Not Hispanic'),
    ('P009', 9, 'female', date(2004, 6, 25), 'Asian', 'Not Hispanic'),
    ('P010', 10, 'male',  date(1946, 2, 14), 'White', 'Not Hispanic'),
]

cursor.executemany(
    "INSERT INTO SMA_MEMBER_MASTER (MEMBER_ID, ROWNUM_ID, GENDER, BIRTH_DATE, RACE, ETHNICITY) "
    "VALUES (:1, :2, :3, :4, :5, :6)", members)
print(f'  {len(members)} patients loaded')

# -- Coverage (FHIR Coverage resources) — all have medical coverage spanning 2026 --
coverages = [
    (mid, 'medical', date(2025, 1, 1), date(2026, 12, 31))
    for mid, *_ in members
]
cursor.executemany(
    "INSERT INTO SMA_COVERAGE (MEMBER_ID, COVERAGE_TYPE, START_DATE, END_DATE) "
    "VALUES (:1, :2, :3, :4)", coverages)
print(f'  {len(coverages)} coverage records')

# -- Procedures (FHIR Procedure resources) --
procedures = [
    # P001: Pap smear 2024 → CCS compliant (within 3 years)
    ('P001', '88175', 'CPT', date(2024, 5, 10), 'completed'),
    # P001: Colonoscopy 2020 → COL compliant (within 10 years)
    ('P001', '45378', 'CPT', date(2020, 9, 15), 'completed'),

    # P002: Mammogram 2025 → BCS compliant (within 2 years)
    ('P002', '77067', 'CPT', date(2025, 3, 20), 'completed'),
    # P002: Pap smear 2023 (age 52, >=30 → pap+HPV co-test path, within 5 years)
    ('P002', '88142', 'CPT', date(2023, 6, 15), 'completed'),

    # P003: (no procedures needed, has FOBT lab and HbA1c lab)

    # P004: Hysterectomy 2015 → CCS excluded
    ('P004', '58150', 'CPT', date(2015, 3, 1), 'completed'),
    # P004: Mammogram 2026 → BCS compliant
    ('P004', '77057', 'CPT', date(2026, 1, 15), 'completed'),

    # P005: Colonoscopy 2018 → COL compliant (within 10 years)
    ('P005', '45380', 'CPT', date(2018, 11, 8), 'completed'),

    # P010: Colonoscopy 2022 → COL (but excluded due to colorectal cancer dx)
    ('P010', '45378', 'CPT', date(2022, 4, 10), 'completed'),
]
cursor.executemany(
    "INSERT INTO SMA_PROCEDURE (MEMBER_ID, PROC_CODE, CODE_SYSTEM, SERVICE_DATE, STATUS) "
    "VALUES (:1, :2, :3, :4, :5)", procedures)
print(f'  {len(procedures)} procedure records')

# -- Diagnoses (FHIR Condition resources) --
diagnoses = [
    # P003: Diabetes → CDC initial population
    ('P003', 'E11.9', 'ICD10', date(2025, 3, 1), 'active'),

    # P005: Essential Hypertension → CBP initial population
    ('P005', 'I10', 'ICD10', date(2025, 8, 15), 'active'),

    # P007: Diabetes → CDC initial population (non-compliant)
    ('P007', 'E11.65', 'ICD10', date(2024, 11, 1), 'active'),

    # P008: Essential Hypertension → CBP initial population (non-compliant)
    ('P008', 'I10', 'ICD10', date(2026, 2, 1), 'active'),

    # P010: Colorectal Cancer → COL denominator exclusion
    ('P010', 'C18.9', 'ICD10', date(2019, 6, 1), 'active'),
    # P010: Essential Hypertension → CBP initial population
    ('P010', 'I10', 'ICD10', date(2025, 5, 10), 'active'),
]
cursor.executemany(
    "INSERT INTO SMA_DIAGNOSIS (MEMBER_ID, DX_CODE, CODE_SYSTEM, SERVICE_DATE, STATUS) "
    "VALUES (:1, :2, :3, :4, :5)", diagnoses)
print(f'  {len(diagnoses)} diagnosis records')

# -- Lab Results (FHIR Observation resources) --
labs = [
    # P002: HPV test within 4 days of Pap (2023-06-15) → CCS co-test path
    ('P002', '87624', 'HPV detection, high-risk types', date(2023, 6, 16),
     None, 'Negative', None, 'final'),

    # P003: FOBT 2026 → COL compliant
    ('P003', '82274', 'Blood, occult, feces, immunoassay', date(2026, 4, 1),
     None, 'Negative', None, 'final'),
    # P003: HbA1c = 6.8 → CDC compliant (<8.0)
    ('P003', '4548-4', 'Hemoglobin A1c', date(2026, 7, 15),
     6.8, '6.8', '%', 'final'),

    # P005: Systolic BP 128 → CBP compliant (<140)
    ('P005', '8480-6', 'Systolic blood pressure', date(2026, 9, 1),
     128.0, '128', 'mmHg', 'final'),
    # P005: Diastolic BP 78 → CBP compliant (<90)
    ('P005', '8462-4', 'Diastolic blood pressure', date(2026, 9, 1),
     78.0, '78', 'mmHg', 'final'),

    # P007: HbA1c = 9.5 → CDC non-compliant (>=8.0, poor control >9.0)
    ('P007', '4548-4', 'Hemoglobin A1c', date(2026, 5, 20),
     9.5, '9.5', '%', 'final'),

    # P008: Systolic BP 155 → CBP non-compliant (>=140)
    ('P008', '8480-6', 'Systolic blood pressure', date(2026, 8, 10),
     155.0, '155', 'mmHg', 'final'),
    # P008: Diastolic BP 95 → CBP non-compliant (>=90)
    ('P008', '8462-4', 'Diastolic blood pressure', date(2026, 8, 10),
     95.0, '95', 'mmHg', 'final'),

    # P010: Systolic BP 132 → CBP compliant (<140)
    ('P010', '8480-6', 'Systolic blood pressure', date(2026, 10, 5),
     132.0, '132', 'mmHg', 'final'),
    # P010: Diastolic BP 82 → CBP compliant (<90)
    ('P010', '8462-4', 'Diastolic blood pressure', date(2026, 10, 5),
     82.0, '82', 'mmHg', 'final'),
]
cursor.executemany(
    "INSERT INTO SMA_LAB_RESULTS "
    "(MEMBER_ID, LOINC_CODE, TEST_NAME, RESULT_DATE, NUMERIC_VALUE, STRING_VALUE, UNITS, STATUS) "
    "VALUES (:1, :2, :3, :4, :5, :6, :7, :8)", labs)
print(f'  {len(labs)} lab result records')

conn.commit()

# ============================================================================
# Step 3: Verify and print expected outcomes
# ============================================================================
print('\n=== Verification ===')

for tbl in ['SMA_MEMBER_MASTER', 'SMA_COVERAGE', 'SMA_CLAIMS',
            'SMA_DIAGNOSIS', 'SMA_PROCEDURE', 'SMA_PHARMACY', 'SMA_LAB_RESULTS']:
    cursor.execute(f'SELECT COUNT(*) FROM {tbl}')
    print(f'  {tbl}: {cursor.fetchone()[0]} rows')

print('\n=== Expected HEDIS Outcomes (Measurement Year 2026) ===')
print()
print('Patient  Age  Sex  CCS          BCS          COL          CBP          CDC')
print('-------  ---  ---  -----------  -----------  -----------  -----------  -----------')
print('P001     40   F    Compliant    N/A(age)     Compliant    N/A          N/A')
print('P002     55   F    Compliant*   Compliant    Compliant†   N/A          N/A')
print('P003     50   M    N/A(male)    N/A(male)    Compliant    N/A          Compliant')
print('P004     60   F    Excluded‡    Compliant    Compliant†   N/A          N/A')
print('P005     70   M    N/A(male)    N/A(male)    Compliant    Compliant    N/A')
print('P006     30   F    Non-Compl    N/A(age)     N/A(age)     N/A          N/A')
print('P007     52   M    N/A(male)    N/A(male)    Compliant†   N/A          Non-Compl')
print('P008     45   M    N/A(male)    N/A(male)    N/A(age)     Non-Compl    N/A')
print('P009     22   F    N/A(age)     N/A(age)     N/A(age)     N/A          N/A')
print('P010     80   M    N/A(male)    N/A(male)    Excluded§    Compliant    N/A')
print()
print('* Pap+HPV co-test path (age>=30)')
print('† Age in COL range (46-75) but no screening procedure — depends on measure logic')
print('‡ Hysterectomy → denominator exclusion')
print('§ Colorectal cancer → denominator exclusion')

conn.close()
print('\nTest patient data loaded successfully.')

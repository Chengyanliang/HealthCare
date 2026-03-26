#!/usr/bin/env python3
"""Populate empty tables in local Oracle XE (hedis schema)."""
import oracledb
from datetime import date

conn = oracledb.connect(user='hedis', password='SafeMed2026', dsn='localhost:1521/XEPDB1')
cur = conn.cursor()

# ── 1. SMA_CLAIMS ──────────────────────────────────────────────────────────────
# One outpatient claim per procedure/diagnosis visit per patient
print('=== SMA_CLAIMS ===')
cur.execute('DELETE FROM SMA_CLAIMS')

claims = [
    # CLAIM_ID        MEMBER  TYPE    SERVICE_DATE        END_DATE           PRIMARY_DX  POS
    ('CLM-P001-001', 'P001', 'PRO',  date(2024, 5, 10),  date(2024, 5, 10), 'Z12.4',   '11'),  # Pap smear
    ('CLM-P001-002', 'P001', 'PRO',  date(2020, 9, 15),  date(2020, 9, 15), 'Z12.11',  '11'),  # Colonoscopy
    ('CLM-P002-001', 'P002', 'PRO',  date(2025, 3, 20),  date(2025, 3, 20), 'Z12.31',  '11'),  # Mammogram
    ('CLM-P002-002', 'P002', 'PRO',  date(2023, 6, 15),  date(2023, 6, 15), 'Z12.4',   '11'),  # Pap smear
    ('CLM-P002-003', 'P002', 'PRO',  date(2023, 6, 16),  date(2023, 6, 16), 'Z11.51',  '11'),  # HPV test
    ('CLM-P003-001', 'P003', 'PRO',  date(2025, 3,  1),  date(2025, 3,  1), 'E11.9',   '11'),  # Diabetes dx
    ('CLM-P003-002', 'P003', 'PRO',  date(2026, 4,  1),  date(2026, 4,  1), 'Z12.11',  '11'),  # FOBT
    ('CLM-P003-003', 'P003', 'PRO',  date(2026, 7, 15),  date(2026, 7, 15), 'E11.65',  '11'),  # HbA1c
    ('CLM-P004-001', 'P004', 'PRO',  date(2026, 1, 10),  date(2026, 1, 10), 'Z12.31',  '11'),  # Mammogram
    ('CLM-P004-002', 'P004', 'PRO',  date(2019, 6, 20),  date(2019, 6, 20), 'Z90.710', '11'),  # Hysterectomy
    ('CLM-P005-001', 'P005', 'PRO',  date(2018, 8, 12),  date(2018, 8, 12), 'Z12.11',  '11'),  # Colonoscopy
    ('CLM-P005-002', 'P005', 'PRO',  date(2026, 2, 14),  date(2026, 2, 14), 'I10',     '11'),  # HTN BP visit
    ('CLM-P005-003', 'P005', 'PRO',  date(2025,11,  5),  date(2025,11,  5), 'I10',     '11'),  # HTN dx
    ('CLM-P006-001', 'P006', 'PRO',  date(2026, 1,  5),  date(2026, 1,  5), 'Z01.419', '11'),  # Annual exam
    ('CLM-P007-001', 'P007', 'PRO',  date(2025, 9, 10),  date(2025, 9, 10), 'E11.9',   '11'),  # Diabetes dx
    ('CLM-P007-002', 'P007', 'PRO',  date(2026, 3,  1),  date(2026, 3,  1), 'E11.65',  '11'),  # HbA1c
    ('CLM-P008-001', 'P008', 'PRO',  date(2026, 1, 20),  date(2026, 1, 20), 'I10',     '11'),  # HTN BP visit
    ('CLM-P008-002', 'P008', 'PRO',  date(2025,10, 15),  date(2025,10, 15), 'I10',     '11'),  # HTN dx
    ('CLM-P009-001', 'P009', 'PRO',  date(2026, 2, 28),  date(2026, 2, 28), 'Z00.00',  '11'),  # Annual exam
    ('CLM-P010-001', 'P010', 'PRO',  date(2026, 1,  8),  date(2026, 1,  8), 'I10',     '11'),  # HTN BP visit
    ('CLM-P010-002', 'P010', 'PRO',  date(2025, 6, 12),  date(2025, 6, 12), 'I10',     '11'),  # HTN dx
    ('CLM-P010-003', 'P010', 'PRO',  date(2025, 4, 20),  date(2025, 4, 20), 'E11.9',   '11'),  # Diabetes dx
]
cur.executemany(
    "INSERT INTO SMA_CLAIMS (CLAIM_ID,MEMBER_ID,CLAIM_TYPE,SERVICE_DATE,END_DATE,PRIMARY_DX,PLACE_OF_SERVICE) "
    "VALUES (:1,:2,:3,:4,:5,:6,:7)", claims)
print(f'  Inserted {len(claims)} claims')

# ── 2. SMA_PHARMACY ────────────────────────────────────────────────────────────
# Drug fills for diabetic and hypertensive patients
print('=== SMA_PHARMACY ===')
cur.execute('DELETE FROM SMA_PHARMACY')

pharmacy = [
    # MEMBER  NDC           GPI                  DISPENSED            DAYS  QTY
    # P003 — metformin (diabetes)
    ('P003', '00093-1042-01', '27400010100320', date(2025, 3, 10),  90, 180),
    ('P003', '00093-1042-01', '27400010100320', date(2025, 6, 10),  90, 180),
    ('P003', '00093-1042-01', '27400010100320', date(2025, 9, 10),  90, 180),
    # P005 — lisinopril (hypertension)
    ('P005', '00116-0673-01', '36100010000320', date(2025,11, 10),  90,  90),
    ('P005', '00116-0673-01', '36100010000320', date(2026, 2, 10),  90,  90),
    # P007 — metformin (diabetes)
    ('P007', '00093-1042-01', '27400010100320', date(2025, 9, 15),  90, 180),
    ('P007', '00093-1042-01', '27400010100320', date(2025,12, 15),  90, 180),
    # P008 — amlodipine (hypertension)
    ('P008', '00069-1540-30', '36100060000320', date(2025,10, 20),  90,  90),
    ('P008', '00069-1540-30', '36100060000320', date(2026, 1, 20),  90,  90),
    # P010 — lisinopril (hypertension)
    ('P010', '00116-0673-01', '36100010000320', date(2025, 6, 15),  90,  90),
    ('P010', '00116-0673-01', '36100010000320', date(2025, 9, 15),  90,  90),
    ('P010', '00116-0673-01', '36100010000320', date(2025,12, 15),  90,  90),
]
cur.executemany(
    "INSERT INTO SMA_PHARMACY (MEMBER_ID,NDC_CODE,GPI_CODE,DISPENSED_DATE,DAYS_SUPPLY,QUANTITY) "
    "VALUES (:1,:2,:3,:4,:5,:6)", pharmacy)
print(f'  Inserted {len(pharmacy)} pharmacy records')

# ── 3. SMA_CQL_RESULTS_STG ────────────────────────────────────────────────────
# Mirror of SMA_CQL_RESULTS as partition 1
print('=== SMA_CQL_RESULTS_STG ===')
cur.execute('DELETE FROM SMA_CQL_RESULTS_STG')
cur.execute("""
    INSERT INTO SMA_CQL_RESULTS_STG
        (PARTITION_ID, PATIENT_ID, MEASURE_ID, INITIAL_POP, DENOMINATOR,
         NUMERATOR, DENOM_EXCLUSION, DENOM_EXCEPTION, FAILURE_DATE,
         NUMERIC_RESULT, PROCESS_DATE)
    SELECT 1, PATIENT_ID, MEASURE_ID, INITIAL_POP, DENOMINATOR,
           NUMERATOR, DENOM_EXCLUSION, 'N', FAILURE_DATE,
           NUMERIC_RESULT, PROCESS_DATE
    FROM SMA_CQL_RESULTS
    WHERE JOB_ID = 'E2E_TEST'
""")
stg_count = cur.rowcount
print(f'  Inserted {stg_count} staging rows')

# ── 4. SMA_CQL_MEASURE_RATES ──────────────────────────────────────────────────
print('=== SMA_CQL_MEASURE_RATES ===')
cur.execute("DELETE FROM SMA_CQL_MEASURE_RATES WHERE JOB_ID = 'E2E_TEST'")
cur.execute("""
    INSERT INTO SMA_CQL_MEASURE_RATES
        (JOB_ID, MEASURE_ID, INITIAL_POP_COUNT, DENOM_COUNT,
         NUMER_COUNT, EXCLUSION_COUNT, RATE, PROCESS_DATE)
    SELECT
        'E2E_TEST',
        MEASURE_ID,
        SUM(CASE WHEN INITIAL_POP = 'Y' THEN 1 ELSE 0 END),
        SUM(CASE WHEN DENOMINATOR = 'Y' AND DENOM_EXCLUSION != 'Y' THEN 1 ELSE 0 END),
        SUM(CASE WHEN NUMERATOR = 'Y' THEN 1 ELSE 0 END),
        SUM(CASE WHEN DENOM_EXCLUSION = 'Y' THEN 1 ELSE 0 END),
        ROUND(
            SUM(CASE WHEN NUMERATOR = 'Y' THEN 1.0 ELSE 0 END) /
            NULLIF(SUM(CASE WHEN DENOMINATOR = 'Y' AND DENOM_EXCLUSION != 'Y'
                            THEN 1.0 ELSE 0 END), 0) * 100, 2),
        SYSDATE
    FROM SMA_CQL_RESULTS
    WHERE JOB_ID = 'E2E_TEST'
    GROUP BY MEASURE_ID
""")
rate_count = cur.rowcount
print(f'  Inserted {rate_count} measure rates')

conn.commit()

# ── Summary ───────────────────────────────────────────────────────────────────
print('\n=== Final row counts ===')
for t in ['SMA_MEMBER_MASTER','SMA_COVERAGE','SMA_CLAIMS','SMA_DIAGNOSIS',
          'SMA_PROCEDURE','SMA_PHARMACY','SMA_LAB_RESULTS',
          'SMA_CQL_MEASURES','SMA_CQL_VALUESETS','SMA_CQL_RESULTS',
          'SMA_CQL_RESULTS_STG','SMA_CQL_MEASURE_RATES']:
    cur.execute(f'SELECT COUNT(*) FROM {t}')
    print(f'  {t:<28} {cur.fetchone()[0]:>4} rows')

conn.close()
print('\nDone.')

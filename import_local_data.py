#!/usr/bin/env python3
"""Create tables and import oracl_data/ CSVs into CQL_MOCKED_LOCAL schema."""
import csv
import datetime
import getpass
import os
import oracledb

WALLET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'wallet')
DATA_DIR   = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'oracl_data')

# Table DDL — no partitioning (not needed for local mock schema)
TABLES_DDL = [
    """CREATE TABLE SMA_MEMBER_MASTER (
        MEMBER_ID   VARCHAR2(30)  NOT NULL PRIMARY KEY,
        ROWNUM_ID   NUMBER(10),
        GENDER      VARCHAR2(10),
        BIRTH_DATE  DATE,
        RACE        VARCHAR2(50),
        ETHNICITY   VARCHAR2(50)
    )""",
    """CREATE TABLE SMA_COVERAGE (
        MEMBER_ID     VARCHAR2(30) NOT NULL,
        COVERAGE_TYPE VARCHAR2(20),
        START_DATE    DATE,
        END_DATE      DATE
    )""",
    """CREATE TABLE SMA_CLAIMS (
        CLAIM_ID        VARCHAR2(50),
        MEMBER_ID       VARCHAR2(30),
        CLAIM_TYPE      VARCHAR2(20),
        SERVICE_DATE    DATE,
        END_DATE        DATE,
        PRIMARY_DX      VARCHAR2(20),
        PLACE_OF_SERVICE VARCHAR2(10)
    )""",
    """CREATE TABLE SMA_DIAGNOSIS (
        MEMBER_ID    VARCHAR2(30),
        DX_CODE      VARCHAR2(20),
        CODE_SYSTEM  VARCHAR2(20),
        SERVICE_DATE DATE,
        STATUS       VARCHAR2(20)
    )""",
    """CREATE TABLE SMA_PROCEDURE (
        MEMBER_ID    VARCHAR2(30),
        PROC_CODE    VARCHAR2(20),
        CODE_SYSTEM  VARCHAR2(20),
        SERVICE_DATE DATE,
        STATUS       VARCHAR2(20)
    )""",
    """CREATE TABLE SMA_LAB_RESULTS (
        MEMBER_ID    VARCHAR2(30),
        LOINC_CODE   VARCHAR2(20),
        TEST_NAME    VARCHAR2(200),
        RESULT_DATE  DATE,
        NUMERIC_VALUE NUMBER(10,4),
        STRING_VALUE VARCHAR2(200),
        UNITS        VARCHAR2(50),
        STATUS       VARCHAR2(20)
    )""",
    """CREATE TABLE SMA_CQL_MEASURES (
        MEASURE_ID    VARCHAR2(20)  NOT NULL,
        VERSION       VARCHAR2(20)  NOT NULL,
        CQL_TEXT      CLOB,
        STATUS        CHAR(1)       DEFAULT 'A',
        CREATED_DATE  DATE          DEFAULT SYSDATE,
        MODIFIED_DATE DATE          DEFAULT SYSDATE,
        PRIMARY KEY (MEASURE_ID, VERSION)
    )""",
    """CREATE TABLE SMA_CQL_VALUESETS (
        VALUESET_NAME VARCHAR2(200) NOT NULL,
        CODE          VARCHAR2(50)  NOT NULL,
        CODE_SYSTEM   VARCHAR2(20)  NOT NULL,
        DISPLAY       VARCHAR2(500),
        VERSION       VARCHAR2(20)  DEFAULT 'HEDIS-2026',
        PRIMARY KEY (VALUESET_NAME, CODE, CODE_SYSTEM, VERSION)
    )""",
    """CREATE TABLE SMA_CQL_RESULTS (
        PATIENT_ID      VARCHAR2(30)  NOT NULL,
        MEASURE_ID      VARCHAR2(20)  NOT NULL,
        INITIAL_POP     CHAR(1),
        DENOMINATOR     CHAR(1),
        NUMERATOR       CHAR(1),
        DENOM_EXCLUSION CHAR(1),
        FAILURE_DATE    DATE,
        NUMERIC_RESULT  NUMBER(10,4),
        JOB_ID          VARCHAR2(50),
        PROCESS_DATE    DATE,
        PRIMARY KEY (PATIENT_ID, MEASURE_ID)
    )""",
    """CREATE TABLE SMA_CQL_RESULTS_STG (
        PARTITION_ID    NUMBER(10),
        PATIENT_ID      VARCHAR2(30),
        MEASURE_ID      VARCHAR2(20),
        INITIAL_POP     CHAR(1),
        DENOMINATOR     CHAR(1),
        NUMERATOR       CHAR(1),
        DENOM_EXCLUSION CHAR(1),
        DENOM_EXCEPTION CHAR(1),
        FAILURE_DATE    DATE,
        NUMERIC_RESULT  NUMBER(10,4),
        PROCESS_DATE    DATE
    )""",
    """CREATE TABLE SMA_CQL_MEASURE_RATES (
        JOB_ID             VARCHAR2(50)  NOT NULL,
        MEASURE_ID         VARCHAR2(20)  NOT NULL,
        INITIAL_POP_COUNT  NUMBER(10),
        DENOM_COUNT        NUMBER(10),
        NUMER_COUNT        NUMBER(10),
        EXCLUSION_COUNT    NUMBER(10),
        RATE               NUMBER(5,2),
        PROCESS_DATE       DATE,
        PRIMARY KEY (JOB_ID, MEASURE_ID)
    )""",
]

# Columns that hold dates — used to parse CSV strings to Python date objects
DATE_COLUMNS = {
    'BIRTH_DATE', 'START_DATE', 'END_DATE', 'SERVICE_DATE',
    'RESULT_DATE', 'CREATED_DATE', 'MODIFIED_DATE',
    'FAILURE_DATE', 'PROCESS_DATE',
}

def parse_date(val):
    if not val:
        return None
    for fmt in ('%Y-%m-%d %H:%M:%S', '%Y-%m-%d'):
        try:
            return datetime.datetime.strptime(val, fmt)
        except ValueError:
            continue
    return None

def coerce_row(cols, row):
    result = []
    for col, val in zip(cols, row):
        if val == '':
            result.append(None)
        elif col in DATE_COLUMNS:
            result.append(parse_date(val))
        else:
            result.append(val)
    return result

# --- Connect ---
db_password    = os.environ.get('CQL_LOCAL_PASSWORD') or getpass.getpass('CQL_MOCKED_LOCAL password: ')
wallet_password = os.environ.get('HEDIS_WALLET_PASSWORD') or getpass.getpass('Wallet password: ')

print('Connecting as CQL_MOCKED_LOCAL...')
conn = oracledb.connect(
    user='CQL_MOCKED_LOCAL',
    password=db_password,
    dsn='hediscql_tp',
    config_dir=WALLET_DIR,
    wallet_location=WALLET_DIR,
    wallet_password=wallet_password,
)
print(f'Connected to Oracle {conn.version}\n')
cur = conn.cursor()

# --- Create tables ---
print('=== Creating tables ===')
for ddl in TABLES_DDL:
    name = [l.strip() for l in ddl.splitlines() if l.strip()][0]
    try:
        cur.execute(ddl)
        print(f'  CREATED: {name}')
    except oracledb.DatabaseError as e:
        if 'ORA-00955' in str(e):
            print(f'  EXISTS:  {name}')
        else:
            print(f'  ERROR:   {name}: {e}')
conn.commit()

# --- Import CSVs ---
print('\n=== Importing data ===')
for csv_file in sorted(os.listdir(DATA_DIR)):
    if not csv_file.endswith('.csv'):
        continue
    table = csv_file[:-4]
    path  = os.path.join(DATA_DIR, csv_file)

    with open(path, newline='') as f:
        reader = csv.reader(f)
        cols   = next(reader)
        rows   = [coerce_row(cols, r) for r in reader]

    if not rows:
        print(f'  {table}: 0 rows (skip)')
        continue

    placeholders = ','.join(f':{i+1}' for i in range(len(cols)))
    sql = f"INSERT INTO {table} ({','.join(cols)}) VALUES ({placeholders})"

    try:
        cur.executemany(sql, rows)
        conn.commit()
        print(f'  {table}: {len(rows)} rows imported')
    except oracledb.DatabaseError as e:
        conn.rollback()
        print(f'  {table}: ERROR — {e}')

conn.close()
print('\nDone.')

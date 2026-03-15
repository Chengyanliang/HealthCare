#!/usr/bin/env python3
"""Create HEDIS CQL schema tables and load initial data."""
import oracledb
import os
import getpass
import json
import sys

WALLET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'wallet')
SCHEMA_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'schema', 'create_tables.sql')
VALUESETS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'valuesets', 'hedis_2026_valuesets.json')
MEASURES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'measures')

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

# --- Step 1: Create tables ---
print('\n=== Creating schema tables ===')

# Parse SQL file, skip comments and commented-out blocks
with open(SCHEMA_FILE) as f:
    sql_text = f.read()

# Remove block comments /* ... */
import re
sql_text = re.sub(r'/\*.*?\*/', '', sql_text, flags=re.DOTALL)

# Split into statements
statements = [s.strip() for s in sql_text.split(';') if s.strip()]

for stmt in statements:
    # Skip empty or comment-only
    lines = [l for l in stmt.split('\n') if l.strip() and not l.strip().startswith('--')]
    if not lines:
        continue
    clean = '\n'.join(lines)
    # Extract table/index name for display
    name = clean.split('(')[0].strip().split('\n')[0] if '(' in clean else clean[:60]
    try:
        cursor.execute(clean)
        print(f'  OK: {name}')
    except oracledb.DatabaseError as e:
        err = str(e)
        if 'ORA-00955' in err:  # name already used
            print(f'  EXISTS: {name}')
        else:
            print(f'  ERROR: {name}: {e}')

conn.commit()

# --- Step 2: Load value sets ---
print('\n=== Loading value sets ===')

with open(VALUESETS_FILE) as f:
    valuesets = json.load(f)

insert_sql = """
    MERGE INTO SMA_CQL_VALUESETS t
    USING (SELECT :1 VALUESET_NAME, :2 CODE, :3 CODE_SYSTEM, :4 DISPLAY, :5 VERSION FROM DUAL) s
    ON (t.VALUESET_NAME = s.VALUESET_NAME AND t.CODE = s.CODE
        AND t.CODE_SYSTEM = s.CODE_SYSTEM AND t.VERSION = s.VERSION)
    WHEN NOT MATCHED THEN
        INSERT (VALUESET_NAME, CODE, CODE_SYSTEM, DISPLAY, VERSION)
        VALUES (s.VALUESET_NAME, s.CODE, s.CODE_SYSTEM, s.DISPLAY, s.VERSION)
"""

total_codes = 0
for vs_name, codes in valuesets.items():
    rows = [(vs_name, c['code'], c['system'], c.get('display', ''), 'HEDIS-2026') for c in codes]
    cursor.executemany(insert_sql, rows)
    total_codes += len(rows)
    print(f'  {vs_name}: {len(rows)} codes')

conn.commit()
print(f'  Total: {total_codes} value set entries loaded')

# --- Step 3: Load CQL measures ---
print('\n=== Loading CQL measures ===')

measure_sql = """
    MERGE INTO SMA_CQL_MEASURES t
    USING (SELECT :1 MEASURE_ID, :2 VERSION, :3 CQL_TEXT FROM DUAL) s
    ON (t.MEASURE_ID = s.MEASURE_ID AND t.VERSION = s.VERSION)
    WHEN MATCHED THEN
        UPDATE SET CQL_TEXT = s.CQL_TEXT, MODIFIED_DATE = SYSDATE
    WHEN NOT MATCHED THEN
        INSERT (MEASURE_ID, VERSION, CQL_TEXT, STATUS)
        VALUES (s.MEASURE_ID, s.VERSION, s.CQL_TEXT, 'A')
"""

for cql_file in sorted(os.listdir(MEASURES_DIR)):
    if not cql_file.endswith('.cql'):
        continue
    measure_id = cql_file.split('_')[0]
    filepath = os.path.join(MEASURES_DIR, cql_file)
    with open(filepath) as f:
        cql_text = f.read()
    cursor.execute(measure_sql, [measure_id, 'HEDIS-2026', cql_text])
    print(f'  {measure_id}: {cql_file} ({len(cql_text)} bytes)')

conn.commit()

# --- Step 4: Verify ---
print('\n=== Verification ===')

cursor.execute('SELECT COUNT(*) FROM SMA_CQL_MEASURES')
print(f'  Measures: {cursor.fetchone()[0]}')

cursor.execute('SELECT COUNT(*) FROM SMA_CQL_VALUESETS')
print(f'  Value set entries: {cursor.fetchone()[0]}')

cursor.execute('SELECT COUNT(DISTINCT VALUESET_NAME) FROM SMA_CQL_VALUESETS')
print(f'  Value sets: {cursor.fetchone()[0]}')

cursor.execute("SELECT TABLE_NAME FROM USER_TABLES WHERE TABLE_NAME LIKE 'SMA_CQL%' ORDER BY TABLE_NAME")
print(f'  Tables: {", ".join(r[0] for r in cursor.fetchall())}')

conn.close()
print('\nSchema setup complete.')

#!/usr/bin/env python3
"""Export all HEDIS Oracle tables to CSV files in oracl_data/."""
import csv
import getpass
import os
import sys
import oracledb

WALLET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'wallet')
OUT_DIR    = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'oracl_data')
os.makedirs(OUT_DIR, exist_ok=True)

TABLES = [
    'SMA_MEMBER_MASTER',
    'SMA_COVERAGE',
    'SMA_CLAIMS',
    'SMA_DIAGNOSIS',
    'SMA_PROCEDURE',
    'SMA_LAB_RESULTS',
    'SMA_CQL_MEASURES',
    'SMA_CQL_VALUESETS',
    'SMA_CQL_RESULTS',
    'SMA_CQL_RESULTS_STG',
    'SMA_CQL_MEASURE_RATES',
]

db_password = os.environ.get('HEDIS_DB_PASSWORD') or getpass.getpass('ADMIN password: ')
wallet_password = os.environ.get('HEDIS_WALLET_PASSWORD') or getpass.getpass('Wallet password: ')

print('Connecting...')
conn = oracledb.connect(
    user='ADMIN',
    password=db_password,
    dsn='hediscql_tp',
    config_dir=WALLET_DIR,
    wallet_location=WALLET_DIR,
    wallet_password=wallet_password,
)
print(f'Connected to Oracle {conn.version}\n')

cursor = conn.cursor()
for table in TABLES:
    try:
        cursor.execute(f'SELECT * FROM {table}')
        rows = cursor.fetchall()
        cols = [d[0] for d in cursor.description]
        out_path = os.path.join(OUT_DIR, f'{table}.csv')
        with open(out_path, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(cols)
            writer.writerows(rows)
        print(f'  {table}: {len(rows)} rows → {table}.csv')
    except oracledb.Error as e:
        print(f'  {table}: SKIP ({e})')

conn.close()
print(f'\nDone. Files saved to {OUT_DIR}/')

#!/usr/bin/env python3
"""Test Oracle Autonomous Database connection."""
import oracledb
import os
import getpass
import sys

WALLET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'wallet')

db_password = os.environ.get('HEDIS_DB_PASSWORD')
if not db_password:
    db_password = getpass.getpass('ADMIN password: ')

wallet_password = os.environ.get('HEDIS_WALLET_PASSWORD')
if not wallet_password:
    wallet_password = getpass.getpass('Wallet password (set when downloading wallet): ')

print(f'Wallet dir: {WALLET_DIR}')
print('Connecting (10s timeout)...')
sys.stdout.flush()

try:
    conn = oracledb.connect(
        user='ADMIN',
        password=db_password,
        dsn='hediscql_tp',
        config_dir=WALLET_DIR,
        wallet_location=WALLET_DIR,
        wallet_password=wallet_password,
        tcp_connect_timeout=10,
        retry_count=1
    )
    print(f'Connected! Version: {conn.version}')

    cursor = conn.cursor()
    cursor.execute('SELECT SYSDATE FROM DUAL')
    print(f'Server time: {cursor.fetchone()[0]}')
    conn.close()
    print('Connection test passed.')
except oracledb.Error as e:
    print(f'Oracle error: {e}')
except Exception as e:
    print(f'Error: {type(e).__name__}: {e}')

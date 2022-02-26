# Requires
# pip3 install -U tabulate --no-cache-dir --no-deps --force-reinstall
# pip3 install -U plyvel --no-cache-dir --no-deps --force-reinstall

import plyvel
from tabulate import tabulate
import sys
import argparse


parser = argparse.ArgumentParser(description='Print LevelDB entire database - Key | Value')
parser.add_argument('string', metavar='P', type=str,
                    help='directory path to database file - .ldb')

args = parser.parse_args()

db = plyvel.DB('/home/branco/levelDB/' + sys.argv[1], create_if_missing=False) 
lis = []
for key, value in db:
    # Decode b'key' -> key
    key = key.decode("utf-8") 
    value = value.decode("utf-8")
    #print(f"{key} : {value}") 
    lis.append([key, value])

print(tabulate(lis, headers=['Key', 'Value']))
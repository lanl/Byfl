from __future__ import print_function
import mysql.connector
from mysql.connector import errorcode
DB_NAME = 'byfl3'
TABLES = {}

# run table
TABLES['runs'] = (
    "CREATE TABLE `runs` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `datetime` datetime NOT NULL,"
    "  `name` varchar(1028) NOT NULL," 
    "  `run_no` bigint(20) UNSIGNED NOT NULL,"
    "  `output_id` varchar(64) NOT NULL," 
    "  `bf_options` varchar(1028) NOT NULL," 
    "  PRIMARY KEY (`sec`, `usec`)"
    ") ENGINE=InnoDB")


# -bf-types 
TABLES['loadstores'] = (
    "CREATE TABLE `loadstores` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `lsid` int(10) NOT NULL," # unique id for this summary info
    "  `tally` bigint(20) UNSIGNED NOT NULL," # tally
    "  `memop` bit(1) NOT NULL," # 0=load, 1=store
    "  `memref` bit(1) NOT NULL," # 0=notpointer, 1=pointer
    "  `memagg` bit(1) NOT NULL," # 0=notvector, 1=vector
    "  `memsize` tinyint(1) NOT NULL," # 0=8, 1=16, 2=32, 3=64, 4=128, 5=other
    "  `memtype` tinyint(1) NOT NULL," # 0=int, 1=fp, 2=other
    "  PRIMARY KEY (`sec`, `usec`, `lsid`)"
    ") ENGINE=InnoDB")

# -bf-every-bb and -bf-merge-bb
TABLES['basicblocks'] = (
    "CREATE TABLE `basicblocks` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `bbid`  bigint(20) unsigned NOT NULL,"
    "  `num_merged` bigint(20) unsigned NOT NULL,"
    "  `LD_bytes` bigint(20) unsigned NOT NULL,"
    "  `ST_bytes` bigint(20) unsigned NOT NULL,"
    "  `LD_ops` bigint(20) unsigned NOT NULL,"
    "  `ST_ops` bigint(20) unsigned NOT NULL,"
    "  `Flops` bigint(20) unsigned NOT NULL,"
    "  `FP_bits` bigint(20) unsigned NOT NULL,"
    "  `Int_ops` bigint(20) unsigned NOT NULL,"
    "  `Int_op_bits` bigint(20) unsigned NOT NULL,"
    "  PRIMARY KEY (`sec`, `usec`, `bbid`)"
    ") ENGINE=InnoDB")

# -bf-types and -bf-inst-mix
TABLES['instmix'] = (
    "CREATE TABLE `instmix` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `inst_type` varchar (25) NOT NULL,"
    "  `tally` bigint(20) UNSIGNED NOT NULL,"
    "  PRIMARY KEY (`sec`, `usec`, `inst_type`)"
    ") ENGINE=InnoDB")

# -bf-vectors 
TABLES['vectorops'] = (
    "CREATE TABLE `vectorops` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `vectid` bigint(20) NOT NULL,"
    "  `Elements` int(11) NOT NULL,"
    "  `Elt_bits` int(11) NOT NULL,"
    "  `IsFlop` tinyint(11) NOT NULL,"
    "  `Tally` bigint(20) NOT NULL,"
    "  `Function` varchar(128) NOT NULL,"
    "  PRIMARY KEY (`sec`, `usec`, `vectid`)"
    ") ENGINE=InnoDB")

# -bf-by-func
TABLES['functions'] = (
    "  CREATE TABLE `functions` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `stackid` bigint(20) NOT NULL,"
    "  `LD_bytes` bigint(20) NOT NULL,"
    "  `ST_bytes` bigint(20) NOT NULL,"
    "  `LD_ops` bigint(20) NOT NULL,"
    "  `ST_ops` bigint(20) NOT NULL,"
    "  `Flops` bigint(20) NOT NULL,"
    "  `FP_bits` bigint(20) NOT NULL,"
    "  `Int_ops` bigint(20) NOT NULL,"
    "  `Int_op_bits` bigint(20) NOT NULL,"
    "  `Uniq_bytes` bigint(20) NOT NULL,"
    "  `Cond_brs` bigint(20) NOT NULL,"
    "  `Invocations` bigint(20) NOT NULL,"
    "  `Function` varchar(128) NOT NULL,"
    "  `Parent_func1` varchar(128) NOT NULL,"
    "  `Parent_func2` varchar(128) NOT NULL,"
    "  `Parent_func3` varchar(128) NOT NULL,"
    "  `Parent_func4` varchar(128) NOT NULL,"
    "  `Parent_func5` varchar(128) NOT NULL,"
    "  `Parent_func6` varchar(128) NOT NULL,"
    "  `Parent_func7` varchar(128) NOT NULL,"
    "  `Parent_func8` varchar(128) NOT NULL,"
    "  `Parent_func9` varchar(128) NOT NULL,"
    "  `Parent_func10` varchar(128) NOT NULL,"
    "  `Parent_func11` varchar(128) NOT NULL,"
    "  PRIMARY KEY (`sec`, `usec`, `stackid`)"
    ") ENGINE=InnoDB")

# -bf-by-func 
TABLES['callee'] = (
    "  CREATE TABLE `callee` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `Invocations` bigint(20) NOT NULL,"
    "  `Byfl`tinyint(1) NOT NULL, "
    "  `Function` varchar(128) NOT NULL,"
    "  PRIMARY KEY (`sec`,`usec`,`function`)"
    ") ENGINE=InnoDB")

#  derived measurements
TABLES['derived'] = (
    "  CREATE TABLE `derived` ("
    "  `sec` bigint(20) UNSIGNED NOT NULL,"
    "  `usec` bigint(6) UNSIGNED NOT NULL,"
    "  `bytes_loaded_per_byte_stored` DOUBLE(20,4) NOT NULL," 
    "  `ops_per_load_instr` DOUBLE(20,4) NOT NULL," 
    "  `bits_loaded_stored_per_memory_op` DOUBLE(20,4) NOT NULL,"
    "  `flops_per_conditional_indirect_branch` DOUBLE(20,4) NOT NULL," 
    "  `ops_per_conditional_indirect_branch` DOUBLE(20,4) NOT NULL,"
    "  `vector_ops_per_conditional_indirect_branch` DOUBLE(20,4) NOT NULL," 
    "  `vector_ops_per_flop` DOUBLE(20,4) NOT NULL," 
    "  `vector_ops_per_op` DOUBLE(20,4) NOT NULL," 
    "  `ops_per_instruction` DOUBLE(20,4) NOT NULL," 
    "  `bytes_per_flop` DOUBLE(20,4) NOT NULL," 
    "  `bits_per_flop_bit` DOUBLE(20,4) NOT NULL," 
    "  `bytes_per_op` DOUBLE(20,4) NOT NULL," 
    "  `bits_per_nonmemory_op_bit` DOUBLE(20,4) NOT NULL," 
    "  `unique_bytes_per_flop` DOUBLE(20,4) NOT NULL," 
    "  `unique_bits_per_flop_bit` DOUBLE(20,4) NOT NULL," 
    "  `unique_bytes_per_op` DOUBLE(20,4) NOT NULL," 
    "  `unique_bits_per_nonmemory_op_bit` DOUBLE(20,4) NOT NULL," 
    "  `bytes_per_unique_byte` DOUBLE(20,4) NOT NULL," 
    "  PRIMARY KEY (`sec`,`usec`)"
    ") ENGINE=InnoDB")
 
"""
The preceding code shows how we are storing the CREATE statements in a Python dictionary called TABLES. We also define the database in a global variable called DB_NAME, which enables you to easily use a different schema.
"""

# Don't forget the localhost argument or it won't work

cnx = mysql.connector.connect(user='cahrens', host='localhost')
cursor = cnx.cursor()

"""
A single MySQL server can manage multiple databases. Typically, you specify the database to switch to when connecting to the MySQL server. This example does not connect to the database upon connection, so that it can make sure the database exists, and create it if not:

"""

def create_database(cursor):
    try:
        cursor.execute(
            "CREATE DATABASE {} DEFAULT CHARACTER SET 'utf8'".format(DB_NAME))
    except mysql.connector.Error as err:
        print("Failed creating database: {}".format(err))
        exit(1)
try:
    cnx.database = DB_NAME    
except mysql.connector.Error as err:
    if err.errno == errorcode.ER_BAD_DB_ERROR:
        create_database(cursor)
        cnx.database = DB_NAME
    else:
      print(err)
      exit(1)

for name, ddl in TABLES.iteritems():
    try:
        print("Creating table {}: ".format(name), end='')
        cursor.execute(ddl)
    except mysql.connector.Error as err:
        if err.errno == errorcode.ER_TABLE_EXISTS_ERROR:
            print("already exists.")
        else:
            print(err)
    else:
        print("OK")

cursor.close()
cnx.close()

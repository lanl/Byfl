#include "byfl-common.h"
#include "byfl-binary.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <fstream>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;

namespace bytesflops {

int get_db_vars(char** bf_db_location, char** bf_db_name, char** bf_db_user, 
      char** bf_db_password) 
{

  *bf_db_location = getenv("BF_DB_LOCATION");
  *bf_db_name = getenv("BF_DB_NAME");
  *bf_db_user = getenv("BF_DB_USER");
  *bf_db_password = getenv("BF_DB_PASSWORD");

  if (!*bf_db_location) {
    //cout << "Cannot connect to database without location.\nPlease set BF_DB_LOCATION environment variable." << endl;
    return 0;
  }

  if (!*bf_db_name) {
    cout << "Cannot connect to database without db name.\nPlease set BF_DB_NAME environment variable." << endl;
    return 0;
  }
 
  if (!*bf_db_user) {
    cout << "Cannot connect to database without db user name.\nPlease set BF_DB_USER environment variable." << endl;
    return 0;
  }

  if (!*bf_db_password) {
    cout << "No database password set in BF_DB_PASSWORD environment variable.\nAssuming there is no password required." << endl;
    return 1;
  }

  return 1;
} 

sql::Driver *driver;
sql::Connection *con;
sql::Statement *stmt;
stringstream sqlstr;

int connect_database(char* lvalue, char* db, char* user, char* pvalue) 
{


  cout << endl;
  cout << "connecting to database...";

  try {

    /* Create a connection */
    driver = get_driver_instance();

    con = driver->connect(lvalue?lvalue:"localhost", user, pvalue?pvalue:"");

    /* Connect to the MySQL test database */
    con->setSchema(db);
    stmt = con->createStatement();
    stmt->executeQuery("SELECT 'Connected!' AS _message");
    cout << "connected!\n";

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line " 
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
    return 0;
  }

  cout << endl;
  return 1;

}

void insert_loadstores(uint64_t sec, uint64_t usec, uint64_t lsid, uint64_t tally,
   short memop, short memref, short memagg, short memsize, short memtype)
{
  try {

  sqlstr.str("");
  sqlstr << "INSERT INTO loadstores(sec,usec,lsid,tally,memop,memref,memagg,memsize,memtype)";
  sqlstr << " VALUES (";
  sqlstr << sec << ", " << usec << ", "
    << lsid << ", " 
    << tally << ", " << memop << ", "
    << memref << ", " << memagg << ", "
    << memsize << ", " << memtype << ")";
  stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

}

void insert_basicblocks(uint64_t sec, uint64_t usec, uint64_t bbid, uint64_t num_merged,
    uint64_t LD_bytes, uint64_t ST_bytes, uint64_t LD_ops, uint64_t ST_ops, 
    uint64_t Flops, uint64_t FP_bits, uint64_t Int_ops, uint64_t Int_op_bits) 
{
  try {
 
  sqlstr.str("");
  sqlstr << "INSERT INTO basicblocks(sec,usec,bbid,num_merged,LD_bytes,ST_Bytes,LD_ops,ST_ops,Flops,FP_bits,Int_ops,Int_op_bits)";
  sqlstr << " VALUES (";
  sqlstr << sec << ", " << usec << ", " 
    << bbid << ", " 
    << num_merged << ", " << LD_bytes << ", " 
    << ST_bytes << ", " << LD_ops << ", " 
    << ST_ops << ", " << Flops << ", " 
    << FP_bits << ", " << Int_ops << ", " 
    << Int_op_bits << ")";
  stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

}


void insert_instmix(uint64_t sec, uint64_t usec, const char* inst_type, uint64_t tally)
{
  try {

  sqlstr.str("");
  sqlstr << "INSERT INTO instmix (sec,usec,inst_type,tally)";
  sqlstr << " VALUES (";
  sqlstr << sec << ", " 
    << usec << ", " 
    << "'" << inst_type << "'" << ", " 
    << tally << ")";
  //cout << sqlstr.str();
  stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

}


void insert_vectorops(uint64_t sec, uint64_t usec, uint64_t vectid, int Elements,
    int Elt_bits, short IsFlop, uint64_t Tally, const char* Function)
{
  try {
 
  sqlstr.str("");
  sqlstr << "INSERT INTO vectorops (sec,usec,vectid,Elements,Elt_bits,IsFlop,Tally,Function)";
  sqlstr << " VALUES (";
  sqlstr << sec << ", "  << usec << ", "
    << vectid << ", " 
    << Elements << ", "  << Elt_bits << ", "
    << IsFlop << ", "  << Tally << ", '"
    << Function << "')";
  stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

}

void insert_functions(bf_functions_table &bf_functions_tbl)
{
  try {
 
  sqlstr.str("");
  sqlstr << "INSERT INTO functions (sec,usec,stackid,LD_bytes,ST_Bytes,LD_ops,ST_ops,Flops,FP_bits,Int_ops,Int_op_bits,Uniq_bytes,Cond_brs,Invocations,Function,Parent_func1,Parent_func2,Parent_func3,Parent_func4,Parent_func5,Parent_func6,Parent_func7,Parent_func8,Parent_func9,Parent_func10,Parent_func11)";
  sqlstr << " VALUES (";
  sqlstr << bf_functions_tbl.sec << ", " << bf_functions_tbl.usec << ", "  
    << bf_functions_tbl.stackid << ", " 
    << bf_functions_tbl.LD_bytes << ", " 
    << bf_functions_tbl.ST_bytes << ", " << bf_functions_tbl.LD_ops << ", " 
    << bf_functions_tbl.ST_ops << ", " << bf_functions_tbl.Flops << ", " 
    << bf_functions_tbl.FP_bits << ", " << bf_functions_tbl.Int_ops << ", " 
    << bf_functions_tbl.Int_op_bits <<  ", " << bf_functions_tbl.Uniq_bytes << ", "
    << bf_functions_tbl.Cond_brs <<  ", " << bf_functions_tbl.Invocations << ", '"
    << bf_functions_tbl.Function << "', '"
    << bf_functions_tbl.Parent_func1 << "', '" << bf_functions_tbl.Parent_func2 << "', '"
    << bf_functions_tbl.Parent_func3 << "', '" << bf_functions_tbl.Parent_func4 << "', '"
    << bf_functions_tbl.Parent_func5 << "', '" << bf_functions_tbl.Parent_func6 << "', '"
    << bf_functions_tbl.Parent_func7 << "', '" << bf_functions_tbl.Parent_func8 << "', '"
    << bf_functions_tbl.Parent_func9 << "', '" << bf_functions_tbl.Parent_func10 << "', '"
    << bf_functions_tbl.Parent_func11 << "')";
  stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

}


void insert_callee(uint64_t sec, uint64_t usec, uint64_t Invocations, short Byfl,
    const char* Function)
{
  try {

    sqlstr.str("");
    sqlstr << "INSERT INTO callee (sec,usec,Invocations,Byfl,Function)";
    sqlstr << " VALUES (";
    sqlstr << sec << ", "  
      << usec << ", " 
      << Invocations << ", "
      << Byfl << ", '"  << Function << "')";
    stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

}

void insert_runs(uint64_t sec, uint64_t usec, char* datetime, 
    string name, int64_t run_no, string output_id, const char* bf_options)
{

  try {
    sqlstr.str("");
    sqlstr << "INSERT INTO runs (sec, usec, datetime, name, run_no, output_id, bf_options)";
    sqlstr << " VALUES (";
    sqlstr 
      << sec << ", "  
      << usec << ", '"
      << datetime << "', '"  
      << name << "', "  
      << run_no << ", '"  
      << output_id << "', '"  
      << bf_options << "');";
    //cout << sqlstr.str();
    stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }
}

#ifdef CMA
void insert_derived_measurements(
  bytes_loaded_per_byte_stored,
  ops_per_load_instr,
  bits_loaded_stored_per_memory_op,
  flops_per_conditional_indirect_branch,
  ops_per_conditional_indirect_branch,
  vector_ops_per_conditional_indirect_branch,
  vector_ops_per_flop,
  vector_ops_per_op,
  ops_per_instruction,
  bytes_per_flop,
  bits_per_flop_bit,
  bytes_per_op,
  bits_per_nonmemory_op_bit,
  unique_bytes_per_flop,
  unique_bits_per_flop_bit,
  unique_bytes_per_op,
  unique_bits_per_nonmemory_op_bit,
  bytes_per_unique_byte
  // we can add more here
){}
#endif 
   

void insert_derived(uint64_t sec, uint64_t usec, const derived_measurements& dm) 
{
  try {

#ifdef CMA
    // To do the derived measurements in SQL, here's one for bytes_loaded_per_byte_stored
    // Lots of work, so just passing the calculated values from byfl for now.
    sqlstr.str << "(SELECT ";
    sqlstr.str << "(SELECT sum(tally) FROM loadstores where ((memop=0) and (sec=");
    sqlstr.str << sec << ") and (usec =" << usec << ")))";
    sqlstr.str << " / ";
    sqlstr.str << "(SELECT sum(tally) FROM loadstores where ((memop=1) and (sec=");
    sqlstr.str << sec << ") and (usec =" << usec << ")))";
    sqlstr.str << "FROM DUAL),";
    sql::ResultSet *res;
#endif

    sqlstr.str("");

    sqlstr << "INSERT INTO derived (sec, usec, \
      bytes_loaded_per_byte_stored, \
      ops_per_load_instr, \
      bits_loaded_stored_per_memory_op, \
      flops_per_conditional_indirect_branch, \
      ops_per_conditional_indirect_branch, \
      vector_ops_per_conditional_indirect_branch, \
      vector_ops_per_flop, \
      vector_ops_per_op, \
      ops_per_instruction, \
      bytes_per_flop, \
      bits_per_flop_bit, \
      bytes_per_op, \
      bits_per_nonmemory_op_bit, \
      unique_bytes_per_flop, \
      unique_bits_per_flop_bit, \
      unique_bytes_per_op, \
      unique_bits_per_nonmemory_op_bit, \
      bytes_per_unique_byte)";

    sqlstr << " VALUES (";
 
    sqlstr << sec << ", " << usec << ", " 
      << dm.bytes_loaded_per_byte_stored << ", "
      << dm.ops_per_load_instr << ", "
      << dm.bits_loaded_stored_per_memory_op << ", "
      << dm.flops_per_conditional_indirect_branch << ", "
      << dm.ops_per_conditional_indirect_branch << ", "
      << dm.vector_ops_per_conditional_indirect_branch << ", "
      << dm.vector_ops_per_flop << ", "
      << dm.vector_ops_per_op << ", "
      << dm.ops_per_instruction << ", "
      << dm.bytes_per_flop << ", "
      << dm.bits_per_flop_bit << ", "
      << dm.bytes_per_op << ", "
      << dm.bits_per_nonmemory_op_bit << ", "
      << dm.unique_bytes_per_flop << ", "
      << dm.unique_bits_per_flop_bit << ", "
      << dm.unique_bytes_per_op << ", "
      << dm.unique_bits_per_nonmemory_op_bit << ", "
      << dm.bytes_per_unique_byte << ")";

    cout << sqlstr.str();
    stmt->execute(sqlstr.str());

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line "
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }
}



// print out everything stored in the database so far
void queryDatabase() 
{

  try {
    sql::ResultSet *res;

    // loadstores table 
    cout << "SELECT from loadstores:" << endl << endl;
    res = stmt->executeQuery("SELECT * FROM loadstores ORDER BY lsid;");

    while (res->next()) {
      /* Access column data by numeric offset, 1 is the first column */
      cout << res->getString(1) << " " << res->getString(2) << " " << res->getString(3) << " ";
      cout << res->getString(4) << " " << res->getString(5) << " " << res->getString(6) << " ";
      cout << res->getString(7) << " " << res->getString(8) << " " << res->getString(9) << endl;
    }

    // basicblocks table
    cout << "SELECT from basicblocks:" << endl << endl;
    res = stmt->executeQuery("SELECT * FROM basicblocks ORDER BY bbid;");

    while (res->next()) {
      /* Access column data by numeric offset, 1 is the first column */
      cout << res->getString(1) << " " << res->getString(2) << " " << res->getString(3) << " ";
      cout << res->getString(4) << " " << res->getString(5) << " " << res->getString(6) << " ";
      cout << res->getString(7) << " " << res->getString(8) << " " << res->getString(9) << " ";
      cout << res->getString(10) << " " << res->getString(11) << " " << res->getString(12) << endl;
    }

    // instmix table 
    cout << "SELECT from instmix:" << endl << endl;
    res = stmt->executeQuery("SELECT * FROM instmix ORDER BY inst_type;");

    while (res->next()) {
      /* Access column data by numeric offset, 1 is the first column */
      cout << res->getString(1) << " " << res->getString(2) << " " << res->getString(3) 
      <<  " " << res->getString(4) << endl;
    }

    // vectorops table 
    cout << "SELECT from vectorops:" << endl << endl;
    res = stmt->executeQuery("SELECT * FROM vectorops ORDER BY vectid;");

    while (res->next()) {
      /* Access column data by numeric offset, 1 is the first column */
      cout << res->getString(1) << " " << res->getString(2) << " " << res->getString(3) << " ";
      cout << res->getString(4) << " " << res->getString(5) << " " << res->getString(6) << " ";
      cout << res->getString(7) << " " << res->getString(8) << endl;
    }


    // functions table
    cout << "SELECT from functions:" << endl << endl;
    res = stmt->executeQuery("SELECT * FROM functions ORDER BY stackid;");

    while (res->next()) {
      /* Access column data by numeric offset, 1 is the first column */
      cout << res->getString(1) << " " << res->getString(2) << " " << res->getString(3) << " ";
      cout << res->getString(4) << " " << res->getString(5) << " " << res->getString(6) << " ";
      cout << res->getString(7) << " " << res->getString(8) << " " << res->getString(9) << " ";
      cout << res->getString(10) << " " << res->getString(11) << " " << res->getString(12) << " ";
      cout << res->getString(13) << " " << res->getString(14) << " " << res->getString(15) << " ";
      cout << res->getString(16) << " " << res->getString(17) << " " << res->getString(18) << " ";
      cout << res->getString(19) << " " << res->getString(20) << " " << res->getString(21) << " ";
      cout << res->getString(22) << " " << res->getString(23) << " " << res->getString(24) << " ";
      cout << res->getString(25) << " " << res->getString(26) << endl;
    }


    // callee table 
    cout << "SELECT from callee:" << endl << endl;
    res = stmt->executeQuery("SELECT * FROM callee;");

    while (res->next()) {
      /* Access column data by numeric offset, 1 is the first column */
      cout << res->getString(1) << " " << res->getString(2) << " " << res->getString(3) << " ";
      cout << res->getString(4) << " " << res->getString(5) << endl;
    }

    delete res;

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line " 
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }
}

void close_database() {
  if(stmt) delete stmt;
  if(con) delete con;
  stmt = NULL;
  con = NULL;
}

}

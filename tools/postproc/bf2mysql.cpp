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

// Take binary Byfl output as input and store in Byfl MySQL database
// Assumes mysqld is already running, although we could add that and
// make it start it up, do the import, then shut it down.
// Note: databasename, user, location and password must all be in single quotes

// bf2mysql databasename user -l location -p password -f binaryfilename 
// If no binaryfile is named, it is taken from stdout

// For example:
// bf2mysql 'byfl' 'cahrens' -l 'localhost' -f /home/cahrens/momentbyfl2/bfrun/byflout.bin

  char *lvalue = NULL; // location of database
  char *pvalue = NULL; // database password for user
  char *fvalue = NULL; // binary filename
  char *db = NULL;
  char *user = NULL;


  sql::Driver *driver;
  sql::Connection *con;
  sql::Statement *stmt;

int getArgs(int argc, char** argv) {
  int index;
  int c;

  opterr = 0;

  while ((c = getopt (argc, argv, "lpf:")) != -1)
    switch (c)
    {
      case 'l':
        lvalue = optarg;
        break;
      case 'p':
        pvalue = optarg;
        break;
      case 'f':
        fvalue = optarg;
        break;
      case '?':
        if ((optopt == 'l') || (optopt == 'p') || (optopt == 'f'))
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
              "Unknown option character `\\x%x'.\n",
              optopt);
        return 1;
      default:
        abort ();
    }

  printf ("location = %d, password = %d, binary filename = %s\n",
      lvalue, pvalue, fvalue);

  if (optind+2 != argc) {
    fprintf(stderr, "bf2mysql requires a database and user name.\n");
    return 1;
  }

  index = optind;
  db = argv[index++];
  user = argv[index++];

  for (; index < argc; index++)
    printf ("Unused argument: %s\n", argv[index]);
  return 0;
}

ifstream* bfbinin;                  // Stream from which to read binary input

void openBinaryFile() 
{
  if (fvalue) {
    bfbinin = new ifstream(fvalue, ios_base::in | ios::binary);
    if (bfbinin->fail()) {
      cerr << "Failed to open binary input file " << fvalue << '\n';
      exit(1);
    }
  }
}


void connectDatabase()  {
  cout << endl;
  cout << "Connecting to database...";

  try {
    sql::ResultSet *res;

    /* Create a connection */
    driver = get_driver_instance();

    con = driver->connect(lvalue?lvalue:"localhost", user, pvalue?pvalue:"");

    /* Connect to the MySQL test database */
    con->setSchema(db);
    stmt = con->createStatement();
    res = stmt->executeQuery("SELECT 'Connected!' AS _message");
    cout << "connected!\n";

  } catch (sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line " 
      << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
  }

  cout << endl;

}

// The following need to be consistent with byfl-common.h.
const char *memop2name[] = {"loads of ", "stores of "};
const char *memref2name[] = {"", "pointers to "};
const char *memagg2name[] = {"", "vectors of "};
const char *memwidth2name[] = {"8-bit ", "16-bit ", "32-bit ",
  "64-bit ", "128-bit ", "oddly sized "};
const char *memtype2name[] = {"integers", "floating-point values",
  "\"other\" values (not integers or FP values)"};

void print_loadstores_table(bf_loadstores_table&  bft) {
  cout << "bf_loadstores record: ";
  cout << "(sec: " << bft.sec << ")";
  cout << "(usec: " << bft.usec << ")";
  cout << "(lsid: " << bft.lsid << ")";
  cout << "  " << bft.tally << " ";
  cout << memop2name[bft.memop];
  cout << memref2name[bft.memref];
  cout << memagg2name[bft.memagg];
  cout << memwidth2name[bft.memsize];
  cout << memtype2name[bft.memtype];
  cout << "\n";
}

void print_basicblocks_table(bf_basicblocks_table&  bft) {
  cout << "bf_basicblocks record: ";
  cout << "(sec: " << bft.sec << ")";
  cout << "(usec: " << bft.usec << ")";
  cout << "(bbid: " << bft.bbid << ")";
  cout << "  " << bft.num_merged ;
  cout << "  " << bft.LD_bytes ;
  cout << "  " << bft.ST_bytes ;
  cout << "  " << bft.LD_ops ;
  cout << "  " << bft.ST_ops ;
  cout << "  " << bft.Flops ;
  cout << "  " << bft.FP_bits ;
  cout << "  " << bft.Int_ops ;
  cout << "  " << bft.Int_op_bits ;
  cout << "\n";
}

void print_instmix_table(bf_instmix_table&  bft) {
  cout << "bf_instmix record: ";
  cout << "(sec: " << bft.sec << ")";
  cout << "(usec: " << bft.usec << ")";
  cout << "  " << bft.inst_type;
  cout << "  " << bft.tally;
  cout << "\n";
}

void print_vectorops_table(bf_vectorops_table&  bft) {
  cout << "bf_vectorops record: ";
  cout << "(sec: " << bft.sec << ")";
  cout << "(usec: " << bft.usec << ")";
  cout << "  " << bft.vectid;
  cout << "  " << bft.Elements;
  cout << "  " << bft.Elt_bits;
  cout << "  " << bft.IsFlop;
  cout << "  " << bft.Tally;
  cout << "  " << bft.Function ;
  cout << "\n";
}

void print_functions_table(bf_functions_table&  bft) {
  cout << "bf_functions record: ";
  cout << "(sec: " << bft.sec << ")";
  cout << "(usec: " << bft.usec << ")";
  cout << "  " << bft.stackid;
  cout << "  " << bft.LD_bytes ;
  cout << "  " << bft.ST_bytes ;
  cout << "  " << bft.LD_ops ;
  cout << "  " << bft.ST_ops ;
  cout << "  " << bft.Flops ;
  cout << "  " << bft.FP_bits ;
  cout << "  " << bft.Int_ops ;
  cout << "  " << bft.Int_op_bits ;
  cout << "  " << bft.Uniq_bytes;
  cout << "  " << bft.Cond_brs;
  cout << "  " << bft.Invocations ;
  cout << "  " << bft.Function ;
  cout << "  " << bft.Parent_func1 ;
  cout << "  " << bft.Parent_func2;
  cout << "  " << bft.Parent_func3;
  cout << "  " << bft.Parent_func4;
  cout << "  " << bft.Parent_func5;
  cout << "  " << bft.Parent_func6;
  cout << "  " << bft.Parent_func7;
  cout << "  " << bft.Parent_func8;
  cout << "  " << bft.Parent_func9;
  cout << "  " << bft.Parent_func10 ;
  cout << "  " << bft.Parent_func11 ;
  cout << "\n";
}

void print_callee_table(bf_callee_table&  bft) {
  cout << "bf_callee record: ";
  cout << "(sec: " << bft.sec << ")";
  cout << "(usec: " << bft.usec << ")";
  cout << "  " << bft.Invocations ;
  cout << "  " << bft.Byfl ;
  cout << "  " << bft.Function ;
  cout << "\n";
}

void print_runs_table(bf_runs_table& bft) {
  cout << "bf_runs record: ";
  cout << "(sec: " << bft.sec << ")";
  cout << "(usec: " << bft.usec << ")";
  cout << "(datetime: " << bft.datetime << ")";
  cout << "(name: " << bft.name << ")";
  cout << "(run_no: " << bft.run_no << ")";
  cout << "(output_id: " << bft.output_id << ")";
  cout << "(bf_options: " << bft.bf_options << ")";
  cout << "\n";
}


void populateDatabase() 
{
  stringstream sql;

  try {
    bf_table_t bf_table;
    bf_loadstores_table bf_loadstores_tbl;
    bf_basicblocks_table bf_bb_tbl;
    bf_instmix_table bf_instmix_tbl;
    bf_vectorops_table bf_vectorops_tbl;
    bf_functions_table bf_functions_tbl;
    bf_callee_table bf_callee_tbl;
    bf_runs_table bf_runs_tbl;
    bf_derived_table bf_derived_tbl;
    bfbinin->seekg(0);
    while (!bfbinin->eof()) {
      bfbinin->read((char*)&bf_table, sizeof(bf_table));
      switch(bf_table) {

        case BF_LOADSTORES:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_loadstores_tbl, sizeof(bf_loadstores_table));
            //print_loadstores_table(bf_loadstores_tbl);
            sql.str("");
            sql << "INSERT INTO loadstores(sec,usec,lsid,tally,memop,memref,memagg,memsize,memtype)";
            sql << " VALUES (";
            sql << bf_loadstores_tbl.sec << ", " << bf_loadstores_tbl.usec << ", "
              << bf_loadstores_tbl.lsid << ", " 
              << bf_loadstores_tbl.tally << ", " << bf_loadstores_tbl.memop << ", "
              << bf_loadstores_tbl.memref << ", " << bf_loadstores_tbl.memagg << ", "
              << bf_loadstores_tbl.memsize << ", " << bf_loadstores_tbl.memtype << ")";
            stmt->execute(sql.str());
          }
          break;

        case BF_BASICBLOCKS:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_bb_tbl, sizeof(bf_basicblocks_table));
            //print_basicblocks_table(bf_bb_tbl);
            sql.str("");
            sql << "INSERT INTO basicblocks(sec,usec,bbid,num_merged,LD_bytes,ST_Bytes,LD_ops,ST_ops,Flops,FP_bits,Int_ops,Int_op_bits)";
            sql << " VALUES (";
            sql << bf_bb_tbl.sec << ", " << bf_bb_tbl.usec << ", " 
              << bf_bb_tbl.bbid << ", " 
              << bf_bb_tbl.num_merged << ", " << bf_bb_tbl.LD_bytes << ", " 
              << bf_bb_tbl.ST_bytes << ", " << bf_bb_tbl.LD_ops << ", " 
              << bf_bb_tbl.ST_ops << ", " << bf_bb_tbl.Flops << ", " 
              << bf_bb_tbl.FP_bits << ", " << bf_bb_tbl.Int_ops << ", " 
              << bf_bb_tbl.Int_op_bits << ")";
            stmt->execute(sql.str());
          }
          break;

        case BF_INSTMIX:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_instmix_tbl, sizeof(bf_instmix_table));
            //print_instmix_table(bf_instmix_tbl);
            sql.str("");
            sql << "INSERT INTO instmix (sec,usec,inst_type,tally)";
            sql << " VALUES (";
            sql << bf_instmix_tbl.sec << ", " 
              << bf_instmix_tbl.usec << ", '" 
              << bf_instmix_tbl.inst_type << "', " 
              << bf_instmix_tbl.tally << ")";
            //cout << sql.str();
            stmt->execute(sql.str());
          }
          break;

        case BF_VECTOROPS:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_vectorops_tbl, sizeof(bf_vectorops_table));
            //print_vectorops_table(bf_vectorops_tbl);
            sql.str("");
            sql << "INSERT INTO vectorops (sec,usec,vectid,Elements,Elt_bits,IsFlop,Tally,Function)";
            sql << " VALUES (";
            sql << bf_vectorops_tbl.sec << ", "  << bf_vectorops_tbl.usec << ", "
              << bf_vectorops_tbl.vectid << ", " 
              << bf_vectorops_tbl.Elements << ", "  << bf_vectorops_tbl.Elt_bits << ", "
              << bf_vectorops_tbl.IsFlop << ", "  << bf_vectorops_tbl.Tally << ", '"
              << bf_vectorops_tbl.Function << "')";
            stmt->execute(sql.str());
          }
          break;

        case BF_FUNCTIONS:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_functions_tbl, sizeof(bf_functions_table));
            //print_functions_table(bf_functions_tbl);
            sql.str("");
            sql << "INSERT INTO functions (sec,usec,stackid,LD_bytes,ST_Bytes,LD_ops,ST_ops,Flops,FP_bits,Int_ops,Int_op_bits,Uniq_bytes,Cond_brs,Invocations,Function,Parent_func1,Parent_func2,Parent_func3,Parent_func4,Parent_func5,Parent_func6,Parent_func7,Parent_func8,Parent_func9,Parent_func10,Parent_func11)";
            sql << " VALUES (";
            sql << bf_functions_tbl.sec << ", " << bf_functions_tbl.usec << ", "  
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
            stmt->execute(sql.str());
          }
          break;

        case BF_CALLEE:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_callee_tbl, sizeof(bf_callee_table));
            //print_callee_table(bf_callee_tbl);
            sql.str("");
            sql << "INSERT INTO callee (sec,usec,Invocations,Byfl,Function)";
            sql << " VALUES (";
            sql << bf_callee_tbl.sec << ", "  
              << bf_callee_tbl.usec << ", " 
              << bf_callee_tbl.Invocations << ", "
              << bf_callee_tbl.Byfl << ", '"  << bf_callee_tbl.Function << "')";
            stmt->execute(sql.str());
          }
          break;

        case BF_RUNS:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_runs_tbl, sizeof(bf_runs_table));
            //print_runs_table(bf_runs_tbl);
            sql.str("");
            sql << "INSERT INTO runs (sec, usec, datetime, name, run_no,output_id, bf_options)";
            sql << " VALUES (";
            sql 
              << bf_runs_tbl.sec << ", "  
              << bf_runs_tbl.usec << ","
              << "'"  << bf_runs_tbl.datetime << "', '"  
              << bf_runs_tbl.name << "', "  
              << bf_runs_tbl.run_no << ", '"  
              << bf_runs_tbl.output_id << "', '"  
              << bf_runs_tbl.bf_options << "')";
            stmt->execute(sql.str());
          }
          break;

        case BF_DERIVED:

          if (!bfbinin->eof()) {
            bfbinin->read((char*)&bf_derived_tbl, sizeof(bf_derived_table));
            sql.str("");
            sql << "INSERT INTO derived (sec, usec, \
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

            sql << " VALUES (";
            sql 
              << bf_derived_tbl.sec << ", "  
              << bf_derived_tbl.usec << ", "  
              << bf_derived_tbl.dm.bytes_loaded_per_byte_stored << ", "  
              << bf_derived_tbl.dm.ops_per_load_instr << ", "  
              << bf_derived_tbl.dm.bits_loaded_stored_per_memory_op << ", "  
              << bf_derived_tbl.dm.flops_per_conditional_indirect_branch << ", "  
              << bf_derived_tbl.dm.ops_per_conditional_indirect_branch << ", "  
              << bf_derived_tbl.dm.vector_ops_per_conditional_indirect_branch << ", "  
              << bf_derived_tbl.dm.vector_ops_per_flop << ", "  
              << bf_derived_tbl.dm.vector_ops_per_op << ", "  
              << bf_derived_tbl.dm.ops_per_instruction << ", "  
              << bf_derived_tbl.dm.bytes_per_flop << ", "  
              << bf_derived_tbl.dm.bits_per_flop_bit << ", "  
              << bf_derived_tbl.dm.bytes_per_op << ", "  
              << bf_derived_tbl.dm.bits_per_nonmemory_op_bit << ", "  
              << bf_derived_tbl.dm.unique_bytes_per_flop << ", "  
              << bf_derived_tbl.dm.unique_bits_per_flop_bit << ", "  
              << bf_derived_tbl.dm.unique_bytes_per_op << ", "  
              << bf_derived_tbl.dm.unique_bits_per_nonmemory_op_bit << ", "  
              << bf_derived_tbl.dm.bytes_per_unique_byte << ")";
            //cout << sql.str() << endl;
            stmt->execute(sql.str());
          }
          break;

        default:
          break;
      }
    }

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

void closeDatabase() {
  if(stmt) delete stmt;
  if(con) delete con;
  stmt = NULL;
  con = NULL;
}
      
    
int main (int argc, char** argv) {

  if (getArgs(argc, argv)) {
    exit(1);
  }

  connectDatabase();
  openBinaryFile();
  populateDatabase();
  //queryDatabase();

}




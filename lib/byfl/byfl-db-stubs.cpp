#include "byfl-common.h"
#include "byfl-binary.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <fstream>

using namespace std;

namespace bytesflops {

int get_db_vars(char** bf_db_location, char** bf_db_name, char** bf_db_user, 
      char** bf_db_password) 
{ return 0;}

int connect_database(char* lvalue, char* db, char* user, char* pvalue) 
{ return 0; }

void insert_loadstores(uint64_t sec, uint64_t usec, uint64_t lsid, uint64_t tally,
   short memop, short memref, short memagg, short memsize, short memtype)
{ }

void insert_basicblocks(uint64_t sec, uint64_t usec, uint64_t bbid, uint64_t num_merged,
    uint64_t LD_bytes, uint64_t ST_bytes, uint64_t LD_ops, uint64_t ST_ops, 
    uint64_t Flops, uint64_t FP_bits, uint64_t Int_ops, uint64_t Int_op_bits) 
{ }

void insert_instmix(uint64_t sec, uint64_t usec, const char* inst_type, uint64_t tally)
{}

void insert_vectorops(uint64_t sec, uint64_t usec, uint64_t vectid, int Elements,
    int Elt_bits, short IsFlop, uint64_t Tally, const char* Function)
{}

void insert_functions(bf_functions_table &bf_functions_tbl)
{}

void insert_callee(uint64_t sec, uint64_t usec, uint64_t Invocations, short Byfl,
    const char* Function)
{}

void insert_runs(uint64_t sec, uint64_t usec, char* datetime, 
    string name, int64_t run_no, string output_id, const char* bf_options)
{}

void insert_derived(uint64_t sec, uint64_t usec, const derived_measurements& dm) 
{}

void close_database() {}

}

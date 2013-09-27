#include <iostream>
#include <fstream>
#include "byfl-common.h"
#include "byfl-binary.h"

using namespace std;

namespace bytesflops {

void binout_derived(uint64_t utc_sec, uint64_t utc_usec, ofstream* bfbinout, 
  const derived_measurements& dm) 
{
  bf_table_t table = BF_DERIVED;

  // write out the type of table entry this is
  bfbinout->write((char*)&table, sizeof(bf_table_t));

  bf_derived_table bftable = { utc_sec, utc_usec, dm};

  bfbinout->write((char*)&bftable, sizeof(bf_derived_table));
}

}


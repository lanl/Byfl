/*
 * Helper library for computing bytes:flops ratios
 * (thread-related functions)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

static pthread_mutex_t megalock = PTHREAD_MUTEX_INITIALIZER;    // Lock protecting all library data structures

namespace bytesflops {

// Initialize some of our variables at first use.
void initialize_threading (void) {
}

// Take the mega-lock.
extern "C"
void bf_acquire_mega_lock (void)
{
  if (pthread_mutex_lock(&megalock) != 0) {
    cerr << "Failed to acquire a mutex\n";
    bf_abend();
  }
}

// Release the mega-lock.
extern "C"
void bf_release_mega_lock (void)
{
  if (pthread_mutex_unlock(&megalock) != 0) {
    cerr << "Failed to release a mutex\n";
    bf_abend();
  }
}

} // namespace bytesflops

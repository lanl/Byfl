/*
 * Helper library for computing bytes:flops ratios
 * (class definitions for binary data output)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BINARYOUTPUT_H_
#define _BINARYOUTPUT_H_

#include "binarytagdefs.h"

namespace bytesflops {}
using namespace bytesflops;

namespace bytesflops {

// Wrap an output stream so as to output data in a binary format.
class BinaryOStream {
public:
  virtual void flush() { }

  virtual BinaryOStream& operator<<(const uint8_t val);
  virtual BinaryOStream& operator<<(const uint64_t val);
  virtual BinaryOStream& operator<<(const char *str);
};

// Subclass a BinaryOStream into a version that writes its output to a
// real stream.
class BinaryOStreamReal : public BinaryOStream
{
public:
  BinaryOStreamReal(std::ostream& wrapped_stream) : ostr(wrapped_stream) { }

  void flush() {
    ostr.flush();
  }

  BinaryOStreamReal& operator<<(const uint8_t val);
  BinaryOStreamReal& operator<<(const uint64_t val);
  BinaryOStreamReal& operator<<(const char *str);

private:
  std::ostream& ostr;    // Underlying output stream

  // Write an arbitrary number of bits in binary big-endian format.
  // The input value must be cast to a uint64_t before calling this
  // function.
  void write_big_endian_integer(const uint64_t val, const size_t valid_bits)
  {
    uint64_t shift = uint64_t(valid_bits - 8);
    uint64_t mask = UINT64_C(0xff) << shift;

    for (; mask > 0; shift -= 8, mask >>= 8)
      ostr << uint8_t((val&mask) >> shift);
  }

  // Write a null-terminated string to the underlying stream.
  void write_null_terminated(const char *str)
  {
    ostr << str;
  }
};

} // namespace bytesflops

#endif

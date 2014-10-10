/*
 * Helper library for computing bytes:flops ratios
 * (binary data output)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "byfl.h"
#include <cstring>

namespace bytesflops {}
using namespace bytesflops;

namespace bytesflops {

// Write an unsigned 8-bit integer in binary big-endian format.
BinaryOStreamReal& BinaryOStreamReal::operator<< (const uint8_t val)
{
  write_big_endian_integer(uint64_t(val), 8);
  return *this;
}

// Write an unsigned 64-bit integer in binary big-endian format.
BinaryOStreamReal& BinaryOStreamReal::operator<< (const uint64_t val)
{
  write_big_endian_integer(val, 64);
  return *this;
}

// Write a string (i.e., char *) as a binary big-endian length
// followed by the raw string data.
BinaryOStreamReal& BinaryOStreamReal::operator<< (const char *str)
{
  write_big_endian_integer(uint64_t(std::strlen(str)), 16);
  write_null_terminated(str);
  return *this;
}

// Write a C++ string as a binary big-endian length followed by the
// raw string data.
BinaryOStreamReal& BinaryOStreamReal::operator<< (const string& str)
{
  *this << str.c_str();
  return *this;
}

// Write a boolean value as an 8-bit integer.
BinaryOStreamReal& BinaryOStreamReal::operator<< (const bool val)
{
  write_big_endian_integer(uint64_t(val), 8);
  return *this;
}



// Discard an unsigned 8-bit integer.
#pragma GCC diagnostic ignored "-Wunused-parameter"
BinaryOStream& BinaryOStream::operator<< (const uint8_t val)
{
  return *this;
}

// Discard an unsigned 64-bit integer.
#pragma GCC diagnostic ignored "-Wunused-parameter"
BinaryOStream& BinaryOStream::operator<< (const uint64_t val)
{
  return *this;
}

// Discard a string (i.e., char *).
#pragma GCC diagnostic ignored "-Wunused-parameter"
BinaryOStream& BinaryOStream::operator<< (const char *str)
{
  return *this;
}

// Discard a C++ string.
#pragma GCC diagnostic ignored "-Wunused-parameter"
BinaryOStream& BinaryOStream::operator<< (const string& str)
{
  return *this;
}

// Discard a boolean value.
#pragma GCC diagnostic ignored "-Wunused-parameter"
BinaryOStream& BinaryOStream::operator<< (const bool val)
{
  return *this;
}

} // namespace bytesflops

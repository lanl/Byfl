/***********************************
 * Do some simple, pointless work  *
 * By Scott Pakin <pakin@lanl.gov> *
 ***********************************/

#include <iostream>
#include <cstdlib>

int main (int argc, const char *argv[])
{
  int iters = argc > 1 ? atoi(argv[1]) : 100000;
  int i;
  int sum = 0;

  for (i = 0; i < iters; i++)
    sum = sum*34564793 + i;
  std::cout << "Sum is " << sum << '\n';
  return 0;
}

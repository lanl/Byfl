/***********************************
 * Do some simple, pointless work  *
 * By Scott Pakin <pakin@lanl.gov> *
 ***********************************/

#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
  int iters = argc > 1 ? atoi(argv[1]) : 100000;
  int i;
  int sum = 0;

  for (i = 0; i < iters; i++)
    sum = sum*34564793 + i;
  printf("Sum is %d\n", sum);
  return 0;
}

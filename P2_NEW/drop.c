/*
  Simple demo of a pseudo-random "packet drop" mechanism for testing of
  CS3102 Coursework P2.

  saleem, Jan 2024 20 Feb 2023.

  compile: clang -o drop drop.c

  A higher value for p gives better approximation to d, e.g. try:

    ./drop 10 100

  several times, then:

    ./drop 10 10000
*/


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MIN_D       1
#define MAX_D      50
#define MIN_P     100
#define MAX_P 1000000

void
usage()
{
  printf("drop d p\n");
  printf("  d : drop percentage, integer, range [%d,%d] \n", MIN_D, MAX_D);
  printf("  p : number of 'packets, integer, range [%d,%d]\n", MIN_P, MAX_P);
  exit(0);
}

int
main(int argc, char *argv[])
{
  if (argc != 3) usage();

  int d, p;

  if (sscanf(argv[1], "%d", &d) != 1 || d < MIN_D || d > MAX_D)
    usage();

  if (sscanf(argv[2], "%d", &p) != 1 || p < MIN_P || p > MAX_P)
    usage();

  srandom(time((time_t *) 0) * getuid() + 42); // pseudo-random seed

  int drop = 0;
  for(int i = 0; i < p; ++i) {
    printf("%d/%d", i, p);
    if (random() % 100 < d) { // this is the line that emulates the drop
      ++drop;
      printf(" <-- drop");
    }
    printf("\n");
  }

  printf("dropped %d/%d, which is %.1f%% with a target of %d%%.\n",
         drop, p, 100.0 * (float) drop / (float) p, d);

  return 0;
}

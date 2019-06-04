/* sort.c 

   Test program to sort a large number of integers.
 
   Intention is to stress virtual memory system.
 
   Ideally, we could read the unsorted array off of the file
   system, and store the result back to the file system! */
#include <stdio.h>
//#include "threads/malloc.h"
/* Size of array to sort. */
#define SORT_SIZE 100

int asdf[400000];

int
main (void)
{

  /* Array to sort.  Static to reduce stack usage. */
 
  long long int sum = 0;
  asdf[396666]=15;
  printf("%d\n",asdf[396666]*123);
  int i, j, tmp;
  printf("RESULT : %lld",sum);
  /* First initialize the array in descending order. */
  /*for (i = 0; i < SORT_SIZE; i++)
    array[i] = SORT_SIZE - i - 1;

  Then sort in ascending order. 
  for (i = 0; i < SORT_SIZE - 1; i++)
    for (j = 0; j < SORT_SIZE - 1 - i; j++)
      if (array[j] > array[j + 1])
	{
	  tmp = array[j];
	  array[j] = array[j + 1];
	  array[j + 1] = tmp;
	}
*/
  printf ("sort exiting with code %d\n", 0);
  return 0;
}
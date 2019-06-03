/* sort.c 

   Test program to sort a large number of integers.
 
   Intention is to stress virtual memory system.
 
   Ideally, we could read the unsorted array off of the file
   system, and store the result back to the file system! */
#include <stdio.h>

/* Size of array to sort. */
#define SORT_SIZE 1000000

int a[400000];

int
main (void)
{
  /*

  int i, j, tmp;

  
  for (i = 0; i < SORT_SIZE; i++)
    array[i] = SORT_SIZE - i - 1;


  for (i = 0; i < SORT_SIZE - 1; i++)
    for (j = 0; j < SORT_SIZE - 1 - i; j++)
      if (array[j] > array[j + 1])
	{
	  tmp = array[j];
	  array[j] = array[j + 1];
	  array[j + 1] = tmp;
	}

  for(i = 0; i<SORT_SIZE-2;i++){
    printf("%d ",array[i]);
  }*/
  printf ("sort exiting with code %d\n", 0);
  return 0;
}

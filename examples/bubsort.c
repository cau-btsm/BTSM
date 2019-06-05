/* sort.c 

   Test program to sort a large number of integers.
 
   Intention is to stress virtual memory system.
 
   Ideally, we could read the unsorted array off of the file
   system, and store the result back to the file system! */
#include <stdio.h>
//#include "threads/malloc.h"
/* Size of array to sort. */
#define SORT_SIZE 370000

int asdf[SORT_SIZE];

int
main (void)
{
  int temp;
  printf("SETUP\n");
  for(int i=0;i<SORT_SIZE;i++){
    asdf[i]=SORT_SIZE-i;
  }
  
//  for(int i=0;i<SORT_SIZE;i++){
    for(int j=0;j<SORT_SIZE-1;j++){
      if(asdf[j]>asdf[j+1]){
        temp=asdf[j];
        asdf[j]=asdf[j+1];
        asdf[j+1]=temp;
      }
    }
//  }
printf("CALCULATING\n");
  /* Array to sort.  Static to reduce stack usage. */
 
  printf("RESULT : %lld",0);
  /* First initialize the rray in descending order. */

  printf ("sort exiting with code %d\n", 0);
  return 0;
}
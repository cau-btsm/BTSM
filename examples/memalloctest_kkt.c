#include <stdio.h>
#include <string.h>

#include "../threads/thread.h"
#include "../threads/malloc.h"
#include "../threads/palloc.h"

#include "memalloctest_kkt.h"

 void run_memalloc_test(char **argv UNUSED)
{
	size_t i;
	char* dynamicmem[11];

 	for (i=0; i<10; i++) {
		dynamicmem[i] = (char *) malloc (145000);
		memset (dynamicmem[i], 0x00, 145000);
	}

 	dynamicmem[10] = (char *) malloc (16000);
	memset (dynamicmem[10], 0x00, 16000);
	printf ("dump page status : \n");
	palloc_get_status (0);

 	thread_sleep (100);

 	for (i=0; i<11; i++) {
		free(dynamicmem[i]);
		printf ("dump page status : \n");
		palloc_get_status (0);
	}
}


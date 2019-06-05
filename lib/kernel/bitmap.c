#include "bitmap.h"
#include <debug.h>
#include <limits.h>
#include <round.h>
#include <stdio.h>
#include "threads/malloc.h"
#ifdef FILESYS
#include "filesys/file.h"
#endif

enum Selector{
  FIRST_FIT,
  NEXT_FIT,
  BEST_FIT,
  BUDDY_SYSTEM
};

enum Selector Select = FIRST_FIT;

/* MINJUN */
// static size_t lastAllocated = 0;

/* For buddy system,
root buddy node */
/*BuddyNode_p root_node = (BuddyNode_p)malloc(sizeof(BuddyNode));

void initRoot(void){
  root_node -> allocated = false;
  root_node -> hasChild = false;
  
  root_node -> leftChild_p = NULL;
  root_node -> rightChild_p = NULL;
  root_node -> parentNode = NULL;
  root_node -> sibling = NULL;

  startIdx = -1;
  size = 256;
}

BuddyNode_p createBuddy(size_t size){
  BuddyNode_p tempNode = (BuddyNode_p)malloc(sizeof(BuddyNode));
  tempNode->allocated = false;
  tempNode->hasChild = false;
  
  tempNode->leftChild_p = NULL;
  tempNode->rightChild_p = NULL;
  tempNode->parentNode = NULL;
  tempNode->sibling = NULL;

  tempNode->startIdx = -1;
  tempNode->size = size;
  
  return tempNode;
}

 cnt size 만큼의 page를 할당 할 수 있는지 없는지를 반환.
이것을 보고 bitmap_scan에서 처럼 BITMAP_ERROR를 반환함.
size_t updateBuddyTree(size_t cnt){
  BuddyNode_p head = root_node;
  size_t bestBuddySize = head -> size;

  if( bestBuddySize >= cnt){
    
    이 반복문을 통해,
    요구한 메모리 크기보다 큰 buddy 중에
    가장 작은 buddy size를 찾는다. 
    이 반복문을 돌고 난 후 bestBuddySize가 가장 최적의 buddy 사이즈가 된다.
    while(1){
      if(head->allocated == false){
        if( bestBuddySize/2 >= cnt ){
        
          if(head -> hasChild == true){
            bestBuddySize = bestBuddySize/2;
            head = head -> leftChild_p;

          }
          
        
          else{
            divideBuddy(head);
          }
        }

        else
        {
          break;
        }
      }
      
      // head가 allocated 되어 있을 경우.
      else{
        break;
      }
      
    }

    return BITMAP_ERROR;
  }

   root buddy size 즉, 256으로 정함.
  이것 보다 요구한 메모리 크가가 더 크면,
  page를 할당없으므로 예외 처리. 
  else
  {
    return BITMAP_ERROR;
  }
  
}

BuddyNode_p findProperBuddy(size_t cnt){
  BuddyNode_p head = root_node;
  
  while(1){
    head->size
  }
}

void divideBuddy(BuddyNode_p parent, size_t size){
  BuddyNode_p tempNode1 = createBuddy(size);
  BuddyNode_p tempNode2 = createBuddy(size);

  tempNode1 -> parentNode = parent;
  tempNode1 -> sibling = tempNode2;
  
  tempNode2 -> parentNode = parent;
  tempNode2 -> sibling = tempNode1;

  parent->hasChild = true;
  parent->leftChild_p = tempNode1;
  parent->rightChild_p = tempNode2;
}*/

/* Element type.
   This must be an unsigned integer type at least as wide as int.
   Each bit represents one bit in the bitmap.
   If bit 0 in an element represents bit K in the bitmap,
   then bit 1 in the element represents bit K+1 in the bitmap,
   and so on. */
typedef unsigned long elem_type;

/* Number of bits in an element. */
#define ELEM_BITS (sizeof (elem_type) * CHAR_BIT)

/* From the outside, a bitmap is an array of bits.  From the
   inside, it's an array of elem_type (defined above) that
   simulates an array of bits. */
struct bitmap
  {
    size_t bit_cnt;     /* Number of bits. */
    elem_type *bits;    /* Elements that represent bits. */
  };

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t
elem_idx (size_t bit_idx) 
{
  return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type
bit_mask (size_t bit_idx) 
{
  return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt (size_t bit_cnt)
{
  return DIV_ROUND_UP (bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt (size_t bit_cnt)
{
  return sizeof (elem_type) * elem_cnt (bit_cnt);
}

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
static inline elem_type
last_mask (const struct bitmap *b) 
{
  int last_bits = b->bit_cnt % ELEM_BITS;
  return last_bits ? ((elem_type) 1 << last_bits) - 1 : (elem_type) -1;
}

/* Creation and destruction. */

/* Creates and returns a pointer to a newly allocated bitmap with room for
   BIT_CNT (or more) bits.  Returns a null pointer if memory allocation fails.
   The caller is responsible for freeing the bitmap, with bitmap_destroy(),
   when it is no longer needed. */
struct bitmap *
bitmap_create (size_t bit_cnt) 
{
  struct bitmap *b = malloc (sizeof *b);
  if (b != NULL)
    {
      b->bit_cnt = bit_cnt;
      b->bits = malloc (byte_cnt (bit_cnt));
      if (b->bits != NULL || bit_cnt == 0)
        {
          bitmap_set_all (b, false);
          return b;
        }
      free (b);
    }
  return NULL;
}

/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *
bitmap_create_in_buf (size_t bit_cnt, void *block, size_t block_size UNUSED)
{
  struct bitmap *b = block;
  
  ASSERT (block_size >= bitmap_buf_size (bit_cnt));

  b->bit_cnt = bit_cnt;
  b->bits = (elem_type *) (b + 1);
  bitmap_set_all (b, false);
  return b;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t
bitmap_buf_size (size_t bit_cnt) 
{
  return sizeof (struct bitmap) + byte_cnt (bit_cnt);
}

/* Destroys bitmap B, freeing its storage.
   Not for use on bitmaps created by bitmap_create_in_buf(). */
void
bitmap_destroy (struct bitmap *b) 
{
  if (b != NULL) 
    {
      free (b->bits);
      free (b);
    }
}

/* Bitmap size. */

/* Returns the number of bits in B. */
size_t
bitmap_size (const struct bitmap *b)
{
  return b->bit_cnt;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered IDX in B to VALUE. */
void
bitmap_set (struct bitmap *b, size_t idx, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  if (value)
    bitmap_mark (b, idx);
  else
    bitmap_reset (b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void
bitmap_mark (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] |= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the OR instruction in [IA32-v2b]. */
  asm ("orl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void
bitmap_reset (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] &= ~mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the AND instruction in [IA32-v2a]. */
  asm ("andl %1, %0" : "=m" (b->bits[idx]) : "r" (~mask) : "cc");
}

/* Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void
bitmap_flip (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] ^= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the XOR instruction in [IA32-v2b]. */
  asm ("xorl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Returns the value of the bit numbered IDX in B. */
bool
bitmap_test (const struct bitmap *b, size_t idx) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  return (b->bits[elem_idx (idx)] & bit_mask (idx)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
void
bitmap_set_all (struct bitmap *b, bool value) 
{
  ASSERT (b != NULL);

  bitmap_set_multiple (b, 0, bitmap_size (b), value);
}

/* Sets the CNT bits starting at START in B to VALUE. */
void
bitmap_set_multiple (struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    bitmap_set (b, start + i, value);
}

/* Returns the number of bits in B between START and START + CNT,
   exclusive, that are set to VALUE. */
size_t
bitmap_count (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i, value_cnt;

  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  value_cnt = 0;
  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      value_cnt++;
  return value_cnt;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool
bitmap_contains (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      return true;
  return false;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_any (const struct bitmap *b, size_t start, size_t cnt) 
{
  return bitmap_contains (b, start, cnt, true);
}

/* Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_none (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, true);
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool
bitmap_all (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t
bitmap_scan (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  printf("bitmap_scan called. ");
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);

  /* For next fit,
  remember a location Allocated latest.
  initially, initialize it into start value. */
  const size_t start_const = start;
  static size_t lastestAlloc = 0;
  /* compile 에러 나면 static size_t lastestAlloc = 0; 으로만 하기. */

  /* For buddy system,
  실행 작업을 위해 이 함수가 호출되는 횟수를
  담고 있는 변수.  */
  static size_t check_cnt = 0;

  /* For buddy system,
  실행 작업을 위해 할당되는 공간의 크기를
  담고 있는 변수. */
  static size_t check_size = 0;

  if(check_cnt < 3)
  {
    check_size = check_size + cnt;
  }

  /* KKT 
  * fitst fit, next fit, best fit, buddy system */
  if (cnt <= b->bit_cnt) 
    {
      size_t last = b -> bit_cnt - cnt;
      size_t i;

      if(Select == FIRST_FIT){
        
        for(i = start; i <= last; i++){
         
          if(!bitmap_contains(b, i, cnt, !value)){
            printf("first fit.\n");
            printf("page index : %d\n\n", i);
            return i;
          }
        }
      } else if(Select == NEXT_FIT){
        printf("next fit.\n");
        for(i = lastestAlloc; i <= last; i++){
          if(!bitmap_contains(b, i, cnt, !value)){
            printf("found in lastest ~ last\n");
            printf("page index : %d\n\n", i);
            lastestAlloc = i;
            return i;
          }
        }
        
        for(i = start; i <= lastestAlloc; i++){
          if(!bitmap_contains(b, i, cnt, !value)){
            printf("found in start ~ latest\n");
            printf("page index : %d\n\n", i);
            lastestAlloc = i;
            return i;
          }
        }
        printf("맞는 크기의 페이지를 할당하지 못했다. \nbitmap error 반환.\n\n");

      } else if(Select == BEST_FIT){
        

        /* start 부터 메모리를 탐색*/
        i = start;
        
        /* BESTPAGEINDEX :
        요구하는 memory 크기보다 큰 page들 중에서 
        가장 크기가 작은 page의 위치를 가리킨다. */
        size_t bestPageIdx = ULONG_MAX;

        /* BESTPAGESIZE :
        요구하는 memory 크기보다 큰 page들 중에서
        가장 크기가 작은 page의 size를 나타낸다. */
        size_t bestPageSize = 0;
        
        /* TEMPIDX :
        가장 best한 크기라고 생각되는 page의 
        위치를 가리킨다. */
        size_t temp_idx = ULONG_MAX;  // 0으로 초기화 해도 된다.
        
        /* TEMPSIZE :
        가장 best한 크기라고 생각되는 page의
        size를 나타낸다. */
        size_t temp_size = 0;

        /* 
        메모리를 start 부터 순회하며,
        best 크기의 page의 위치를 return. */
        while(1){
          
          /* 메모리 전체에 대한 순회가 끝나면*/
          if(i > last){
            
            /* 메모리 전체를 검사한 후에 요청한 크기에 맞는 
            할당 가능한 페이지가 전혀 없으면 */
            if(bestPageIdx == ULONG_MAX){
              
              printf("현재 할당 가능한 페이지가 없음.");
              return BITMAP_ERROR;
            }
            
            printf("best fit \n");
            printf("best page idx : %d, best page size : %d\n\n", bestPageIdx, bestPageSize);
            return bestPageIdx;
          }
          
          if(bitmap_test(b, i) == false){
            // 제일 처음 free한 위치를 저장한다.
            temp_idx = i;
            
            /* 처음 free한 index부터 
            bitmap index를 1씩 증가시키며,
            not free한 위치를 찾는다. */
            while(1){
              i++;

              if(i > last)
                break;

              if(bitmap_test(b, i) == true)
                break;
            }

            /* temp_idx부터 not free한 위치까지의 index 개수. */
            temp_size = bitmap_count(b, temp_idx, i-temp_idx, false);
            
            /* 할당할 수 있는 page 발견 시 */
            if(cnt <= temp_size){
              /* 제일 초기. 
              반복에서 이전에 bestPage가 
              하나도 발견되지 않았을 때 */
              if(bestPageSize == 0){
                bestPageIdx = temp_idx;
                bestPageSize = temp_size;
              } 
              
              /* 반복에서 이전에 best page가 한번이라도 발견되었을 때 
              이전에 best였던 page 크기보다 현재 계산한 페이지 크기가 더 작으면,
              현재 페이지를 best page로 바꿔준다. */
              else if((bestPageSize - cnt) > (temp_size - cnt)){
                bestPageIdx = temp_idx;
                bestPageSize = temp_size;
              }
            } 
          } else{
            // 할당할 수 있는 페이지 발견 못할시 다음 위치를 본다.
            i++;
          }

        }

      } 
       else if(Select == BUDDY_SYSTEM){
         /* Buddy System 기법 사용시, 
         최대 할당할 수 있는 공간의 크기. */
        size_t max = 512;

         /* 요구한 메모리 크기에 가장 근접한 공간의 크기를
         찾기 위해 쓰이는 변수. */
         size_t bound = 512;
        /* 할당되어 있는 공간의 2^k 크기를
       구하기 위한 변수. */
         size_t offset = 0;

         /* 해당 bitmap 인덱스 부분이 빈공간인지 아닌지
         알기 위해 반복문에서 사용되는 변수. */
         size_t j = 0;
        
         i=0;

         if(cnt > max){
           /* 요구한 메모리 크기가
           할당할 수 있는 최대 공간의 크기보다 클 경우  */
          
           return BITMAP_ERROR;
         }

         while(1){
           /* 요구한 메모리 크기에 가장 가까운
           free한 공간을 찾는 반복문. */
           bound = bound / 2;

           if(cnt > bound){
            
             while(1){
            
               if(i >= max + check_size){
                 /* 요구한 메모리 크기에 가장 가까운
                 free한 공간을 찾을 수 없는 경우 */
                 return BITMAP_ERROR;
               }

               if(bitmap_test(b, i) == true){
                 /* 해당 위치(i)가 할당되어 있을 경우,
                 할당되어 있는 공간의 2^k 크기를 구한다. */
                 j = i;
                 while(1){
                   /* 할당 되어 있는 공간의 크기를 구하는 반복문. */

                   if(j >= max + check_size){
                     /* 빈 공간이 없는 경우에 대한 예외 처리. */
                     return BITMAP_ERROR;
                   }

                   if(bitmap_test(b, j) == true){
                     offset++;
                   }
                   else{
                     break;
                   }

                   j++;
                 }

                 if(offset > 256){
                   /* 최대가 512이닌깐 256보다 큰 공간이 할당 되어 있으면,
                   빈 공간이 없는 것.
                   빈 공간이 없는 경우에 대한 예외 처리. */
                   offset = 512;
                   return NULL;
                 }
                 else if(offset > 128){
                   /* offset 변수 값을 보다 큰 2^k으로 바꾼다. */
                   offset = 256;
                 }
                 else if(offset > 64){
                   offset = 128;
                 }
                 else if(offset > 32){
                   offset = 64;
                 }
                 else if(offset > 16){
                   offset = 32;
                 }
                 else if(offset > 8){
                   offset = 16;
                 }
                 else if(offset > 4){
                   offset = 8;
                 }
                 else if(offset > 2){
                   offset = 4;
                 }
                 else if(offset >1){
                   offset = 2;
                 }
                 else if(offset >0){
                   offset = 1;
                 } // KKT 수정. 원래는 else{ offset = 1; } 임.

                 if(offset > bound * 2){
                   i = i + offset;
                 }
                 else{
                   i = i + bound * 2;
                 }
               }
              
              
               else{
                 /* 요구한 메모리 크기에 가장 맞는 공간의 위치를 찾는 반복문에서,
                 bitmap_test(b, i) == false 이면 */
                
                
                 if(bound != 0){
                   if(!bitmap_contains(b, i, bound * 2, !value)){
                     printf("할당된 공간 위치 i : %10d\t", i-3);
                     printf("요구한 메모리 크기 cnt : %10d\n", cnt);
                     return i;
                   }

                   i = i + bound * 2;
                 }
                 else{
                  
                     if(bitmap_test(b, i) == false){
                       printf("할당된 위치 i : %10d\t", i-3);
                       printf("요구한 메모리 크기 cnt : %10d\n", cnt);
                       return i;
                     }
                  
                   i = i + 1;
                 }
               }
             }
           }
         }

       }
      else {
        printf("enum Select error");
      }

    }
  return BITMAP_ERROR;
}

/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t
bitmap_scan_and_flip (struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan (b, start, cnt, value);
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, idx, cnt, !value);


   for(int i=0;i<b->bit_cnt;i++){
      printf("%d ",bitmap_contains(b,i,1,true));
   }

  return idx;
}

/* File input and output. */

#ifdef FILESYS
/* Returns the number of bytes needed to store B in a file. */
size_t
bitmap_file_size (const struct bitmap *b) 
{
  return byte_cnt (b->bit_cnt);
}

/* Reads B from FILE.  Returns true if successful, false
   otherwise. */
bool
bitmap_read (struct bitmap *b, struct file *file) 
{
  bool success = true;
  if (b->bit_cnt > 0) 
    {
      off_t size = byte_cnt (b->bit_cnt);
      success = file_read_at (file, b->bits, size, 0) == size;
      b->bits[elem_cnt (b->bit_cnt) - 1] &= last_mask (b);
    }
  return success;
}

/* Writes B to FILE.  Return true if successful, false
   otherwise. */
bool
bitmap_write (const struct bitmap *b, struct file *file)
{
  off_t size = byte_cnt (b->bit_cnt);
  return file_write_at (file, b->bits, size, 0) == size;
}
#endif /* FILESYS */

/* Debugging. */

/* Dumps the contents of B to the console as hexadecimal. */
void
bitmap_dump (const struct bitmap *b) 
{
  hex_dump (0, b->bits, byte_cnt (b->bit_cnt), false);
}
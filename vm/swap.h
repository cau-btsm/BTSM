#ifndef VM_SWAP_H
#define VM_SWAP_H

typedef uint32_t swap_index_t;


/* 스왑 테이블 함수 */


void vm_swap_init (void);

/**
 * 스왑 아웃:: 페이지의 내용을 스왑 디스크에 쓰고,
 * 스왑 영역 인덱스가 있는 경우 반환한다.
 */
swap_index_t vm_swap_out (void *page);

/**
 * 스왑 인:: 지정된 스왑 내용을 읽고
 * 매핑된 스왑 블록에서 PGSIZE 바이트를 페이지에 저장한다.
 */
void vm_swap_in (swap_index_t swap_index, void *page);

void vm_swap_free (swap_index_t swap_index);


#endif /* vm/swap.h */
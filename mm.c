/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */ 
#define ALIGNMENT 8         // 정렬 기준

/* rounds up to the nearest multiple of ALIGNMENT */
// 선형의 가장 가까운 배수까지 반올림합니다
// block 사이즈 기록하는 헤더 생성 (최소중요비트 포함)
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE			4				/* Word and header/footer sizxe (bytes) */
#define DSIZE			8				/* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
// 2^10 byte

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)			(*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define	GET_SIZE(p)		(GET(p) & ~0x7)
#define GET_ALLOC(p)	(GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)	((char *)(bp) - WSIZE)
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)	((char *)(bp)  + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)	((char *)(bp)  - GET_SIZE(((char *)(bp) - DSIZE)))

static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
static void place(void *, size_t);
static void *heap_listp;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the inital empty heap */
    /*
        mem_sbrk(int incr) : 힙 영역을 incr bytes 만큼 확장 후 새로 할당된 힙 영역의 첫번재 byte를 가리키는 제네릭 포인터 리턴
        성공 시 힙 마지막 byte+1 포인터 리턴 실패시 -1(void *) 리턴 
    */
    if((heap_listp=mem_sbrk(4*WSIZE)) == (void *)-1){
        // Prologue header/footer, Epilogue header + 패딩 1word 확장.
        //!!!! 패딩을 왜 제일 첫번재에 넣는가? 뒤에 넣어도 별 상관 없을텐데
        // 할당 실패시 -1리턴
        return -1;
    }
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp+(1*WSIZE), PACK(DSIZE, 1));      /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));	/* Prologue footer */
    //!! 데이터 추가는 이 사이에 이루어지는데 prologue H/F 의 존재 이유
    /*
        프롤로그 헤더,푸터(prologue header/footer) : 메모리를 할당하거나 반환하다보면 블록을 연결해야 할 일이 생긴다. 그 연결과정 동안에 가장자리 조건을 없애기 위한 트릭이라고 생각하면 된다.
        에필로그 헤더(epilogue header) : 에필로그 헤더 또한 프롤로그와 같이 일종의 트릭이다.
    */
	PUT(heap_listp + (3*WSIZE), PACK(0, 1));		/* Epilogue header */
	heap_listp += (2*WSIZE);        // Proloue header와 footer사이로 포인터 옮김.

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;
	return 0;
}

static void *extend_heap(size_t words)
{
    //!!! 왜 char이지?
	char *bp;
	size_t size;

	/* Allocate an e 0ven number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE: words * WSIZE;
	if((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));					/* Free block header */
	PUT(FTRP(bp), PACK(size, 0));					/* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

	/* Coalese if the previous block was free */
	return coalesce(bp);
}
static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size       = GET_SIZE(HDRP(bp));

	if(prev_alloc && next_alloc){					/* Case 1 */
		return bp;
	}

	else if(prev_alloc && !next_alloc){		/* Case 2 */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if(!prev_alloc && next_alloc){		/* Case 3 */
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	else {																/* Case 4 */
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;				/* Adjusted block size */
	size_t extendsize;	/* Amount to extend heap if no fit */
	char *bp;

	/* Ignore spurious requests */
	if(size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if(size <= DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

	/* Search the free list for a fit */
	if((bp = find_fit(asize)) != NULL){
		place(bp, asize);
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	if((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
	size_t size = GET_SIZE(HDRP(bp));

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    // - SIZE_T_SIZE : malloc 시 size+SIZE_T_SIZE 해주기 때문 
	//copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(ptr));
	if (size < copySize)
      copySize = size;   // 재할당 요청사이즈보다 ,copySize가 더 크면 copySize를 요청 size로 설정
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *find_fit(size_t asize){
	void *bp;
	for(bp = heap_listp;  GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
		if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
			return bp;
		}
	}
	return NULL;
}

static void place(void *bp, size_t asize){
	size_t csize = GET_SIZE(HDRP(bp));

	if((csize - asize) >= (2*DSIZE)){
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
	}
	else {
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}














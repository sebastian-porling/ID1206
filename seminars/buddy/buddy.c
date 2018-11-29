#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

/* Settings */
#define MIN 5
#define LEVELS 8
#define PAGE 4096

/* Head of block*/
enum flag {Free, Taken};

struct head {
    enum flag status;
    short int level;
    struct head *next;
    struct head *prev;
};

/* Init the free list */
struct head *flists[LEVELS] = {NULL};

/* Init number of pages */
int number_of_pages = 0;

/* Make a new block */
struct head *new(){
    struct head *new = (struct head *) mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(new == MAP_FAILED){
        printf("mmap failed: error %d\n", errno);
        return NULL;
    }
    assert(((long int)new & 0xfff) == 0);
    new->status = Free;
    new->level = LEVELS - 1;
    number_of_pages++;
    return new;
}

/* Get the number of pages */
int getNumberOfPages(){
    return number_of_pages;
}

/* Find the buddy */
struct head *buddy(struct head* block){
    int index = block->level;
    long int mask = 0x1 << (index + MIN);
    return (struct head*)((long int)block ^ mask);
}

/* Split the block*/
struct head *split(struct head* block){
    int index = block->level - 1;
    long int mask = 0x1 << (index + MIN);
    struct head* split = (struct head*)((long int)block | mask);
    split->level = block->level-1;
    block->level = block->level-1;
    return split;
}

/* Merge buddies */
struct head *merge(struct head* block, struct head* sibling){
    struct head *primary;
    if(sibling < block){
        primary = sibling;
    }else{
        primary = block;
    }
    primary->level = primary->level + 1;
    primary->status = Free;
    return primary;
}

/* Finds the primary block of the given block */
struct head *primary(struct head * block) {
  int index = block->level;
  long int mask = 0xffffffffffffffff << (1 + index + MIN);
  struct head *primary = (struct head *) ((long int) block & mask);
  primary->level = index + 1;
  return primary;
}


/* Magic, hide the head structure */
void *hide(struct head* block){
    return (void*)(block + 1);
}

/* Reveals the head structure */
struct head *magic(void *memory){
    return ((struct head*)memory - 1);
}

/* Give the right level of allocation */
int level(int size){
    int req = size + sizeof(struct head);

    int i = 0;
    req = req >> MIN;
    while(req > 0){
        i++;
        req = req >> 1;
    }
    return i;
}

/* Gets the size of the given memory */
int get_size(void *memory){
    struct head *block = magic(memory);
    switch(block->level){
        case 0:
            return 32;
        case 1:
            return 64;
        case 2:
            return 128;
        case 3:
            return 256;
        case 4:
            return 512;
        case 5:
            return 1024;
        case 6:
            return 2048;
        case 7:
            return 4096;
    }
}

/* Add given block to the flists */
void add_to_flists(struct head *block) {
    // Set block status to free
    block->status = Free;
    // If flists at given index is null
    // Remove the blocks links and add it first
    if (flists[block->level] == NULL) {
        block->next = NULL;
        block->prev = NULL;
        flists[block->level] = block;
    // Other put it first and change the links
    }else {
        block->next = flists[block->level];
        block->next->prev = block;
        flists[block->level] = block;
    }
}

/* We remove from flists when we split */
struct head *rem_from_flists(int level) {
    struct head *block;
    if (flists[level] == NULL) {
        return NULL;
    }
    else if (flists[level]->next != NULL) {
        block = flists[level];
        flists[level]->next->prev = NULL;
        flists[level] = flists[level]->next;
    }
    else {
        block = flists[level];
        flists[level] = NULL;
    }
    return block;
}

/*  Finds a block from the free list
    If a block exists in the given level return block
    else check higher level.
    If the checked level is at max and it is empty
    mapp memory.
*/
struct head *find(int index, int level) {
    // If the given index of flists is empty
    // Check if level is max, then add new page
    // Else check one level higher.
    if (flists[index] == NULL) {
            if (index == LEVELS - 1) {
                add_to_flists(new());
                find(index, level);
            }
            else {
                find(index + 1, level);
            }
    // If the index is not the same as the level we wanted
    // Split the block and do find again
    } else if (index != level) {
            struct head *block = split(rem_from_flists(index));
            add_to_flists(block);
            add_to_flists(buddy(block));
            find(index - 1, level);
    // Else remove the block from flist and return block.
    } else {
            struct head *block = rem_from_flists(index);
            block->level = index;
            return block;
    }
}

/* Unlinks the given block from the free list */
void unlink_from_flists(struct head *block) {
    // When the block is in the middle of flists
    if (block->next != NULL && block->prev != NULL) {
        block->prev->next = block->next;
        block->next->prev = block->prev;
    }
    // When the block is at the first position in the flists
    else if (block->next != NULL && block->prev == NULL) {
        flists[block->level] = block->next;
        block->next->prev = NULL;
    }
    // When the block is at the last position in the flists
    else if (block->prev != NULL && block->next == NULL) {
        block->prev->next = NULL;
    }
    // When the block is alone in flists
    else {
        flists[block->level] = NULL;
    }
    block->next = NULL;
    block->prev = NULL;
}


/*  Inserts the block in the free list.
    If the blocks buddy is not taken, merge and insert again.
    If the level is at max level we will remove the mapping of memory.
 */
void insert(struct head * block) {
    // Set block status to free
    block->status = Free;
    // Unmap memory if level is at max
    if (block->level == LEVELS - 1) {
        munmap(block, 4096);
        number_of_pages--;
    // If buddy is free merge. Else add to flists
    }else {
        struct head *bud = buddy(block);
        if ((bud->status == Free) && bud->level == block->level) {
            unlink_from_flists(bud);
            insert(primary(block));
        }
        else {
            add_to_flists(block);
        }
        return;
    }
}


/* Allocates memory from the free list */
void *balloc(size_t size){
    if (size == 0) {
        return NULL;
    }
    int index = level(size);
    struct head *taken = find(index, index);
    taken->status = Taken;
    return hide(taken);
}

/* Deallocates memory from the free list */
void bfree(void *memory){
    if (memory != NULL) {
        struct head *block = magic(memory);
        insert(block);
    }
    return;
}

void test(){
    struct head* block = new();
    printf("New Level: %d, Adress: %p\n",  block->level, block);
    printf("Hide: %p, Magic: %p\n", hide(block), magic(hide(block)));
    struct head* block2 = split(block);
    printf("Split Level: %d, Adress: %p\n",  block2->level, block2);
    struct head* block3 = buddy(block2);
    printf("Buddy Level: %d, Adress: %p\n", block3->level, block3);
    struct head* split2 = split(block2);
    printf("split Level: %d, Adress: %p\n",  split2->level, split2);
    struct head* split2buddy = buddy(split2);
    printf("Split Split buddy  Level: %d, Adress: %p\n",  split2buddy->level, split2buddy);
    struct head* mergeSplit2buddy = merge(split2, split2buddy);
    printf("Merge Buddy Split buddy  Level: %d, Adress: %p\n",  mergeSplit2buddy->level, mergeSplit2buddy);
    struct head* block6 = merge(block, block2);
    printf("Merge Level: %d, Adress: %p\n", block6->level, block6);
    printf("Sibling Level: %d, Adress: %p\n",  block3->level, block3);
    printf("size of head is: %ld\n", sizeof(struct head));
    printf("size of head pointer is: %ld\n", sizeof(struct head*));
    printf("size of enum flag is: %ld\n", sizeof(enum flag));
    printf("level for 20 should be 1: %d\n", level(2048));
    
    int *test[10000];
    for(int i = 0; i < 100; i++){
        printf("Balloc: %d\n", i);
        test[i] = balloc(sizeof(int));
    }
    for(int i = 0; i < LEVELS; i++)
    {
        struct head *current = flists[i];
        while(current != NULL){
            printf("Level: %d adres: %p\n", i, current);
            current = (struct head*)current->next;
        }
    }
    for(int i = 0; i < 100; i++){
        printf("Bfree: %d\n", i);
        bfree(test[i]);
    }
    
    
    printf("remove memory\n");
    for(int i = 0; i < LEVELS; i++)
    {
        struct head *current = flists[i];
        while(current != NULL){
            printf("Level: %d adres: %p\n", i, current);
            current = current->next;
        }
    }
}

void test_constant(int count, int size) {
  clock_t start, middle, end;
  double balloc_time, bfree_time, totalt_time;
  double malloc_time, free_time, mtotalt_time;
  int *memory[count];
  int i;

  if(size == 0 || (size - 24) > 4096) {
    printf("incorrect size\n\n");
    return;
  }
  printf("Count: %d, Size: %d\n", count, size);
  // BALLOC BFREE
  start = clock();
  for (i = 0; i < count; i++) {
    memory[i] = balloc(size - 24);
  }
  printf("Efter ball\n");
  middle = clock();
  for (i = 0; i < count; i++) {
    bfree(memory[i]);
  }
  printf("Efter bfre\n");
  end = clock();
  balloc_time = (((double) (middle - start)) / CLOCKS_PER_SEC) * 1000;
  bfree_time = (((double) (end - middle)) / CLOCKS_PER_SEC) * 1000;
  totalt_time = (((double) (end - start)) / CLOCKS_PER_SEC) * 1000;

  // MALLOC FREE
  start = clock();
  for (i = 0; i < count; i++) {
    memory[i] = malloc(size - 24);
  }
  middle = clock();
  for (i = 0; i < count; i++) {
    free(memory[i]);
  }
  end = clock();

  malloc_time = (((double) (middle - start)) / CLOCKS_PER_SEC) * 1000;
  free_time = (((double) (end - middle)) / CLOCKS_PER_SEC) * 1000;
  mtotalt_time = (((double) (end - start)) / CLOCKS_PER_SEC) * 1000;
  printf("balloc_time: %fms, average: %fms\n", balloc_time, balloc_time/count);
  printf("bfree_time: %fms, average: %fms\n", bfree_time, bfree_time/count);
  printf("totalt_time: %fms, average: %fms\n", totalt_time, totalt_time/count);
  printf("\n");
  printf("malloc_time: %fms, average: %fms\n", malloc_time, malloc_time/count);
  printf("free_time: %fms, average: %fms\n", free_time, free_time/count);
  printf("mtotalt_time: %fms, average: %fms\n", mtotalt_time, mtotalt_time/count);
  printf("\n");
}

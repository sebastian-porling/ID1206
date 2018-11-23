#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

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

struct head *flists[LEVELS] = {NULL};

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
    return new;
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
    return primary;
}

/* Magic, hide the head structure */
void *hide(struct head* block){
    return (void*)(block + 1);
}

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

// Implementera

struct head *merged_flists(struct head *block, int index){
    if(block->level == index){
        return block;
    }
    struct head *splitted_block = split(block);

    if(flists[splitted_block->level] != NULL){
        splitted_block->next = flists[splitted_block->level];
        flists[splitted_block->level]->prev = splitted_block;
        flists[splitted_block->level] = splitted_block;
    }else{
        flists[splitted_block->level] = splitted_block;
    }
    return merged_flists(buddy(splitted_block), index);
}

struct head *find(int index){
    
    if (flists[index] != NULL) {
        struct head *block = flists[index];
        block->next = NULL;
        if(flists[index]->next != NULL){
            flists[index] = flists[index]->next;
            flists[index]->prev = NULL;
        }else{
            flists[index] = NULL;
        }
        block->status = 1;
        return block;
    }

    
    for(int i = index + 1; i < LEVELS; i++)
    {
        if(i == LEVELS-1){
            struct head *new_block = new();
            return merged_flists(new_block, index);
        }
        if(flists[i] != NULL){
            return merged_flists(flists[i], index);
            //struct head *splitedBlock = split(flists[i]);
            //splitedBlock->next = NULL;
            //if(flists[i]->next != NULL){
            //    flists[i] = flists[i]->next;
            //    flists[i]->prev = NULL;
            //}else{
            //    flists[i] = NULL;
            //}
            //struct head *temp = flists[i-1];
            //flists[i-1] = buddy(splitedBlock);
            //flists[i-1]->next = temp;
        }
    }
    //return NULL;
}



void insert_flists(struct head *block){
    if(flists[block->level] != NULL){
        block->next = flists[block->level];
        flists[block->level]->prev = block;
        flists[block->level] = block;
    }else{
        flists[block->level] = block;
    }
    return;
}

void insert(struct head *block){
    if(buddy(block)->status == Taken){
        insert_flists(block);
    }else{
        struct head *buddy_block = buddy(block);
        struct head *merged_block = merge(block, buddy_block);
        if(merged_block->level == LEVELS-1){
            merged_block == NULL;
        }else{
            insert(merged_block);
        }
    }
    return;
}

/*  */
void *balloc(size_t size){
    if (size == 0) {
        return NULL;
    }
    int index = level(size);
    struct head *taken = find(index);
    return hide(taken);
}

/*  */
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
    struct head* block2 = split(block);
    printf("Split Level: %d, Adress: %p\n",  block2->level, block2);
    struct head* block3 = buddy(block2);
    printf("Buddy Level: %d, Adress: %p\n", block3->level, block3);
    struct head* split2 = split(block3);
    printf("Buddy split Level: %d, Adress: %p\n",  split2->level, split2);
    struct head* split2buddy = buddy(split2);
    printf("Buddy Split buddy  Level: %d, Adress: %p\n",  split2buddy->level, split2buddy);
    struct head* mergeSplit2buddy = merge(split2, split2buddy);
    printf("Merge Buddy Split buddy  Level: %d, Adress: %p\n",  mergeSplit2buddy->level, mergeSplit2buddy);
    struct head* block6 = merge(block, block2);
    printf("Merge Level: %d, Adress: %p\n", block6->level, block6);
    printf("Sibling Level: %d, Adress: %p\n",  block3->level, block3);
    printf("size of head is: %ld\n", sizeof(struct head));
    printf("size of head pointer is: %ld\n", sizeof(struct head*));
    printf("size of enum flag is: %ld\n", sizeof(enum flag));
    printf("level for 20 should be 1: %d\n", level(20));
    int *test[100];
    for(int i = 0; i < 100; i++)
    {
        test[i] = balloc(sizeof(int)*4);
        printf("Test %d: %p\n", i, test[i]);
    }
    
    bfree(test);
    printf("Test %p", test);
}

int main(){
    test();
    return 0;
}
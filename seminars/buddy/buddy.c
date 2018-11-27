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
    primary->status = Free;
    return primary;
}

struct head *primary(struct head *block){
    int index = block->level;
    printf("Primary\n");
    long int mask = 0xffffffffffffffff << (1 + index + MIN);
    printf("Mask\n");
    struct head *primary = (struct head*)((long int)block | mask);
    printf("Level %d\n", block->level);
    //primary->level = index + 1;
    printf("Change lvl\n");
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
    if(block->level == index && block != NULL){
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
        if(flists[index]->next != NULL){
            flists[index] = flists[index]->next;
            flists[index]->prev = NULL;
        }else{
            flists[index] = NULL;
        }
        block->status = 1;
        return block;
    }
    for(int i = index; i < LEVELS; i++)
    {
        if(i == LEVELS-1){
            struct head *new_block = new();
            struct head *reduced_block = merged_flists(new_block, index);
            reduced_block->status = 1;
            return reduced_block;
        }
        if(flists[i] != NULL){
            struct head *new = flists[i];
            if(flists[i]->next != NULL){
                flists[i] = flists[i]->next;
                flists[i]->prev = NULL;
            }else{
                flists[i] = NULL;
            }
            struct head *old_reduced_block = merged_flists(new, index);
            old_reduced_block->status = 1;
            return old_reduced_block;
        }
    }
}



void insert_flists(struct head *block){
    if(flists[block->level] != NULL){
        block->next = flists[block->level];
        flists[block->level]->prev = block;
        flists[block->level] = block;
    }else{
        flists[block->level] = block;
    }
    flists[block->level]->status = 0;
    return;
}

void remove_from_flists(struct head *block){
    //printf("###\tIn remove from flists\n");
    //printf("###\t remove block: %p\n", block);
    if(block->prev == NULL && block->next == NULL){
        printf("block: %p, lvl: %d, st: %d, n: %p, p: %p\n", flists[block->level], flists[block->level]->level, flists[block->level]->status, flists[block->level]->next, flists[block->level]->prev);
        flists[block->level] = NULL;
        printf("block: %p, lvl: %d, st: %d, n: %p, p: %p\n", block, block->level, block->status, block->next, block->prev);
        return;
    }
    // printf("###\t middel remove block: %p\n", block);
    // printf("###\tMiddle remove from flists\n");
    if((block->prev == NULL && block->next != NULL)){
        /* printf("###\tIn if thing\n");
        printf("###\t Next_block: %p\n", block->next);
        printf("###\t test: %p status: %d, level: %d: \n", flists[block->level], flists[block->level]->status, flists[block->level]->level);
        printf("###\tflist next: %p\n", flists[block->level]->next); */

        flists[block->level]->next->prev = flists[block->level]->prev;
        
        // printf("###\tSet next prev null\n");
        
        flists[block->level] = flists[block->level]->next;
        
        /* printf("###\tnew start in flists: %p\n", flists[block->level]);
        printf("###\tSet current to next\n"); */
        
        return;
    }
    if(block->prev != NULL && block->next == NULL){
        /* printf("######\tNy bajs\n");
        printf("###\tPrev: %p, Current: %p, Next: %p\n", flists[block->level]->prev, flists[block->level], flists[block->level]->next); */
        
        flists[block->level] = flists[block->level]->prev;
        
        flists[block->level]->next = NULL;
        
        // printf("###\t2 Prev: %p, Current: %p, Next: %p\n", flists[block->level]->prev, flists[block->level], flists[block->level]->next);
        
        return;
    } 
    /* printf("###\tAlmost end remove from flists\n");
    printf("###\t Next_block: %p\n", block->next);
    printf("########\t Prev_block: %p, prevagain: %p\n", block->prev, block->prev->prev); */
    block->next->prev = block->prev;
    // printf("###\t prev_block: %p\n", block->prev->next);
    block->prev->next = block->next;
    // printf("###\tDone remove from flists\n");
    return;
}

void insert(struct head *block){
    // printf("#\tWe look for buddy\n");
    struct head *block_buddy = (struct head*)buddy(block);
    // printf("#\tWe check if taken\n");
    if(block_buddy->status == Taken){
        // printf("#\tIs taken\n");
        insert_flists(block);
    }else{
        // printf("#\tNot taken, We now merge\n");
        remove_from_flists(block_buddy);
        struct head *merged_block = merge(block, block_buddy);
        // printf("#\tWe remove buddy from list\n");
        
        // printf("#\tWe check if lvl 7\n");
        if(merged_block->level == LEVELS-1){
            // printf("#\tWe munmap\n");
            int err = munmap(merged_block, 4096);
            merged_block = NULL;
            if(err != 0){
                printf("mmap failed: error %d\n", errno);
            }
        }else{
            // printf("#\tRecursion\n");
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
        printf("#\tWe do magic\n");
        struct head *block = magic(memory);
        printf("#\tWe insert stuff\n");
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
    
    //printf("Balloc: %p\n", bfree(balloc(2048)));
    //printf("Test: %d\n", ((struct head*)buddy(magic(balloc(500))))->status);
    //int *p = balloc(500);
    //bfree(balloc(1024));
    int *test[10000];
    for(int i = 0; i < 100; i++){
        printf("Balloc: %d\n", i);
        test[i] = balloc(sizeof(int));
        printf("Magic: %p", magic(test[i]));
    }
    for(int i = 0; i < 100; i++){
        printf("Bfree: %d\n", i);
        bfree(test[i]);
    }
    

    //printf("Find: %d\n", find(5)->level);
    //balloc(32);
    //bfree(balloc(256));
    
    for(int i = 0; i < LEVELS; i++)
    {
        struct head *current = flists[i];
        while(current != NULL){
            printf("Level: %d Address: %p\n", i, current);
            printf("#Level: %d Address %p\n", i, current->next);
            current = (struct head*)current->next;
        }
    }
    //bfree(p);
    printf("Remove memory\n");
    for(int i = 0; i < LEVELS; i++)
    {
        struct head *current = flists[i];
        while(current != NULL){
            printf("Level: %d Address: %p\n", i, current);
            current = current->next;
        }
    }
    
}

int main(){
    test();
    return 0;
}
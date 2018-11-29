#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include "rand.h"
#include "buddy.h"

#define ROUNDS 100
#define LOOP 1000

#define BUFFER 10
int median[8] = {0};

void increment(int size){
    switch(size){
        case 32:
            median[0]++;
            break;
        case 64:
            median[1]++;
            break;
        case 128:
            median[2]++;
            break;
        case 256:
            median[3]++;
            break;
        case 512:
            median[4]++;
            break;
        case 1024:
            median[5]++;
            break;
        case 2048:
            median[6]++;
            break;
        case 4096:
            median[7]++;
            break;
    }
}

int main() {
    void *init = sbrk(0);
    void *current;

    void *buffer[BUFFER];
    for(int i =0; i < BUFFER; i++) {
        buffer[i] = NULL;
    }
    long int totalAlloc = 0;

    printf("The initial top of the heap is %p.\n", init); 
    FILE *file = fopen("res.dat", "w");
    fprintf(file, "# Rounds: %d, Loops: %d, Buffer: %d\n", ROUNDS, LOOP, BUFFER);
    fprintf(file, "#Round\tPages\tAlloc\tPAlloc\tFree\n");
    for(int j = 0; j < ROUNDS; j++) {
        for(int i= 0; i < LOOP ; i++) { 
            int index = rand() % BUFFER;
            if(buffer[index] != NULL) {
                totalAlloc -= get_size(buffer[index]);
                bfree(buffer[index]);
            }
            size_t size = (size_t)request();
            //printf("Size: %ld\n", size);
            int *memory; 
            memory = balloc(size);
            

            if(memory == NULL) {
            fprintf(stderr, "memory myllocation failed\n");
            return(1);
            }
            buffer[index] = memory;
            totalAlloc += get_size(memory);
            increment(get_size(memory));
            /* writing to the memory so we know it exists */
            //*memory = 123;
        }
        int pages = getNumberOfPages();
        int pagesAlloc = pages*4096;
        fprintf(file, "%d\t%d\t%ld\t%d\t%ld\n", j, pages, totalAlloc, pagesAlloc, pagesAlloc-totalAlloc);
        current = sbrk(0);
        //int allocated = ((long)current - (long)init) / 1024;
        //printf("The final top of the heap is %p.\n", current);
        //printf("   increased by %d Ki byte\n", allocated);
    }
    fclose(file);
    FILE *file2 = fopen("block.dat", "w");
    fprintf(file2, "#Block\tAlloc\n");
    int exp = 5;
    for(int i = 0; i < 8; i++)
    {
        fprintf(file2, "%d\t%d\t%d\n", i, (int)pow(2,exp), median[i]);
        exp++;
    }
    fclose(file2);
    
  return 0;
}

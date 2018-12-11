#include <stdio.h>
#include "green.h"

void *test(void *arg){
    int i = *(int*)arg;
    int loop = 1000;
    
    while(loop > 0){
        printf("thread %d: %d\n", i, loop);
        loop--;
        green_yield();
    }
}

int flag = 0;
green_cond_t cond;
green_mutex_t mutex;

void *test_two(void *arg){
    int id = *(int*)arg;
    int loop = 10000;
    green_mutex_lock(&mutex);
    while(loop > 0){
        if (flag == id) {
            printf("thread %d: %d\n", id, loop);
            loop--;
            flag = (id + 1) % 2;
            green_cond_signal(&cond);
            green_mutex_unlock(&mutex);
        }
        else {
            green_cond_wait(&cond, &mutex);
        }
        sleep(1/100);
    }
}

void *test_three(void *arg){
    int id = *(int*)arg;
    int loop = 4;
    int count = 0;
    
    while(loop > 0){
        count++;
        if (flag == id) {
            printf("thread %d: %d\n", id, loop);
            loop--;
            flag = (id + 1) % 2;
            printf("Looped, %d times\n", count);
            count = 0;
        }
        else {
        }
        //sleep(1/100);
    }
}


void *test_four(void *arg){
    int id = *(int*)arg;
    int loop = 4;
    int count = 0;
    while(loop > 0){
        green_mutex_lock(&mutex);
        count++;
        if (flag == id) {
            
            printf("thread %d: %d\n", id, loop);
            loop--;
            flag = (id + 1) % 2;
            printf("Looped, %d times\n", count);
            count = 0;
            
        }
        else {
            
        }
        green_mutex_unlock(&mutex);
        //sleep(1/100);
        
    }
    
}

void *test_five(void *arg){
    green_mutex_lock(&mutex);
    while(1){
        if(flag == 0){
            flag = 1;
            printf("if flag: %d\n", flag);
            green_cond_signal(&cond);
            green_mutex_unlock(&mutex);
            break;
        } else {
            printf("else flag: %d\n", flag);
            //green_mutex_unlock(&mutex);
            //green_cond_wait(&cond);
            green_cond_wait(&cond, &mutex);
        }
    }
}

int main()
{
    green_mutex_init(&mutex);
    green_cond_init(&cond);
    green_t g0, g1;
    int a0 = 0;
    int a1 = 1;
    //int a2 = 2;
    printf("done\n");
    green_create(&g0, test_two, &a0);
    printf("efter första create\n");
    green_create(&g1, test_two, &a1);
    printf("efter andra create\n");
    //green_create(&g2, test_two, &a2);
    green_join(&g0);
    printf("efter första join\n");
    green_join(&g1);
    printf("done\n");
    //green_join(&g2);
    return 0;
}


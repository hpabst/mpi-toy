#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>


void create_data(int*, int);
int comp_func(const void*, const void*);

int main(int argc, char* argv[]){
    int dataSize;
    int* data;
    struct timeval before, after;
    gettimeofday(&before, NULL);

    sscanf(argv[1], "%d", &dataSize);

    data = calloc(dataSize, sizeof(int));

    create_data(data, dataSize);

    qsort(data, dataSize, sizeof(int), comp_func);

    gettimeofday(&after, NULL);
    fprintf(stderr, "Final time count for %d items sorted sequentially:\n %ld microseconds\n %ld seconds\n", dataSize,
            (long int) after.tv_usec - (long int) before.tv_usec, (long int) after.tv_sec - (long int) before.tv_sec);
    return 1;
}



void create_data(int* data, int numData){
    /**
    ** Fills the passed in int array with numData random numbers.
    **/
    srandom(20);
    int i;
    for(i = 0; i < numData; i++){
        data[i] = random()%1000;
    }
    return;
}

int comp_func(const void* item1, const void* item2){
    int* it1, *it2;
    it1 = (int*) item1;
    it2 = (int*) item2;
    return (*it1 - *it2);
}

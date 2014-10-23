/**
*Main file for CMPUT 481 assignment 2.
**/
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEBUG 1


void create_data(int*, int);
int comp_func(const void*, const void*);

int main(int argc, char* argv[]){
    /** PHASE 0: CREATE AND DISTRIBUTE DATA. **/
    int numProcessors, dataSize, myid, numProcessorsMPI, namelen, regSampleIndex, pivotIndex;
    int i;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    MPI_Comm intercomm;

    if(argc != 3){
        fprintf(stderr, "Error: Expected 2 arguments. The first is the number of processors and the second is the amount of data.\n");
        fprintf(stderr, "Actual value of argc: %d.\n", argc);
        exit(0);
    }

    sscanf(argv[1], "%d", &numProcessors);
    sscanf(argv[2], "%d", &dataSize);

#if DEBUG
    fprintf(stderr, "Value of numProcessors: %d.\n", numProcessors);
    fprintf(stderr, "Value of dataSize: %d.\n", dataSize);
#endif

    if(!(dataSize%numProcessors == 0)){
        fprintf(stderr, "Error: The number of items to be sorted must be divisible by the number of processors.");
        exit(0);
    }

    regSampleIndex = dataSize / (pow(numProcessors, 2));
    pivotIndex = (int) floor(numProcessors/2);

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcessorsMPI);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Get_processor_name(processor_name, &namelen);
    MPI_Comm_get_parent(&intercomm);


    int mydata[dataSize/numProcessors];
    memset(mydata, 0, sizeof(int)*(dataSize/numProcessors));
    if(myid == 0){ /**0 is the master machine, so it creates and distributes the data.**/
        int baseData[dataSize];
        memset(baseData, 0, sizeof(int)*dataSize);
        create_data(baseData, dataSize);
#if DEBUG
        for(i = 0; i < dataSize; i++){
            fprintf(stderr, "Data item %d: %d.\n", i, baseData[i]);
        }
#endif
        for(i = 1; i < numProcessors; i++){ /**Send the data to all the other processors.**/
            MPI_Send(&baseData[0] + (i*numProcessors), dataSize/numProcessors, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
        memcpy(mydata, baseData, sizeof(int) * (dataSize/numProcessors));
    } else { /**Otherwise you're a child machine, so you wait for your data and then wait at the barrier for Phase 1.**/
        MPI_Recv(mydata, dataSize/numProcessors, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    MPI_Barrier(MPI_COMM_WORLD);
#if DEBUG
    fprintf(stderr, "I am process %d that just passed the barrier with %d as my first value!.\n", myid, mydata[0]);
#endif
    /** PHASE 1: SORT LOCAL DATA AND FIND LOCAL SAMPLES **/

    qsort(mydata, dataSize/numProcessors, sizeof(int), comp_func);
    int localSample[numProcessors];
    memset(localSample, 0, sizeof(int) * (numProcessors));
    for(i = 0; i < numProcessors; i++){
        localSample[i] = mydata[i*regSampleIndex];
    }

    MPI_Barrier(MPI_COMM_WORLD);
    return 0;
}


void create_data(int* data, int numData){
    /**
    ** Fills the passed in int array with numData random numbers.
    **/
    srandom(20);
    int i;
    for(i = 0; i < numData; i++){
        data[i] = random();
    }
}

int comp_func(const void* item1, const void* item2){
    int* it1, *it2;
    it1 = (int*) item1;
    it2 = (int*) item2;
    return (*it1 - *it2);
}

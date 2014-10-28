/**
*Main file for CMPUT 481 assignment 2.
**/
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEBUG 1
#define SETDATA 1


void create_data(int*, int);
int comp_func(const void*, const void*);
void create_partitions(int numProcessors, int dataSize, int*, int*, int[][numProcessors/dataSize]);
void merge(int*, int*, int*);

int main(int argc, char* argv[]){


    /** PHASE 0: CREATE AND DISTRIBUTE DATA. **/
    int numProcessors, dataSize, myid,namelen, w, p;
    /**numProcessors is the number of processors we are using to sort, dataSize is the number of integers we are sorting,
    ** myid is the id number that each processor will have, w and p are values for determining regular samples and pivot locations,
    **these 2 variable names come from the PSRS paper. **/
    int i;//Not following C99 standard, so we can't declare loop variables inside the for loop structure, just reuse this i variable.
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

    w = dataSize / (pow(numProcessors, 2));
    p = (int) floor(numProcessors/2);

    MPI_Init(&argc, &argv);
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
            MPI_Send(&baseData[0] + (i*(dataSize/numProcessors)), dataSize/numProcessors, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
        memcpy(mydata, baseData, sizeof(int) * (dataSize/numProcessors));
    } else { /**Otherwise you're a child machine, so you wait for your data and then wait at the barrier for Phase 1.**/
        MPI_Recv(mydata, dataSize/numProcessors, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    MPI_Barrier(MPI_COMM_WORLD);
#if DEBUG
    fprintf(stderr, "I am process %d that just passed the phase 1 barrier with %d as my first value!.\n", myid, mydata[0]);
#endif


    /** PHASE 1: SORT LOCAL DATA AND FIND LOCAL SAMPLES **/

    qsort(mydata, dataSize/numProcessors, sizeof(int), comp_func);
    int localSample[numProcessors];
    memset(localSample, 0, sizeof(int) * (numProcessors));
    for(i = 0; i < numProcessors; i++){
        localSample[i] = mydata[i*w];
    }
    MPI_Barrier(MPI_COMM_WORLD);
#if DEBUG
    for(i=0; i< numProcessors; i++){
        fprintf(stderr, "I am process %d that passed the phase 2 barrier with %d as one of my regular samples!\n", myid, localSample[i]);
    }
#endif


    /**PHASE 2: FIND PIVOTS AND PARTITION **/
    int collectedSamples[numProcessors * numProcessors];
    if(myid == 0){
        memset(collectedSamples, 0, sizeof(int) * (numProcessors * numProcessors));
        memcpy(collectedSamples, localSample, numProcessors * sizeof(int));
        for(i = 1; i < numProcessors; i++){
            MPI_Recv(&collectedSamples[0] + (i*numProcessors), numProcessors, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            /**Receives the regular sample from processor i and stores it in collectedSamples.**/
        }
        qsort(collectedSamples, numProcessors * numProcessors, sizeof(int), comp_func);
#if DEBUG
        for(i = 0; i < (numProcessors*numProcessors); i++){
            fprintf(stderr, "Collected Sample number %d is %d.\n", i, collectedSamples[i]);
        }
#endif
    } else {
        MPI_Send(localSample, numProcessors, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD); //Mid phase 2 barrier.

    int pivots[numProcessors-1];
    memset(pivots, 0, sizeof(int) * (numProcessors-1));
    if(myid == 0){
        for(i = 1; i < numProcessors; i++){
            pivots[i-1] = collectedSamples[(i*numProcessors)+p-1];
        }
#if DEBUG
            for(i = 0; i < (numProcessors -1); i++){
                fprintf(stderr, "Pivot %d is %d.\n", i, pivots[i]);
            }
#endif
    }
    MPI_Bcast(pivots, numProcessors-1, MPI_INT, 0, MPI_COMM_WORLD);//Send the pivots from the master process to all the other processes.
    MPI_Barrier(MPI_COMM_WORLD);

    /**PHASE 3: FIND AND EXCHANGE PARTITIONS **/

    int localPartitions[numProcessors][dataSize/numProcessors]; //Using a 2-dimensional array to store local partitions.
    //For each process there will be numProcessors partitions with a maximum size of dataSize/numProcessors each.
    memset(localPartitions, -1, dataSize * sizeof(int)); //Since we only deal with non-negative numbers, a value of -1 indicates the end of the partition.
    create_partitions(numProcessors, dataSize, pivots, mydata, localPartitions);

    int sharedPartitions[numProcessors][dataSize/numProcessors];
    memset(sharedPartitions, -1, dataSize * sizeof(int));
    for(i = 0; i < numProcessors; i++){
        MPI_Gather(&localPartitions[i][0], dataSize/numProcessors, MPI_INT, &sharedPartitions[0][0],
                   dataSize/numProcessors, MPI_INT, i, MPI_COMM_WORLD);
    }

    /**PHASE 4: MERGE PARTITIONS **/
    int resultArray[dataSize];
    int tempHolder[dataSize];
    memset(resultArray, -1, dataSize * sizeof(int));
    memset(tempHolder, -1, dataSize * sizeof(int));
    merge(sharedPartitions[0], sharedPartitions[1], resultArray);
    for(i = 2; i < numProcessors; i++){
        memcpy(tempHolder, resultArray, dataSize * sizeof(int));
        merge(tempHolder, &sharedPartitions[i][0], resultArray);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /**PHASE 5: GATHER AND CONCATENATE MERGED PARTITIONS**/
    int gatheredPartitions[dataSize * numProcessors];
    int sortedKeys[dataSize];//The final container of the sorted keys.
    int sortMarker = 0;
    if(myid == 0){
        //No point in doing this across all processes.
        memset(gatheredPartitions, 0, dataSize * numProcessors * sizeof(int));
        memset(sortedKeys, 0, dataSize * sizeof(int));
    }
    MPI_Gather(resultArray, dataSize, MPI_INT, gatheredPartitions, dataSize, MPI_INT, 0, MPI_COMM_WORLD);

    if(myid == 0){
        for(i = 0; i < dataSize * numProcessors; i++){
            if(gatheredPartitions[i] != -1){
                sortedKeys[sortMarker] = gatheredPartitions[i];
                sortMarker++;
            }
        }
    }

#if DEBUG
    if(myid == 0){
        fprintf(stderr, "Final sorted keys:\n\n");
        for(i = 0; i < dataSize; i++){
            fprintf(stderr, "Element %d: %d\n", i, sortedKeys[i]);
        }
    }
#endif
    MPI_Finalize();
    return 0;
}





void create_partitions(int numProcessors, int dataSize, int* pivots, int* local_data, int partition_array[][dataSize/numProcessors]){
    /**
    * partition_array is a 2-dimensional array of ints, [numProcessors][dataSize/numProcessors],
    * pivots is an array of numProcessors-1 ints,
    * local_data is an array of dataSize/numProcessors ints.
    * This function divides the local_data into partitions according to the passed in pivots and stores them in the passed in partition_array.
    **/
    int i, j = 0, part_size = 0;
    int* part_start = local_data;
    for(i = 0; i < (numProcessors-1); i++){//Iterate over each of the pivots.
        while(local_data[j] <= pivots[i]){
            j += 1;
            part_size += 1;
        }
        memcpy(&partition_array[i][0], part_start, sizeof(int) * part_size);
        part_size = 0;
        part_start = &local_data[j];
    }
    memcpy(&partition_array[numProcessors-1][0], part_start, sizeof(int) * ((dataSize/numProcessors) - j));
    //Put everything to the right of the last pivot into the final partition.
    return;
}


void create_data(int* data, int numData){
    /**
    ** Fills the passed in int array with numData random numbers.
    **/
    data[0] = 16;
    data[1] = 2;
    data[2] = 17;
    data[3] = 24;
    data[4] = 33;
    data[5] = 28;
    data[6] = 30;
    data[7] = 1;
    data[8] = 0;
    data[9] = 27;
    data[10] = 9;
    data[11] = 25;
    data[12] = 34;
    data[13] = 23;
    data[14] = 19;
    data[15] = 18;
    data[16] = 11;
    data[17] = 7;
    data[18] = 21;
    data[19] = 13;
    data[20] = 8;
    data[21] = 35;
    data[22] = 12;
    data[23] = 29;
    data[24] = 6;
    data[25] = 3;
    data[26] = 4;
    data[27] = 14;
    data[28] = 22;
    data[29] = 15;
    data[30] = 32;
    data[31] = 10;
    data[32] = 26;
    data[33] = 31;
    data[34] = 20;
    data[35] = 5;
   /** srandom(20);
    int i;
    for(i = 0; i < numData; i++){
        data[i] = random()%100;
    }**/
    return;
}

int comp_func(const void* item1, const void* item2){
    int* it1, *it2;
    it1 = (int*) item1;
    it2 = (int*) item2;
    return (*it1 - *it2);
}

void merge(int* array1, int* array2, int* resultantArray){
    //Merges 2 arrays and places the result in resultantArray.
    //Taken from http://www.programmingsimplified.com/c/source-code/c-program-merge-two-arrays
    //because I am terrible at C.
    int i, j, k, m = 0, n = 0;
    while(array1[m] != -1){
        m++;
    }
    while(array2[n] != -1){
        n++;
    }
    j = k = 0;
    for(i = 0; i < m + n;){
        if(j < m && k < n){
            if(array1[j] < array2[k]){
                resultantArray[i] = array1[j];
                j++;
            } else {
                resultantArray[i] = array2[k];
                k++;
            }
            i++;
        } else if (j == m){
            for(; i < m+n;){
                resultantArray[i] = array2[k];
                k++;
                i++;
            }
        } else {
            for(; i < m + n;){
                resultantArray[i] = array1[j];
                j++;
                i++;
            }
        }
    }
}

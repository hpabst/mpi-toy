Name: Henry Pabst
CCID: hpabst
Student ID: 1289890

This is the README for CMPUT 481 assignment 2.
There are 2 main files for the assignment, main.c and seq.c.
main.c implements PSRS and seq.c implements sequential quicksort.
Each can be compiled into debug versions by calling make debug
and into final versions by calling make final.
The seq program takes a single input via command-line: the number of
keys with which to sort and outputs timing information to stderr.
The main program takes 2 arguments: the first is the number of processes
the program will be performing PSRS with, and the second is the number of
keys to be sorted. The number of keys to be sorted must be divisible by
the number of processes.
main outputs timing information to a text file called output.txt. This
is because MPICH has a known issue where calling MPI_Finalize() causes all
the processes to segfault. So rather than digging through all the output from
that, it just writes it to a file.

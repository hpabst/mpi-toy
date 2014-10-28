clean:
	rm main

debug:
	mpicc -Wall -g -o main main.c -lm

final:
	mpicc -Wall -O3 -o main main.c -lm
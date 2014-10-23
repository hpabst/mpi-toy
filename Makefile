clean:
	rm main

debug:
	mpicc -Wall -g -o main main.c -lm
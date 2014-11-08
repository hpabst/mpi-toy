clean:
	rm main
	rm seq

debug:
	mpicc -Wall -g -o main main.c -lm
	gcc -Wall -g -o seq seq.c

final:
	mpicc -Wall -O3 -o main main.c -lm
	gcc -Wall -O3 -o seq seq.c
s-talk: list.o main.o
	gcc -o s-talk -pthread list.o main.o

main.o: main.c list.h
	gcc -c main.c list.h

clean:
	rm main.o



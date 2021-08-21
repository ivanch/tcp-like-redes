
all: clean altbit gbn

clean:
	rm -f altbit gbn

altbit:
	gcc altbit.c -o altbit

gbn:
	gcc gbn.c -o gbn

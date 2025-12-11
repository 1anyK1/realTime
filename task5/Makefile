CC = gcc
CFLAGS = -Wall -std=c99 -Wextra -I./src
LDFLAGS = -lrt

.PHONY: all clean test

all: task1_latency task2_mlock task3_benchmark

task1_latency: src/task1_latency.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

task2_mlock: src/task2_mlock.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

task3_benchmark: src/task3_benchmark.c src/mempool.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test:
	sudo ./task1_latency > task1_test.txt
	sudo ./task2_mlock > task2_test.txt

plot:
	python3 plot_create.py task1_test.txt
	python3 plot_create.py task2_test.txt

clean:
	rm -f task1_latency task2_mlock task3_benchmark *.txt

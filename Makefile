all : circular_queue_example

circular_queue_example : c_circular_queue.c example.c
	gcc -g -Wall -o $@ $^ -lpthread

clean :
	rm circular_queue_example

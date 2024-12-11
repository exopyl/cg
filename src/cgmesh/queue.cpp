#include <stdlib.h>

#include "queue.h"

Queue::Queue()
{
	ndata = 1000000;
	data = (int*)malloc(ndata*sizeof(int));
    currentSize = 0;
    front = 0;
    back = -1;
}

int
Queue::isEmpty (void)
{
    return currentSize == 0;
}

int
Queue::isFull (void)
{
    return currentSize == ndata;
}

int
Queue::get (void)
{
	if (isEmpty()) return -1;

    currentSize--;
    int ret = data[front];
    front++;
    return ret;
}

void
Queue::add (int value)
{
	if (isFull())
	{
		ndata *= 2;
		data = (int*)realloc ((void*)data, ndata*sizeof(int));
	}
    data[++back] = value;
    currentSize++;
}

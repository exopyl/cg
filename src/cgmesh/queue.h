#ifndef __QUEUE_H__
#define __QUEUE_H__

// Queue class -- array implementation
//
// CONSTRUCTION: with or without a capacity; default is 10
//
// ******************PUBLIC OPERATIONS*********************
// void enqueue( x )      --> Insert x
// void dequeue( )--> Return and remove least recently inserted item
// Object getFront( )     --> Return least recently inserted item
// bool isEmpty( )--> Return true if empty; else false
// bool isFull( ) --> Return true if full; else false
// void makeEmpty( )      --> Remove all items
// ******************ERRORS********************************
// Overflow and Underflow thrown as needed

//template <class Object>
class Queue
{
  public:
	Queue ();

    int isEmpty (void);
    int isFull (void);

	void add (int value);
	int  get (void);

  private:
	int *data;
	int  ndata;
    int  currentSize;
    int  front;
    int  back;
};

#endif /* __QUEUE_H__ */

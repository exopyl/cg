#include <stdlib.h>

#include "garray.h"

Cgarray::Cgarray ()
{
  size     = 0;
  size_max = 0;
  data     = NULL;
}

Cgarray::~Cgarray ()
{
  free (data);
}

int Cgarray::get_size (void)
{
  return size;
}

void* Cgarray::get_data (int index)
{
  return (index<size && index>=0)? data[index] : NULL;
}

int Cgarray::set_data (void *element, int index)
{
  if (index<0)
    return -1;
  if (index<size)
    data[index] = element;
  else /* i >= array->size_max */
  {
    while (index >= size_max)
      size_max++;
    data = (void**)realloc(data, size_max*sizeof(void*));
    if (data == NULL)
      return -1;
    data[index] = element;
  }
  if (size<=index)
    size = index+1;
  return 1;
}

int Cgarray::add (void *element)
{
  return this->set_data (element, size);
}

int Cgarray::contains (void *element)
{
  int i=0;
  for (i=0; i<size; i++)
    if (data[i] == element)
      return i;
  return -1;
}

int Cgarray::add_unique (void *element)
{
  if (contains (element) < 0)
    return add (element);
  return 1;
}

int Cgarray::del_index (int i)
{
  if (i<0 || i>=size)
    return -1;
  size--;
  while (i<size)
    {
      data[i] = data[i+1];
      i++;
    }
  return 1;
}

int Cgarray::del_data (void *element)
{
  int rank = contains (element);
  if (rank >= 0)
    return del_index (rank);
  return 1;
}

/*
garray_t garray_copy (garray_t array)
{
  garray_t clone;
  int i, n;
  assert (array);

  clone = garray_init (array->size_max);
  assert (clone);

  n = garray_get_size (array);
  for (i=0; i<n; i++)
    garray_add (clone, garray_get_value(array, i));
  return clone;
}
*/

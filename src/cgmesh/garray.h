#ifndef __GARRAY2_H__
#define __GARRAY2_H__

class Cgarray
{
 public:
  Cgarray ();
  ~Cgarray();
  
  int   get_size   (void);
  int   set_data   (void *element, int index);
  void* get_data   (int index);
  int   add        (void *element);
  int   add_unique (void *element);
  int   contains   (void *element);
  int   del_index  (int index);
  int   del_data   (void *element);
  
 private:
  int   size;
  int   size_max;
  void  **data;
};

#endif // __GARRAY_H__

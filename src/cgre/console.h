#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "gl_wrapper.h"

extern void font_init (void);
extern void font_destroy (void);
extern void font_print (float x, float y, char *text, ...);

class Console
{
 public:
	static Console* getInstance (void);

  void set_size_screen (unsigned int width, unsigned height);
  void set_font (void *font);
  void set_color (float r, float g, float b);

  void print (int x, int y, char *fmt, ...);

 private:
	static Console *m_pInstance;
  Console ();
  ~Console ();


  char *font;
  /*
    GLUT_BITMAP_9_BY_15
    GLUT_BITMAP_8_BY_13
    GLUT_BITMAP_TIMES_ROMAN_10
    GLUT_BITMAP_TIMES_ROMAN_24
    GLUT_BITMAP_HELVETICA_10
    GLUT_BITMAP_HELVETICA_12
    GLUT_BITMAP_HELVETICA_18
  */
  float r, g, b;
  
  unsigned int width, height;
};

#endif // __CONSOLE_H__

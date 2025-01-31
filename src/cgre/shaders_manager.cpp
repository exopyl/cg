#ifdef WIN32

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstdio>

//#define ACTIVATE_REPORT
#ifdef ACTIVATE_REPORT
#define PRINT_OPENGL_ERROR() report_opengl_error(__FILE__, __LINE__)
//#define PRINT_OPENGL_ERROR() print_opengl_error(__FILE__, __LINE__)
#else
#define PRINT_OPENGL_ERROR() void()
#endif

#include "shaders_manager.h"

ShadersManager *ShadersManager::m_pInstance = new ShadersManager;

#define BUFFER_OFFSET(bytes) ((GLubyte*) NULL + (bytes))

//
// print_shader_info_log
//
void ShadersManager::print_shader_info_log (GLuint shader)
{
  int     info_log_length = 0;
  int     chars_written  = 0;
  GLchar *info_log;

  glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &info_log_length); PRINT_OPENGL_ERROR ();

  if (info_log_length > 1)
    {
      info_log = new GLchar[info_log_length];
      glGetShaderInfoLog (shader, info_log_length, &chars_written, info_log); PRINT_OPENGL_ERROR ();
	  this->report_info ("Shader InfoLog: ");
	  this->report_info (info_log);
	  this->report_info ("\n");
      delete[] info_log;
    }
}

//
// print_program_info_log
//
void ShadersManager::print_program_info_log (GLuint program)
{
  int     info_log_length = 0;
  int     chars_written  = 0;
  GLchar *info_log;

  glGetProgramiv (program, GL_INFO_LOG_LENGTH, &info_log_length); PRINT_OPENGL_ERROR ();

  if (info_log_length > 1)
  {
      info_log = new GLchar[info_log_length];
      glGetProgramInfoLog (program, info_log_length, &chars_written, info_log); PRINT_OPENGL_ERROR ();
	  this->report_info ("Program InfoLog: ");
	  this->report_info (info_log);
	  this->report_info ("\n");
      delete[] info_log;
  }
  PRINT_OPENGL_ERROR ();  // Check for OpenGL errors
}

//
// get_uni_loc
//

GLint ShadersManager::getUniformLocation (GLuint program, const char *name)
{
  GLint loc = glGetUniformLocation (program, name); PRINT_OPENGL_ERROR ();
  if (loc == -1)
    std::cerr << "No such uniform named \"" << name << "\"" << std::endl;

  return loc;
}

//
// get_file_content
//
static std::string get_file_content (const std::string &filename)
{
  std::string ret;
  if(FILE *fp = fopen(filename.c_str (), "r"))
    {
      char buf[1024];
      while (size_t len = fread (buf, 1, sizeof(buf), fp))
          ret += std::string (buf, buf + len);
      fclose (fp);
    }
  return ret;
}

//
// read_shader
//
GLuint ShadersManager::AddShader (	const std::string &vertex_filename,
									const std::string &fragment_filename,
									const std::string &geometry_filename )
{
  GLint        status;
  std::string  source;
  const char  *cstring;
  GLuint       program;

  program = glCreateProgram (); PRINT_OPENGL_ERROR ();

  // Create the vertex shader
  if (vertex_filename != "")
	{
      GLuint handle = glCreateShader (GL_VERTEX_SHADER); PRINT_OPENGL_ERROR ();
      source = get_file_content (vertex_filename);
      cstring = source.c_str ();
      glShaderSource (handle, 1, &cstring, NULL);   PRINT_OPENGL_ERROR ();

      // Compile the vertex shader, and print out the compiler log file
      glCompileShader (handle); PRINT_OPENGL_ERROR ();
      glGetShaderiv (handle, GL_COMPILE_STATUS, &status); PRINT_OPENGL_ERROR ();
      print_shader_info_log (handle);
      glAttachShader (program, handle); PRINT_OPENGL_ERROR ();
      if (!status) return 0;
    }

  // Create the fragment shader
  if (fragment_filename != "")
    {
      GLuint handle = glCreateShader (GL_FRAGMENT_SHADER); PRINT_OPENGL_ERROR ();
      source = get_file_content (fragment_filename);
      cstring = source.c_str ();
      glShaderSource (handle, 1, &cstring, NULL); PRINT_OPENGL_ERROR ();
      
	  // Compile the fragment shader, and print out the compiler log file
      glCompileShader (handle); PRINT_OPENGL_ERROR ();
      glGetShaderiv (handle, GL_COMPILE_STATUS, &status); PRINT_OPENGL_ERROR ();
      print_shader_info_log (handle);
      glAttachShader (program, handle); PRINT_OPENGL_ERROR ();
      if (!status) return 0;
    }

  // Create the geometry shader
  if (geometry_filename != "")
    {
      GLuint handle = glCreateShader (GL_GEOMETRY_SHADER); PRINT_OPENGL_ERROR ();
      source = get_file_content (geometry_filename);
      cstring = source.c_str ();
      glShaderSource (handle, 1, &cstring, NULL); PRINT_OPENGL_ERROR ();
      
	  // Compile the geometry shader, and print out the compiler log file
      glCompileShader (handle); PRINT_OPENGL_ERROR ();
      glGetShaderiv (handle, GL_COMPILE_STATUS, &status); PRINT_OPENGL_ERROR ();
      print_shader_info_log (handle);
      glAttachShader (program, handle); PRINT_OPENGL_ERROR ();
      if (!status) return 0;
    }
 
  return program;
}

bool ShadersManager::LinkShader (GLuint program)
{
	GLint status;
	glLinkProgram (program); PRINT_OPENGL_ERROR ();
	glGetProgramiv (program, GL_LINK_STATUS, &status); PRINT_OPENGL_ERROR ();
	print_program_info_log (program);
	return true;
}

bool ShadersManager::ActivateShader (GLuint program)
{
	// Link the program object and print out the info log
	glUseProgram (program); PRINT_OPENGL_ERROR ();

	return true;
}

bool ShadersManager::DesactivateShader ()
{
	glUseProgram (0);

	return true;
}

ShadersManager::ShadersManager()
{
	m_nEffects = 0;
}

ShadersManager::~ShadersManager()
{
}

void ShadersManager::Initialize (void)
{
}

//
// reporting
//
void ShadersManager::report_info (char *info)
{
#ifdef ACTIVATE_REPORT
	FILE *ptr = fopen (m_file_report, "a");
	fprintf (ptr, "%s", info);
	fclose (ptr);
#else
	printf ("%s\n", info);
#endif
}

void ShadersManager::report_opengl_error (const char *file, int line)
{
#ifdef ACTIVATE_REPORT
	GLenum error = glGetError ();
	while (error != GL_NO_ERROR)
	{
		char *info = (char*)malloc(1024*sizeof(char));
		sprintf (info, "glError in file %s line %d : %s\n", file, line, gluErrorString (error));
		report_info (info);
		error = glGetError ();
	}
#endif
}

#else

#include <stdlib.h>
#include "shaders_manager.h"
ShadersManager *ShadersManager::m_pInstance = NULL;

#endif

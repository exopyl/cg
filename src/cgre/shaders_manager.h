#pragma once

#include <string>

#include "gl_wrapper.h"

//
//
//
class ShadersManager
{
#ifdef WIN32
  typedef struct effect_s {
		GLuint program;
		GLuint handle_vertex_shader;
		GLuint handle_fragment_shader;
		GLuint handle_geometry_shader;
	} effect_s;
#endif
private:
	ShadersManager();
	~ShadersManager ();

public:
	static ShadersManager* getInstance (void) { return m_pInstance; };
#ifdef WIN32
	void Initialize (void);

	GLuint AddShader (const std::string &vertex_filename, const std::string &fragment_filename, const std::string &geometry_filename);
	bool LinkShader (GLuint program);
	bool ActivateShader (GLuint);
	bool DesactivateShader();

	void ShadersManager::print_shader_info_log (GLuint shader);
	void print_program_info_log (GLuint program);
	GLint getUniformLocation (GLuint program, const char *name);

private:
	char *m_file_report;

	int m_nEffects;
	effect_s pEffects[8];

	// reporting
	void report_info (char *info);
	void report_opengl_error (const char *file, int line);
#endif
	static ShadersManager *m_pInstance;
};

#include "gl_program.h"

#include "diagnostics.h"

#include <utility>
#include <vector>

namespace cgre
{

namespace
{
	const char* ShaderTypeName (GLenum type)
	{
		switch (type)
		{
		case GL_VERTEX_SHADER:          return "vertex";
		case GL_FRAGMENT_SHADER:        return "fragment";
		case GL_GEOMETRY_SHADER:        return "geometry";
		case GL_TESS_CONTROL_SHADER:    return "tess-control";
		case GL_TESS_EVALUATION_SHADER: return "tess-evaluation";
		case GL_COMPUTE_SHADER:         return "compute";
		default:                        return "?";
		}
	}

	std::string ShaderInfoLog (GLuint shader)
	{
		GLint length = 0;
		glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &length);
		if (length <= 1)
			return std::string ();

		std::vector<char> log (static_cast<size_t> (length));
		GLsizei written = 0;
		glGetShaderInfoLog (shader, length, &written, log.data ());
		return std::string (log.data (), static_cast<size_t> (written));
	}

	std::string ProgramInfoLog (GLuint program)
	{
		GLint length = 0;
		glGetProgramiv (program, GL_INFO_LOG_LENGTH, &length);
		if (length <= 1)
			return std::string ();

		std::vector<char> log (static_cast<size_t> (length));
		GLsizei written = 0;
		glGetProgramInfoLog (program, length, &written, log.data ());
		return std::string (log.data (), static_cast<size_t> (written));
	}
}

GlProgram::~GlProgram ()
{
	Release ();
}

GlProgram::GlProgram (GlProgram&& other) noexcept
	: m_program (other.m_program)
	, m_uniformCache (std::move (other.m_uniformCache))
{
	other.m_program = 0;
	other.m_uniformCache.clear ();
}

GlProgram& GlProgram::operator= (GlProgram&& other) noexcept
{
	if (this != &other)
	{
		Release ();
		m_program = other.m_program;
		m_uniformCache = std::move (other.m_uniformCache);
		other.m_program = 0;
		other.m_uniformCache.clear ();
	}
	return *this;
}

void GlProgram::Release ()
{
	if (m_program != 0)
	{
		glDeleteProgram (m_program);
		m_program = 0;
	}
	m_uniformCache.clear ();
}

bool GlProgram::Build (const std::map<GLenum, std::string>& sources, const char* debugName)
{
	const char* name = debugName ? debugName : "sans-nom";

	if (sources.empty ())
	{
		Logf ("shader « %s » : aucune source fournie.", name);
		return false;
	}

	// Toute reconstruction repart de zero : le cache d'uniformes de l'ancien
	// programme n'a aucun sens pour le nouveau.
	Release ();

	const GLuint program = glCreateProgram ();
	if (program == 0)
	{
		Logf ("shader « %s » : glCreateProgram a echoue (contexte GL courant ?).", name);
		CGRE_CHECK_GL ("glCreateProgram");
		return false;
	}

	std::vector<GLuint> stages;
	stages.reserve (sources.size ());
	bool allCompiled = true;

	for (const auto& entry : sources)
	{
		const GLenum type   = entry.first;
		const GLuint shader = glCreateShader (type);
		if (shader == 0)
		{
			Logf ("shader « %s » : glCreateShader(%s) a echoue.", name, ShaderTypeName (type));
			allCompiled = false;
			continue;
		}

		const GLchar* source = entry.second.c_str ();
		glShaderSource (shader, 1, &source, nullptr);
		glCompileShader (shader);

		GLint compiled = GL_FALSE;
		glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);

		const std::string log = ShaderInfoLog (shader);
		if (compiled == GL_TRUE)
		{
			// Le pilote peut emettre des avertissements sur un shader valide :
			// les remonter aussi, ils annoncent souvent le bug suivant.
			if (!log.empty ())
				Logf ("shader « %s » (%s) : %s", name, ShaderTypeName (type), log.c_str ());
			glAttachShader (program, shader);
		}
		else
		{
			allCompiled = false;
			Logf ("shader « %s » (%s) NE COMPILE PAS : %s",
			      name, ShaderTypeName (type),
			      log.empty () ? "le pilote ne donne aucun detail." : log.c_str ());
		}

		stages.push_back (shader);
	}

	glLinkProgram (program);

	GLint linked = GL_FALSE;
	glGetProgramiv (program, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE)
	{
		const std::string log = ProgramInfoLog (program);
		Logf ("shader « %s » : EDITION DE LIENS ECHOUEE : %s",
		      name, log.empty () ? "le pilote ne donne aucun detail." : log.c_str ());
	}

	// Detacher puis detruire dans tous les cas : une fois lies, les objets
	// shader ne servent plus, et les garder attaches les maintient en vie.
	for (const GLuint shader : stages)
	{
		glDetachShader (program, shader);
		glDeleteShader (shader);
	}

	if (!allCompiled || linked != GL_TRUE)
	{
		glDeleteProgram (program);
		CGRE_CHECK_GL ("GlProgram::Build (echec)");
		return false;
	}

	m_program = program;
	CGRE_CHECK_GL ("GlProgram::Build");
	return true;
}

void GlProgram::Use () const
{
	glUseProgram (m_program);
}

void GlProgram::Unuse ()
{
	glUseProgram (0);
}

GLint GlProgram::UniformLocation (const std::string& name) const
{
	if (m_program == 0)
		return -1;

	const auto it = m_uniformCache.find (name);
	if (it != m_uniformCache.end ())
		return it->second;

	const GLint location = glGetUniformLocation (m_program, name.c_str ());
	m_uniformCache.emplace (name, location);
	return location;
}

void GlProgram::SetInt (const std::string& name, int value) const
{
	const GLint location = UniformLocation (name);
	if (location >= 0)
		glUniform1i (location, value);
}

void GlProgram::SetFloat (const std::string& name, float value) const
{
	const GLint location = UniformLocation (name);
	if (location >= 0)
		glUniform1f (location, value);
}

void GlProgram::SetVec3 (const std::string& name, const float* v) const
{
	const GLint location = UniformLocation (name);
	if (location >= 0 && v)
		glUniform3fv (location, 1, v);
}

void GlProgram::SetVec4 (const std::string& name, const float* v) const
{
	const GLint location = UniformLocation (name);
	if (location >= 0 && v)
		glUniform4fv (location, 1, v);
}

void GlProgram::SetMat3 (const std::string& name, const float* m) const
{
	const GLint location = UniformLocation (name);
	if (location >= 0 && m)
		glUniformMatrix3fv (location, 1, GL_FALSE, m);
}

void GlProgram::SetMat4 (const std::string& name, const float* m) const
{
	const GLint location = UniformLocation (name);
	// GL_FALSE : cgmath::TMatrix4 est colonne-majeur par defaut, comme GL.
	if (location >= 0 && m)
		glUniformMatrix4fv (location, 1, GL_FALSE, m);
}

} // namespace cgre

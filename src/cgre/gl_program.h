#pragma once

#include "gl_wrapper.h"

#include <map>
#include <string>
#include <unordered_map>

//
// Programme GLSL : compilation, edition de liens, cycle de vie, uniformes.
//
// REMPLACE trois mecanismes concurrents dont aucun ne fonctionnait :
//   - GL_Shader / ShaderWireframe / ShaderBackground (shader.cpp, 857 LOC) :
//     jamais instancie, Execute() sous #if 0, sources GLSL concatenees SANS
//     saut de ligne -- « #version 330 core » collait a la directive suivante,
//     donc ce shader n'a jamais pu compiler ;
//   - ShadersManager (234 LOC) : LinkShader renvoyait true inconditionnellement,
//     le registre pEffects[8] annonce n'etait jamais rempli, les objets shader
//     jamais detruits, et Initialize() -- le seul appel de sinaia -- avait un
//     corps vide ;
//   - shaders/GL_ShadersManager (2 112 LOC) : inconstructible, depend d'un
//     repertoire ViewVP/ absent du depot.
//
// La seule piece correcte des trois, GL_Shader::CompileAndLinkShaders, est
// reprise ici : elle testait bien GL_COMPILE_STATUS puis GL_LINK_STATUS, et
// detachait puis detruisait ses objets shader apres l'edition de liens.
//
// PARTAGE DE CONTEXTE. Un programme est un objet de DONNEES : il appartient au
// groupe de partage GL, donc un GlProgram cree dans un onglet de sinaia est
// utilisable dans les autres (cf. MyGLCanvas::SetSharedContext). Ce n'est PAS le
// cas des VAO ni des FBO, qui restent propres a chaque contexte.
//
namespace cgre
{

class GlProgram
{
public:
	GlProgram () = default;
	~GlProgram ();

	// Possede un GLuint : la copie serait un partage silencieux suivi d'une
	// double suppression. Deplacable, non copiable.
	GlProgram (const GlProgram&)            = delete;
	GlProgram& operator= (const GlProgram&) = delete;
	GlProgram (GlProgram&& other) noexcept;
	GlProgram& operator= (GlProgram&& other) noexcept;

	// sources : { GL_VERTEX_SHADER, code }, { GL_FRAGMENT_SHADER, code }, ...
	// `debugName` n'apparait que dans les messages de diagnostic.
	//
	// En cas d'echec, journalise le log de compilation ou d'edition de liens du
	// pilote et laisse l'objet invalide -- l'ancien programme, s'il y en avait
	// un, est de toute facon detruit. Renvoie false : le silence sur un shader
	// qui ne compile pas est precisement ce qui rendait l'ancien code
	// indebogable.
	bool Build (const std::map<GLenum, std::string>& sources,
	            const char* debugName = nullptr);

	bool   IsValid () const { return m_program != 0; }
	GLuint Id ()      const { return m_program; }

	void        Use () const;
	static void Unuse ();

	// -1 si l'uniforme est absent ou optimise par le compilateur GLSL.
	// Resolu une seule fois puis mis en cache : glGetUniformLocation est une
	// recherche par chaine cote pilote, a ne surtout pas refaire par image.
	GLint UniformLocation (const std::string& name) const;

	// Poseurs surs : sans effet si l'uniforme est absent, donc un shader qui
	// n'utilise pas encore une entree ne fait pas echouer l'appelant.
	//
	// PRECONDITION : le programme doit etre courant (Use () appele). glUniform*
	// s'applique au programme lie, pas a `this`.
	void SetInt   (const std::string& name, int value)     const;
	void SetFloat (const std::string& name, float value)   const;
	void SetVec3  (const std::string& name, const float* v) const;
	void SetVec4  (const std::string& name, const float* v) const;
	void SetMat3  (const std::string& name, const float* m) const;
	void SetMat4  (const std::string& name, const float* m) const;

	void Release ();

private:
	GLuint m_program = 0;
	mutable std::unordered_map<std::string, GLint> m_uniformCache;
};

} // namespace cgre

#pragma once

#include <string>
#include <vector>
#include <map>
#include <../cgmesh/material.h>
#include <../cgmath/TMatrix4.h>
#include <../cgmath/TVector4.h>

#include "gl_wrapper.h"

typedef enum variable_type { floatType, intType, uIntType } variable_type;
typedef enum shader_type {
	shaderWireframe,
	shaderBackground
} shader_type;

class GL_Shader
{
public:
	
	virtual bool LoadShadersFromString() = 0;
	virtual bool LoadShadersFromFiles(const std::string& shaderDirectory = std::string("")) = 0;
	virtual void Execute() const = 0;

protected:
	GL_Shader(shader_type shaderType);
	virtual ~GL_Shader();

	shader_type m_shaderType;
	unsigned int m_shaderID;
	bool m_isValidShader;

	unsigned int GetShaderID() const;

	bool GetGLProcs();

	// common setters
	
	void SetMaterial(const MaterialColor& material) const;

	// methods to load files
	bool LoadShaders();
	bool CompileAndLinkShaders(const std::map<GLenum, std::string>& shaderNames, bool fromString);
	static std::string LoadSourceStringFromFile(const std::string& fileSourceName);
	void PrintProgramInfoLog() const;
	void PrintShaderInfoLog(GLuint shaderID) const;

	void SetBasicMatrices(const Matrix4f& modelviewPassMatrix) const;

	void BindAttributes(int& attribLoc, const char* name) const;
	void SetShaderVariables() const;

	void ActivateShader() const;
	void DeactivateShader() const;
	void SetMatrices(const Matrix4f& modelviewPassMatrix) const;
	void SetPolygonOffset(bool cancelGeometricOffset = false) const;
	void ActivateAndBindTexture(GLenum targetType, GLuint textureID, const char* texName) const;
	int GetAttribLocation(const char* name) const;
	int GetUniformLocation(const char* name) const;
	void SetGLUniformMatrixV(const char* name, GLsizei count, GLboolean transpose, const GLfloat* pValue, unsigned short columnNum, unsigned short rowNum = 0) const;
	void SetGLUniformVariable(const char* name, GLfloat v0) const;
	void SetGLUniformVariable(const char* name, GLint v0) const;
	void SetGLUniformVariable(const char* name, GLuint v0) const;
	void SetGLUniformXV(const char* name, ::variable_type type, unsigned short elemNum, unsigned short count, const GLfloat* pVarPtr) const;
	void DisableAttribArrays(int locs[], int size) const;
	
	static HGLRC m_curContext;
};

// wireframe
class ShaderWireframe : public GL_Shader
{
public:
	ShaderWireframe();
	virtual bool LoadShadersFromString() override;
	virtual bool LoadShadersFromFiles(const std::string& shaderDirectory = std::string(""));

	virtual void Execute() const override;
};

// background
class ShaderBackground : public GL_Shader
{
public:
	ShaderBackground();
	virtual ~ShaderBackground();
	virtual bool LoadShadersFromString() override;
	virtual bool LoadShadersFromFiles(const std::string& shaderDirectory = std::string("")) override;
	virtual void Execute() const override;
};

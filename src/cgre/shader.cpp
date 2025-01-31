#include "shader.h"

#include "gl_wrapper.h"
#include "shaders_manager.h"
#include "capabilities_manager.h"

#include <fstream>

static PFNGLCREATEPROGRAMPROC   glCreateProgram = NULL;
static PFNGLCREATESHADERPROC    glCreateShader = NULL;
static PFNGLSHADERSOURCEPROC	glShaderSource = NULL;
static PFNGLCOMPILESHADERPROC	glCompileShader = NULL;
static PFNGLGETSHADERIVPROC		glGetShaderiv = NULL;
static PFNGLATTACHSHADERPROC	glAttachShader = NULL;
static PFNGLDETACHSHADERPROC	glDetachShader = NULL;
static PFNGLDELETEPROGRAMPROC   glDeleteProgram = NULL;
static PFNGLLINKPROGRAMPROC		glLinkProgram = NULL;
static PFNGLUSEPROGRAMPROC		glUseProgram = NULL;
static PFNGLDELETESHADERPROC	glDeleteShader = NULL;
static PFNGLGETPROGRAMIVPROC    glGetProgramiv = NULL;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
static PFNGLUNIFORM1IPROC glUniform1i = NULL;
static PFNGLUNIFORM1UIPROC glUniform1ui = NULL;
static PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = NULL;
static PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;
static PFNGLUNIFORM1FPROC glUniform1f = NULL;
static PFNGLUNIFORM3FPROC glUniform3f = NULL;
static PFNGLGENBUFFERSPROC glGenBuffers = NULL;
static PFNGLBINDBUFFERPROC glBindBuffer = NULL;
static PFNGLBUFFERDATAPROC glBufferData = NULL;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = NULL;
static PFNGLUNIFORM4FVPROC glUniform4fv = NULL;
static PFNGLUNIFORM3FVPROC glUniform3fv = NULL;
static PFNGLUNIFORM2FVPROC glUniform2fv = NULL;
static PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
static PFNGLMEMORYBARRIERPROC glMemoryBarrier = NULL;
static PFNGLBINDIMAGETEXTUREPROC glBindImageTexture = NULL;
static PFNGLPATCHPARAMETERIPROC glPatchParameteri = NULL;
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
static PFNGLBLENDEQUATIONPROC glBlendEquation = NULL;
static PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture = NULL;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
static PFNGLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
static PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
static PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
static PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;


static bool verbose = false;

static void DefineProc()
{
	static bool done = false;

	if (!done)
	{

		glCreateProgram = (PFNGLCREATEPROGRAMPROC)(void*)wglGetProcAddress("glCreateProgram");
		glCreateShader = (PFNGLCREATESHADERPROC)(void*)wglGetProcAddress("glCreateShader");
		glShaderSource = (PFNGLSHADERSOURCEPROC)(void*)wglGetProcAddress("glShaderSource");
		glCompileShader = (PFNGLCOMPILESHADERPROC)(void*)wglGetProcAddress("glCompileShader");
		glGetShaderiv = (PFNGLGETSHADERIVPROC)(void*)wglGetProcAddress("glGetShaderiv");
		glAttachShader = (PFNGLATTACHSHADERPROC)(void*)wglGetProcAddress("glAttachShader");
		glDetachShader = (PFNGLDETACHSHADERPROC)(void*)wglGetProcAddress("glDetachShader");
		glDeleteProgram = (PFNGLDELETEPROGRAMPROC)(void*)wglGetProcAddress("glDeleteProgram");
		glLinkProgram = (PFNGLLINKPROGRAMPROC)(void*)wglGetProcAddress("glLinkProgram");
		glUseProgram = (PFNGLUSEPROGRAMPROC)(void*)wglGetProcAddress("glUseProgram");
		glDeleteShader = (PFNGLDELETESHADERPROC)(void*)wglGetProcAddress("glDeleteShader");
		glGetProgramiv = (PFNGLGETPROGRAMIVPROC)(void*)wglGetProcAddress("glGetProgramiv");
		glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)(void*)wglGetProcAddress("glGetShaderInfoLog");
		glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)(void*)wglGetProcAddress("glGetProgramInfoLog");;
		glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)(void*)wglGetProcAddress("glGetUniformLocation");
		glUniform1i = (PFNGLUNIFORM1IPROC)(void*)wglGetProcAddress("glUniform1i");
		glUniform1ui = (PFNGLUNIFORM1UIPROC)(void*)wglGetProcAddress("glUniform1ui");
		glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)(void*)wglGetProcAddress("glGetAttribLocation");
		glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)(void*)wglGetProcAddress("glUniformMatrix4fv");
		glUniform1f = (PFNGLUNIFORM1FPROC)(void*)wglGetProcAddress("glUniform1f");
		glUniform3f = (PFNGLUNIFORM3FPROC)(void*)wglGetProcAddress("glUniform3f");
		glGenBuffers = (PFNGLGENBUFFERSPROC)(void*)wglGetProcAddress("glGenBuffers");
		glBindBuffer = (PFNGLBINDBUFFERPROC)(void*)wglGetProcAddress("glBindBuffer");
		glBufferData = (PFNGLBUFFERDATAPROC)(void*)wglGetProcAddress("glBufferData");
		glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)(void*)wglGetProcAddress("glBindVertexArray");
		glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)(void*)wglGetProcAddress("glVertexAttribPointer");
		glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)(void*)wglGetProcAddress("glEnableVertexAttribArray");
		glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)(void*)wglGetProcAddress("glDisableVertexAttribArray");
		glUniform4fv = (PFNGLUNIFORM4FVPROC)(void*)wglGetProcAddress("glUniform4fv");
		glUniform3fv = (PFNGLUNIFORM3FVPROC)(void*)wglGetProcAddress("glUniform3fv");
		glUniform2fv = (PFNGLUNIFORM2FVPROC)(void*)wglGetProcAddress("glUniform2fv");
		glActiveTexture = (PFNGLACTIVETEXTUREPROC)(void*)wglGetProcAddress("glActiveTexture");
		glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)(void*)wglGetProcAddress("glMemoryBarrier");
		glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)(void*)wglGetProcAddress("glBindImageTexture");
		glPatchParameteri = (PFNGLPATCHPARAMETERIPROC)(void*)wglGetProcAddress("glPatchParameteri");
		glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)(void*)wglGetProcAddress("glGenFramebuffers");
		glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)(void*)wglGetProcAddress("glDeleteFramebuffers");
		glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)(void*)wglGetProcAddress("glBindFramebuffer");
		glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)(void*)wglGetProcAddress("glFramebufferTexture2D");
		glBlendEquation = (PFNGLBLENDEQUATIONPROC)(void*)wglGetProcAddress("glBlendEquation");
		glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)(void*)wglGetProcAddress("glFramebufferTexture");
		glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)(void*)wglGetProcAddress("glCheckFramebufferStatus");
		glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)(void*)wglGetProcAddress("glGenVertexArrays");
		glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)wglGetProcAddress("glGenerateMipmap");
		glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)wglGetProcAddress("glGenRenderbuffers");
		glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)wglGetProcAddress("glBindRenderbuffer");
		glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)wglGetProcAddress("glRenderbufferStorage");
		glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)wglGetProcAddress("glFramebufferRenderbuffer");
		done = true;
	}
}


HGLRC GL_Shader::m_curContext = NULL;

bool GL_Shader::GetGLProcs()
{
	if (m_curContext == wglGetCurrentContext())
		return true;

	m_curContext = wglGetCurrentContext();

	float versionNumber = CapabilitiesManager::getInstance()->GetVersion();
	if (versionNumber == 0.f)
		return false;

	DefineProc();

	return true;
}

GL_Shader::GL_Shader(shader_type shaderType)
	: m_isValidShader(false), m_shaderID(0), m_shaderType(shaderType)
{
}

GL_Shader::~GL_Shader()
{
	if (GetShaderID())
		glDeleteProgram(m_shaderID);
}

bool GL_Shader::LoadShaders()
{
	bool bLoaded;

	bLoaded = LoadShadersFromString();

	//const std::string& shaderDirectory = GL_ShadersManager::GetInstance()->GetShaderDirectory();
	//bLoaded = LoadShadersFromFiles(shaderDirectory);

	return bLoaded;		
}

bool GL_Shader::CompileAndLinkShaders(const std::map<GLenum, std::string>& shaderNames, bool fromString)
{
	std::vector<GLuint> shaderIdList;
	bool bRes = GetGLProcs();
	if (bRes == false)
		return false;

	if (m_shaderID > 0)
		glDeleteProgram(m_shaderID);

	m_shaderID = glCreateProgram();
	if (m_shaderID == 0)
		return false;

	bool success = true;
	//for (int i = 0; i < shaderNames.size(); i++)
	for (auto& elt : shaderNames)
	{
		GLenum type = elt.first;
		GLuint shaderID = glCreateShader(type);

		
		std::string source = elt.second;
		//source = LoadSourceStringFromFile(source);

		const GLchar* shaderSource = source.c_str();

		glShaderSource(shaderID, 1, (const GLchar**)&shaderSource, 0);

		glCompileShader(shaderID);

		int compiled;
		glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compiled);
		success = success && compiled;

		if (compiled)
			glAttachShader(m_shaderID, shaderID);

		PrintShaderInfoLog(shaderID);

		shaderIdList.push_back(shaderID);
	}

	// link program
	glLinkProgram(m_shaderID);

	int isLinked;
	glGetProgramiv(m_shaderID, GL_LINK_STATUS, &isLinked);
	if (!isLinked)
		PrintProgramInfoLog();

	success = success && isLinked;
	m_isValidShader = isLinked == GL_TRUE;
	for (int i = 0; i < shaderIdList.size(); i++)
	{
		glDetachShader(m_shaderID, shaderIdList[i]);
		glDeleteShader(shaderIdList[i]);
	}

	return success;
}

std::string GL_Shader::LoadSourceStringFromFile(const std::string& fileSourceName)
{
	std::string source = "";

	std::ifstream in(fileSourceName.c_str(), std::ios::in);

	if (!in)
	{
		std::cerr << "[Shader] File not found : " << fileSourceName << std::endl;
		return source;
	}

	const int maxBuffersize = 2048;
	char buffer[maxBuffersize];
	while (in.getline(buffer, maxBuffersize))
	{
		source += std::string(buffer) + "\n";
	}

	return source;
}

void GL_Shader::PrintShaderInfoLog(GLuint shaderID) const
{
	GLint infoLen = 0;
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infoLen);
	if (infoLen > 1)
	{
		char* infoLog = (char*)malloc(sizeof(char) * infoLen);
		glGetShaderInfoLog(shaderID, infoLen, NULL, infoLog);
		printf("\n[SHADER] shader compilation log : ");
		printf("%d : \n", m_shaderType);
		printf("%s \n\n", infoLog);
		free(infoLog);
	}
}

void GL_Shader::PrintProgramInfoLog() const
{
	GLint infoLen = 0;
	glGetProgramiv(m_shaderID, GL_INFO_LOG_LENGTH, &infoLen);
	if (infoLen > 1)
	{
		char* infoLog = (char*)malloc(sizeof(char) * infoLen);
		glGetProgramInfoLog(m_shaderID, infoLen, NULL, infoLog);
		std::cerr << "[Shader] Error linking program " << m_shaderType << ": " << infoLog << std::endl;
		free(infoLog);
	}
}

GLuint GL_Shader::GetShaderID() const
{
	return m_shaderID;
}

void GL_Shader::ActivateShader() const
{
	glUseProgram(m_shaderID);
}

void GL_Shader::DeactivateShader() const
{
	glUseProgram(0);
}

// http://bakura.developpez.com/tutoriels/jeux/utilisation-vbo-avec-opengl-3-x/
void GL_Shader::BindAttributes(int& attribLoc, const char* name) const
{
	attribLoc = GetAttribLocation(name);
	if (attribLoc < 0)
		return;
#if 0
	GLuint& vertexBufferId = floatBlock.bufferId();
	HGLRC& vboContext = floatBlock.glContext();
	if (vertexBufferId == 0)
	{
		vboContext = wglGetCurrentContext();
		glGenBuffers(1, &vertexBufferId);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
		glBufferData(GL_ARRAY_BUFFER, floatBlock.m_size_elem() * floatBlock.nb_elem() * sizeof(float), (GLfloat*)floatBlock.w_first(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
	glEnableVertexAttribArray(attribLoc);
	glVertexAttribPointer(attribLoc, floatBlock.m_size_elem(), GL_FLOAT, GL_FALSE, 0, (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif
}

void GL_Shader::DisableAttribArrays(int locs[], int size) const
{
	for (int i = 0; i < size; i++)
	{
		if (locs[i] >= 0)
			glDisableVertexAttribArray(locs[i]);
	}
}

int GL_Shader::GetUniformLocation(const char* name) const
{
	return glGetUniformLocation(m_shaderID, name);
}

void GL_Shader::ActivateAndBindTexture(GLenum targetType, GLuint textureID, const char* texName) const
{
#if 0
	GLint activeTextureId = ShadersManager::getInstance()->GetActiveTextureId(std::string(texName));
	GLint whichID = GetBindingTextureId(texName);
	glUniform1i(whichID, activeTextureId);
	glActiveTexture(GL_TEXTURE0 + activeTextureId);
	glBindTexture(targetType, textureID);
	glActiveTexture(GL_TEXTURE0);
#endif
}

int GL_Shader::GetAttribLocation(const char* name) const
{
	return glGetAttribLocation(m_shaderID, name);
}

void GL_Shader::SetGLUniformMatrixV(const char* name, GLsizei count, GLboolean transpose, const GLfloat* pValue, unsigned short columnNum, unsigned short rowNum) const
{
	int location = GetUniformLocation(name);
	if (location < 0)
	{
		if (verbose)
			std::cout << "Warning: " << name << " not used.\n";
		return;
	}
	if (rowNum == 0)
	{
		switch (columnNum)
		{
			/*case 2:
			{
				glUniformMatrix2fv(location, count, transpose, value);
				break;
			}
			case 3:
			{
				glUniformMatrix2fv(location, count, transpose, value);
				break;
			}*/
		case 4:
		{
			glUniformMatrix4fv(location, count, transpose, pValue);
			break;
		}
		}
	}
}

void GL_Shader::SetGLUniformVariable(const char* name, GLfloat v0) const
{
	int location = GetUniformLocation(name);
	if (location < 0)
	{
		if (verbose)
			std::cout << "Warning: " << name << " not used.\n";
		return;
	}

	glUniform1f(location, v0);
}

void GL_Shader::SetGLUniformVariable(const char* name, GLint v0) const
{
	int location = GetUniformLocation(name);
	if (location < 0)
	{
		if (verbose)
			std::cout << "Warning: " << name << " not used.\n";
		return;
	}

	glUniform1i(location, v0);
}

void GL_Shader::SetGLUniformVariable(const char* name, GLuint v0) const
{
	int location = GetUniformLocation(name);
	if (location < 0)
	{
		if (verbose)
			std::cout << "Warning: " << name << " not used.\n";
		return;
	}

	glUniform1ui(location, (GLuint)v0);
}

void GL_Shader::SetGLUniformXV(const char* name, ::variable_type type, unsigned short elemNum, unsigned short count, const GLfloat* pVarPtr) const
{
	int location = GetUniformLocation(name);
	if (location < 0)
	{
		if (verbose)
			std::cout << "Warning: " << name << " not used.\n";
		return;
	}

	switch (type)
	{
	case floatType:
	{
		switch (elemNum)
		{
			/*case 1:*/
		case 2:
			glUniform2fv(location, count, (GLfloat*)pVarPtr);
			break;
		case 3:
			glUniform3fv(location, count, (GLfloat*)pVarPtr);
			break;
		case 4:
			glUniform4fv(location, count, (GLfloat*)pVarPtr);
			break;
		}

		break; //floatType
	}
	case intType:
	{
		break;
	}
	case uIntType:
	{
		break;
	}
	}
}

void GL_Shader::SetBasicMatrices(const Matrix4f& modelviewPassMatrix) const
{
	float *modelMatrix, viewMatrix[16], projectionMatrix[16];

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
	glPopMatrix();

	Matrix4f modelMatrixPass(modelviewPassMatrix);

	//modelMatrixPass.to_float_16(modelMatrix);
	modelMatrix = modelMatrixPass.GetMatPtr();

	glGetFloatv(GL_PROJECTION_MATRIX, projectionMatrix);

	SetGLUniformMatrixV("uModelMatrix", 1, GL_FALSE, modelMatrix, 4, 0);
	SetGLUniformMatrixV("uViewMatrix", 1, GL_FALSE, viewMatrix, 4, 0);
	SetGLUniformMatrixV("uProjectionMatrix", 1, GL_FALSE, projectionMatrix, 4, 0);
}

void GL_Shader::SetMatrices(const Matrix4f& modelviewPassMatrix) const
{
	float *modelMatrix, *invTranspModelMatrix, viewMatrix[16], *invViewMatrix, projectionMatrix[16], *invTranspModelViewMatrix;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
	glPopMatrix();

	Matrix4f modelMatrixPass(modelviewPassMatrix), viewMatrixPass(viewMatrix), modelViewMatrixPass(modelMatrixPass * viewMatrixPass);

	// todo : use a cache to store the inverse and the to_float_16 matrices
	modelMatrix = modelMatrixPass.GetMatPtr();
	modelMatrixPass.SetInverse();
	modelMatrixPass.Transpose();
	invTranspModelMatrix = modelMatrixPass.GetMatPtr();

	viewMatrixPass.SetInverse();
	invViewMatrix = viewMatrixPass.GetMatPtr();

	glGetFloatv(GL_PROJECTION_MATRIX, projectionMatrix);

	modelViewMatrixPass.SetInverse();
	modelViewMatrixPass.Transpose();
	invTranspModelViewMatrix = modelViewMatrixPass.GetMatPtr();

	SetGLUniformMatrixV("uInvTranspModelViewMatrix", 1, GL_FALSE, invTranspModelViewMatrix, 4, 0);
	SetGLUniformMatrixV("uModelMatrix", 1, GL_FALSE, modelMatrix, 4, 0);
	SetGLUniformMatrixV("uInvTranspModelMatrix", 1, GL_FALSE, invTranspModelMatrix, 4, 0);
	SetGLUniformMatrixV("uViewMatrix", 1, GL_FALSE, viewMatrix, 4, 0);
	Vector4f cameraPosition = viewMatrixPass * Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
	float cameraPositionT[3] = { cameraPosition.x, cameraPosition.y, cameraPosition.z };
	SetGLUniformXV("uCameraPosition", floatType, 3, 1, cameraPositionT);

	SetGLUniformMatrixV("uProjectionMatrix", 1, GL_FALSE, projectionMatrix, 4, 0);
}

void GL_Shader::SetPolygonOffset(bool cancelGeometricOffset) const
{
#if 0
	if (cancelGeometricOffset)
	{
		SetGLUniformVariable("uPositionOffsetType", 0);
		SetGLUniformVariable("uPositionOffsetValue", 0.f);
	}
	else
	{
		GLint geometricOffsetType = pPass->m_geometricOffsetType == noOffset ? 0 : pPass->m_geometricOffsetType == geometricOffset ? 1 : 2;

		SetGLUniformVariable("uPositionOffsetType", geometricOffsetType);
		SetGLUniformVariable("uPositionOffsetValue", pPass->m_geometricOffsetValue);
	}


	if (pPass->m_hasPolyOffset)
	{
		glEnable(pPass->m_polyOffsetType);
		glPolygonOffset(pPass->m_polyOffsetVal.x, pPass->m_polyOffsetVal.y);
	}
#endif
}


void GL_Shader::SetMaterial(const MaterialColor& material) const
{
	Vector4f ambientColorFront;
	Vector4f diffuseColorFront;
	Vector4f specularColorFront;
	Vector4f ambientColorBack;
	Vector4f diffuseColorBack;
	Vector4f specularColorBack;
	float shininessFront = 0.f;
	float shininessBack = 0.f;
#if 0
	material.ComputeColors(ambientColorFront, diffuseColorFront, specularColorFront,
		ambientColorBack, diffuseColorBack, specularColorBack,
		shininessFront, shininessBack);
#endif
	GLfloat colorFloat[4] = { material.GetFloatRed(), material.GetFloatGreen(), material.GetFloatBlue(), material.GetFloatAlpha()};

	GLfloat specColorFloatF[3] = { specularColorFront.x, specularColorFront.y, specularColorFront.z };
	GLfloat ambientColorFloatF[3] = { ambientColorFront.x, ambientColorFront.y, ambientColorFront.z };
	GLfloat diffuseColorFloatF[4] = { diffuseColorFront.x, diffuseColorFront.y, diffuseColorFront.z, diffuseColorFront.w };

	GLfloat specColorFloatB[3] = { specularColorBack.x, specularColorBack.y, specularColorBack.z };
	GLfloat ambientColorFloatB[3] = { ambientColorBack.x, ambientColorBack.y, ambientColorBack.z };
	GLfloat diffuseColorFloatB[4] = { diffuseColorBack.x, diffuseColorBack.y, diffuseColorBack.z, diffuseColorBack.w };

	SetGLUniformXV("uMaterial.m_color", floatType, 4, 1, colorFloat);

	SetGLUniformXV("uMaterial.m_specularColorF", floatType, 3, 1, specColorFloatF);
	SetGLUniformXV("uMaterial.m_ambientColorF", floatType, 3, 1, ambientColorFloatF);
	SetGLUniformXV("uMaterial.m_diffuseColorF", floatType, 4, 1, diffuseColorFloatF);
	SetGLUniformVariable("uMaterial.m_exponentF", shininessFront);

	SetGLUniformXV("uMaterial.m_specularColorB", floatType, 3, 1, specColorFloatB);
	SetGLUniformXV("uMaterial.m_ambientColorB", floatType, 3, 1, ambientColorFloatB);
	SetGLUniformXV("uMaterial.m_diffuseColorB", floatType, 4, 1, diffuseColorFloatB);
	SetGLUniformVariable("uMaterial.m_exponentB", shininessBack);
}

void GL_Shader::SetShaderVariables() const
{
#if O
	SetMatrices(pPass->GetModelViewMatrix());
	SetPolygonOffset(pPass);
#endif
}


//
// References :
// https://subscription.packtpub.com/book/game-development/9781789342253/7/ch07lvl1sec70/drawing-a-wireframe-on-top-of-a-shaded-mesh
//
ShaderWireframe::ShaderWireframe()
	: GL_Shader(shaderWireframe)
{
}

bool ShaderWireframe::LoadShadersFromString()
{
	const char* vertexShader =
		"#version 330 core"
		"layout(location = 0) in vec3 aPos;"
		"void main()"
		"{"
		"	gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);"
		"}";
	const char* fragmentShader =
		"#version 330 core"
		"out vec4 FragColor;"
		"void main()"
		"{"
		"	FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);"
		"}";
	std::map<GLenum, std::string> shaderNames;
	shaderNames.insert(std::make_pair<GLenum, std::string>(GL_VERTEX_SHADER, vertexShader));
	shaderNames.insert(std::make_pair<GLenum, std::string>(GL_FRAGMENT_SHADER, fragmentShader));

	return CompileAndLinkShaders(shaderNames, true);
}

bool ShaderWireframe::LoadShadersFromFiles(const std::string& shaderDirectory)
{
	std::map<GLenum, std::string> shaderPaths;
	shaderPaths.insert(std::make_pair<GLenum, std::string>(GL_VERTEX_SHADER, ""));
	shaderPaths.insert(std::make_pair<GLenum, std::string>(GL_FRAGMENT_SHADER, ""));

	return CompileAndLinkShaders(shaderPaths, false);
}


void ShaderWireframe::Execute() const
{
	if (!m_isValidShader)
		return;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	ActivateShader();

#if 0
	SetBasicMatrices(pPass->GetModelViewMatrix());

	const Vector4f& materialColor = pPass->m_texturage.GetMaterial().GetColor();

	GLfloat colorFloat[4] = { materialColor.x, materialColor.y, materialColor.z, materialColor.w };
	SetGLUniformXV("uColor", floatType, 4, 1, colorFloat);

	const int size = 2;
	int locs[size] = { -1, -1 };
	BindAttributes(pPass->m_vertex, locs[0], "inVtxPosition");

	if (pPass->m_normals.valid() && pPass->m_normals.nb_elem() > 0)
	{
		SetPolygonOffset(pPass);
		BindAttributes(pPass->m_normals, locs[1], "inVtxNormal");
	}
	else
		SetPolygonOffset(pPass, true);

	glLineWidth(pPass->m_lineWidth);

	glDrawArrays(pPass->m_primitive, 0, pPass->m_nb_elems);

	DisableAttribArrays(locs, size);
#endif

	glPopAttrib();

	DeactivateShader();
}

ShaderBackground::ShaderBackground()
	: GL_Shader(shaderBackground)
{

}

ShaderBackground::~ShaderBackground()
{

}

bool ShaderBackground::LoadShadersFromString()
{
	return false;
}

bool ShaderBackground::LoadShadersFromFiles(const std::string& shaderDirectory)
{
	std::map<GLenum, std::string> shaderPaths;
	shaderPaths.insert(std::make_pair<GLenum, std::string>(GL_VERTEX_SHADER, ""));
	shaderPaths.insert(std::make_pair<GLenum, std::string>(GL_FRAGMENT_SHADER, ""));

	return CompileAndLinkShaders(shaderPaths, false);
}

void ShaderBackground::Execute() const
{
	ActivateShader();

#if 0
	//as for Util_Room::RenderSkyBox
	Matrix4f projection;
	const GL_Cam* pCamera = pRenderer->GetCamera();
	if (pCamera)
	{
		float zoom = pCamera->GetZoom();
		float l, r, width, height;
		pCamera->GetViewport(l, r, width, height);
		float fov = 2 * atanf(zoom);
		float farval = 5000.0f;
		float nearval = 1.f;

		float x, y, c, d;
		x = cosf(0.5f * fov) / sinf(0.5f * fov);
		y = (x * width) / height;
		c = -(farval + nearval) / (farval - nearval);
		d = -(2.0f*farval*nearval) / (farval - nearval);

		projection[0][0] = x;    projection[0][1] = 0.0F;  projection[0][2] = 0.0f;      projection[0][3] = 0.0F;
		projection[1][0] = 0.0F;  projection[1][1] = y;     projection[1][2] = 0.0f;      projection[1][3] = 0.0F;
		projection[2][0] = 0.0F;  projection[2][1] = 0.0F;  projection[2][2] = c;			projection[2][3] = d;
		projection[3][0] = 0.0F; projection[3][1] = 0.0F;  projection[3][2] = -1.0F;		projection[3][3] = 0.0F;
	}
	else
		projection.set_identity();

	float modelMatrix[16], viewMatrix[16], projectionMatrix[16];

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
	glPopMatrix();

	Matrix4f modelMatrixPass(pPass->GetModelViewMatrix());

	modelMatrixPass.to_float_16(modelMatrix);

	projection.to_float_16(projectionMatrix);

	SetGLUniformMatrixV("uModelMatrix", 1, GL_FALSE, modelMatrix, 4, 0);
	SetGLUniformMatrixV("uViewMatrix", 1, GL_FALSE, viewMatrix, 4, 0);
	SetGLUniformMatrixV("uProjectionMatrix", 1, GL_FALSE, projectionMatrix, 4, 0);

	GLboolean writeMask = false;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &writeMask);
	glDepthMask(GL_FALSE);

	const int size = 1;
	int locs[size] = { -1};
	BindAttributes(pPass->m_vertex, locs[0], "inVtxPosition");

	glDrawArrays(pPass->m_primitive, 0, pPass->m_nb_elems);
	glBindVertexArray(0);

	glDepthMask(writeMask);

	DisableAttribArrays(locs, size);

#endif
	DeactivateShader();
}



#if 0
void GL_Shader::RenderCube()
{
	static unsigned int cubeVAO = 0;
	static unsigned int cubeVBO = 0;

	// initialize (if necessary)
	if (cubeVAO == 0)
	{
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
			// front face
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			// left face
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			// right face
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
			 // bottom face
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			  1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			 -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			 // top face
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			  1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			  1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
			  1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			 -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		};

		for (int i = 0; i < 36; i++)
		{
			int idStart = i * 8;
			vertices[idStart] *= -1.f;
			float temp = vertices[idStart + 1];
			vertices[idStart + 1] = vertices[idStart + 2];
			vertices[idStart + 2] = temp;

			vertices[idStart + 3] *= -1.f;
			temp = vertices[idStart + 4];
			vertices[idStart + 4] = vertices[idStart + 5];
			vertices[idStart + 5] = temp;

			vertices[idStart + 7] = 0.5f - (vertices[idStart + 7] - 0.5f);
		}

		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
#endif

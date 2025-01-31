#include	"ViewVP/ViewVP_PCH.h"

#include	<utils/2d3dAssert.hxx>
#include	<Win32_GL_utils/GL.hxx>
#include	<Win32_GL_utils/GL_TemporaryGlContext.hxx>
#include	<Win32_GL_utils/GL_CapabilitiesManager.hxx>
#include	<ViewVP/GL_ShadersManager.hxx>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// uncomment to activate the following behaviors :
// - reload a shader each time it is requested by GetShader().
// - no Shaders Diagnostic done. m_bUseShaders, m_bAOSupported & m_bSMSupported are set to true
//#define DEBUG_SHADER


class FreeShadersManagerSingleton {
public:
	FreeShadersManagerSingleton() {};
	~FreeShadersManagerSingleton() {
		if (GL_ShadersManager::m_pInstance != nullptr)
		{
			delete GL_ShadersManager::m_pInstance;
			GL_ShadersManager::m_pInstance = nullptr;
		}
	};
};
static FreeShadersManagerSingleton _free;

GL_ShadersManager *GL_ShadersManager::m_pInstance = nullptr;

GL_ShadersManager* GL_ShadersManager::GetInstance()
{
	if (m_pInstance == nullptr)
	{
		m_pInstance = new GL_ShadersManager();
		m_pInstance->ComputeShaderDirectory();
#ifdef DEBUG_SHADER
		m_pInstance->m_bUseShaders = true;
		m_pInstance->m_bAOSupported = true;
		m_pInstance->m_bSMSupported = true;
#else
		m_pInstance->DiagnosticShaders();
#endif
	}	
	return m_pInstance;
}

GL_ShadersManager::GL_ShadersManager()
{
	m_refCount = 0;
	m_shaderDirectory == ChaineInc("");
	m_bUseShaders = false;
	m_bAOSupported = false;
	m_bSMSupported = false;
}

GL_ShadersManager::~GL_ShadersManager()
{
	ASSERT(m_shaders.size() == 0);
}

bool GL_ShadersManager::UseShaders() const
{
	return m_bUseShaders;
}

bool GL_ShadersManager::IsAOSupported() const
{
	return m_bAOSupported;
}

bool GL_ShadersManager::IsSMSupported() const
{
	return m_bSMSupported;
}

bool LocalGL_ShadersManager::CanUseShader(bool withCurrentContext)
{
	if (GL_ShadersManager::GetInstance()->UseShaders()) {
		if (withCurrentContext)
			DiagnosticShadersInCurrentContext();
		else
			DiagnosticShaders();
		return UseShaders();
		}
	return false;
}
bool LocalGL_ShadersManager::UseShaders() const
{
	return GL_ShadersManager::UseShaders();
}
bool LocalGL_ShadersManager::IsAOSupported() const
{
	return GL_ShadersManager::IsAOSupported();
}
bool LocalGL_ShadersManager::IsSMSupported() const
{
	return GL_ShadersManager::IsSMSupported();
}

void GL_ShadersManager::EmptyMapShaders()
{
	for (int i = 0; i < (int)m_shaders.size(); i++)
		delete m_shaders[i];
	m_shaders.clear();
}

GL_Shader* GL_ShadersManager::CreateNewShader(shader_type type) const
{
	GL_Shader* pShader = nullptr;

	switch (type)
	{
	case shaderDiffuse:								pShader = new ShaderDiffuse(); break;
	case shaderGeometry:							pShader = new ShaderGeometry(); break;
	case shaderTransparency:						pShader = new ShaderTransparency(); break;
	case shaderAOPass:								pShader = new AOPassShader(); break;
	case shaderAOBlurPass:							pShader = new AOBlurPassShader(); break;
	case shaderCombine:								pShader = new CombineShader(); break;
	case shaderShadowMappingPoint:					pShader = new ShadowMappingPointPassShader(); break;
	case shaderDiffusePBR:							pShader = new ShaderDiffusePBR(); break;
	case shaderDiffuseOffset:						pShader = new ShaderDiffuseOffset(); break;
	case shaderDiffusePBROffset:					pShader = new ShaderDiffusePBROffset(); break;

	case shaderEquirectangularToCubemap:			pShader = new ShaderEquirectangularToCubemap(); break;
	case shaderIrradiance:							pShader = new ShaderIrradiance(); break;
	case shaderPrefilter:							pShader = new ShaderPrefilter(); break;
	case shaderBrdf:								pShader = new ShaderBrdf(); break;
	case shaderBackground:							pShader = new ShaderBackground(); break;
	default:
		break;
	}

	return pShader;
}

bool GL_ShadersManager::AddShader(shader_type type)
{
	GL_Shader* pShader = CreateNewShader(type);
	if (!pShader)
		return false;

	bool bLoaded = pShader->LoadShaders();
	
	if (!bLoaded)
	{
		delete pShader;
		return false;
	}

	return m_shaders.add(type, pShader);
}

void GL_ShadersManager::RecompileShader(shader_type type)
{
	if (!UseShaders())
		return;

	int id = m_shaders.find(type);
	if (id > -1)
		m_shaders[id]->LoadShaders();
}

const Map<shader_type, GL_Shader*>& GL_ShadersManager::GetShaders() const
{
	return m_shaders;
}

GL_Shader* GL_ShadersManager::GetShader(shader_type type)
{
	if (!UseShaders())
		return nullptr;

	GL_Shader* const* pShader = m_shaders.look(type);

#ifdef DEBUG_SHADER
	int id = m_shaders.find(type);
	if (id > -1)
		m_shaders[id]->LoadShaders();
#endif
	
	if (pShader == nullptr)
	{
		bool bRes = AddShader(type);
		if (bRes)
			pShader = m_shaders.look(type);
		else
			return nullptr;
	}
	return *pShader;
}

void GL_ShadersManager::AddRef()
{
	m_refCount++;
}

void GL_ShadersManager::Release()
{
	// we delete the shaders as long as an OpenGL context is set.
	// when the GL_ShadersManager is destroyed, it will be too late.
	if (--m_refCount == 0)
		EmptyMapShaders();
}

bool GL_ShadersManager::ComputeShaderDirectory()
{
	TCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	ChaineInc bufferCh(buffer);
	m_shaderDirectory = bufferCh;
	int pos = m_shaderDirectory.ReverseFind('\\');
	if (pos > 0)
		m_shaderDirectory = bufferCh.Left(pos);
	pos = m_shaderDirectory.ReverseFind('\\');
	if (pos > 0)
		m_shaderDirectory = m_shaderDirectory.Left(pos);

	m_shaderDirectory = m_shaderDirectory + "\\shadersAO\\";

	DWORD ftyp = GetFileAttributes(m_shaderDirectory.chars());

	if (ftyp == INVALID_FILE_ATTRIBUTES)
	{
		printf("Cannot retrieve shaders files attributes\n");
		return false;
	}

	return true;
}

bool GL_ShadersManager::DiagnosticShaders()
{
	WNDCLASSEX fakewcex;
	fakewcex.cbSize = sizeof(WNDCLASSEX);
	fakewcex.style = CS_HREDRAW | CS_VREDRAW;
	fakewcex.lpfnWndProc = DefWindowProc;
	fakewcex.cbClsExtra = 0;
	fakewcex.cbWndExtra = 0;
	fakewcex.hInstance = NULL;
	fakewcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	fakewcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	fakewcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	fakewcex.lpszMenuName = 0;
	fakewcex.lpszClassName = TEXT("FakeWindow");
	fakewcex.hIconSm = 0;
	RegisterClassEx(&fakewcex);

	TemporaryGlContext glContext;
	bool bRes = glContext.IsValid();
	if (bRes)
		DiagnosticShadersInCurrentContext();

	return bRes;
}

bool GL_ShadersManager::CheckShader(shader_type shaderType)
{
	GL_Shader* pShader = CreateNewShader(shaderType);
	if (pShader == nullptr)
		return false;

	bool bLoaded = pShader->LoadShaders();

	delete pShader;
	return bLoaded;
}

bool GL_ShadersManager::CheckShaders(const shader_type* shaderTypes, int nShaderTypes)
{
	for (int i = 0; i < nShaderTypes; i++)
		if (CheckShader(shaderTypes[i]) == false)
			return false;
	return true;
}

void GL_ShadersManager::DiagnosticShadersInCurrentContext()
{
	// do we use the shaders ?
	long noShader = 0;
	Config_Modaris::get(_T("NoShader"), noShader);
	if (noShader == 1)
	{
		m_bUseShaders = false;
		return;
	}

	// check OpenGL version
	float fOpenGLVersion = GL_CapabilitiesManager::GetInstance()->GetVersion();
	if (fOpenGLVersion < 2.0)
	{
		m_bUseShaders = false;
		return;
	}

	// check GLSL version
	float fOpenGLSLVersion = GL_CapabilitiesManager::GetInstance()->GetGLSLVersion();
	if (fOpenGLSLVersion == 0.f)
	{
		m_bUseShaders = false;
		return;
	}

	//
	// test shader diffuse
	//
	m_bUseShaders = CheckShader(shaderDiffuse);
	if (m_bUseShaders == false)
		return;

	// test AO
	shader_type AOShaders[3] = { shaderAOPass , shaderAOBlurPass, shaderCombine };
	m_bAOSupported = CheckShaders(AOShaders, sizeof(AOShaders) / sizeof(shader_type));

	// test SM
	//	m_bSMSupported = CheckShader(shaderGeometry);
	shader_type SMShaders[3] = { shaderGeometry, shaderTransparency, shaderShadowMappingPoint };
	m_bSMSupported = CheckShaders(SMShaders, sizeof(SMShaders) / sizeof(shader_type));
}

GLint GL_ShadersManager::GetActiveTextureId(const ChaineInc& name)
{
	if (m_maxActiveTextureId == 0)
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_maxActiveTextureId);


	int id = m_activeTextureMap.find(name);
	if (id < 0)
	{
		m_activeTextureId++;
		if (m_activeTextureId > m_maxActiveTextureId)
		{
			m_activeTextureId = m_maxActiveTextureId - 1;
			assert(false);
		}

		m_activeTextureMap.add(name, m_activeTextureId);
		return m_activeTextureId;
	}
	else
		return m_activeTextureMap.val(id);
}

const ChaineInc& GL_ShadersManager::GetShaderDirectory() const
{
	return m_shaderDirectory;
}

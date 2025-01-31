#include	<stdlib.h>
#include	<Win32_GL_utils/GL_Wrapper.hxx>
#include	<Win32_GL_utils/GL_TemporaryGlContext.hxx>
#include	<Win32_GL_utils/GL_CapabilitiesManager.hxx>
#include	<utils/getconfig.hxx>
#include	<utils/FileSystem.h>

class FreeCapabilitiesManagerSingleton {
public:
	FreeCapabilitiesManagerSingleton() {};
	~FreeCapabilitiesManagerSingleton() {
		if (GL_CapabilitiesManager::m_pInstance != nullptr)
		{
			delete GL_CapabilitiesManager::m_pInstance;
			GL_CapabilitiesManager::m_pInstance = nullptr;
		}
	};
};
static FreeCapabilitiesManagerSingleton _free;

GL_CapabilitiesManager *GL_CapabilitiesManager::m_pInstance = nullptr;

GL_CapabilitiesManager::GL_CapabilitiesManager()
{
	m_vendor = nullptr;
	m_renderer = nullptr;
	m_extensions = nullptr;
	m_openGLVersion = 0.f;
	m_openGLSLVersion = 0.f;
	m_useShaders = false;

	InitializeInfo();
}

GL_CapabilitiesManager* GL_CapabilitiesManager::GetInstance()
{
	if (m_pInstance == nullptr)
		m_pInstance = new GL_CapabilitiesManager();
	return m_pInstance;
}

bool GL_CapabilitiesManager::InitializeInfo()
{
	// backup
	HGLRC hglrcBackup = wglGetCurrentContext();
	HDC hdcBackup = wglGetCurrentDC();

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
		GetInfo();

	return bRes;
}

void GL_CapabilitiesManager::GetInfo()
{
	m_vendor = (char *)::glGetString(GL_VENDOR);
	m_renderer = (char *)::glGetString(GL_RENDERER);
	m_version = (char *)::glGetString(GL_VERSION);
	m_extensions = (char *)::glGetString(GL_EXTENSIONS);

	// get the GL version
	m_openGLVersion = (m_version && *m_version != '\0') ? (float)atof(m_version) : 0.f;

	// get the GLSL version
	m_openGLSLVersion = 0.f;
	const char* glslVersion = (const char*)::glGetString(GL_SHADING_LANGUAGE_VERSION);
	if (glCheckError(__FUNCTION__, __LINE__) == 0)
		m_openGLSLVersion = (glslVersion && *glslVersion != '\0') ? (float)atof(glslVersion) : 0.f;

	// do we use the shaders ?
	long noShader = 0;
	Config_Modaris::get(_T("NoShader"), noShader);
	m_useShaders = (noShader == 0 && m_openGLVersion >= 2.0) ? true : false;
}

bool GL_CapabilitiesManager::IsShadersSupported() const
{
	return m_useShaders;
}

bool GL_CapabilitiesManager::IsAOSupported() const
{
	return GetVersion() >= 4.0;
}

bool GL_CapabilitiesManager::IsSMSupported() const
{
	return GetVersion() >= 4.0;
}

bool GL_CapabilitiesManager::IsTextureCompressionSupported(CompressionType eType) const
{
	PFNGLGETCOMPRESSEDTEXIMAGEARBPROC glGetCompressedTexImageARB = (PFNGLGETCOMPRESSEDTEXIMAGEARBPROC)wglGetProcAddress("glGetCompressedTexImageARB");
	PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)wglGetProcAddress("glCompressedTexImage2DARB");
	if (glGetCompressedTexImageARB == nullptr ||
		glCompressedTexImage2DARB == nullptr)
		return false;

	if (eType == COMPRESSION_TYPE_GENERIC)
	{
		return GetVersion() >= 1.1;
	}
	else if (eType == COMPRESSION_TYPE_S3TC)
	{
		GLint numCompressedFormat = 0;
		glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB, &numCompressedFormat);

		bool bDXT1 = false;
		bool bDXT5 = false;
		if (numCompressedFormat > 0)
		{
			GLint* pCompressedFormat = (GLint*)malloc(numCompressedFormat * sizeof(GLint));
			if (pCompressedFormat)
			{
				glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS_ARB, pCompressedFormat);
			
				for (int i = 0; i < numCompressedFormat; i++)
				{
					if (pCompressedFormat[i] == GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
						bDXT5 = true;
					if (pCompressedFormat[i] == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
						bDXT1 = true;
				}
				free(pCompressedFormat);
			}			
		}

		return GetVersion() >= 1.1 && HasExtensions("GL_EXT_texture_compression_s3tc") && bDXT1 && bDXT5;
	}
	return false;
}

float GL_CapabilitiesManager::GetGLSLVersion() const
{
	return m_openGLSLVersion;
}

float GL_CapabilitiesManager::GetVersion() const
{
	char* versionStr = (char*)::glGetString(GL_VERSION);
	if (glCheckError(__FUNCTION__, __LINE__) == 0)
	{
		const auto versionF = (float)atof(versionStr);

		static bool done = false;
		if (!done)
		{
			done = true;
			_tprintf(_T("GL_CapabilitiesManager::GetVersion(): %.5f\n"), versionF);
		}
		
		return versionF;
	}
	return 0.f;
}

const char* GL_CapabilitiesManager::GetExtensions() const
{
	return (char *)::glGetString(GL_EXTENSIONS);
}

const char* GL_CapabilitiesManager::GetWGLExtensions() const
{
	// Try To Use wglGetExtensionStringARB On Current DC, If Possible
	PROC glrc_wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");
	return (glrc_wglGetExtString)? ((const char*(__stdcall*)(HDC))glrc_wglGetExtString)(wglGetCurrentDC()) : nullptr;
}


const char* GL_CapabilitiesManager::GetVendor() const
{
	return (char *)::glGetString(GL_VENDOR);
}

const char* GL_CapabilitiesManager::GetRenderer() const
{
	return (char *)::glGetString(GL_RENDERER);
}

bool GL_CapabilitiesManager::HasExtensions(const char* neededExtensions, bool bUseWglGetExtensionsStringARB) const
{
	const char* extensions = (bUseWglGetExtensionsStringARB)? GetWGLExtensions() : GetExtensions();
	//printf("%s\n", extensions);
	if (!extensions)
		return false;

	char seps[] = " ";
	char* context = nullptr;
	
	char tabNeededExtensions[512];
	strncpy(tabNeededExtensions, neededExtensions, strlen(neededExtensions));
	tabNeededExtensions[strlen(neededExtensions)] = '\0';

	char* token = strtok_s(tabNeededExtensions, " ", &context);
	while (token != NULL)
	{
		const size_t extlen = strlen(token);
		
		bool found = false;
		// begin examination at start of string, and increment by 1 on false match
		for (const char* p = extensions; ; p++)
		{
			// advance p up to the next possible match
			p = strstr(p, token);

			if (p == nullptr)
				return false; // token not found

			if ((p == extensions || p[-1] == ' ') // make sure the former character is space or start of m_extensions
				&&
				(p[extlen] == '\0' || p[extlen] == ' ')) // make sure the following character is space or NULL
			{
				found = true;
				break;
			}
		}
		token = strtok_s(nullptr, " ", &context);
	}

	return true;
}

int GL_CapabilitiesManager::GetAvailableMemory() const
{
	// ref : http://developer.download.nvidia.com/opengl/specs/GL_NVX_gpu_memory_info.txt
	if (GetVersion() >= 2. && HasExtensions("GL_NVX_gpu_memory_info"))
	{
		unsigned long GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX = 0x9049;
		GLint currentAvailableMemry;
		glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &currentAvailableMemry);

		return currentAvailableMemry;
	}

	return -1;
}

void GL_CapabilitiesManager::GetMaxTextureDimensions(int& width, int& height) const
{
	width = 0;
	height = 0;
	glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &width);
	glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &height);
}

bool GL_CapabilitiesManager::ExportDiagnostic() const
{
	FILE* ptr = nullptr;
	ChaineInc path;
	AppFolder::GetApplicationDataDirectory(path);
	_tfopen_s(&ptr, (path + ChaineInc("\\graphic_card_info.txt")).chars(), _T("w"));
	if (!ptr)
		return false;

	//
	// General
	//
	fprintf(ptr, "Vendor : %s\n", GetVendor());
	fprintf(ptr, "Renderer : %s\n", GetRenderer());
	fprintf(ptr, "GL Version : %.2f\n", GetVersion());
	fprintf(ptr, "GLSL Version : %.2f\n", GetGLSLVersion());

	//
	// memory
	//
	fprintf(ptr, "\nMemory\n");
	// ref : http://developer.download.nvidia.com/opengl/specs/GL_NVX_gpu_memory_info.txt
	if (GetVersion() >= 2. && HasExtensions("GL_NVX_gpu_memory_info"))
	{
		unsigned long GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX = 0x9047;
		unsigned long GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX = 0x9048;
		unsigned long GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX = 0x9049;
		unsigned long GPU_MEMORY_INFO_EVICTION_COUNT_NVX = 0x904A;
		unsigned long GPU_MEMORY_INFO_EVICTED_MEMORY_NVX = 0x904B;
		GLint info;
		glGetIntegerv(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &info);
		fprintf(ptr, "dedicated video memory : %dMo\n", info / 1000);
		glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &info);
		fprintf(ptr, "total available memory : %dMo\n", info / 1000);
		glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &info);
		fprintf(ptr, "current available dedicated video memory : %dMo\n", info / 1000);
		glGetIntegerv(GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &info);
		fprintf(ptr, "count of total evictions seen by system : %d\n", info);
		glGetIntegerv(GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &info);
		fprintf(ptr, "size of total video memory evicted : %dMo\n", info / 1000);
	}

	//
	// shaders
	//
	fprintf(ptr, "\nShaders\n");

	fprintf(ptr, "geometry shader:\n");
	// GL_ARB_geometry_shader4 : https://www.opengl.org/registry/specs/ARB/geometry_shader4.txt
	unsigned long MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_ARB = 0x8C29;
	unsigned long MAX_GEOMETRY_VARYING_COMPONENTS_ARB = 0x8DDD;
	unsigned long MAX_VERTEX_VARYING_COMPONENTS_ARB = 0x8DDE;
	unsigned long MAX_VARYING_COMPONENTS = 0x8B4B;
	unsigned long MAX_GEOMETRY_UNIFORM_COMPONENTS_ARB = 0x8DDF;
	unsigned long MAX_GEOMETRY_OUTPUT_VERTICES_ARB = 0x8DE0;
	unsigned long MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_ARB = 0x8DE1;
	GLint info;
	glGetIntegerv(MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_ARB, &info);
	fprintf(ptr, "  Max Geometry Texture Units           : %u\n", info);
	glGetIntegerv(MAX_GEOMETRY_VARYING_COMPONENTS_ARB, &info);
	fprintf(ptr, "  Max Geometry Varying Components      : %u\n", info);
	glGetIntegerv(MAX_VERTEX_VARYING_COMPONENTS_ARB, &info);
	fprintf(ptr, "  Max Vertex Varying Components        : %u\n", info);
	glGetIntegerv(MAX_VARYING_COMPONENTS, &info);
	fprintf(ptr, "  Max Varying Components               : %u\n", info);
	glGetIntegerv(MAX_GEOMETRY_UNIFORM_COMPONENTS_ARB, &info);
	fprintf(ptr, "  Max Geometry Uniform Components      : %u\n", info);
	glGetIntegerv(MAX_GEOMETRY_OUTPUT_VERTICES_ARB, &info);
	fprintf(ptr, "  Max Geometry Output Vertices         : %u\n", info);
	glGetIntegerv(MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_ARB, &info);
	fprintf(ptr, "  Max Geometry Total Output Components : %u\n", info);

	fprintf(ptr, "vertex shader:\n");
	// GL_ARB_vertex_shader : https://www.opengl.org/registry/specs/ARB/vertex_shader.txt
	unsigned long MAX_VERTEX_UNIFORM_COMPONENTS_ARB = 0x8B4A;
	unsigned long MAX_VARYING_FLOATS_ARB = 0x8B4B;
	unsigned long MAX_VERTEX_ATTRIBS_ARB = 0x8869;
	unsigned long MAX_TEXTURE_IMAGE_UNITS_ARB = 0x8872;
	unsigned long MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB = 0x8B4C;
	unsigned long MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB = 0x8B4D;
	unsigned long MAX_TEXTURE_COORDS_ARB = 0x8871;

	glGetIntegerv(MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &info);
	fprintf(ptr, "  Max uniform vertex composantes       : %d\n", info);
	glGetIntegerv(MAX_VARYING_FLOATS_ARB, &info);
	fprintf(ptr, "  Max Variante virgule flottante       : %d\n", info);
	glGetIntegerv(MAX_VERTEX_ATTRIBS_ARB, &info);
	fprintf(ptr, "  Max sommet attribute                 : %d\n", info);
	glGetIntegerv(MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, &info);
	fprintf(ptr, "  Max combined tex image units         : %d\n", info);
	glGetIntegerv(MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB, &info);
	fprintf(ptr, "  Max vertex tex image units           : %d\n", info);

	fprintf(ptr, "fragment shader:\n");
	// GL_ARB_fragment_shader : https://www.opengl.org/registry/specs/ARB/fragment_shader.txt
	unsigned long MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB = 0x8B49;
	glGetIntegerv(MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &info);
	fprintf(ptr, "  Max uniform fragment composantes     : %d \n", info);

	fclose(ptr);

	return true;
}


void GL_CapabilitiesManager::GetMaxTextureSize(int& size) const
{
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
}

void GL_CapabilitiesManager::GetMaxCubeMapTextureSize(int& size) const
{
	glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &size);
}

#include "capabilities_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <stdexcept>
#include "gl_wrapper.h"

CapabilitiesManager *CapabilitiesManager::m_pInstance = new CapabilitiesManager;


template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
	int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
	auto size = static_cast<size_t>(size_s);
	std::unique_ptr<char[]> buf(new char[size]);
	std::snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

static char* convertInternalFormatToString(GLenum format)
{
	char* formatName;

	switch (format)
	{
	case GL_STENCIL_INDEX:      // 0x1901
		formatName = "GL_STENCIL_INDEX";
		break;
	case GL_DEPTH_COMPONENT:    // 0x1902
		formatName = "GL_DEPTH_COMPONENT";
		break;
	case GL_ALPHA:              // 0x1906
		formatName = "GL_ALPHA";
		break;
	case GL_RGB:                // 0x1907
		formatName = "GL_RGB";
		break;
	case GL_RGBA:               // 0x1908
		formatName = "GL_RGBA";
		break;
	case GL_LUMINANCE:          // 0x1909
		formatName = "GL_LUMINANCE";
		break;
	case GL_LUMINANCE_ALPHA:    // 0x190A
		formatName = "GL_LUMINANCE_ALPHA";
		break;
	case GL_R3_G3_B2:           // 0x2A10
		formatName = "GL_R3_G3_B2";
		break;
	case GL_ALPHA4:             // 0x803B
		formatName = "GL_ALPHA4";
		break;
	case GL_ALPHA8:             // 0x803C
		formatName = "GL_ALPHA8";
		break;
	case GL_ALPHA12:            // 0x803D
		formatName = "GL_ALPHA12";
		break;
	case GL_ALPHA16:            // 0x803E
		formatName = "GL_ALPHA16";
		break;
	case GL_LUMINANCE4:         // 0x803F
		formatName = "GL_LUMINANCE4";
		break;
	case GL_LUMINANCE8:         // 0x8040
		formatName = "GL_LUMINANCE8";
		break;
	case GL_LUMINANCE12:        // 0x8041
		formatName = "GL_LUMINANCE12";
		break;
	case GL_LUMINANCE16:        // 0x8042
		formatName = "GL_LUMINANCE16";
		break;
	case GL_LUMINANCE4_ALPHA4:  // 0x8043
		formatName = "GL_LUMINANCE4_ALPHA4";
		break;
	case GL_LUMINANCE6_ALPHA2:  // 0x8044
		formatName = "GL_LUMINANCE6_ALPHA2";
		break;
	case GL_LUMINANCE8_ALPHA8:  // 0x8045
		formatName = "GL_LUMINANCE8_ALPHA8";
		break;
	case GL_LUMINANCE12_ALPHA4: // 0x8046
		formatName = "GL_LUMINANCE12_ALPHA4";
		break;
	case GL_LUMINANCE12_ALPHA12:// 0x8047
		formatName = "GL_LUMINANCE12_ALPHA12";
		break;
	case GL_LUMINANCE16_ALPHA16:// 0x8048
		formatName = "GL_LUMINANCE16_ALPHA16";
		break;
	case GL_INTENSITY:          // 0x8049
		formatName = "GL_INTENSITY";
		break;
	case GL_INTENSITY4:         // 0x804A
		formatName = "GL_INTENSITY4";
		break;
	case GL_INTENSITY8:         // 0x804B
		formatName = "GL_INTENSITY8";
		break;
	case GL_INTENSITY12:        // 0x804C
		formatName = "GL_INTENSITY12";
		break;
	case GL_INTENSITY16:        // 0x804D
		formatName = "GL_INTENSITY16";
		break;
	case GL_RGB4:               // 0x804F
		formatName = "GL_RGB4";
		break;
	case GL_RGB5:               // 0x8050
		formatName = "GL_RGB5";
		break;
	case GL_RGB8:               // 0x8051
		formatName = "GL_RGB8";
		break;
	case GL_RGB10:              // 0x8052
		formatName = "GL_RGB10";
		break;
	case GL_RGB12:              // 0x8053
		formatName = "GL_RGB12";
		break;
	case GL_RGB16:              // 0x8054
		formatName = "GL_RGB16";
		break;
	case GL_RGBA2:              // 0x8055
		formatName = "GL_RGBA2";
		break;
	case GL_RGBA4:              // 0x8056
		formatName = "GL_RGBA4";
		break;
	case GL_RGB5_A1:            // 0x8057
		formatName = "GL_RGB5_A1";
		break;
	case GL_RGBA8:              // 0x8058
		formatName = "GL_RGBA8";
		break;
	case GL_RGB10_A2:           // 0x8059
		formatName = "GL_RGB10_A2";
		break;
	case GL_RGBA12:             // 0x805A
		formatName = "GL_RGBA12";
		break;
	case GL_RGBA16:             // 0x805B
		formatName = "GL_RGBA16";
		break;
	case GL_DEPTH_COMPONENT16:  // 0x81A5
		formatName = "GL_DEPTH_COMPONENT16";
		break;
	case GL_DEPTH_COMPONENT24:  // 0x81A6
		formatName = "GL_DEPTH_COMPONENT24";
		break;
	case GL_DEPTH_COMPONENT32:  // 0x81A7
		formatName = "GL_DEPTH_COMPONENT32";
		break;
	case GL_DEPTH_STENCIL:      // 0x84F9
		formatName = "GL_DEPTH_STENCIL";
		break;
	case GL_RGBA32F:            // 0x8814
		formatName = "GL_RGBA32F";
		break;
	case GL_RGB32F:             // 0x8815
		formatName = "GL_RGB32F";
		break;
	case GL_RGBA16F:            // 0x881A
		formatName = "GL_RGBA16F";
		break;
	case GL_RGB16F:             // 0x881B
		formatName = "GL_RGB16F";
		break;
	case GL_DEPTH24_STENCIL8:   // 0x88F0
		formatName = "GL_DEPTH24_STENCIL8";
		break;
	default:
		formatName = (char*)malloc(32);
		sprintf(formatName, "Unknown Format(0x%x)", format);
	}

	return formatName;
}

static bool hasExtension(const char* extensionName)
{
	const char* extensionsList = (const char*)glGetString(GL_EXTENSIONS);
	const char* extensionMatch = NULL;

	while ( (extensionMatch = strstr(extensionsList, extensionName)) != NULL )
	{
		if ( strncmp(extensionMatch,extensionName,strlen(extensionName)) == 0 )
			if ( extensionMatch[strlen(extensionName)] == ' ')
				return true;

		extensionsList = extensionMatch + strlen(extensionMatch);
	}

	return false;
}

float CapabilitiesManager::GetVersion() const
{
	return atof((char*)::glGetString(GL_VERSION));
}

void CapabilitiesManager::GetCardInfo(std::string& cardInfo) const
{
	cardInfo = string_format("Vendor : %s\nRenderer : %s\nVersion : %s\nGLSL Version : %s\n",
		(char*)::glGetString(GL_VENDOR), (char*)::glGetString(GL_RENDERER), (char*)::glGetString(GL_VERSION), (char*)::glGetString(GL_SHADING_LANGUAGE_VERSION));
}

void CapabilitiesManager::GetExtensions(std::string& extensions) const
{
	char *str = (char *)::glGetString( GL_EXTENSIONS );
	if (1)
	{
		extensions = std::string(str);
	}
	else
	{
		char *p = strtok (str, " ");
		while (p)
		{
			extensions += std::string(" ") + std::string(p);
			p = strtok (nullptr, " ");
		}
	}
}

void CapabilitiesManager::GetMemoryInfo(std::string& memoryInfo) const
{
	memoryInfo.clear();
	// ref : http://developer.download.nvidia.com/opengl/specs/GL_NVX_gpu_memory_info.txt
	if (hasExtension("NVX_gpu_memory_info"))
	{
		unsigned long GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX = 0x9047;
		unsigned long GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX = 0x9048;
		unsigned long GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX = 0x9049;
		unsigned long GPU_MEMORY_INFO_EVICTION_COUNT_NVX = 0x904A;
		unsigned long GPU_MEMORY_INFO_EVICTED_MEMORY_NVX = 0x904B;
		GLint info;
		glGetIntegerv(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &info);
		memoryInfo += string_format("dedicated video memory : %dMo\n", info / 1000);
		glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &info);
		memoryInfo += string_format("total available memory : %dMo\n", info / 1000);
		glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &info);
		memoryInfo += string_format("current available dedicated video memory : %dMo\n", info/1000);
		glGetIntegerv(GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &info);
		memoryInfo += string_format("count of total evictions seen by system : %d\n", info);
		glGetIntegerv(GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &info);
		memoryInfo += string_format("size of total video memory evicted : %dMo\n", info/1000);

	}
}

static char* getTextureParameters(GLuint id)
{
	if (glIsTexture(id) == GL_FALSE)
		return "Not texture object";

	int width, height, format;
	glBindTexture(GL_TEXTURE_2D, id);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);            // get texture width
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);          // get texture height
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format); // get texture internal format
	glBindTexture(GL_TEXTURE_2D, 0);

	char* formatName = convertInternalFormatToString(format);

	char* strParameters = (char*)malloc(64);
	sprintf(strParameters, "width %d height %d formatName %s\n", width, height, formatName);

	free(formatName);
	return strParameters;
}

static char* getRenderbufferParameters(GLuint id)
{
	if (glIsRenderbuffer(id) == GL_FALSE)
		return "Not Renderbuffer object";

	int width, height, format;
	glBindRenderbuffer(GL_RENDERBUFFER, id);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);    // get renderbuffer width
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);  // get renderbuffer height
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format); // get renderbuffer internal format
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	char* formatName = convertInternalFormatToString(format);

	char* strParameters = (char*)malloc(64);
	sprintf(strParameters, "width %d height %d %s\n", width, height, formatName);
	return strParameters;
}

void CapabilitiesManager::GetFramebufferObject(std::string& frameBufferObjet) const
{
	frameBufferObjet.clear();

	// ref : https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGetFramebufferAttachmentParameteriv.xml

	PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)wglGetProcAddress("glGetFramebufferAttachmentParameteriv");

	// print max # of colorbuffers supported by FBO
	int colorBufferCount = 0;
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &colorBufferCount);
	frameBufferObjet += string_format("Max Number of Color Buffer Attachment Points: %d \n", colorBufferCount);

	int width, height;
	glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &width);
	glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &height);
	frameBufferObjet += string_format("Max width and height of Color Buffer: %d x %d\n", width, height);

	return;
	// the following info is about the framebuffer currently bounded

	GLint objectType = 0;
	GLint objectId = 0;

	// print info of the colorbuffer attachable image
	for (int i = 0; i < colorBufferCount; ++i)
	{
		GLenum err = glGetError();
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0 + i,
			GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
			&objectType);
		err = glGetError();
		if (objectType != GL_NONE)
		{
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0 + i,
				GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
				&objectId);

			printf("Color Attachment %d :", i);
			char* parameters;
			if (objectType == GL_TEXTURE)
			{
				parameters = getTextureParameters(objectId);
				printf("GL_TEXTURE, %s", parameters);
				free (parameters);
			}
			else if (objectType == GL_RENDERBUFFER)
			{
				parameters = getRenderbufferParameters(objectId);
				printf("GL_RENDERBUFFER, %s", parameters);
				free(parameters);
			}
		}
		else
			printf("No image is attached");
		printf("\n");
	}

	// print info of the depthbuffer attachable image
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
		&objectType);
	if (objectType != GL_NONE)
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
			GL_DEPTH_ATTACHMENT,
			GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
			&objectId);

		char* parameters;
		printf("Depth Attachment: ");
		switch (objectType)
		{
		case GL_TEXTURE:
			{
				parameters = getTextureParameters(objectId);
				printf("GL_TEXTURE, %s", parameters);
				free (parameters);
			}
			break;
		case GL_RENDERBUFFER:
			{
				parameters = getRenderbufferParameters(objectId);
				printf("GL_RENDERBUFFER, %s", parameters);
				free(parameters);
			}
			break;
		}
	}
	else
		printf("No image is attached");
	printf("\n");


	// print info of the stencilbuffer attachable image
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
		GL_STENCIL_ATTACHMENT,
		GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
		&objectType);
	if (objectType != GL_NONE)
	{
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
			GL_STENCIL_ATTACHMENT,
			GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
			&objectId);

		char* parameters;
		printf("Stencil Attachment: ");
		switch (objectType)
		{
		case GL_TEXTURE:
			{
				parameters = getTextureParameters(objectId);
				printf("GL_TEXTURE, %s", parameters);
				free(parameters);
			}
			break;
		case GL_RENDERBUFFER:
			{
				parameters = getRenderbufferParameters(objectId);
				printf("GL_RENDERBUFFER, %s", parameters);
				free(parameters);
			}
			break;
		}
	}
	else
		printf("No image is attached");
	printf("\n");

}

void CapabilitiesManager::GetShadersInfo(std::string& shadersInfo) const
{
	shadersInfo.clear();

	shadersInfo += string_format("geometry shader:\n");
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
	shadersInfo += string_format("  Max Geometry Texture Units           : %d\n", info);
	glGetIntegerv(MAX_GEOMETRY_VARYING_COMPONENTS_ARB, &info);
	shadersInfo += string_format("  Max Geometry Varying Components      : %d\n", info);
	glGetIntegerv(MAX_VERTEX_VARYING_COMPONENTS_ARB, &info);
	shadersInfo += string_format("  Max Vertex Varying Components        : %d\n", info);
	glGetIntegerv(MAX_VARYING_COMPONENTS, &info);
	shadersInfo += string_format("  Max Varying Components               : %d\n", info);
	glGetIntegerv(MAX_GEOMETRY_UNIFORM_COMPONENTS_ARB, &info);
	shadersInfo += string_format("  Max Geometry Uniform Components      : %d\n", info);
	glGetIntegerv(MAX_GEOMETRY_OUTPUT_VERTICES_ARB, &info);
	shadersInfo += string_format("  Max Geometry Output Vertices         : %d\n", info);
	glGetIntegerv(MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_ARB, &info);
	shadersInfo += string_format("  Max Geometry Total Output Components : %d\n", info);

	shadersInfo += string_format("vertex shader:\n");
	// GL_ARB_vertex_shader : https://www.opengl.org/registry/specs/ARB/vertex_shader.txt
	unsigned long MAX_VERTEX_UNIFORM_COMPONENTS_ARB = 0x8B4A;
	unsigned long MAX_VARYING_FLOATS_ARB = 0x8B4B;
	unsigned long MAX_VERTEX_ATTRIBS_ARB = 0x8869;
	unsigned long MAX_TEXTURE_IMAGE_UNITS_ARB = 0x8872;
	//unsigned long MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB = 0x8B4C;
	unsigned long MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB = 0x8B4D;
	unsigned long MAX_TEXTURE_COORDS_ARB = 0x8871;

	glGetIntegerv(MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &info);
	shadersInfo += string_format("  Max uniform vertex composantes       : %d\n", info);
	glGetIntegerv(MAX_VARYING_FLOATS_ARB, &info);
	shadersInfo += string_format("  Max Variante virgule flottante       : %d\n", info);
	glGetIntegerv(MAX_VERTEX_ATTRIBS_ARB, &info);
	shadersInfo += string_format("  Max sommet attribute                 : %d\n", info);
	glGetIntegerv(MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, &info);
	shadersInfo += string_format("  Max combined tex image units         : %d\n", info);
	//glGetIntegerv(MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB, &info);
	//printf("  Max vertex tex image units           : %d\n", info);

	shadersInfo += string_format("fragment shader:\n");
	// GL_ARB_fragment_shader : https://www.opengl.org/registry/specs/ARB/fragment_shader.txt
	unsigned long MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB = 0x8B49;
	GLenum err = glGetError();
	glGetIntegerv(MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &info);
	err = glGetError();
	shadersInfo += string_format("  Max uniform fragment composantes     : %d \n", info);
}

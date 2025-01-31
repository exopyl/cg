#ifndef GL_CAPABILITIESMANAGER_H
#define GL_CAPABILITIESMANAGER_H

//
// CapabilitiesManager is defined as a singleton
//
class GL_CapabilitiesManager
{
	friend class FreeCapabilitiesManagerSingleton;
public:
	static GL_CapabilitiesManager* GetInstance();

	// only for CAccelerationDialog. do not use elsewhere.
	bool IsShadersSupported() const;
	bool IsAOSupported() const;
	bool IsSMSupported() const;

	typedef enum {
		COMPRESSION_TYPE_GENERIC = 0,
		COMPRESSION_TYPE_S3TC
	} CompressionType;
	virtual bool IsTextureCompressionSupported(CompressionType eType = COMPRESSION_TYPE_GENERIC) const;

	virtual float GetGLSLVersion() const;
	virtual float GetVersion() const;
	virtual const char* GetVendor() const;
	virtual const char* GetRenderer() const;
	virtual int GetAvailableMemory() const;
	void GetMaxTextureDimensions(int& width, int& height) const;

	void GetMaxTextureSize(int& size) const;
	void GetMaxCubeMapTextureSize(int& size) const;

	// Specify the needed extents by their name and separate by a blank
	// example: needed_extents="GL_ARB_multitexture GL_EXT_vertex_weighting GL_KTX_buffer_region"
	virtual bool HasExtensions(const char* neededExtensions, bool bUseWglGetExtensionsStringARB = false) const;

	bool ExportDiagnostic() const;

protected :
	virtual const char* GetExtensions() const;
	virtual const char* GetWGLExtensions() const;

	static GL_CapabilitiesManager* m_pInstance;

	void GetInfo();
	bool InitializeInfo();

	GL_CapabilitiesManager();

	float m_openGLVersion;
	float m_openGLSLVersion;
	bool m_useShaders;

	char* m_vendor;
	char* m_version;
	char* m_renderer;
	char* m_extensions;
};

#endif // GL_CAPABILITIESMANAGER_H

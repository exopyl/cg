#ifndef GL_SHADERSMANAGER_HXX
#define GL_SHADERSMANAGER_HXX

#include	<ViewVP/GL_Shader.h>

//
// GL_ShadersManager is defined as a singleton
//
class GL_ShadersManager
{
	friend class FreeShadersManagerSingleton;
	friend class GL_ShadersManagerTest;
public:
	static GL_ShadersManager* GetInstance();
	~GL_ShadersManager();


	const Map<shader_type, GL_Shader*>& GetShaders() const;
	virtual GL_Shader* GetShader(shader_type type);
	virtual void AddRef();
	virtual void Release();

	void RecompileShader(shader_type type);

	GLint GetActiveTextureId(const ChaineInc& name);
	const ChaineInc& GetShaderDirectory() const;

protected:
	friend class CAccelerationDlg;
	friend class LocalGL_ShadersManager;
	virtual bool UseShaders() const;
	virtual bool IsAOSupported() const;
	virtual bool IsSMSupported() const;

protected:
	GL_ShadersManager();

	bool ComputeShaderDirectory();
	virtual void EmptyMapShaders();

	virtual GL_Shader* CreateNewShader(shader_type type) const;
	bool AddShader(shader_type type);

	static GL_ShadersManager* m_pInstance;

	ChaineInc m_shaderDirectory;
	Map<shader_type, GL_Shader*> m_shaders;

	unsigned int m_refCount;

	virtual bool DiagnosticShaders();
	void DiagnosticShadersInCurrentContext();
	virtual bool CheckShader(shader_type shaderType);
	bool CheckShaders(const shader_type* shaderType, int shaderTypes);
	bool m_bUseShaders;
	bool m_bAOSupported;
	bool m_bSMSupported;

	GLint m_activeTextureId = 0;
	GLint m_maxActiveTextureId = 0;
	Map<ChaineInc, GLint> m_activeTextureMap;
};


class LocalGL_ShadersManager : public GL_ShadersManager {
public:
	bool CanUseShader(bool withCurrentContext);
	bool UseShaders() const;
	bool IsAOSupported() const;
	bool IsSMSupported() const;
};

#endif // GL_SHADERSMANAGER_HXX

#pragma once

#include <string>

class CapabilitiesManager
{
public:
	static CapabilitiesManager* getInstance (void) { return m_pInstance; };

	float GetVersion() const;

	void GetCardInfo(std::string& cardInfo) const;
	void GetExtensions(std::string& extensions) const;
	void GetMemoryInfo(std::string& memoryInfo) const;
	void GetFramebufferObject(std::string& frameBufferObject) const;
	void GetShadersInfo(std::string& shadersInfo) const;

private:
	CapabilitiesManager() {};
	~CapabilitiesManager() {};
	static CapabilitiesManager *m_pInstance;
};

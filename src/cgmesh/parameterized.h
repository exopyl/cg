#pragma once
#include <string>
#include <vector>
#include <functional>

class Mesh;

//
// Parameter: a typed description of a value that can be edited from the UI.
// Geometries expose a list of Parameters via IParameterized::GetParameters().
// The UI reads/writes them through the getter/setter callbacks.
//
class Parameter
{
public:
	enum Type { INT, FLOAT, BOOL, ENUM };

	// Factory helpers
	static Parameter MakeInt(const std::string &name, int *value, int minV, int maxV);
	static Parameter MakeFloat(const std::string &name, float *value, float minV, float maxV);
	static Parameter MakeBool(const std::string &name, bool *value);
	static Parameter MakeEnum(const std::string &name, int *value, const std::vector<std::string> &choices);

	// Accessors used by the UI
	Type                            GetType() const      { return m_type; }
	const std::string&              GetName() const      { return m_name; }
	const std::vector<std::string>& GetChoices() const   { return m_choices; }
	float                           GetMinFloat() const  { return m_minFloat; }
	float                           GetMaxFloat() const  { return m_maxFloat; }
	int                             GetMinInt() const    { return m_minInt; }
	int                             GetMaxInt() const    { return m_maxInt; }

	int   GetInt() const           { return *m_pInt; }
	float GetFloat() const         { return *m_pFloat; }
	bool  GetBool() const          { return *m_pBool; }

	void  SetInt(int v)            { *m_pInt = v; }
	void  SetFloat(float v)        { *m_pFloat = v; }
	void  SetBool(bool v)          { *m_pBool = v; }

private:
	Type m_type;
	std::string m_name;
	std::vector<std::string> m_choices;

	// value pointers (only one is used depending on type)
	int *m_pInt = nullptr;
	float *m_pFloat = nullptr;
	bool *m_pBool = nullptr;

	int m_minInt = 0, m_maxInt = 0;
	float m_minFloat = 0.f, m_maxFloat = 0.f;
};

//
// IParameterized: interface for any object (typically a geometry generator)
// that exposes a set of editable parameters.
//
class IParameterized
{
public:
	virtual ~IParameterized() {}

	// Return the list of parameters exposed to the UI.
	// Called once when the UI binds to the object.
	virtual std::vector<Parameter> GetParameters() = 0;

	// Rebuild the output (mesh/geometry) from the current parameter values.
	// Called whenever a parameter is edited in the UI.
	virtual void Regenerate() = 0;

	// Human-readable name for the UI (e.g. menu label, panel title).
	virtual std::string GetName() const = 0;

	// Transfer ownership of the current renderable mesh to the caller, if
	// this parameterized object produces one. Default: nullptr (no mesh).
	// Subclasses that produce a Mesh override this.
	virtual Mesh* TakeMesh() { return nullptr; }
};

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
	// STRING : une valeur TEXTUELLE, sans bornes ni choix. C'est le seul type qui
	// ne se ramene pas a un nombre, d'ou son ajout tardif -- il a fallu le texte
	// 3D, dont la chaine a extruder EST le parametre principal, pour qu'il ait
	// une raison d'exister. Les interfaces doivent donc l'ecrire par un chemin
	// distinct de leur ecriture numerique habituelle.
	enum Type { INT, FLOAT, BOOL, ENUM, STRING };

	// Factory helpers
	static Parameter MakeInt(const std::string &name, int *value, int minV, int maxV);
	static Parameter MakeFloat(const std::string &name, float *value, float minV, float maxV);
	static Parameter MakeBool(const std::string &name, bool *value);
	static Parameter MakeEnum(const std::string &name, int *value, const std::vector<std::string> &choices);
	// `multiline` n'est qu'un INDICE de presentation : l'interface est libre de
	// l'ignorer, la valeur reste une chaine dans les deux cas.
	static Parameter MakeString(const std::string &name, std::string *value,
	                            bool multiline = false);

	// Accessors used by the UI
	Type                            GetType() const      { return m_type; }
	const std::string&              GetName() const      { return m_name; }
	const std::vector<std::string>& GetChoices() const   { return m_choices; }
	float                           GetMinFloat() const  { return m_minFloat; }
	float                           GetMaxFloat() const  { return m_maxFloat; }
	int                             GetMinInt() const    { return m_minInt; }
	int                             GetMaxInt() const    { return m_maxInt; }

	bool  IsMultiline() const      { return m_multiline; }

	int   GetInt() const           { return *m_pInt; }
	float GetFloat() const         { return *m_pFloat; }
	bool  GetBool() const          { return *m_pBool; }
	const std::string& GetString() const { return *m_pString; }

	void  SetInt(int v)            { *m_pInt = v; }
	void  SetFloat(float v)        { *m_pFloat = v; }
	void  SetBool(bool v)          { *m_pBool = v; }
	void  SetString(const std::string &v) { *m_pString = v; }

private:
	Type m_type;
	std::string m_name;
	std::vector<std::string> m_choices;

	// value pointers (only one is used depending on type)
	int *m_pInt = nullptr;
	float *m_pFloat = nullptr;
	bool *m_pBool = nullptr;
	std::string *m_pString = nullptr;

	int m_minInt = 0, m_maxInt = 0;
	float m_minFloat = 0.f, m_maxFloat = 0.f;
	bool m_multiline = false;
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

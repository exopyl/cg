#include "parameterized.h"

Parameter Parameter::MakeInt(const std::string &name, int *value, int minV, int maxV)
{
	Parameter p;
	p.m_type = INT;
	p.m_name = name;
	p.m_pInt = value;
	p.m_minInt = minV;
	p.m_maxInt = maxV;
	return p;
}

Parameter Parameter::MakeFloat(const std::string &name, float *value, float minV, float maxV)
{
	Parameter p;
	p.m_type = FLOAT;
	p.m_name = name;
	p.m_pFloat = value;
	p.m_minFloat = minV;
	p.m_maxFloat = maxV;
	return p;
}

Parameter Parameter::MakeBool(const std::string &name, bool *value)
{
	Parameter p;
	p.m_type = BOOL;
	p.m_name = name;
	p.m_pBool = value;
	return p;
}

Parameter Parameter::MakeEnum(const std::string &name, int *value, const std::vector<std::string> &choices)
{
	Parameter p;
	p.m_type = ENUM;
	p.m_name = name;
	p.m_pInt = value;
	p.m_choices = choices;
	p.m_minInt = 0;
	p.m_maxInt = (int)choices.size() - 1;
	return p;
}

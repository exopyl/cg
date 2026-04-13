#pragma once
// Rule
//
class LSRule
{
public:
	LSRule ();
	~LSRule ();

	void Init (char *antecedent, char *image);
	bool IsApplicable (char *str);

public:
	char *m_antecedent;
	char *m_image;
};

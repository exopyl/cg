#pragma once
// Rule
//
class LSRule
{
public:
	LSRule ();
	~LSRule ();

	void Init (char *antecedent, char *image);
	bool IsApplicable (char *str, int length=1);

public:
	char *m_antecedent;
	char *m_image;
};

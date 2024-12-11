#ifndef __LSRULE_H__
#define __LSRULE_H__

//
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


#endif // __LSRULE_H__

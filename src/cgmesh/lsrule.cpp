#include <stdlib.h>
#include <string.h>

#include "lsrule.h"


// Constructor
LSRule::LSRule ()
{
	m_antecedent = nullptr;
	m_image = nullptr;
}

// Destructor
LSRule::~LSRule ()
{
	if (m_antecedent) free (m_antecedent);
	if (m_image) free (m_image);
}

//
void LSRule::Init (char *antecedent, char *image)
{
	m_antecedent = strdup (antecedent);
	m_image = strdup (image);
}

//
bool LSRule::IsApplicable (char *str)
{
	if (!m_antecedent) return false;
	int length = strlen(m_antecedent);
	return (memcmp (m_antecedent, str, length) == 0)? true : false;
}

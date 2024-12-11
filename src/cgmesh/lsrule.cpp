#include <stdlib.h>
#include <string.h>

#include "lsrule.h"


// Constructor
LSRule::LSRule ()
{
	m_antecedent = NULL;
	m_image = NULL;
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
bool LSRule::IsApplicable (char *str, int length)
{
	return (memcmp (m_antecedent, str, length) == 0)? true : false;
}

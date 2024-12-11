#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "lsystem.h"
#include "../cgmath/TQuaternion.h"

// constructor
LSystem::LSystem ()
{
	m_strName = NULL;

	//
	m_string = NULL;
	m_iNumberRules = 0;
	m_oRules = (LSRule**)malloc(2*sizeof(LSRule*));;

	// graphical representation
	m_iDimension = 0;

	m_fX = m_fY = m_fZ = 0.0;
	m_fCurrentAngle = 90.*3.14159/180.;
	m_fAngle = 0.0;

	m_fCurrentLength = 1.;
	m_fLength = 1.;

	m_iNumberMaxPoints = 100000;
	m_walk = (float*)malloc(m_iNumberMaxPoints*2*sizeof(float));
	m_walk[0] = m_walk[1] = 0.0;
	m_walk[2] = 0.0;
	m_iNumberPoints = 1;

	m_fStackPosition = NULL;
	m_fStackAngle = NULL;
	m_bDrawable = NULL;
}

// destructor
LSystem::~LSystem ()
{
	if (m_strName) free (m_strName);
	if (m_string) free (m_string);
	if (m_walk) free (m_walk);
	if (m_bDrawable) free (m_bDrawable);
	for (int i=0; i<m_iNumberRules; i++)
	{
		delete m_oRules[i];
	}
	free (m_oRules);
}

//
void LSystem::SetName (char *strName)
{
	if (m_strName)
		free (m_strName);
	m_strName = strdup (strName);
}

//
char* LSystem::GetName (void)
{
	return m_strName;
}


// set the angle
void LSystem::SetAngle (float fAngle)
{
	m_fAngle = fAngle;
}

void LSystem::SetLength (float fLength)
{
	m_fLength = fLength;
	m_fCurrentLength = m_fLength;
}

// add a rule
void LSystem::AddRule (char *antecedent, char *image)
{
	LSRule *rule = new LSRule ();
	rule->Init (antecedent, image);
	if (rule)
	{
		m_oRules[m_iNumberRules] = rule;
		m_iNumberRules++;
	}
}

// initiazes the LSystem
void LSystem::Init (char *basis)
{
	m_string = (char*)malloc(strlen(basis)+1);
	m_string = (char*)memcpy (m_string, basis, strlen (basis));
	m_string[strlen (basis)] = '\0';
}

// computes the next iteration
void LSystem::Next()
{
	char *newString = (char*)malloc(70000*sizeof(char*));
	int LengthNewString = 0;
	int LengthString = strlen (m_string);
	for (int i=0; i<LengthString; i++)
	{
		bool rewritten = false;
		for (int j=0; j<m_iNumberRules; j++)
		{
			if (m_oRules[j]->IsApplicable (&m_string[i]))
			{
				memcpy (&newString[LengthNewString], m_oRules[j]->m_image, strlen(m_oRules[j]->m_image));
				LengthNewString += strlen(m_oRules[j]->m_image);
				rewritten = true;
			}
		}
		if (!rewritten)
		{
			newString[LengthNewString++] = m_string[i];
		}
	}
	newString[LengthNewString] = '\0';
	//free (m_string);
	m_string = newString;
}

// compute the 2D graphical interpretation of the LSystem
void LSystem::ComputeGraphicalInterpretation2D (void)
{
	m_iDimension = 2;

	int n = strlen (m_string);
	
	m_iNumberPoints = 1;
	m_iStackIndex = 0;
	m_fStackPosition = (float*)malloc(m_iNumberMaxPoints*2*sizeof(float));
	m_fStackAngle = (float*)malloc(m_iNumberMaxPoints*sizeof(float));
	m_bDrawable = (bool*)malloc(m_iNumberMaxPoints*sizeof(bool));

	for (int i=0; i<n; i++)
	{
		switch (m_string[i])
		{
		case 'X':
		case 'Y':
			break;
		case 'F':
		case 'A':
		case 'B':
			m_walk[2*m_iNumberPoints]   = m_walk[2*(m_iNumberPoints-1)] + m_fCurrentLength*cos(m_fCurrentAngle);
			m_walk[2*m_iNumberPoints+1] = m_walk[2*(m_iNumberPoints-1)+1] + m_fCurrentLength*sin(m_fCurrentAngle);
			m_bDrawable[m_iNumberPoints] = true;
			m_iNumberPoints++;
			break;
		case '+':
			m_fCurrentAngle += m_fAngle;
			if (m_fCurrentAngle > 2.*3.14159265358979323846) m_fCurrentAngle -= 2.*3.14159265358979323846;
			break;
		case '-':
			m_fCurrentAngle -= m_fAngle;
			if (m_fCurrentAngle < 0.) m_fCurrentAngle += 2.*3.14159265358979323846;
			break;
		case '<':
			m_fCurrentLength /= m_fLength;
			break;
		case '>':
			m_fCurrentLength *= m_fLength;
			break;
		case '[':
			m_fStackPosition[2*m_iStackIndex] = m_walk[2*(m_iNumberPoints-1)];
			m_fStackPosition[2*m_iStackIndex+1] = m_walk[2*(m_iNumberPoints-1)+1];
			m_fStackAngle[m_iStackIndex] = m_fCurrentAngle;
			m_iStackIndex++;
			if (m_iStackIndex >= m_iNumberMaxPoints) return;
			break;
		case ']':
			m_walk[2*m_iNumberPoints] = m_fStackPosition[2*(m_iStackIndex-1)];
			m_walk[2*m_iNumberPoints+1] = m_fStackPosition[2*(m_iStackIndex-1)+1];
			m_fCurrentAngle = m_fStackAngle[m_iStackIndex-1];
			m_bDrawable[m_iNumberPoints-1] = false;
			m_iNumberPoints++;
			m_iStackIndex--;
			break;
		default:
			break;
		}
	}
	if (m_fStackPosition) free (m_fStackPosition);
	if (m_fStackAngle) free (m_fStackAngle);

	UpdateBoundingBox ();
	Centerize ();
}

// compute the 3D graphical interpretation of the LSystem
void LSystem::ComputeGraphicalInterpretation3D (void)
{
	m_iDimension = 3;

	int n = strlen (m_string);

	m_iStackIndex = 0;
	m_fStackPosition = (float*)malloc(m_iNumberMaxPoints*3*sizeof(float));
	float *fStackDirection = (float*)malloc(m_iNumberMaxPoints*3*sizeof(float));
	m_bDrawable = (bool*)malloc(m_iNumberMaxPoints*sizeof(bool));

	float m_direction[3];
	m_direction[0] = 1.0;
	m_direction[1] = 0.0;
	m_direction[2] = 0.0;

	float m_vecUp[3];
	m_vecUp[0] = 0.0;
	m_vecUp[1] = 0.0;
	m_vecUp[2] = 1.0;
	float fAngle;

	for (int i=0; i<n; i++)
	{
		float xtmp = m_direction[0];
		float ytmp = m_direction[1];
		float ztmp = m_direction[2];

		float upx = m_vecUp[0];
		float upy = m_vecUp[1];
		float upz = m_vecUp[2];

		switch (m_string[i])
		{
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			break;
		case 'F':
		//case 'L':
		case 'X':
			m_walk[3*m_iNumberPoints]   = m_walk[3*(m_iNumberPoints-1)] + xtmp;
			m_walk[3*m_iNumberPoints+1] = m_walk[3*(m_iNumberPoints-1)+1] + ytmp;
			m_walk[3*m_iNumberPoints+2] = m_walk[3*(m_iNumberPoints-1)+2] + ztmp;
			m_bDrawable[m_iNumberPoints] = true;
			m_iNumberPoints++;
			break;
		case '+':
			{
				//m_direction[0] = xtmp * cos(m_fAngle) + ytmp * sin(m_fAngle);
				//m_direction[1] = -xtmp * sin(m_fAngle) + ytmp * cos(m_fAngle);
				//m_direction[2] = ztmp;

				Vector3f axis (m_vecUp[0], m_vecUp[1], m_vecUp[2]);
				Quaternionf q (axis, m_fAngle);
				q.Normalize ();
				Vector3f orig (m_direction[0], m_direction[1], m_direction[2]);
				orig.Normalize ();
				Vector3f dest (0., 0., 0.);
				q.rotate (dest, orig);

				m_direction[0] = dest[0];
				m_direction[1] = dest[1];
				m_direction[2] = dest[2];
			}
			break;
		case '-':
			{
				//m_direction[0] = xtmp * cos(-m_fAngle) + ytmp * sin(-m_fAngle);
				//m_direction[1] = -xtmp * sin(-m_fAngle) + ytmp * cos(-m_fAngle);
				//m_direction[2] = ztmp;

				Vector3f axis (m_vecUp[0], m_vecUp[1], m_vecUp[2]);
				Quaternionf q (axis,-m_fAngle);
				Vector3f orig (m_direction[0], m_direction[1], m_direction[2]);
				orig.Normalize ();
				Vector3f dest (0., 0., 0.);
				q.rotate (dest, orig);

				m_direction[0] = dest[0];
				m_direction[1] = dest[1];
				m_direction[2] = dest[2];
			}
			break;
		case '&':
			{
				Vector3f Up (m_vecUp[0], m_vecUp[1], m_vecUp[2]);
				Vector3f Dir (m_direction[0], m_direction[1], m_direction[2]);
				Vector3f axis (Dir ^ Up);
				Quaternionf q (axis, m_fAngle);

				Dir.Normalize ();
				Vector3f NewDir (0., 0., 0.);
				q.rotate (NewDir, Dir);

				m_direction[0] = NewDir[0];
				m_direction[1] = NewDir[1];
				m_direction[2] = NewDir[2];

				Up.Normalize ();
				Vector3f NewUp (0., 0., 0.);
				q.rotate (NewUp, Up);

				m_vecUp[0] = NewUp[0];
				m_vecUp[1] = NewUp[1];
				m_vecUp[2] = NewUp[2];
			}
			/*
			m_direction[0] = xtmp * cos(m_fAngle) - ztmp * sin(m_fAngle);
			m_direction[1] = ytmp;
			m_direction[2] = xtmp * sin(m_fAngle) + ztmp * cos(m_fAngle);

			m_vecUp[0] = upx * cos(m_fAngle) - upz * sin(m_fAngle);
			m_vecUp[1] = upy;
			m_vecUp[2] = upx * sin(m_fAngle) + upz * cos(m_fAngle);
			*/
			break;
		case '^':
			{
				Vector3f Up (m_vecUp[0], m_vecUp[1], m_vecUp[2]);
				Vector3f Dir (m_direction[0], m_direction[1], m_direction[2]);
				Vector3f axis (Dir ^ Up);
				Quaternionf q (axis, -m_fAngle);

				Dir.Normalize ();
				Vector3f NewDir (0., 0., 0.);
				q.rotate (NewDir, Dir);

				m_direction[0] = NewDir[0];
				m_direction[1] = NewDir[1];
				m_direction[2] = NewDir[2];

				Up.Normalize ();
				Vector3f NewUp (0., 0., 0.);
				q.rotate (NewUp, Up);

				m_vecUp[0] = NewUp[0];
				m_vecUp[1] = NewUp[1];
				m_vecUp[2] = NewUp[2];
			}
			/*
			m_direction[0] = xtmp * cos(-m_fAngle) - ztmp * sin(-m_fAngle);
			m_direction[1] = ytmp;
			m_direction[2] = xtmp * sin(-m_fAngle) + ztmp * cos(-m_fAngle);
			
			m_vecUp[0] = upx * cos(-m_fAngle) - upz * sin(-m_fAngle);
			m_vecUp[1] = upy;
			m_vecUp[2] = upx * sin(-m_fAngle) + upz * cos(-m_fAngle);
			*/
			break;
		case '\\':
			{
				Vector3f Up (m_vecUp[0], m_vecUp[1], m_vecUp[2]);
				Vector3f Dir (m_direction[0], m_direction[1], m_direction[2]);
				Vector3f axis (Dir);
				Quaternionf q (axis, -m_fAngle);

				Up.Normalize ();
				Vector3f NewUp (0., 0., 0.);
				q.rotate (NewUp, Up);

				m_vecUp[0] = NewUp[0];
				m_vecUp[1] = NewUp[1];
				m_vecUp[2] = NewUp[2];
			}
			break;
		case '/':
			{
				Vector3f Up (m_vecUp[0], m_vecUp[1], m_vecUp[2]);
				Vector3f Dir (m_direction[0], m_direction[1], m_direction[2]);
				Vector3f axis (Dir);
				Quaternionf q (axis, m_fAngle);

				Up.Normalize ();
				Vector3f NewUp (0., 0., 0.);
				q.rotate (NewUp, Up);

				m_vecUp[0] = NewUp[0];
				m_vecUp[1] = NewUp[1];
				m_vecUp[2] = NewUp[2];
			}
			/*
			m_direction[0] = xtmp;
			m_direction[1] = ytmp * cos(-m_fAngle) - ztmp * sin(-m_fAngle);
			m_direction[2] = ytmp * sin(-m_fAngle) + ztmp * cos(-m_fAngle);

			m_vecUp[0] = upx;
			m_vecUp[1] = upy * cos(-m_fAngle) - upz * sin(-m_fAngle);
			m_vecUp[2] = upy * sin(-m_fAngle) + upz * cos(-m_fAngle);
			*/
			break;
		case '|':
			{
				Vector3f axis (m_vecUp[0], m_vecUp[1], m_vecUp[2]);
				Quaternionf q (axis, 3.14159);
				q.Normalize ();
				Vector3f orig (m_direction[0], m_direction[1], m_direction[2]);
				orig.Normalize ();
				Vector3f dest (0., 0., 0.);
				q.rotate (dest, orig);

				m_direction[0] = dest[0];
				m_direction[1] = dest[1];
				m_direction[2] = dest[2];
			}
			/*
			m_direction[0] = xtmp * cos(3.14159) + ytmp * sin(3.14159);
			m_direction[1] = -xtmp * sin(3.14159) + ytmp * cos(3.14159);
			m_direction[2] = ztmp;
			*/

			//m_vecUp[0] = upx * cos(3.14159) + upy * sin(3.14159);
			//m_vecUp[1] = -upx * sin(3.14159) + upy * cos(3.14159);
			//m_vecUp[2] = upz;
			break;
		case '[':
			m_fStackPosition[3*m_iStackIndex]   = m_walk[3*(m_iNumberPoints-1)];
			m_fStackPosition[3*m_iStackIndex+1] = m_walk[3*(m_iNumberPoints-1)+1];
			m_fStackPosition[3*m_iStackIndex+2] = m_walk[3*(m_iNumberPoints-1)+2];
			
			fStackDirection[3*m_iStackIndex]   = m_direction[0];
			fStackDirection[3*m_iStackIndex+1] = m_direction[1];
			fStackDirection[3*m_iStackIndex+2] = m_direction[2];
			
			m_iStackIndex++;
			if (m_iStackIndex >= m_iNumberMaxPoints) return;
			break;
		case ']':
			m_walk[3*m_iNumberPoints]   = m_fStackPosition[3*(m_iStackIndex-1)];
			m_walk[3*m_iNumberPoints+1] = m_fStackPosition[3*(m_iStackIndex-1)+1];
			m_walk[3*m_iNumberPoints+2] = m_fStackPosition[3*(m_iStackIndex-1)+2];
			
			m_direction[0] = fStackDirection[3*(m_iStackIndex-1)];
			m_direction[1] = fStackDirection[3*(m_iStackIndex-1)+1];
			m_direction[2] = fStackDirection[3*(m_iStackIndex-1)+2];

			m_bDrawable[m_iNumberPoints-1] = false;
			m_iNumberPoints++;
			m_iStackIndex--;
			break;
		default:
			break;
		}
	}
	if (m_fStackPosition) free (m_fStackPosition);
	if (fStackDirection) free (fStackDirection);

	UpdateBoundingBox ();
	//Centerize ();
}

// dump
void LSystem::Dump (void)
{
	for (int i=0; i<m_iNumberPoints; i++)
	{
		printf ("%f %f\n", m_walk[2*i], m_walk[2*i+1]);
	}
}


// scale the graphical representation
void LSystem::Scaling (float scale)
{
	if (scale == 0.0) return;

	int i;
	switch (m_iDimension)
	{
	case 2:
		{
			for (i=0; i<m_iNumberPoints; i++)
			{
				m_walk[2*i] *= scale;
				m_walk[2*i+1] *= scale;
			}
		}
		break;
	case 3:
		{
			for (i=0; i<m_iNumberPoints; i++)
			{
				m_walk[3*i] *= scale;
				m_walk[3*i+1] *= scale;
				m_walk[3*i+2] *= scale;
			}
		}
		break;
	default:
		break;
	}
}

// computes the bounding box of the graphical representation
void LSystem::UpdateBoundingBox (void)
{
	int i;
	switch (m_iDimension)
	{
	case 2:
		{
			XMinBBox = m_walk[0];
			XMaxBBox = m_walk[0];
			YMinBBox = m_walk[1];
			YMaxBBox = m_walk[1];
			for (i=0; i<m_iNumberPoints; i++)
			{
				if (m_walk[2*i] > XMaxBBox) XMaxBBox = m_walk[2*i];
				if (m_walk[2*i] < XMinBBox) XMinBBox = m_walk[2*i];
				if (m_walk[2*i+1] > YMaxBBox) YMaxBBox = m_walk[2*i+1];
				if (m_walk[2*i+1] < YMinBBox) YMinBBox = m_walk[2*i+1];
			}
			ZMinBBox = 0.0;
			ZMaxBBox = 0.0;
		}
		break;
	case 3:
		{
			XMinBBox = m_walk[0];
			XMaxBBox = m_walk[0];
			YMinBBox = m_walk[1];
			YMaxBBox = m_walk[1];
			ZMinBBox = m_walk[2];
			ZMaxBBox = m_walk[2];
			for (i=0; i<m_iNumberPoints; i++)
			{
				if (m_walk[3*i] > XMaxBBox) XMaxBBox = m_walk[3*i];
				if (m_walk[3*i] < XMinBBox) XMinBBox = m_walk[3*i];
				if (m_walk[3*i+1] > YMaxBBox) YMaxBBox = m_walk[3*i+1];
				if (m_walk[3*i+1] < YMinBBox) YMinBBox = m_walk[3*i+1];
				if (m_walk[3*i+2] > ZMaxBBox) ZMaxBBox = m_walk[3*i+2];
				if (m_walk[3*i+2] < ZMinBBox) ZMinBBox = m_walk[3*i+2];
			}
		}
		break;
	default:
		break;
	}
}

// centerizes the graphical representation
void LSystem::Centerize (void)
{
	int i;
	switch (m_iDimension)
	{
	case 2:
		{
			for (i=0; i<m_iNumberPoints; i++)
			{
				m_walk[2*i] -= (XMaxBBox+XMinBBox)/2.0;
				m_walk[2*i+1] -= (YMaxBBox+YMinBBox)/2.0;
			}
			XMinBBox -= (XMaxBBox+XMinBBox)/2.0;
			XMaxBBox -= (XMaxBBox+XMinBBox)/2.0;
			YMinBBox -= (YMaxBBox+YMinBBox)/2.0;
			YMaxBBox -= (YMaxBBox+YMinBBox)/2.0;
		}
		break;
	case 3:
		{
			for (i=0; i<m_iNumberPoints; i++)
			{
				m_walk[3*i]   -= (XMaxBBox+XMinBBox)/2.0;
				m_walk[3*i+1] -= (YMaxBBox+YMinBBox)/2.0;
				m_walk[3*i+2] -= (ZMaxBBox+ZMinBBox)/2.0;
			}
			XMinBBox -= (XMaxBBox+XMinBBox)/2.0;
			XMaxBBox -= (XMaxBBox+XMinBBox)/2.0;
			YMinBBox -= (YMaxBBox+YMinBBox)/2.0;
			YMaxBBox -= (YMaxBBox+YMinBBox)/2.0;
			ZMinBBox -= (ZMaxBBox+ZMinBBox)/2.0;
			ZMaxBBox -= (ZMaxBBox+ZMinBBox)/2.0;
		}
		break;
	default:
		break;
	}
}

// normalizes the graphical representation
void LSystem::Normalize (void)
{
	switch (m_iDimension)
	{
	case 2:
		{
			if (XMaxBBox-XMinBBox > YMaxBBox-YMinBBox)
			{
				if (XMaxBBox-XMinBBox != 0.0)
					Scaling (1.0/(XMaxBBox-XMinBBox));
			}
			else
			{
				if (YMaxBBox-YMinBBox != 0.0)
					Scaling (1.0/(YMaxBBox-YMinBBox));
			}
		}
		break;
	case 3:
		{
			if (XMaxBBox-XMinBBox > YMaxBBox-YMinBBox)
			{
				if (XMaxBBox-XMinBBox != 0.0)
					Scaling (1.0/(XMaxBBox-XMinBBox));
			}
			else
			{
				if (YMaxBBox-YMinBBox != 0.0)
					Scaling (1.0/(YMaxBBox-YMinBBox));
			}
		}
		break;
	default:
		break;
	}
}

void LSystem::Translate (float fX, float fY)
{
	if (m_iDimension != 2)
		return;

	for (unsigned int i=0; i<m_iNumberPoints; i++)
	{
		m_walk[2*i]		+= fX;
		m_walk[2*i+1]	+= fY;
	}
}

void LSystem::MirrorAroundX (float fHeight)
{
	if (m_iDimension != 2)
		return;

	for (unsigned int i=0; i<m_iNumberPoints; i++)
	{
		m_walk[2*i+1] = fHeight - m_walk[2*i+1];
	}
}

void LSystem::FittIn (float fWidth, float fHeight, float fMargin)
{
	UpdateBoundingBox ();
	Centerize ();
	Normalize ();

	//if (XMaxBBox - XMinBBox > YMaxBBox - YMinBBox)
	//	Scaling (fWidth + fMargin);
	//else
	Scaling (fHeight-2.*fMargin);
	Translate (fWidth/2., fHeight/2.);
	MirrorAroundX (fHeight);
}


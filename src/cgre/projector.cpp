#include <stdio.h>

#include "../cgmath/cgmath.h"
#include "../cgimg/cgimg.h"
#include "projector.h"


/* matrix = identity */
void
matrixIdentity(GLfloat matrix[16])
{
  matrix[0] = 1.0;
  matrix[1] = 0.0;
  matrix[2] = 0.0;
  matrix[3] = 0.0;
  matrix[4] = 0.0;
  matrix[5] = 1.0;
  matrix[6] = 0.0;
  matrix[7] = 0.0;
  matrix[8] = 0.0;
  matrix[9] = 0.0;
  matrix[10] = 1.0;
  matrix[11] = 0.0;
  matrix[12] = 0.0;
  matrix[13] = 0.0;
  matrix[14] = 0.0;
  matrix[15] = 1.0;
}

Projector::Projector ()
{
	m_linearFilter = GL_FALSE;
	m_showProjection = GL_TRUE;

	matrixIdentity((GLfloat *) m_textureXform);

	m_eye[0] = 0.0;
	m_eye[1] = 0.0;
	m_eye[2] = 0.4;
	m_lookAt[0] = 0.0;
	m_lookAt[1] = 0.0;
	m_lookAt[2] = 0.0;
	m_up[0] = 0.0;
	m_up[1] = 1.0;
	m_up[2] = 0.0;

	m_angle1 = 0.0;
	m_angle2 = 0.0;

	xmin = -0.01; xmax = 0.01;
	ymin = -0.01; ymax = 0.01;
	nnear = 0.1;
	ffar = 0.7;

}

/* load SGI .rgb image (pad with a border of the specified width and color) */
static void
imgLoad(char *filenameIn, int borderIn, int *wOut, int *hOut, GLubyte ** imgOut)
{
  int border = 0;//borderIn;
  int width, height;
  int w, h;
  GLubyte *image, *img, *p;
  int i, j;

  Img *pImg = new Img ();
  pImg->load (filenameIn);
  //img->save ("toto.ppm");
  image = pImg->m_pPixels;
  width = pImg->width ();
  height = pImg->height ();
  //image = (GLubyte *) read_texture(filenameIn, &width, &height, &components);
  w = width + 2 * border;
  h = height + 2 * border;
  img = (GLubyte *) calloc(w * h, 4 * sizeof(unsigned char));

  p = img;
   for (j = height-1; j >= 0; j--) {
    for (i = 0; i < width; i++) {
        p[0] = image[4 * (j * width + i) + 0];
        p[1] = image[4 * (j * width + i) + 1];
        p[2] = image[4 * (j * width + i) + 2];
        p[3] = 0xff;
       p += 4;
    }
 }
   delete pImg;
  *wOut = w;
  *hOut = h;
  *imgOut = img;
}

/* Create a simple spotlight pattern and make it the current texture */
void Projector::loadSpotlightTexture(void)
{
  static int texWidth = 64, texHeight = 64;
  static GLubyte *texData;
  GLfloat borderColor[4] =
  {0.1, 0.1, 0.1, 1.0};

  if (!texData) {
    GLubyte *p;
    int i, j;

    texData = (GLubyte *) malloc(texWidth * texHeight * 4 * sizeof(GLubyte));

    p = texData;
    for (j = 0; j < texHeight; ++j) {
      float dy = (texHeight * 0.5 - j + 0.5) / (texHeight * 0.5);

      for (i = 0; i < texWidth; ++i) {
        float dx = (texWidth * 0.5 - i + 0.5) / (texWidth * 0.5);
        float r = cos(M_PI / 2.0 * sqrt(dx * dx + dy * dy));
        float c;

        r = (r < 0) ? 0 : r * r;
        c = 0xff * (r + borderColor[0]);
        p[0] = (c <= 0xff) ? c : 0xff;
        c = 0xff * (r + borderColor[1]);
        p[1] = (c <= 0xff) ? c : 0xff;
        c = 0xff * (r + borderColor[2]);
        p[2] = (c <= 0xff) ? c : 0xff;
        c = 0xff * (r + borderColor[3]);
        p[3] = (c <= 0xff) ? c : 0xff;
        p += 4;
      }
    }
  }
  if (m_linearFilter)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  //gluBuild2DMipmaps(GL_TEXTURE_2D, 4, texWidth, texHeight, GL_RGBA, GL_UNSIGNED_BYTE, texData); // deprecated
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData); // TODO : to check it works
  glGenerateMipmap(GL_TEXTURE_2D);
}

/**
* Set a picture
*/
void Projector::SetPicture (char *filename)
{
  static int texWidth, texHeight;
  static GLubyte *texData;

	if (!filename)
	{
		loadSpotlightTexture ();
		return;
	}

  if (!texData && filename)
  {
    imgLoad(filename, 2, &texWidth, &texHeight, &texData);
  }
  if (m_linearFilter)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  GLfloat borderColor[4] =
  {0.1, 0.1, 0.1, 1.0};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  //gluBuild2DMipmaps(GL_TEXTURE_2D, 4, texWidth, texHeight, GL_RGBA, GL_UNSIGNED_BYTE, texData); // deprecated
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData); // TODO : to check it works
  glGenerateMipmap(GL_TEXTURE_2D);
}

/* matrix2 = transpose(matrix1) */
void
matrixTranspose(GLfloat matrix2[16], GLfloat matrix1[16])
{
  matrix2[0] = matrix1[0];
  matrix2[1] = matrix1[4];
  matrix2[2] = matrix1[8];
  matrix2[3] = matrix1[12];

  matrix2[4] = matrix1[1];
  matrix2[5] = matrix1[5];
  matrix2[6] = matrix1[9];
  matrix2[7] = matrix1[13];

  matrix2[8] = matrix1[2];
  matrix2[9] = matrix1[6];
  matrix2[10] = matrix1[10];
  matrix2[11] = matrix1[14];

  matrix2[12] = matrix1[3];
  matrix2[13] = matrix1[7];
  matrix2[14] = matrix1[14];
  matrix2[15] = matrix1[15];
}

int
matrixInverse(GLfloat matrix2[16], GLfloat matrix1[16])
{
   int i,j;
  GLfloat tmp[12]; // temp array for pairs
  GLfloat src[16]; // array of transpose source matrix
  GLfloat dst[16];
  GLfloat det;     // determinant
  
  // transpose matrix
  for (i = 0; i < 4; i++) {
    src[i]      = matrix1[i*4];
    src[i + 4]  = matrix1[i*4 + 1];
    src[i + 8]  = matrix1[i*4 + 2];
    src[i + 12] = matrix1[i*4 + 3];
  }
  // calculate pairs for first 8 elements (cofactors)
  tmp[0]  = src[10] * src[15];
  tmp[1]  = src[11] * src[14];
  tmp[2]  = src[9]  * src[15];
  tmp[3]  = src[11] * src[13];
  tmp[4]  = src[9]  * src[14];
  tmp[5]  = src[10] * src[13];
  tmp[6]  = src[8]  * src[15];
  tmp[7]  = src[11] * src[12];
  tmp[8]  = src[8]  * src[14];
  tmp[9]  = src[10] * src[12];
  tmp[10] = src[8]  * src[13];
  tmp[11] = src[9]  * src[12];
  // calculate first 8 elements (cofactors)
  dst[0] =  tmp[0]*src[5] + tmp[3]*src[6] + tmp[4]*src[7];
  dst[0] -= tmp[1]*src[5] + tmp[2]*src[6] + tmp[5]*src[7];
  dst[1] =  tmp[1]*src[4] + tmp[6]*src[6] + tmp[9]*src[7];
  dst[1] -= tmp[0]*src[4] + tmp[7]*src[6] + tmp[8]*src[7];
  dst[2] =  tmp[2]*src[4] + tmp[7]*src[5] + tmp[10]*src[7];
  dst[2] -= tmp[3]*src[4] + tmp[6]*src[5] + tmp[11]*src[7];
  dst[3] =  tmp[5]*src[4] + tmp[8]*src[5] + tmp[11]*src[6];
  dst[3] -= tmp[4]*src[4] + tmp[9]*src[5] + tmp[10]*src[6];
  dst[4] =  tmp[1]*src[1] + tmp[2]*src[2] + tmp[5]*src[3];
  dst[4] -= tmp[0]*src[1] + tmp[3]*src[2] + tmp[4]*src[3];
  dst[5] =  tmp[0]*src[0] + tmp[7]*src[2] + tmp[8]*src[3];
  dst[5] -= tmp[1]*src[0] + tmp[6]*src[2] + tmp[9]*src[3];
  dst[6] =  tmp[3]*src[0] + tmp[6]*src[1] + tmp[11]*src[3];
  dst[6] -= tmp[2]*src[0] + tmp[7]*src[1] + tmp[10]*src[3];
  dst[7] =  tmp[4]*src[0] + tmp[9]*src[1] + tmp[10]*src[2];
  dst[7] -= tmp[5]*src[0] + tmp[8]*src[1] + tmp[11]*src[2];
  // calculate pairs for second 8 elements (cofactors)
  tmp[0] = src[2]*src[7];
  tmp[1] = src[3]*src[6];
  tmp[2] = src[1]*src[7];
  tmp[3] = src[3]*src[5];
  tmp[4] = src[1]*src[6];
  tmp[5] = src[2]*src[5];
  tmp[6] = src[0]*src[7];
  tmp[7] = src[3]*src[4];
  tmp[8] = src[0]*src[6];
  tmp[9] = src[2]*src[4];
  tmp[10] = src[0]*src[5];
  tmp[11] = src[1]*src[4];
  // calculate second 8 elements (cofactors)
  dst[8] = tmp[0]*src[13] + tmp[3]*src[14] + tmp[4]*src[15];
  dst[8] -= tmp[1]*src[13] + tmp[2]*src[14] + tmp[5]*src[15];
  dst[9] = tmp[1]*src[12] + tmp[6]*src[14] + tmp[9]*src[15];
  dst[9] -= tmp[0]*src[12] + tmp[7]*src[14] + tmp[8]*src[15];
  dst[10] = tmp[2]*src[12] + tmp[7]*src[13] + tmp[10]*src[15];
  dst[10]-= tmp[3]*src[12] + tmp[6]*src[13] + tmp[11]*src[15];
  dst[11] = tmp[5]*src[12] + tmp[8]*src[13] + tmp[11]*src[14];
  dst[11]-= tmp[4]*src[12] + tmp[9]*src[13] + tmp[10]*src[14];
  dst[12] = tmp[2]*src[10] + tmp[5]*src[11] + tmp[1]*src[9];
  dst[12]-= tmp[4]*src[11] + tmp[0]*src[9] + tmp[3]*src[10];
  dst[13] = tmp[8]*src[11] + tmp[0]*src[8] + tmp[7]*src[10];
  dst[13]-= tmp[6]*src[10] + tmp[9]*src[11] + tmp[1]*src[8];
  dst[14] = tmp[6]*src[9] + tmp[11]*src[11] + tmp[3]*src[8];
  dst[14]-= tmp[10]*src[11] + tmp[2]*src[8] + tmp[7]*src[9];
  dst[15] = tmp[10]*src[10] + tmp[4]*src[8] + tmp[9]*src[9];
  dst[15]-= tmp[8]*src[9] + tmp[11]*src[10] + tmp[5]*src[8];
  // calculate determinant
  det=src[0]*dst[0]+src[1]*dst[1]+src[2]*dst[2]+src[3]*dst[3];
  if (det == 0.0)
  {
	  return 0;
  }
  // calculate matrix inverse
  det = 1/det;
  for (j = 0; j < 16; j++)
    matrix2[j] = dst[j] * det;

  return 1;
}


void
Projector::drawTextureProjection(void)
{
	glPushAttrib (GL_ALL_ATTRIB_BITS);

	glPushMatrix ();

	glTranslatef (m_eye[0], m_eye[1], m_eye[2]);
	glRotatef (-m_angle1, 0.0, 1.0, 0.0);
	glRotatef (-m_angle2, 1.0, 0.0, 0.0);
	glTranslatef (-m_eye[0], -m_eye[1], -m_eye[2]);

	glDisable(GL_LIGHTING);

	glLineWidth(2.0);
	glColor3f(0.0, 0.0, 1.0);
	glBegin (GL_LINES);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]-ffar);
	glEnd ();

	// small cone
	glBegin (GL_LINES);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]-xmin, m_eye[1]+ymax, m_eye[2]-nnear);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]+xmin, m_eye[1]+ymax, m_eye[2]-nnear);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]+xmin, m_eye[1]-ymax, m_eye[2]-nnear);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]-xmin, m_eye[1]-ymax, m_eye[2]-nnear);
	glEnd ();

	// big cone
	GLfloat tt = ffar / nnear;
	glBegin (GL_LINES);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]-tt*xmin, m_eye[1]+tt*ymax, m_eye[2]-ffar);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]+tt*xmin, m_eye[1]+tt*ymax, m_eye[2]-ffar);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]+tt*xmin, m_eye[1]-tt*ymax, m_eye[2]-ffar);
	glVertex3f (m_eye[0], m_eye[1], m_eye[2]);
	glVertex3f (m_eye[0]-tt*xmin, m_eye[1]-tt*ymax, m_eye[2]-ffar);
	glEnd ();

	// small quad
	glBegin (GL_LINE_LOOP);
	glVertex3f (m_eye[0]-xmin, m_eye[1]+ymax, m_eye[2]-nnear);
	glVertex3f (m_eye[0]+xmin, m_eye[1]+ymax, m_eye[2]-nnear);
	glVertex3f (m_eye[0]+xmin, m_eye[1]-ymax, m_eye[2]-nnear);
	glVertex3f (m_eye[0]-xmin, m_eye[1]-ymax, m_eye[2]-nnear);
	glEnd ();

	// big quad
	glBegin (GL_LINE_LOOP);
	glVertex3f (m_eye[0]-tt*xmin, m_eye[1]+tt*ymax, m_eye[2]-ffar);
	glVertex3f (m_eye[0]+tt*xmin, m_eye[1]+tt*ymax, m_eye[2]-ffar);
	glVertex3f (m_eye[0]+tt*xmin, m_eye[1]-tt*ymax, m_eye[2]-ffar);
	glVertex3f (m_eye[0]-tt*xmin, m_eye[1]-tt*ymax, m_eye[2]-ffar);
	glEnd ();

    glEnable(GL_LIGHTING);

	glPopMatrix ();

	glPopAttrib ();
}

void
Projector::loadTextureProjection(GLfloat m[16])
{
  /* Should use true inverse, but since m consists only of rotations, we can
     just use the transpose. */
	//GLfloat mInverse[16];
	//matrixTranspose((GLfloat *) mInverse, m);
	//matrixInverse((GLfloat *) mInverse, m);

  glMatrixMode(GL_MODELVIEW);
	  GLfloat objectXform[16];
	  GLfloat objectXformInverse[16];
	  glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *) objectXform);
	  matrixInverse((GLfloat *) objectXformInverse, objectXform);


  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glTranslatef(0.5, 0.5, 0.0);
  //glScalef(0.5, 0.5, 1.0);
  //glOrtho(xmin, xmax, ymin, ymax, nnear, ffar);
  glFrustum(xmin, xmax, ymin, ymax, nnear, ffar);

  //glTranslatef(0.0, 0.0, 2.0);
//glRotatef (-angle, 1.0, 0.0, 0.0);
  //glTranslatef(-m_eye[0], -m_eye[1], -m_eye[2]);
 // glMultMatrixf((GLfloat *) mInverse);

  glMultMatrixf((GLfloat *) m);



 glMultMatrixf((GLfloat *) objectXformInverse);
 


 glMatrixMode(GL_MODELVIEW);
}

void Projector::PreDisplay (void)
{
    if (1)
	{
	  glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();

	  if (0)
	  {
		  glTranslatef (m_eye[0], m_eye[1], m_eye[2]);
		  glRotatef (m_angle1, 0.0, 1.0, 0.0);
		  glRotatef (m_angle2, 1.0, 0.0, 0.0);
		  //glRotatef (m_angle2, 1.0, 0.0, 0.0);
	  }
	  else
	  {
			glRotatef (m_angle1, 0.0, 1.0, 0.0);
			glRotatef (m_angle2, 1.0, 0.0, 0.0);
 			glTranslatef(-m_eye[0], -m_eye[1], -m_eye[2]);
			//glRotatef(m_angle2, 1.0, 0.0, 0.0);
			//glMultMatrixf((GLfloat *) m_textureXform);
	  }
	
      glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *) m_textureXform);
      glPopMatrix();
    }
    loadTextureProjection((GLfloat *) m_textureXform);

    if (m_showProjection)
	{
	  drawTextureProjection();
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glEnable(GL_TEXTURE_GEN_R);
    glEnable(GL_TEXTURE_GEN_Q);
}

void Projector::PostDisplay (void)
{
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_TEXTURE_GEN_S);
  glDisable(GL_TEXTURE_GEN_T);
  glDisable(GL_TEXTURE_GEN_R);
  glDisable(GL_TEXTURE_GEN_Q);
}

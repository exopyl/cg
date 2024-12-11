#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//#include <GL/glut.h>

#include "particle_shape.h"
#include "particles_system.h"

static float rainbow[12][3]= // Rainbow of colors
{
	{1.0f,0.5f,0.5f},{1.0f,0.75f,0.5f},{1.0f,1.0f,0.5f},{0.75f,1.0f,0.5f},
	{0.5f,1.0f,0.5f},{0.5f,1.0f,0.75f},{0.5f,1.0f,1.0f},{0.5f,0.75f,1.0f},
	{0.5f,0.5f,1.0f},{0.75f,0.5f,1.0f},{1.0f,0.5f,1.0f},{1.0f,0.5f,0.75f}
};

/****************/
/*** Particle ***/
/****************/

/**
* Constructor
*/
CParticle::CParticle ()
{
	init_config2 ();
}

/**
* Initialize a particle
*/
void
CParticle::init (void)
{
	m_x = m_y = m_z = 0.0;
	m_r = m_g = m_b = 0.0;
	m_xi = m_yi = m_zi = 0.0;
	m_xg = m_yg = m_zg = 0.0;
	m_active = 1;
	m_life = 1.0; // full life
	m_fade = 1.0;
}

/**
* Initalize with configuration 1.
*/
void
CParticle::init_config1 (void)
{
	set_active (1);
	set_life (2.0);

	set_position (rand()/10000, rand()/10000, (rand()%2)?-50.0:50.0);
	set_fade (float(rand()%100)/1000.0f+0.003f);
	
	// orientation
	float alpha = (rand()/(RAND_MAX+1.0))*2*3.14159;
	set_orientation (1000*cos(alpha),1000*sin(alpha),2000.0);

	// gravity
	set_gravity (0.0, 0.0, -20.0);
}

/**
* Initalize with configuration 2.
*/
void
CParticle::init_config2 (void)
{
	set_active (1);
	set_life (2.0);

	set_position (rand()/10000, rand()/10000, 0.0);
	set_fade (float(rand()%100)/1000.0f+0.003f);

	// orientation
	set_orientation (float((rand()%50)-26.0)*30.0,
		float((rand()%50)-26.0)*30.0,
		1000.0);

	// gravity
	set_gravity (0.0, 0.0, -7.0);
}

void
CParticle::init_config3 (void)
{
	set_active (1);
	set_life (2.0);

	set_position (0., 0., 0.);
	set_fade (float(rand()%100)/1000.0f+0.003f);

	// orientation
	set_orientation (float((rand()%50)-26.0)*30.0,
		float((rand()%50)-26.0)*30.0,
		1000.0);

	// gravity
	set_gravity (0.0, 0.0, -3.0);
}

// getters / setters
void CParticle::set_position (float par_x, float par_y, float par_z) { m_x = par_x; m_y = par_y; m_z = par_z; }
void CParticle::get_position (float *par_x, float *par_y, float *par_z) { *par_x = m_x; *par_y = m_y; *par_z = m_z; }
void CParticle::set_orientation (float par_xi, float par_yi, float par_zi) { m_xi = par_xi; m_yi = par_yi; m_zi = par_zi; }
void CParticle::get_orientation (float *par_xi, float *par_yi, float *par_zi) { *par_xi = m_xi; *par_yi = m_yi; *par_zi = m_zi; }
void CParticle::set_color (float par_r, float par_g, float par_b) { m_r = par_r; m_g = par_g; m_b = par_b; }
void CParticle::get_color (float *par_r, float *par_g, float *par_b) { *par_r = m_r; *par_g = m_g; *par_b = m_b; }
void CParticle::set_gravity (float par_xg, float par_yg, float par_zg) { m_xg = par_xg; m_yg = par_yg; m_zg = par_zg; }
void CParticle::get_gravity (float *par_xg, float *par_yg, float *par_zg) { *par_xg = m_xg; *par_yg = m_yg; *par_zg = m_zg; }
void CParticle::set_life   (float par_life)  { m_life = par_life; }
void CParticle::get_life   (float *par_life) { *par_life = m_life; }
void CParticle::set_fade   (float par_fade)  { m_fade = par_fade; }
void CParticle::get_fade   (float *par_fade) { *par_fade = m_fade; }
void CParticle::set_active (int par_active)  { m_active = par_active; }
int  CParticle::is_active  (void)         { return m_active; }

/************************/
/*** Particles system ***/
/************************/


/**
* Constructor.
* \param n : number of particles.
*/
CParticles_system::CParticles_system (int par_n)
{
	m_n_particles = par_n;
	m_particles = (CParticle**)malloc(m_n_particles*sizeof(CParticle*));
	for (int i=0; i<m_n_particles; i++)
    {
		m_particles[i] = new CParticle ();
		m_particles[i]->init_config3 ();
    }
	m_slowdown = 0.8;
	m_col = 0;
	m_delay = 0;
}

/**
* Attach a texture to the particle system.
* \param texture : identifier for the texture.
* \param type : type of the shape.
*
* \return 1 if everything is ok, 0 otherwise.
*/
/*
int
CParticles_system::attach_texture (unsigned int &par_texture, int par_type)
{
	switch (par_type)
    {
    case 0:
		{
			unsigned char *map, c;
			
			if(!(map=(unsigned char *)malloc(128*128*3)))
			{
				perror ("malloc");
				return 0;
			}
			
			int i,j,k=0;
			for (i=0; i<128; i++)
				for (j=0; j<128; j++)
				{
					float d = sqrt ((float)((64-i)*(64-i)+(64-j)*(64-j)));
					if (d>64.0) d=64.0;
					d = 64.0*sin (d*3.14159/128.0);
					c = (unsigned char) (255.0*(1.0-d/64.0));
					map[k++] = c;
					map[k++] = c;
					map[k++] = c;
				}
				
				glGenTextures (1, &par_texture);
				glBindTexture (GL_TEXTURE_2D, par_texture);
				glTexParameteri (GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
				glTexParameteri (GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
				glTexImage2D (GL_TEXTURE_2D, 0, 3, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE, map);
				
				free (map);
		}
		break;
    case 1:
		{
			int i, j, k;
			double dx,dy,alpha;
			unsigned char *map, c;
			
			if(!(map=(unsigned char *)malloc(128*128*3)))
			{
				perror ("malloc");
				return 0;
			}
#ifndef M_PI			
#define M_PI 3.14159
#endif		   
			k = 0;
			for(i=0; i<128; i++)
			{
				dx = (double) (i-64) / 64.0;
				for(j=0; j<128; j++)
				{
					dy = (double) (j-64 ) / 64.0;
					alpha = 0.9 * cos(M_PI-M_PI / (1.3+fabs( dx + dy )))
						+ 0.9 * cos(M_PI-M_PI / (1.3+fabs( dx - dy )));
					if(alpha<0.0) alpha=0.0;
					if(alpha>1.0) alpha=1.0;
					c = (unsigned char) (alpha*255.0);
					map[k++] = c;
					map[k++] = c;
					map[k++] = c;
				}
			}
			
			glGenTextures (1, &par_texture);
			glBindTexture (GL_TEXTURE_2D, par_texture);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D (GL_TEXTURE_2D, 0, 3, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE, map);
			
			free (map);
		}
		break;
		
    case 2:
		glGenTextures(1, &par_texture);
		glBindTexture(GL_TEXTURE_2D, par_texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, shape1_width, shape1_height, 0, GL_RGB, GL_UNSIGNED_BYTE, shape1);
		break;
		
    case 3:
		glGenTextures(1, &par_texture);
		glBindTexture(GL_TEXTURE_2D, par_texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, shape2_width, shape2_height, 0, GL_RGB, GL_UNSIGNED_BYTE, shape2);
		break;
		
    default:
		printf ("type unknown\n");
    }
	return 1;
}
*/

/**
* Draw the particles system.
*/
/*
void
CParticles_system::draw (void)
{
	glPushAttrib (GL_ALL_ATTRIB_BITS);
    
	glEnable(GL_BLEND);
	glDepthMask(0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDisable(GL_LIGHTING);
	
	GLfloat mat[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, mat);
	
	for (int i=0;i<m_n_particles;i++)
    {
		if (m_particles[i]->m_active)
		{
			float x, y, z;
			m_particles[i]->get_position (&x, &y, &z);
			x/=100.;
			y/=100.;
			z/=100.;
			//printf ("%f %f %f\n", x, y, z);
			
            float rightx = mat[0];
            float righty = mat[4];
            float rightz = mat[8];
			
            rightx *= 1.0;
            righty *= 1.0;
            rightz *= 1.0;
			
            float upx = mat[1];
            float upy = mat[5];
            float upz = mat[9];

            upx *= 1.0;
            upy *= 1.0;
            upz *= 1.0;
	    
			glColor4f(m_particles[i]->m_r,
				m_particles[i]->m_g,
				m_particles[i]->m_b,
				m_particles[i]->m_life);
			
			
            glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(x + (-rightx - upx),
				y + (-righty - upy),
				z + (-rightz - upz));
			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(x + (rightx - upx),
				y + (righty - upy),
				z + (rightz - upz));
			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(x + (rightx + upx),
				y + (righty + upy),
				z + (rightz + upz));
			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(x + (upx - rightx),
				y + (upy - righty),
				z + (upz - rightz));
            glEnd();
	}
    }
  glPopAttrib ();
}
*/

/**
* Apply a step on the particles.
*/
void
CParticles_system::next (void)
{
  for (int i=0;i<m_n_particles;i++)
    {
      if (m_particles[i]->m_active)
	{
	  m_particles[i]->m_x += m_particles[i]->m_xi/(m_slowdown*1000);
	  m_particles[i]->m_y += m_particles[i]->m_yi/(m_slowdown*1000);
	  m_particles[i]->m_z += m_particles[i]->m_zi/(m_slowdown*1000);
	  
	  m_particles[i]->m_xi   += m_particles[i]->m_xg;
	  m_particles[i]->m_yi   += m_particles[i]->m_yg;
	  m_particles[i]->m_zi   += m_particles[i]->m_zg;
	  m_particles[i]->m_life -= m_particles[i]->m_fade;
	  
	  if (m_particles[i]->m_life < 0.0f)
	    {
	      m_particles[i]->init_config3 ();
	      m_particles[i]->set_color (rainbow[m_col][0], rainbow[m_col][1], rainbow[m_col][2]);
	    }
	}
    }
  
  // rainbow
  m_col = rand()%12;
  //if (m_col>11) m_col=0;
  m_delay++;
}


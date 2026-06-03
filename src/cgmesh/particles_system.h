#pragma once
/*** Particle ***/
/****************/
class CParticle
{
	friend class CParticles_system;
public:
	CParticle ();
	~CParticle ();

	void init (void);
	void init_config1 (void);
	void init_config2 (void);
	void init_config3 (void);

	// getters / setters
	void set_position    (float par_x, float par_y, float par_z);
	void get_position    (float *par_x, float *par_y, float *par_z);

	void set_orientation (float par_xi, float par_yi, float par_zi);
	void get_orientation (float *par_xi, float *par_yi, float *par_zi);

	void set_color       (float par_r, float par_g, float par_b);
	void get_color       (float *par_r, float *par_g, float *par_b);

	void set_gravity     (float par_xg, float par_yg, float par_zg);
	void get_gravity     (float *par_xg, float *par_yg, float *par_zg);

	void set_life        (float par_life);
	void get_life        (float *par_life);

	void set_fade        (float par_fade);
	void get_fade        (float *par_fade);

	void set_active      (int par_active);
	int  is_active       (void);

private:
	float m_x = 0.f , m_y = 0.f, m_z = 0.f;    // position
	float m_r = 0.f, m_g = 0.f, m_b = 0.f;    // color
	float m_xi = 0.f, m_yi = 0.f, m_zi = 0.f; // orientation
	float m_xg = 0.f, m_yg = 0.f, m_zg = 0.f; // gravity
	int	m_active = 0;			// active or not
	float m_life = 0.f;			// life of the particle
	float m_fade = 0.f;			// fade speed
};


/************************/
/*** Particles system ***/
/************************/
class CParticles_system
{
public:
	CParticles_system (int par_n);
	~CParticles_system ();

	//int  attach_texture (unsigned int &par_texture, int par_type);

	//void draw (void);
	void next (void);

	int get_n_particles (void) { return m_n_particles; };
	CParticle** get_particles (void) { return m_particles; };

private:
	int m_n_particles;
	CParticle **m_particles;

	float m_slowdown; // slow down particles
	int m_col;        // current color selection
	int m_delay;      // rainbow effect delay
};

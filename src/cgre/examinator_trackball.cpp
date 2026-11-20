#include <math.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdio.h>

#include "examinator_trackball.h"

#include <cgmath/turntable.h>

// Dolly rates, expressed as the exponent applied to the eye distance.
// Right-drag over the full window height multiplies the distance by e^2 (7.4x);
// one wheel notch by e^0.1 (1.1x), matching the usual 10%-per-notch feel.
static const float kDragDollyRate  = 2.0f;
static const float kWheelDollyRate = 0.1f;

// Eye distance bounds used when no scene radius is known.
static const float kFallbackMinDistance = 0.01f;
static const float kFallbackMaxDistance = 1000.f;

// ORIENTATION PAR DEFAUT : la platine d'azimut -30 deg et d'elevation 20 deg,
// vue trois-quarts depuis le quadrant x>0 y<0, legerement en surplomb, +Z vers
// le haut de l'image. Convention des angles : turntable.h.
static const double kDefaultAzimuthDeg   = -30.0;
static const double kDefaultElevationDeg =  20.0;

static const double kDegToRad = 3.14159265358979323846 / 180.0;

Ctrackball::Ctrackball ()
{
	gl_window_width = 0;
	gl_window_height = 0;
	m_orbit.SetDistance (5.f);

	for (int i=0; i<3; i++)
		tb_lastposition[i] = 0.;

	tb_state = RELEASED;
	tb_button = 0;
	lastX = 0;
	lastY = 0;

	m_zNear = .01f;
	m_zFar  = 10.f;
	m_depthCenter[0] = m_depthCenter[1] = m_depthCenter[2] = 0.f;
	m_depthRadius   = 0.f;
	m_framingRadius = 0.f;

	m_rotationMode    = RotationMode::Trackball;
	m_rotationSlowdown = 1.f;
	m_panSlowdown      = 1.f;
	m_pressX = m_pressY = 0;
	m_lastX  = m_lastY  = 0;
	m_anchorValid     = false;
	m_anchorX = m_anchorY = 0;
	m_anchorAzimuth   = 0.0;
	m_anchorElevation = 0.0;
	m_anchorPivot.Set (0.f, 0.f, 0.f);
}

void
Ctrackball::tbPointToVector(int x, int y, float v[3])
{
  // Map the cursor to a point on the virtual trackball (Bell/Shoemake): a
  // sphere near the centre, smoothly continued by a hyperbolic sheet toward the
  // edges. Both the value AND its first derivative are continuous at the
  // sphere/hyperbola seam, so the rotation speed stays uniform across the whole
  // window.
  //
  // Both axes are normalised by the *same* dimension, the smaller one, so the
  // unit disc is inscribed in the window and stays circular: a drag of N pixels
  // yields the same rotation whatever its direction. The domain therefore
  // exceeds +-1 along the long axis, which the hyperbolic sheet handles.
  const double side = (gl_window_width < gl_window_height)? gl_window_width : gl_window_height;
  v[0] = (2.0 * x - gl_window_width)  / side;
  v[1] = (gl_window_height - 2.0 * y) / side;

  const double r  = 1.0;             // trackball radius (in the normalised plane)
  const double d2 = v[0] * v[0] + v[1] * v[1];

  if (d2 <= r * r * 0.5)             // inside the sphere: project onto it
    v[2] = sqrt(r * r - d2);
  else                              // outside: fall onto the hyperbolic sheet
    v[2] = (r * r * 0.5) / sqrt(d2);

  const double a = 1.0 / sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] *= a;
  v[1] *= a;
  v[2] *= a;
}

// mouse callbacks
void
Ctrackball::mouse_press (int button, int state, int x, int y)
{
  if (state == PRESSED)
    {
      switch (button)
		{
			case LEFT_BUTTON:
				tb_state  = PRESSED;
				tb_button = LEFT_BUTTON;
				// Without valid dimensions tbPointToVector would divide by zero
				// and seed tb_lastposition with NaN, which the accumulated
				// rotation then keeps forever.
				if (has_valid_dimensions ())
					tbPointToVector(x, y, tb_lastposition);
				m_pressX = x; m_pressY = y;
				m_lastX  = x; m_lastY  = y;
				m_anchorValid = false;
				m_rotationSlowdown = 1.f;
				break;

			case RIGHT_BUTTON:
				tb_state  = PRESSED;
				tb_button = RIGHT_BUTTON;
				lastX = x; lastY = y;
				break;

			case MIDDLE_BUTTON:
				tb_state  = PRESSED;
				tb_button = MIDDLE_BUTTON;
				lastX = x; lastY = y;
				m_panSlowdown = 1.f;
				break;
		}
    }
  else
    {
      tb_state = RELEASED;
      m_anchorValid = false;
    }
}

void
Ctrackball::mouse_move (int x, int y)
{
  GLfloat current_position[3], dx, dy, dz;
  if (!has_valid_dimensions ())
    return;
  if (tb_state == PRESSED)
    {
      switch (tb_button)
	{
	case  LEFT_BUTTON:
	  m_lastX = x; m_lastY = y;
	  if (m_rotationMode == RotationMode::Turntable)
	    {
	      turntable_move (x, y);
	      break;
	    }
	  tbPointToVector(x, y, current_position);

	  // The rotation angle is the geometric angle between the two trackball
	  // vectors. Both are unit vectors, so their chord c gives the angle as
	  // 2*asin(c/2) radians.
	  dx = (current_position[0] - tb_lastposition[0]);
	  dy = (current_position[1] - tb_lastposition[1]);
	  dz = (current_position[2] - tb_lastposition[2]);
	  {
	    double half_chord = 0.5 * sqrt(dx * dx + dy * dy + dz * dz);
	    if (half_chord > 1.0)   // rounding can push the chord past 2
	      half_chord = 1.0;
	    const double angleRad = 2.0 * asin (half_chord);

	    // axis of rotation (cross product), in screen space
	    const GLfloat axis[3] = {
	      tb_lastposition[1] * current_position[2] -
	        tb_lastposition[2] * current_position[1],
	      tb_lastposition[2] * current_position[0] -
	        tb_lastposition[0] * current_position[2],
	      tb_lastposition[0] * current_position[1] -
	        tb_lastposition[1] * current_position[0]
	    };

	    // L'accumulation se fait en Rodrigues sur CPU. Elle garde contre l'axe
	    // degenere et reorthonormalise la rotation a chaque pas, ce que
	    // l'aller-retour par la pile de matrices GL ne faisait ni l'un ni
	    // l'autre -- et elle s'execute sans contexte GL, donc elle se teste.
	    m_orbit.Orbit (axis, (float)(m_rotationSlowdown * angleRad));
	  }

	  // reset for next time
	  tb_lastposition[0] = current_position[0];
	  tb_lastposition[1] = current_position[1];
	  tb_lastposition[2] = current_position[2];
	  break;

	case MIDDLE_BUTTON:
	  pan_screen (m_panSlowdown * (float)(x - lastX), m_panSlowdown * (float)(y - lastY));
	  lastX = x; lastY = y;
	  break;

	case RIGHT_BUTTON:
	  // Dolly is multiplicative: the same drag always divides the eye distance
	  // by the same factor, so the step stays usable at any scale. Dragging
	  // down moves the eye closer.
	  set_distance (m_orbit.GetDistance () * expf (-kDragDollyRate * (float)(y - lastY) / (float)gl_window_height));
	  lastX = x; lastY = y;
	  break;
	}
    }
}

void
Ctrackball::get_clip_planes (float *zNear, float *zFar) const
{
	if (!zNear || !zFar)
		return;

	// Valeurs de repli tant qu'aucune sphere de profondeur n'est publiee.
	*zNear = m_zNear;
	*zFar  = m_zFar;

	// La sphere connue, les plans l'encadrent depuis l'OEIL : ||e - s||, et non
	// la distance d'oeil. Les deux ne coincident que si le pivot est au centre
	// de la scene, ce qui n'est plus vrai des que le pivot suit un modele
	// excentre.
	if (m_depthRadius > 0.f)
		m_orbit.ClipPlanes (m_depthCenter, m_depthRadius, zNear, zFar);
}

float
Ctrackball::aspect_ratio () const
{
	// A zero height would turn the whole projection matrix into NaN, and the
	// canvas does report 0 while minimised.
	const float safeWidth  = (gl_window_width  > 0)? (float)gl_window_width  : 1.f;
	const float safeHeight = (gl_window_height > 0)? (float)gl_window_height : 1.f;
	return safeWidth / safeHeight;
}

void
Ctrackball::get_projection_matrix (float m[16]) const
{
	const float aspectRatio = aspect_ratio ();
	float zNear, zFar;
	get_clip_planes (&zNear, &zFar);

	// Le demi-champ vient de la camera, en radians. Le convertir en degres pour
	// le reconvertir ici ajouterait un arrondi que get_pick_ray, qui lit la
	// meme grandeur, ne ferait pas : les deux operations cesseraient d'etre
	// inverses l'une de l'autre.
	float ymax = zNear * tanf (0.5f * m_orbit.GetFovY ());
	// ymin = -ymax;
	// xmin = -ymax * aspectRatio;
	float xmax = ymax * aspectRatio;

	// frustrum
	//glhFrustumf2(m, -xmax, xmax, -ymax, ymax, zNear, zFar);
	float left = -xmax;
	float right = xmax;
	float bottom = -ymax;
	float top = ymax;
	float temp, temp2, temp3, temp4;
	temp = 2.f * zNear;
	temp2 = right - left;
	temp3 = top - bottom;
	temp4 = zFar - zNear;
	m[0] = temp / temp2;
	m[1] = 0.f;
	m[2] = 0.f;
	m[3] = 0.f;
	m[4] = 0.f;
	m[5] = temp / temp3;
	m[6] = 0.f;
	m[7] = 0.f;
	m[8] = (right + left) / temp2;
	m[9] = (top + bottom) / temp3;
	m[10] = (-zFar - zNear) / temp4;
	m[11] = -1.f;
	m[12] = 0.f;
	m[13] = 0.f;
	m[14] = (-temp * zFar) / temp4;
	m[15] = 0.f;
}

void
Ctrackball::get_modelview_matrix (float m[16]) const
{
	// La matrice de vue seule : V = T(0,0,-d).R.T(-c). Le panoramique n'ajoute
	// plus rien ici, il est entre dans le pivot -- c'est ce qui fait que la
	// rotation qui suit tourne bien autour de ce que l'utilisateur vient
	// d'amener au centre.
	const TMatrix4<float> view = m_orbit.GetViewMatrix ();
	for (int col=0; col<4; col++)
		for (int row=0; row<4; row++)
			m[4*col+row] = view.at (row, col);
}

void
Ctrackball::get_pick_ray (float ndcX, float ndcY,
                          float origin[3], float direction[3]) const
{
	m_orbit.RayFromNdc (ndcX, ndcY, aspect_ratio (), origin, direction);
}

void Ctrackball::set_camera (void)
{
	float projection[16];
	get_projection_matrix (projection);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMultMatrixf((GLfloat*)projection);

	float modelview[16];
	get_modelview_matrix (modelview);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMultMatrixf((GLfloat*)modelview);
}

// Bounding the eye distance keeps the eye from reaching the pivot or crossing
// it, which would show the model mirrored from the far side.
//
// Les bornes se calibrent sur le rayon de CADRAGE -- l'echelle du sujet
// observe -- et non sur celui de profondeur. Sur celui de profondeur, un decor
// lointain ou un tapis de 450 mm imposerait son echelle au zoom : un cube de
// 0,1 unite pose a 500 mm demande d = 0,27 et se verrait refuser tout en deca
// de 5,0, soit 2,4 % de la hauteur d'image au lieu de 54,5 %.
float
Ctrackball::get_min_distance () const
{
  return (m_framingRadius > 0.f)? m_framingRadius * 0.01f : kFallbackMinDistance;
}

float
Ctrackball::get_max_distance () const
{
  return (m_framingRadius > 0.f)? m_framingRadius * 100.f : kFallbackMaxDistance;
}

void
Ctrackball::set_distance (float distance)
{
  const float minDistance = get_min_distance ();
  const float maxDistance = get_max_distance ();

  if (!(distance > minDistance))   // also catches NaN
    distance = minDistance;
  else if (distance > maxDistance)
    distance = maxDistance;

  m_orbit.SetDistance (distance);
}

void
Ctrackball::zoom_step (float steps)
{
  // A positive step moves the eye closer, like a wheel rolled forward.
  set_distance (m_orbit.GetDistance () * expf (-kWheelDollyRate * steps));
}

namespace
{

// Distance d'entree du rayon o + t.dir dans la boite [mn, mx], pour une
// origine HORS de la boite (test des dalles, en double). false si le rayon la
// manque ou si elle est derriere l'origine.
bool RayEntryIntoBox (const float o[3], const float dir[3],
                      const float mn[3], const float mx[3], double& tEntry)
{
	double t0 = 0.0;
	double t1 = std::numeric_limits<double>::infinity ();
	for (int a = 0; a < 3; ++a)
	{
		if (dir[a] == 0.f)
		{
			if (o[a] < mn[a] || o[a] > mx[a])
				return false;
			continue;
		}
		double ta = ((double)mn[a] - (double)o[a]) / (double)dir[a];
		double tb = ((double)mx[a] - (double)o[a]) / (double)dir[a];
		if (ta > tb)
		{
			const double swap = ta; ta = tb; tb = swap;
		}
		if (ta > t0) t0 = ta;
		if (tb < t1) t1 = tb;
		if (t0 > t1)
			return false;
	}
	tEntry = t0;
	return true;
}

bool BoxContains (const float mn[3], const float mx[3], const float p[3])
{
	for (int a = 0; a < 3; ++a)
		if (!(p[a] >= mn[a] && p[a] <= mx[a]))
			return false;
	return true;
}

} // namespace

float
Ctrackball::SlowdownFactor (float t, float d)
{
	// Les negations attrapent aussi NaN ; t = +inf echoue a ratio < 1.
	if (!(t >= 0.f) || !(d > 0.f) || !std::isfinite (d))
		return 1.f;
	const float ratio = t / d;
	if (!(ratio < 1.f))
		return 1.f;
	return (ratio < kSlowMin)? kSlowMin : ratio;
}

Ctrackball::RotationSlowdown
Ctrackball::evaluate_rotation_slowdown (const SlowdownScene& scene) const
{
	RotationSlowdown out;
	out.d = m_orbit.GetDistance ();

	if (scene.surfaceHit)
	{
		out.source = SlowdownSource::Surface;
		out.t      = scene.surfaceDepth;
		out.s      = SlowdownFactor (out.t, out.d);
		return out;
	}

	if (scene.hasBox)
	{
		float origin[3], direction[3];
		get_pick_ray (0.f, 0.f, origin, direction);
		double tEntry = 0.0;
		if (!BoxContains (scene.boxMin, scene.boxMax, origin)
		    && RayEntryIntoBox (origin, direction, scene.boxMin, scene.boxMax, tEntry))
		{
			out.source = SlowdownSource::SceneBox;
			out.t      = (float)tEntry;
			out.s      = SlowdownFactor (out.t, out.d);
			return out;
		}
	}

	out.source = SlowdownSource::None;
	out.t      = 0.f;
	out.s      = 1.f;
	return out;
}

Ctrackball::SlowdownSceneChoice
Ctrackball::ChooseSlowdownScene (const std::vector<SlowdownImpact>& impacts,
                                 bool hasBox, const float boxMin[3], const float boxMax[3],
                                 const SlowdownClip& clip)
{
	SlowdownSceneChoice out;

	for (size_t i = 0; i < impacts.size (); ++i)
	{
		const SlowdownImpact& impact = impacts[i];
		if (!impact.surface)
			continue;
		if (clip.active && impact.z > clip.z)
		{
			out.clippedHit = true;
			continue;
		}
		if (out.impact < 0 || impact.t < out.scene.surfaceDepth)
		{
			out.impact             = (int)i;
			out.scene.surfaceHit   = true;
			out.scene.surfaceDepth = impact.t;
		}
	}

	if (hasBox)
	{
		for (int a = 0; a < 3; ++a)
		{
			out.scene.boxMin[a] = boxMin[a];
			out.scene.boxMax[a] = boxMax[a];
		}
		if (clip.active && clip.z < out.scene.boxMax[2])
			out.scene.boxMax[2] = clip.z;
		out.scene.hasBox = !(out.scene.boxMin[2] > out.scene.boxMax[2]);
	}
	return out;
}

Ctrackball::RotationSlowdown
Ctrackball::begin_left_rotation (const SlowdownScene& scene)
{
	const RotationSlowdown out = evaluate_rotation_slowdown (scene);
	m_rotationSlowdown = out.s;
	return out;
}

Ctrackball::RotationSlowdown
Ctrackball::begin_pan (const SlowdownScene& scene)
{
	const RotationSlowdown out = evaluate_rotation_slowdown (scene);
	m_panSlowdown = out.s;
	return out;
}

void
Ctrackball::set_zoom (float _zoom)
{
  set_distance (fabsf (_zoom));
}

void
Ctrackball::pan_screen (float dx, float dy)
{
  if (!has_valid_dimensions ())
    return;
  m_orbit.PanScreen (dx, dy, gl_window_height);
}

void Ctrackball::ResetTransformations()
{
    const TMatrix4<float> r = RotationFromTurntable (kDefaultAzimuthDeg * kDegToRad,
                                                     kDefaultElevationDeg * kDegToRad);
    m_orbit.SetRotation (&r.at (0, 0));
    rebase_left_gesture ();
}

void
Ctrackball::set_pivot (float x, float y, float z)
{
	m_orbit.SetPivot (x, y, z);
	rebase_left_gesture ();
}

const char*
Ctrackball::RotationModeName (RotationMode mode)
{
	return (mode == RotationMode::Turntable) ? "turntable" : "trackball";
}

bool
Ctrackball::ParseRotationMode (const char* name, RotationMode& mode)
{
	if (!name)
		return false;
	if (std::strcmp (name, "turntable") == 0)
	{
		mode = RotationMode::Turntable;
		return true;
	}
	if (std::strcmp (name, "trackball") == 0)
	{
		mode = RotationMode::Trackball;
		return true;
	}
	return false;
}

void
Ctrackball::set_rotation_mode (RotationMode mode)
{
	if (mode == m_rotationMode)
		return;
	m_rotationMode = mode;
	if (mode == RotationMode::Turntable)
		straighten_horizon ();
	rebase_left_gesture ();
}

void
Ctrackball::straighten_horizon ()
{
	const TurntableAngles angles = TurntableFromRotation (m_orbit.GetRotation ());
	const TMatrix4<float> r = RotationFromTurntable (angles.azimuth, angles.elevation);
	m_orbit.SetRotation (&r.at (0, 0));
	rebase_left_gesture ();
}

void
Ctrackball::rebase_left_gesture ()
{
	if (tb_state != PRESSED || tb_button != LEFT_BUTTON)
		return;
	m_pressX = m_lastX;
	m_pressY = m_lastY;
	m_anchorValid = false;
	if (has_valid_dimensions ())
		tbPointToVector (m_lastX, m_lastY, tb_lastposition);
}

void
Ctrackball::turntable_move (int x, int y)
{
	if (!m_anchorValid)
	{
		m_anchorX         = m_pressX;
		m_anchorY         = m_pressY;
		m_anchorRotation  = m_orbit.GetRotation ();
		m_anchorPivot     = m_orbit.GetPivot ();
		const TurntableAngles angles = TurntableFromRotation (m_anchorRotation);
		m_anchorAzimuth   = angles.azimuth;
		m_anchorElevation = angles.elevation;
		m_anchorValid     = true;
	}

	// Deplacement nul : l'etat d'ancrage est restitue TEL QUEL, roulis compris.
	// Le reconstruire par RotationFromTurntable perdrait ce roulis et les bits
	// de R. L'ancrage n'est jamais anterieur a la derniere ecriture exterieure
	// de l'orientation ou du pivot (rebase_left_gesture) : le pivot restitue est
	// celui du geste en cours.
	if (x == m_anchorX && y == m_anchorY)
	{
		m_orbit.SetRotation (&m_anchorRotation.at (0, 0));
		m_orbit.SetPivot (m_anchorPivot);
		return;
	}

	// Convention « on saisit l'objet » : vers la droite, la face avant suit le
	// curseur, donc l'oeil part a gauche et l'azimut diminue ; vers le bas (y
	// ecran croissant), le haut de l'objet vient vers soi et l'elevation croit.
	const int side = (gl_window_width < gl_window_height)? gl_window_width : gl_window_height;
	const double k = (double)m_rotationSlowdown * (double)kRotationRadiansPerSide / (double)side;
	const double azimuth = m_anchorAzimuth - k * (double)(x - m_anchorX);
	double elevation     = m_anchorElevation + k * (double)(y - m_anchorY);

	// A la borne, l'ancrage est repose sur le pixel courant : le premier pixel
	// de retour fait redescendre la vue, sans zone morte.
	const bool clamped = (elevation > kTurntableMaxElevation || elevation < -kTurntableMaxElevation);
	if (elevation > kTurntableMaxElevation)
		elevation = kTurntableMaxElevation;
	else if (elevation < -kTurntableMaxElevation)
		elevation = -kTurntableMaxElevation;

	const TMatrix4<float> r = RotationFromTurntable (azimuth, elevation);
	if (clamped)
	{
		m_anchorX         = x;
		m_anchorY         = y;
		m_anchorAzimuth   = azimuth;
		m_anchorElevation = elevation;
		m_anchorRotation  = r;
		m_anchorPivot     = m_orbit.GetPivot ();
	}
	m_orbit.SetRotation (&r.at (0, 0));
}

void
Ctrackball::set_scene_spheres (const float depthCenter[3], float depthRadius,
                               float framingRadius)
{
	if (depthCenter)
		for (int i=0; i<3; i++)
			m_depthCenter[i] = depthCenter[i];
	m_depthRadius   = depthRadius;
	m_framingRadius = framingRadius;
}

void
Ctrackball::get_depth_center (float center[3]) const
{
	if (!center)
		return;
	for (int i=0; i<3; i++)
		center[i] = m_depthCenter[i];
}

void
Ctrackball::get_matrix (GLfloat m[4][4])
{
	const TMatrix4<float>& r = m_orbit.GetRotation ();
	for (int col=0; col<4; col++)
		for (int row=0; row<4; row++)
			m[col][row] = r.at (row, col);
}

void
Ctrackball::get_matrix (GLfloat *m)
{
	const TMatrix4<float>& r = m_orbit.GetRotation ();
	for (int col=0; col<4; col++)
		for (int row=0; row<4; row++)
			m[4*col+row] = r.at (row, col);
}

void
Ctrackball::set_rotation (const float m[16])
{
	m_orbit.SetRotation (m);
	rebase_left_gesture ();
}

void Ctrackball::getCameraPosition (float *x, float *y, float *z)
{
	const TVector3<float> eye = m_orbit.GetEyePosition ();
	if (x) *x = eye.x;
	if (y) *y = eye.y;
	if (z) *z = eye.z;
}

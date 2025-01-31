#pragma once

//
// projective texture mapping
//
// References :
//
// http://www.ozone3d.net/tutorials/glsl_texturing_p08.php#part_8
// projective texture mapping cass everitt : http://developer.download.nvidia.com/assets/gamedev/docs/projective_texture_mapping.pdf
// http://www.sgi.com/products/software/opengl/examples/glut/advanced/
// http://www.sgi.com/products/software/opengl/examples/glut/advanced/source/projtex.c
//

class ProjectorsManager
{
public:
	ProjectorsManager ();
	~ProjectorsManager ();

	void init ();

private:
};

/*****************************************************************************/
//
// File: Structures_3DS.h
//
// Desc: The structures needed for loading and saving easily 3D file format
//
/*****************************************************************************/

#ifndef _STRUCTURES_3DS_HEADER_
#define _STRUCTURES_3DS_HEADER_

#include <vector>
using namespace std;

typedef bool BOOL;
#define TRUE true
#define FALSE false
typedef unsigned char BYTE;
typedef float			FLOAT32;
typedef short			INT16;
typedef unsigned short	UINT16;
typedef int				INT32;
typedef unsigned int	UINT32;

//typedef void* PVOID;

typedef struct VECTOR3_F
{
	FLOAT32 fX;
	FLOAT32 fY;
	FLOAT32 fZ;
} VECTOR3f;

typedef struct VECTOR2_F
{
	FLOAT32 fU;
	FLOAT32 fV;
} VECTOR2f;

typedef struct COLORVALUEUC
{
    BYTE r;
    BYTE g;
    BYTE b;
    BYTE a;
} COLORVALUEUC;

typedef struct MATERIALUC
{
    COLORVALUEUC   Diffuse;	// Diffuse color RGBA
    COLORVALUEUC   Ambient;	// Ambient color RGB
    COLORVALUEUC   Specular;	// Specular 'shininess'
    COLORVALUEUC   Emissive;	// Emissive color RGB
    FLOAT32				Power;		// Sharpness if specular highlight
} MATERIALUC;




// indices structure (since .3DS stores 4 unsigned shorts)
struct tIndices
{							
	UINT16 a, b, c, bVisible;	// This will hold point1, 2, and 3 index's into the vertex array plus a visible flag
};

// face structure
typedef struct _t3DSFace_
{
	INT32 vertIndex[3];			// indices for the verts that make up this triangle
	INT32 coordIndex[3];		// indices for the tex coords to texture this face
	INT32 materialID;			// The texture ID to use, which is the index into our texture array

} t3DSFace;

// Information about the chunk
struct t3DSChunk
{
	UINT16 ID;					// The chunk's ID		
	UINT32 length;				// The length of the chunk
	UINT32 bytesRead;			// The amount of bytes read within that chunk
};

//
// Information about the material
//
typedef struct _t3DSMaterialInfo_
{
	char	strName[255];		// The texture name
	char	strFile[255];		// The texture file name (If this is set it's a texture map)
	BYTE	color[3];			// The color of the object (R, G, B)
	INT32   texureId;			// the texture ID
	FLOAT32 uScale;				// u tiling of texture  (Currently not used)
	FLOAT32 vScale;				// v tiling of texture	(Currently not used)
	FLOAT32 uOffset;			// u offset of texture	(Currently not used)
	FLOAT32 vOffset;			// v offset of texture	(Currently not used)

	INT32	wTiling;			//Tile mapping value
	FLOAT32 fTexBlur;			//Text Blur Value

	MATERIALUC sMaterial;
	FLOAT32 fTransparency;
    
	FLOAT32 fTransparencyFalloffPercent;
	FLOAT32 fReflectionBlurPercent;
	FLOAT32 f2Sided;
	FLOAT32 fAddTrans;
	FLOAT32 fSelfIllum;
	BOOL	bWire;
	FLOAT32 fWireThickness;
	FLOAT32 fInTranc;
	BOOL	bSoften;

} t3DSMaterialInfo;


// Faces Material List
typedef struct _t3DSFacesMaterialList
{
	INT32  materialID;			// Material ID
	INT32  numOfFaces;			// The number of faces
	UINT16* pFacesMaterialsList;	// List of faces concerned by a material for each material used by the object
} t3DSFacesMaterialList;

//
// Information about the trimesh
//
typedef struct _t3DSObject_
{
	char	strName[255];								// The name of the object

	FLOAT32	LocalCoordinateSystem[4][3];				// Local coordinate system

	INT32	numOfVerts;									// The number of verts in the model
	INT32	numOfFaces;									// The number of faces in the model
	INT32	numTexVertex;								// The number of texture coordinates

	INT32	numOfMaterials;								// The number of materials
	vector<t3DSFacesMaterialList> pFacesMaterialList;	// the faces material list

	BOOL    bHasTexture;								// This is TRUE if there is a texture map for this object

	VECTOR3f	*pVerts;							// The object's vertices
	VECTOR3f	*pNormals;							// The object's normals
	VECTOR2f	*pTexVerts;							// The texture's UV coordinates
	t3DSFace	*pFaces;							// The faces information of the object
} t3DSObject;

//
// Information about the lights
//
typedef struct _t3DSLight_
{
	char	strName[255];		// The name of the light
	FLOAT32 position[3];

	FLOAT32 color[3];
	FLOAT32 outerRange;
	FLOAT32 innerRange;
	FLOAT32 multiplier;
	BOOL	isSpotLight;
	FLOAT32 spotLight_target[3];
	FLOAT32 spotLight_hotSpot;
	FLOAT32 spotLight_fallOff;
	
	BOOL	isShadowed;
	FLOAT32	shadowBias;
	FLOAT32	shadowFilter;
	INT16	shadowSize;
	
	FLOAT32	roll;
	FLOAT32 rayBias;
	BOOL	attenuate;

} t3DSLight;

//
// Information about the cameras
//
typedef struct _t3DCamera_
{
	char strName[255];			// The name of the camera
	FLOAT32 position[3];
	FLOAT32 target[3];
	FLOAT32 bank;				// (degrees)
	FLOAT32 lens;

} t3DSCamera;

//
// Information about the animations
//
typedef struct _t3DAnimation_
{
	INT32			objectType; // object type concerned by the animation
	INT32			nTracks;
	vector<INT32>	trackType;
	vector<void*>	track;
} t3DSAnimation;

// Information about the tracks
typedef struct _t3DSTrack_
{
	INT16 flag;
	INT32 unknown1, unknown2;
	INT32 numOfKeys;
	INT32 *keyNumber;
	INT16 *accelerationDataPresent;
} t3DSTrack;

// Header of the tracks
typedef struct _t3DSTrackHeader_
{
	INT16 flag;
	INT32 unknown1, unknown2;
	INT32 numOfKeys;
	INT32 *keyNumber;
	INT16 *accelerationDataPresent;
} t3DSTrackHeader;

// position track
typedef struct _t3DSPositionTrack_
{
	t3DSTrackHeader header;
	FLOAT32 *position;
} t3DSPositionTrack;

// rotation track
typedef struct _t3DSRotationTrack_
{
	t3DSTrackHeader header;
	FLOAT32 *angle;				// radians
	FLOAT32 *axis;
} t3DSRotationTrack;

// scale track
typedef struct _t3DSScaleTrack_
{
	t3DSTrackHeader header;
	FLOAT32 *size;
} t3DSScaleTrack;

// FOV track
typedef struct _t3DSFOVTrack_
{
	t3DSTrackHeader header;
	FLOAT32 *angle;				// degrees
} t3DSFOVTrack;

// Roll track
typedef struct _t3DSRollTrack_
{
	t3DSTrackHeader header;
	FLOAT32 *angle;				// degrees
} t3DSRollTrack;

// Color track
typedef struct _t3DSColorTrack_
{
	t3DSTrackHeader header;
	//FLOAT32 *color; // format not specified !
} t3DSColorTrack;

// Morph track
typedef struct _t3DSMorphTrack_
{
	t3DSTrackHeader header;
	char *strName[255];
} t3DSMorphTrack;

// Hotspot track
typedef struct _t3DSHotspotTrack_
{
	t3DSTrackHeader header;
	FLOAT32 *angle;				// degrees
} t3DSHotspotTrack;

// Falloff track
typedef struct _t3DSFalloffTrack_
{
	t3DSTrackHeader header;
	FLOAT32 *angle;				// degrees
} t3DSFalloffTrack;

// Hide track
typedef struct _t3DSHideTrack_
{
	t3DSTrackHeader header;
} t3DSHideTrack;

//
// Unknown chunks
//
typedef struct _t3DSUnknownChunk
{
	UINT16 chunkID;
	UINT16 fatherID;
} t3DSUnknownChunk;

//
// Information about the model
//
typedef struct _t3DSModel_ 
{
	INT32 fileVersion; // version of the file
	INT32 meshVersion; // version of the mesh

	INT32 numOfObjects;			// number of objects in the model
	INT32 numOfMaterials;		// number of materials for the model
	INT32 numOfLights;
	INT32 numOfCameras;
	INT32 numOfAnimations;
	vector<t3DSMaterialInfo>	pMaterials;		// list of material information (Textures and colors)
	vector<t3DSObject>			pObject;		// list of objects (trimesh)
	vector<t3DSLight>			pLights;		// list of lights
	vector<t3DSCamera>			pCameras;		// list of cameras
	vector<t3DSAnimation>		pAnimations;	// list of animations

	char strPathToModel[255];					// path to the model

	INT32	numOfUnknownChunks;							// The number of unknown chunks
	vector<t3DSUnknownChunk>	pUnknownChunks;			// Unknown chunks
} t3DSModel;

// dump the content of the 3DS structure
extern void Dump3DSFile (t3DSModel *pModel, char* pFilename);

#endif	// _STRUCTURES_3DS_HEADER_

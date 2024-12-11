#ifndef _FILEFORMAT_3DS_HEADER_
#define _FILEFORMAT_3DS_HEADER_

//-----------------------------------------------------------------------------
//Specific to the 3DS file format
//-------
#include "mesh_io_3ds_defines.h"
#include "mesh_io_3ds_structures.h"

	t3DSModel *Allocate3DSModel();

	/*! Load a 3DS file into our 3D Model Structure
	\param szFile - The path to the file to open
	\param ExtraParameters - Some extra parameters
	\return t3DSModel* - An OS independant structure describing the 3D model
	*/
	t3DSModel *Load3DSFile				(char *szFile, void *ExtraParameters );

	/*! Free any previously allocated 3D model
	\param pModel - The model to deallocate
	\return HRESULT - S_OK, an error code otherwise
	*/
	int Free3DSModel				(t3DSModel *pModel);

	// This reads in a string and saves it in the char array passed in
	int GetString						(char *);

	// This reads the next chunk
	void ReadChunk_3DS					(t3DSChunk *);

	// This reads the next large chunk
	void ProcessNextChunk_3DS			(t3DSModel *pModel, t3DSChunk *);

	// This reads the object chunks
	void ProcessNextObjectChunk_3DS		(t3DSModel*, char *strName, t3DSChunk*);
	void ProcessNextTriMeshChunk_3DS	(t3DSModel*, t3DSObject*,	t3DSChunk*);
	void ProcessNextLightChunk_3DS		(t3DSModel*, t3DSLight*,	t3DSChunk*);
	void ProcessNextCameraChunk_3DS		(t3DSModel*, t3DSCamera*,	t3DSChunk*);
	void ProcessNextMaterialChunk_3DS	(t3DSModel*,				t3DSChunk*);
	void ProcessNextKeyFrameChunk_3DS	(t3DSModel*,				t3DSChunk*);
	void ProcessKeyFrameChunk_3DS		(t3DSModel*,				t3DSChunk*);
	void ProcessTrackHeader				(t3DSTrackHeader*,			t3DSChunk*);

	// This reads the RGB value for the object's color
	void ReadColorChunk_3DS				(BYTE *pColor, t3DSChunk *pChunk);
	void ReadPercentageChunk_3DS		(FLOAT32 *pPercentage, t3DSChunk *pChunk);

	// This reads the coordinates of a 3D position
	void Read3DCoordinates_3DS			(FLOAT32 pPosition[3], t3DSChunk*);

	// This reads the objects vertices
	void ReadVertices_3DS				(t3DSObject *pObject, t3DSChunk*);

	// This reads the objects face information
	void ReadVertexIndices_3DS			(t3DSObject *pObject, t3DSChunk*);

	// This reads the texture coordinates of the object
	void ReadUVCoordinates_3DS			(t3DSObject *pObject, t3DSChunk*);

	// This reads in the material name assigned to the object and sets the materialID
	void ReadObjectMaterial_3DS			(t3DSModel *pModel, t3DSObject *pObject, t3DSChunk *pPreviousChunk);
	
	// This reads the local coordinate system
	void ReadLocalCoordinateSystem_3DS	(t3DSObject *pObject, t3DSChunk *pPreviousChunk);

	void StoreUnknownChunk				(t3DSModel *pModel, UINT16 fatherID, UINT16 chunkID);

	// This frees memory and closes the file
	void CleanUp_3DS();


	/*! Save a 3DS structure into a file
	@param t3DSModel* - An OS independant structure describing the 3D model
	@param szFile - The path to the file to open
	@param ExtraParameters - Some extra parameters
	@return int - S_OK, an error code otherwise
	*/
	int Write3DSFile				(t3DSModel *pModel, char *szFile, void *ExtraParameters );

	// Editor
	UINT32 WriteEditor					(t3DSModel *pModel);

	// Materials
	UINT32 WriteMaterials				(t3DSModel *pModel);
	UINT32 WriteMaterial				(t3DSMaterialInfo *pMaterial);
	UINT32 WriteMaterialName			(t3DSMaterialInfo *pMaterial);
	UINT32 WriteMaterialAmbientColor	(t3DSMaterialInfo *pMaterial);
	UINT32 WriteMaterialDiffuseColor	(t3DSMaterialInfo *pMaterial);
	UINT32 WriteMaterialSpecularColor	(t3DSMaterialInfo *pMaterial);

	// Objects
	UINT32 WriteObjects					(t3DSModel *pModel);
	UINT32 WriteObject					(t3DSModel *pModel, void *pObject, UINT16 objectType);

	UINT32 WriteLight					(t3DSLight *pLight);
	UINT32 WriteLightSpotLight			(t3DSLight *pLight);

	UINT32 WriteCamera					(t3DSCamera *pCamera);

	UINT32 WriteTrimesh					(t3DSModel *pModel, t3DSObject *pObject);
	UINT32 WriteVertices				(UINT32 nVertices, VECTOR3f *vertices);
	UINT32 WriteFaces					(t3DSModel *pModel, t3DSObject *pObject);
	UINT32 WriteFacesMaterialList		(t3DSModel *pModel, t3DSObject *pObject, int index);
	UINT32 WriteUVCoordinates			(INT32 numTexVertex, VECTOR2f* pTexVerts);

	// Animations
	UINT32 WriteAnimations				(t3DSModel *pModel);
	UINT32 WriteAnimation				(t3DSAnimation *pAnimation);

	UINT32 WriteColorRGBFLOAT32			(FLOAT32 r, FLOAT32 g, FLOAT32 b);
	UINT32 WriteColorRGB				(BYTE r, BYTE g, BYTE b);
	UINT32 WriteColorRGBA				(BYTE r, BYTE g, BYTE b, BYTE a);
	UINT32 WriteColorRGBAFLOAT32		(FLOAT32 r, FLOAT32 g, FLOAT32 b, FLOAT32 a);

	UINT32 WriteChunkHeader				(UINT16 id_header, fpos_t *position);
	void   WriteChunkFooter				(fpos_t position, UINT32 chunkSize);

#endif	//_FILEFORMAT_3DS_HEADER_

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "mesh_io_3ds.h"

bool trace_unknown_chunks = true;

// Global
int gBuffer[50000] = {0};	// This is used to read past unwanted data

// The file pointer
FILE *g_File3DSPointer = NULL;

// Error description if an error occurs during the parsing of the file
char *errorDesc = NULL;

//*****************************************************************************
// Allocate memory for a 3D model
// @return t3DSModel* - A pointer to the newly allocated model, NULL otherwise
//*****************************************************************************
t3DSModel *Allocate3DSModel()
{
	t3DSModel *pModel = new t3DSModel();
	if (pModel == NULL)
		return NULL;

	memset (pModel, 0, sizeof(t3DSModel));

return pModel;
}

//*****************************************************************************
// Free any previously allocated 3D model
// @param pModel - The model to deallocate
// @return HRESULT - S_OK, an error code otherwise
//*****************************************************************************
int Free3DSModel(t3DSModel *pModel)
{
int hr = 0;

	if (pModel == NULL)
		return 0;

	// When we are done, we need to free all the model data
	// We do this by walking through all the objects and freeing their information

	// Go through all the objects in the scene
	for(int i = 0; i < pModel->numOfObjects; i++)
	{
		// Free the faces, normals, vertices, and texture coordinates.
		delete [] pModel->pObject[i].pFaces;
		delete [] pModel->pObject[i].pNormals;
		delete [] pModel->pObject[i].pVerts;
		delete [] pModel->pObject[i].pTexVerts;
	}
	// Go through all the animations in the scene
	for(int i = 0; i < pModel->numOfAnimations; i++)
	{
		t3DSAnimation animation = pModel->pAnimations[i];
		for (int j=0; j<animation.nTracks; j++)
		{
			switch (animation.trackType[j])
			{
				case CHK3DS_B_POS_TRACK_TAG:
					{
						t3DSPositionTrack* positionTrack = (t3DSPositionTrack*)animation.track[j];
						delete positionTrack->position;
						delete positionTrack;
					}
					break;
				case CHK3DS_B_ROT_TRACK_TAG:
					{
						t3DSRotationTrack* rotationTrack = (t3DSRotationTrack*)animation.track[j];
						delete rotationTrack->angle;
						delete rotationTrack->axis;
						delete rotationTrack;
					}
					break;
				case CHK3DS_B_SCL_TRACK_TA:
					{
						t3DSScaleTrack* scaleTrack = (t3DSScaleTrack*)animation.track[j];
						delete scaleTrack->size;
						delete scaleTrack;
					}
					break;
				default:
					break;
			}
		}
	}

return hr;
}

//*****************************************************************************
// Load a 3DS file into the 3D Model Structure
// @param szFile - The path to the file to open
// @param ExtraParameters - Some extra parameters
// @return t3DSModel* - A structure that decsribe our 3D model
//*****************************************************************************
t3DSModel *Load3DSFile (char *szFile, void *ExtraParameters)
{
t3DSChunk currentChunk;

	if (szFile == NULL)
	{
		printf("no file!", szFile);
		return NULL;
	}

	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Open the 3DS file
	g_File3DSPointer = fopen(szFile, "rb");

	// Make sure we have a valid file pointer (we found the file)
	if(!g_File3DSPointer) 
	{
		printf("Unable to find the file: %s!", szFile);
		return NULL;
	}

	// Once we have the file open, we need to read the very first data chunk
	// to see if it's a 3DS file.  That way we don't read an invalid file.
	// If it is a 3DS file, then the first chunk ID will be equal to PRIMARY (some hex num)

	// Read the first chuck of the file to see if it's a 3DS file
	ReadChunk_3DS(&currentChunk);

	// Make sure this is a 3DS file
	if (currentChunk.ID != CHK3DS_4_M3DMAGIC)
	{
		printf("Unable to load PRIMARY chuck from file: %s!", szFile);
		return NULL;
	}

	// Now we actually start reading in the data.  ProcessNextChunk() is recursive

	//Create a 3D Model object
	t3DSModel *pModel = Allocate3DSModel();
	if (pModel == NULL)
		return NULL;

	// Begin loading objects, by calling this recursive function
	ProcessNextChunk_3DS(pModel, &currentChunk);


	// Clean up after everything
	CleanUp_3DS();

	//Copy the path
	strcpy(pModel->strPathToModel,szFile);

	//Don't forget to Create a new GR3D Object, fill it and return

return pModel;
}


///////////////////////////////// CLEAN UP \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*
/////
/////	This function cleans up our allocated memory and closes the file
/////
///////////////////////////////// CLEAN UP \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*

void CleanUp_3DS()
{
	if (g_File3DSPointer)
	{
		fclose(g_File3DSPointer);					// Close the current file pointer
		g_File3DSPointer = NULL;
	}
}

/*** Reading functions ***/
static void ReadRGBColorFloat (FLOAT32 color[3], t3DSChunk *currentChunk)
{
	currentChunk->bytesRead += fread(color, 1, 3*sizeof(FLOAT32), g_File3DSPointer);
}

static void ReadVector (FLOAT32 vector[3], t3DSChunk *currentChunk)
{
	currentChunk->bytesRead += fread(vector, 1, 3*sizeof(FLOAT32), g_File3DSPointer);
}

static void ReadFloat32 (FLOAT32 *res, t3DSChunk *currentChunk)
{
	currentChunk->bytesRead += fread(res, 1, sizeof(FLOAT32), g_File3DSPointer);
}

static void ReadINT16 (INT16 *res, t3DSChunk *currentChunk)
{
	currentChunk->bytesRead += fread(res, 1, sizeof(INT16), g_File3DSPointer);
}

static void ReadBYTE (BYTE *res, t3DSChunk *currentChunk)
{
	currentChunk->bytesRead += fread(res, 1, sizeof(BYTE), g_File3DSPointer);
}

static void ReadINT32 (INT32 *res, t3DSChunk *currentChunk)
{
	currentChunk->bytesRead += fread(res, 1, sizeof(INT32), g_File3DSPointer);
}

static void ReadUINT32 (UINT32 *res, t3DSChunk *currentChunk)
{
	currentChunk->bytesRead += fread(res, 1, sizeof(UINT32), g_File3DSPointer);
}


///////////////////////////////// PROCESS NEXT CHUNK\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*
/////
/////	This function reads the main sections of the .3DS file, then dives deeper with recursion
/////
///////////////////////////////// PROCESS NEXT CHUNK\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*

void ProcessNextChunk_3DS(t3DSModel *pModel, t3DSChunk *pPreviousChunk)
{
	t3DSChunk currentChunk; // The current chunk to load
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read next Chunk
		ReadChunk_3DS(&currentChunk);

		// Check the chunk ID
		switch (currentChunk.ID)
		{
			/*
				If the file was made in 3D Studio Max, this chunk has an int that 
				holds the file version.  Since there might be new additions to the 3DS file
				format in 4.0, we give a warning to that problem.
				However, if the file wasn't made by 3D Studio Max, we don't 100% what the
				version length will be so we'll simply ignore the value
			*/
			case CHK3DS_0_M3D_VERSION:	// version of the file
			{
				ReadINT32 (&pModel->fileVersion, &currentChunk);

				// If the file version is over 3, give a warning that there could be a problem
				if (pModel->fileVersion > 0x03)
				{
					printf ("This 3DS file is over version 3 so it may load incorrectly\n");
				}
			}
			break;

			// version of the mesh
			case CHK3DS_3_MESH_VERSION:
			{
				ReadINT32 (&pModel->meshVersion, &currentChunk);
			}
			break;

			// This chunk holds the version of the mesh.  It is also the head of the MATERIAL
			// and OBJECT chunks.  From here on we start reading in the material and object info.
			case CHK3DS_3_MDATA:	// This holds the version of the mesh
			{
				// Go to the next chunk, which is the object has a texture, it should be MATERIAL, then OBJECT.
				ProcessNextChunk_3DS(pModel, &currentChunk);
			}
			break;
		
			// This chunk is the header for the material info chunks
			case CHK3DS_A_MAT_ENTRY:	// This holds the material information
			{
				t3DSMaterialInfo newTexture; // This is used to add to our material list
				memset( &newTexture, 0, sizeof(t3DSMaterialInfo));

				pModel->numOfMaterials++; // Increase the number of materials
				pModel->pMaterials.push_back(newTexture); // Add an empty texture structure to our texture list
				ProcessNextMaterialChunk_3DS(pModel, &currentChunk); // Proceed to the material loading function
			}
			break;

			// This holds the name of the object being read
			case CHK3DS_4_NAMED_OBJECT:
			{
				// get the name of the object
				char strName[255];
				currentChunk.bytesRead += GetString(strName);
				ProcessNextObjectChunk_3DS(pModel, strName, &currentChunk);
			}
			break;

			// key frame information
			case CHK3DS_B_KFDATA:
			{
				//ProcessNextKeyFrameChunk_3DS (pModel, &currentChunk);

				// Read past this chunk and add the bytes read to the byte counter
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_0_MASTER_SCALE:
			{
				FLOAT32 res;
				ReadFloat32 (&res, &currentChunk);
			}
			break;

			//
			default:
			{
				if (trace_unknown_chunks)
				{
					StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
				}
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ProcessNextObjectChunk_3DS(t3DSModel *pModel, char *strName, t3DSChunk *pPreviousChunk)
{
	// The current chunk to work with
	t3DSChunk currentChunk;
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Continue to read these chunks until we read the end of this sub chunk
	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read the next chunk
		ReadChunk_3DS(&currentChunk);

		// Check which chunk we just read
		switch (currentChunk.ID)
		{
			// a new object
			case CHK3DS_4_N_TRI_OBJECT:
			{
				t3DSObject newObject;
				memset( &newObject, 0, sizeof(t3DSObject));

				// Increase the object count
				pModel->numOfObjects++;

				// Add a new tObject node to our list of objects
				pModel->pObject.push_back(newObject);

				// Initialize the object and all it's data members
				memset(&(pModel->pObject[pModel->numOfObjects - 1]), 0, sizeof(t3DSObject));

				// Store it, then add the read bytes to our byte counter.
				memcpy ((void*)pModel->pObject[pModel->numOfObjects - 1].strName, (void*)strName, strlen(strName)+1);

				// Now proceed to read in the rest of the object information
				ProcessNextTriMeshChunk_3DS(pModel, &(pModel->pObject[pModel->numOfObjects - 1]), &currentChunk);
			}
			break;

			// a new light
			case CHK3DS_4_N_DIRECT_LIGHT:
			{
				t3DSLight newLight;
				memset( &newLight, 0, sizeof(t3DSLight));

				// Increase the object count
				pModel->numOfLights++;

				// Add a new tObject node to our list of objects
				pModel->pLights.push_back(newLight);

				// Initialize the object and all it's data members
				memset(&(pModel->pLights[pModel->numOfLights - 1]), 0, sizeof(t3DSLight));

				// Store it, then add the read bytes to our byte counter.
				memcpy ((void*)pModel->pLights[pModel->numOfLights - 1].strName, (void*)strName, strlen(strName)+1);

				// Read the position of the light
				ReadVector (pModel->pLights[pModel->numOfLights - 1].position, &currentChunk);

				// Now proceed to read in the rest of the object information
				ProcessNextLightChunk_3DS(pModel, &(pModel->pLights[pModel->numOfLights - 1]), &currentChunk);
			}
			break;

			// a new camera
			case CHK3DS_4_N_CAMERA:
			{
				t3DSCamera newCamera; // This is used to add to our object list
				memset( &newCamera, 0, sizeof(t3DSCamera));

				// Increase the object count
				pModel->numOfCameras++;

				// Add a new tObject node to our list of objects
				pModel->pCameras.push_back(newCamera);

				// Initialize the object and all it's data members
				memset(&(pModel->pCameras[pModel->numOfCameras - 1]), 0, sizeof(t3DSCamera));

				// Store it, then add the read bytes to our byte counter.
				memcpy ((void*)pModel->pCameras[pModel->numOfCameras - 1].strName, (void*)strName, strlen(strName)+1);

				// Read the data
				ReadVector (pModel->pCameras[pModel->numOfCameras - 1].position, &currentChunk);
				ReadVector (pModel->pCameras[pModel->numOfCameras - 1].target, &currentChunk);
				ReadFloat32 (&pModel->pCameras[pModel->numOfCameras - 1].bank, &currentChunk);
				ReadFloat32 (&pModel->pCameras[pModel->numOfCameras - 1].lens, &currentChunk);

				// Now proceed to read in the rest of the object information
				ProcessNextCameraChunk_3DS(pModel, &(pModel->pCameras[pModel->numOfCameras - 1]), &currentChunk);
			}
			break;

			default:
			{
				if (trace_unknown_chunks)
				{
					StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
					//printf ("0x%04x 0x%04x\n", pPreviousChunk->ID, currentChunk.ID);
				}
				// Read past the ignored or unknown chunks
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ProcessNextTriMeshChunk_3DS (t3DSModel *pModel, t3DSObject *pObject, t3DSChunk *pPreviousChunk)
{
	// The current chunk to work with
	t3DSChunk currentChunk;
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Continue to read these chunks until we read the end of this sub chunk
	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read the next chunk
		ReadChunk_3DS(&currentChunk);

		// Check which chunk we just read
		switch (currentChunk.ID)
		{
			// new object
			case CHK3DS_4_N_TRI_OBJECT:
			{
				ProcessNextTriMeshChunk_3DS(pModel, pObject, &currentChunk);
			}
			break;

			// vertices
			case CHK3DS_4_POINT_ARRAY:
			{
				ReadVertices_3DS(pObject, &currentChunk);
			}
			break;

			// face information
			case CHK3DS_4_FACE_ARRAY:
			{
				ReadVertexIndices_3DS(pObject, &currentChunk);
			}
			break;

			// material name
			case CHK3DS_4_MSH_MAT_GROUP:
			{
				t3DSFacesMaterialList newFacesMaterialList;
				memset( &newFacesMaterialList, 0, sizeof(t3DSFacesMaterialList));

				// Increase the material count
				pObject->numOfMaterials++;

				// Add a new tObject node to our list of objects
				pObject->pFacesMaterialList.push_back(newFacesMaterialList);

				// Initialize the structure and all the data members
				memset(&(pObject->pFacesMaterialList[pObject->numOfMaterials - 1]), 0, sizeof(t3DSFacesMaterialList));

				ReadObjectMaterial_3DS(pModel, pObject, &currentChunk);
			}
			break;

			// UV texture coordinates for the object
			case CHK3DS_4_TEX_VERTS:
			{
				ReadUVCoordinates_3DS(pObject, &currentChunk);
			}
			break;
/*
			case CHK3DS_4_SMOOTH_GROUP:
			{
				INT32 *faceSmoothingGroup = new INT32[pObject->numOfFaces];
				currentChunk.bytesRead += fread(faceSmoothingGroup, 1, pObject->numOfFaces*sizeof(INT32), g_File3DSPointer);
				delete faceSmoothingGroup;
			}
			break;
*/
			case CHK3DS_4_MESH_MATRIX:
			{
				ReadLocalCoordinateSystem_3DS(pObject, &currentChunk);
				//currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_4_MESH_COLOR:
			{
				assert (false);
				BYTE color;
				ReadBYTE (&color, &currentChunk);
				//currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			default:
			{
				// Read past the ignored or unknown chunks
				if (trace_unknown_chunks)
				{
					StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
					//printf ("0x%04x 0x%04x\n", pPreviousChunk->ID, currentChunk.ID);
				}
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ProcessNextLightChunk_3DS (t3DSModel *pModel, t3DSLight *pLight, t3DSChunk *pPreviousChunk)
{
	// The current chunk to work with
	t3DSChunk currentChunk;
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Continue to read these chunks until we read the end of this sub chunk
	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read the next chunk
		ReadChunk_3DS(&currentChunk);

		// Check which chunk we just read
		switch (currentChunk.ID)
		{
			// Spot Light
			case CHK3DS_4_DL_SPOTLIGHT:
			{
				pLight->isSpotLight = true;
				ReadVector (pLight->spotLight_target, &currentChunk);    // target
				ReadFloat32 (&pLight->spotLight_hotSpot, &currentChunk); // hotSpot
				ReadFloat32 (&pLight->spotLight_fallOff, &currentChunk); // fallOff
			}
			break;

			// RGB color (float format)
			case CHK3DS_0_COLOR_F:
			{
				ReadRGBColorFloat (pLight->color, &currentChunk);
			}
			break;

			case CHK3DS_4_DL_ATTENUATE:
				{
					pLight->attenuate;
				}
				break;

			case CHK3DS_4_DL_SHADOWED:
				{
					pLight->isShadowed = TRUE;
				}
				break;

			case CHK3DS_4_DL_LOCAL_SHADOW2:
				{
					ReadFloat32 (&pLight->shadowBias, &currentChunk);
					ReadFloat32 (&pLight->shadowFilter, &currentChunk);
					ReadINT16	(&pLight->shadowSize, &currentChunk);
				}
				break;

			case CHK3DS_4_DL_SPOT_ROLL:
				{
					ReadFloat32 (&pLight->roll, &currentChunk);
				}
				break;

			case CHK3DS_4_DL_RAY_BIAS:
				{
					ReadFloat32 (&pLight->rayBias, &currentChunk);
				}
				break;

			//
			case CHK3DS_4_DL_INNER_RANGE:
			{
				ReadFloat32 (&pLight->innerRange, &currentChunk);
			}
			break;

			//
			case CHK3DS_4_DL_OUTER_RANGE:
				{
					ReadFloat32 (&pLight->outerRange, &currentChunk);
				}
				break;

			//
			case CHK3DS_4_DL_MULTIPLIER:
			{
				ReadFloat32 (&pLight->multiplier, &currentChunk);
			}
			break;

/*
			case CHK3DS_4_DL_SPOT_OVERSHOOT:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
*/

			default:
			{
				// Read past the ignored or unknown chunks
				if (trace_unknown_chunks)
				{
					StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
					//printf ("0x%04x 0x%04x\n", pPreviousChunk->ID, currentChunk.ID);
				}
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ProcessNextCameraChunk_3DS (t3DSModel *pModel, t3DSCamera *pCamera, t3DSChunk *pPreviousChunk)
{
	// The current chunk to work with
	t3DSChunk currentChunk;
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Continue to read these chunks until we read the end of this sub chunk
	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read the next chunk
		ReadChunk_3DS(&currentChunk);

		// Check which chunk we just read
		switch (currentChunk.ID)
		{
/*
		case CHK3DS_4_CAM_RANGES:
			{
				FLOAT32 near_range, far_range;
				ReadFloat32 (&near_range, &currentChunk);
				ReadFloat32 (&far_range, &currentChunk);
			}
			break;
*/
		default:
			{
				// Read past the ignored or unknown chunks
				if (trace_unknown_chunks)
				{
					StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
					//printf ("0x%04x 0x%04x\n", pPreviousChunk->ID, currentChunk.ID);
				}
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ProcessNextKeyFrameChunk_3DS (t3DSModel* pModel, t3DSChunk *pPreviousChunk)
{
	// The current chunk to work with
	t3DSChunk currentChunk;
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Continue to read these chunks until we read the end of this sub chunk
	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read the next chunk
		ReadChunk_3DS(&currentChunk);

		// Check which chunk we just read
		switch (currentChunk.ID)
		{
			case CHK3DS_B_AMBIENT_NODE_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_OBJECT_NODE_TAG:
			{
				// allocation
				t3DSAnimation newAnimation;
				memset( &newAnimation, 0, sizeof(t3DSAnimation));

				// insert
				pModel->numOfAnimations++; // Increase the number of animations
				pModel->pAnimations.push_back(newAnimation); // Add an empty animation structure to our animations list

				// init
				pModel->pAnimations[pModel->numOfAnimations - 1].objectType = CHK3DS_B_OBJECT_NODE_TAG;

				// parse
				ProcessKeyFrameChunk_3DS (pModel, &currentChunk);

				//currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
/*
			case CHK3DS_B_CAMERA_NODE_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_TARGET_NODE_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_LIGHT_NODE_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_L_TARGET_NODE_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_SPOTLIGHT_NODE_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
*/
			////

			case CHK3DS_B_KFSEG:
			{
				//cout << "CHK3DS_B_KFSEG (08)" << endl;
				INT32 start, end;
				ReadINT32 (&start, &currentChunk);
				ReadINT32 (&end, &currentChunk);
				//cout << "   " << start << " - " << end << endl;
				//currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

/*
			case CHK3DS_B_KFCURTIME:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_KFHDR:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_INSTANCE_NAME:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_PRESCALE:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_BOUNDBOX:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_MORPH_SMOOTH:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;




*/
			default:
			{
				// Read past the ignored or unknown chunks
				if (trace_unknown_chunks)
				{
					StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
					//printf ("0x%04x 0x%04x\n", pPreviousChunk->ID, currentChunk.ID);
				}
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ProcessKeyFrameChunk_3DS (t3DSModel* pModel, t3DSChunk *pPreviousChunk)
{
	// The current chunk to work with
	t3DSChunk currentChunk;
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Continue to read these chunks until we read the end of this sub chunk
	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read the next chunk
		ReadChunk_3DS(&currentChunk);

		// Check which chunk we just read
		switch (currentChunk.ID)
		{
			case CHK3DS_B_NODE_HDR:
			{
				char strName[255] = {0};
				INT16 flag1, flag2, hierarchyFather;

				//cout << " (10) CHK3DS_B_NODE_HDR : ";
				currentChunk.bytesRead += GetString (strName);
				//cout << strName;
				ReadINT16 (&flag1, &currentChunk);
				ReadINT16 (&flag2, &currentChunk);
				ReadINT16 (&hierarchyFather, &currentChunk);
				//cout << "    hierarchyFather : " << hierarchyFather << endl;
				
				//currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_PIVOT:
			{
				FLOAT32 pivot[3];
				ReadVector (pivot, &currentChunk);
				//cout << " (13) CHK3DS_B_PIVOT : pivot : " << pivot[0] << " , " << pivot[1] << " , " << pivot[2] << endl;
			}
			break;

			case CHK3DS_B_POS_TRACK_TAG:
			{
				// new position track
				t3DSPositionTrack *newPositionTrack = new t3DSPositionTrack;

				// process the header
				ProcessTrackHeader (&newPositionTrack->header, &currentChunk);

				// process the data
				newPositionTrack->position = new FLOAT32[3*newPositionTrack->header.numOfKeys];
				for (int i=0; i<newPositionTrack->header.numOfKeys; i++)
				{
					ReadINT32 (&newPositionTrack->header.keyNumber[i], &currentChunk);
					ReadINT16 (&newPositionTrack->header.accelerationDataPresent[i], &currentChunk);
					ReadVector (newPositionTrack->position+3*i, pPreviousChunk);
				}

				// insert
				pModel->pAnimations[pModel->numOfAnimations - 1].nTracks++;
				pModel->pAnimations[pModel->numOfAnimations - 1].trackType.push_back(CHK3DS_B_POS_TRACK_TAG);
				pModel->pAnimations[pModel->numOfAnimations - 1].track.push_back(newPositionTrack);
			}
			break;

			case CHK3DS_B_ROT_TRACK_TAG:
			{
				// new rotation track
				t3DSRotationTrack *newRotationTrack = new t3DSRotationTrack;

				// process the header
				ProcessTrackHeader (&newRotationTrack->header, &currentChunk);

				// process the data
				newRotationTrack->angle = new FLOAT32[newRotationTrack->header.numOfKeys];
				newRotationTrack->axis = new FLOAT32[3*newRotationTrack->header.numOfKeys];
				for (int i=0; i<newRotationTrack->header.numOfKeys; i++)
				{
					ReadINT32 (&newRotationTrack->header.keyNumber[i], &currentChunk);
					ReadINT16 (&newRotationTrack->header.accelerationDataPresent[i], &currentChunk);
					ReadFloat32 (&newRotationTrack->angle[i], pPreviousChunk);
					ReadVector (newRotationTrack->axis+3*i, pPreviousChunk);
				}

				// insert
				pModel->pAnimations[pModel->numOfAnimations - 1].nTracks++;
				pModel->pAnimations[pModel->numOfAnimations - 1].trackType.push_back(CHK3DS_B_ROT_TRACK_TAG);
				pModel->pAnimations[pModel->numOfAnimations - 1].track.push_back(newRotationTrack);
			}
			break;

			case CHK3DS_B_SCL_TRACK_TA:
			{
				// new scale track
				t3DSScaleTrack *newScaleTrack = new t3DSScaleTrack;

				// process the header
				ProcessTrackHeader (&newScaleTrack->header, &currentChunk);

				// process the data
				newScaleTrack->size = new FLOAT32[3*newScaleTrack->header.numOfKeys];
				for (int i=0; i<newScaleTrack->header.numOfKeys; i++)
				{
					ReadINT32 (&newScaleTrack->header.keyNumber[i], &currentChunk);
					ReadINT16 (&newScaleTrack->header.accelerationDataPresent[i], &currentChunk);
					ReadVector (newScaleTrack->size+3*i, pPreviousChunk);
				}

				// insert
				pModel->pAnimations[pModel->numOfAnimations - 1].nTracks++;
				pModel->pAnimations[pModel->numOfAnimations - 1].trackType.push_back(CHK3DS_B_SCL_TRACK_TA);
				pModel->pAnimations[pModel->numOfAnimations - 1].track.push_back(newScaleTrack);
			}
			break;

/*
			case CHK3DS_B_FOV_TRACK_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_ROLL_TRACK_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_COL_TRACK_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_MORPH_TRACK_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_HOT_TRACK_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_FALL_TRACK_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_HIDE_TRACK_TAG:
			{
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

			case CHK3DS_B_NODE_ID:
			{
				INT16 hierarchy;
				ReadINT16 (&hierarchy, &currentChunk);
				//currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
*/
			default:
			{
				// Read past the ignored or unknown chunks
				if (trace_unknown_chunks)
				{
					StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
					//printf ("0x%04x 0x%04x\n", pPreviousChunk->ID, currentChunk.ID);
				}
				currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ProcessTrackHeader (t3DSTrackHeader* pTrackHeader, t3DSChunk *pPreviousChunk)
{
	ReadINT16 (&pTrackHeader->flag, pPreviousChunk);
	ReadINT32 (&pTrackHeader->unknown1, pPreviousChunk);
	ReadINT32 (&pTrackHeader->unknown2, pPreviousChunk);
	ReadINT32 (&pTrackHeader->numOfKeys, pPreviousChunk);
	pTrackHeader->keyNumber = new INT32[pTrackHeader->numOfKeys];
	pTrackHeader->accelerationDataPresent = new INT16[pTrackHeader->numOfKeys];
}

//*****************************************************************************
//
//
//
//*****************************************************************************
void ProcessNextMaterialChunk_3DS(t3DSModel *pModel, t3DSChunk *pPreviousChunk)
{
	// The current chunk to work with
	t3DSChunk currentChunk;
	memset( &currentChunk, 0, sizeof(t3DSChunk));

	// Continue to read these chunks until we read the end of this sub chunk
	while (pPreviousChunk->bytesRead < pPreviousChunk->length)
	{
		// Read the next chunk
		ReadChunk_3DS(&currentChunk);

		// Check which chunk we just read in
		switch (currentChunk.ID)
		{
/*
		case CHK3DS_0_INT_PERCENTAGE:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;
			*/

		case CHK3DS_A_MAT_NAME:							// This chunk holds the name of the material
			// Here we read in the material name
			currentChunk.bytesRead += fread(pModel->pMaterials[pModel->numOfMaterials - 1].strName, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_AMBIENT:						// This holds the R G B color of our object
			ReadColorChunk_3DS((BYTE*)&(pModel->pMaterials[pModel->numOfMaterials - 1]).sMaterial.Ambient, &currentChunk);
			break;

		case CHK3DS_A_MAT_DIFFUSE:						// This holds the R G B color of our object
			ReadColorChunk_3DS((BYTE*)&(pModel->pMaterials[pModel->numOfMaterials - 1]).sMaterial.Diffuse, &currentChunk);
			break;

		case CHK3DS_A_MAT_SPECULAR:						// This holds the R G B color of our object
			ReadColorChunk_3DS((BYTE*)&(pModel->pMaterials[pModel->numOfMaterials - 1]).sMaterial.Specular, &currentChunk);
			break;

		case CHK3DS_A_MAT_SHININESS:	//Shininess percentage
			ReadPercentageChunk_3DS(&(pModel->pMaterials[pModel->numOfMaterials - 1]).sMaterial.Power, &currentChunk);
			break;

		case CHK3DS_A_MAT_SHIN2PCT:	//
			{
				FLOAT32 percentage;
				ReadPercentageChunk_3DS (&percentage, &currentChunk);
			}
			break;
/*
		case CHK3DS_A_MAT_SHIN3PCT:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;
*/
		case CHK3DS_A_MAT_TRANSPARENCY:	//Shininess percentage
			ReadPercentageChunk_3DS(&(pModel->pMaterials[pModel->numOfMaterials - 1]).fTransparency, &currentChunk);
			break;
/*
		case CHK3DS_A_MAT_XPFALL:	//
			{
				FLOAT32 percentage;
				ReadPercentageChunk_3DS (&percentage, &currentChunk);
			}
			break;

		case CHK3DS_A_MAT_REFBLUR:	//
			{
				FLOAT32 percentage;
				ReadPercentageChunk_3DS (&percentage, &currentChunk);
				//cout << percentage << endl;
			}
			break;

		case CHK3DS_A_MAT_SELF_ILLUM:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_TWO_SIDE:	//
			//&(pModel->pMaterials[pModel->numOfMaterials - 1]).f2Sided
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_DECAL:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_ADDITIVE:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_SELF_ILPCT:	//
			{
				ReadPercentageChunk_3DS (&(pModel->pMaterials[pModel->numOfMaterials - 1]).fSelfIllum, &currentChunk);
				//currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

		case CHK3DS_A_MAT_WIRE:	//
			(pModel->pMaterials[pModel->numOfMaterials - 1]).bWire = TRUE;
			break;

		case CHK3DS_A_MAT_SUPERSMP:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_WIRESIZE:	//
			{
				currentChunk.bytesRead += fread(&(pModel->pMaterials[pModel->numOfMaterials - 1]).fWireThickness, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			}
			break;

		case CHK3DS_A_MAT_FACEMAP:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_XPFALLIN:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_PHONGSOFT:	//
			(pModel->pMaterials[pModel->numOfMaterials - 1]).bSoften = TRUE;
			break;

		case CHK3DS_A_MAT_WIREABS:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_SHADING:	//
			{
				INT16 res;
				ReadINT16 (&res, &currentChunk);
			}
			break;
*/
		case CHK3DS_A_MAT_TEXMAP:							// This is the header for the texture info
			// Proceed to read in the material information
			ProcessNextMaterialChunk_3DS(pModel, &currentChunk);
			break;

		case CHK3DS_A_MAT_MAPNAME:						// This stores the file name of the material
		// Here we read in the material's file name
			currentChunk.bytesRead += fread(pModel->pMaterials[pModel->numOfMaterials - 1].strFile, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;
/*
		case CHK3DS_A_MAT_SPECMAP:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_OPACMAP:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_REFLMAP:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_BUMPMAP:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_USE_XPFALL:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_USE_REFBLUR:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_BUMP_PERCENT:	//
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		//New for handling texture coordinates
		case CHK3DS_A_MAT_MAP_TILING :	//intsh    map options
			currentChunk.bytesRead += fread( &pModel->pMaterials[pModel->numOfMaterials - 1].wTiling, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_TEXBLUR_OLD :
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_TEXBLUR :	//float    map filtering blur ( 7% -> 0.07 )
			currentChunk.bytesRead += fread( &pModel->pMaterials[pModel->numOfMaterials - 1].fTexBlur, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;
*/
		case CHK3DS_A_MAT_MAP_USCALE :		//float 1/U scale
			currentChunk.bytesRead += fread( &pModel->pMaterials[pModel->numOfMaterials - 1].uScale, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_VSCALE :		//float 1/V scale
			currentChunk.bytesRead += fread( &pModel->pMaterials[pModel->numOfMaterials - 1].vScale, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_UOFFSET :		//float U offset 
			currentChunk.bytesRead += fread(&pModel->pMaterials[pModel->numOfMaterials - 1].uOffset, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_VOFFSET :		//float V offset
			currentChunk.bytesRead += fread(&pModel->pMaterials[pModel->numOfMaterials - 1].vOffset, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;
/*
		case CHK3DS_A_MAT_MAP_ANG :	//float    map rotation angle
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_COL1 :	//RGB      lum or alpha tint first color (default=00 00 00)
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_COL2 :	//RGB      lum or alpha tint secnd color (default=FF FF FF)
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_RCOL :	//RGB      RGB tint R channel color (default=FF 00 00)
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_GCOL :	//RGB      RGB tint G channel color (default=00 FF 00)
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;

		case CHK3DS_A_MAT_MAP_BCOL :	//RGB      RGB tint B channel color (default=00 00 FF)
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;
*/
		default:  
			// Read past the ignored or unknown chunks
			if (trace_unknown_chunks)
			{
				StoreUnknownChunk (pModel, pPreviousChunk->ID, currentChunk.ID);
				//printf ("0x%04x 0x%04x\n", pPreviousChunk->ID, currentChunk.ID);
			}
			currentChunk.bytesRead += fread(gBuffer, 1, currentChunk.length - currentChunk.bytesRead, g_File3DSPointer);
			break;
		}

		// Add the bytes read from the last chunk to the previous chunk passed in.
		pPreviousChunk->bytesRead += currentChunk.bytesRead;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ReadChunk_3DS(t3DSChunk *pChunk)
{
	// This reads the chunk ID which is 2 bytes.
	// The chunk ID is like OBJECT or MATERIAL.  It tells what data is
	// able to be read in within the chunks section.  
	pChunk->bytesRead = fread(&pChunk->ID, 1, 2, g_File3DSPointer);

	// Then, we read the length of the chunk which is 4 bytes.
	// This is how we know how much to read in, or read past.
	pChunk->bytesRead += fread(&pChunk->length, 1, 4, g_File3DSPointer);
}

//*****************************************************************************
//
//*****************************************************************************
int GetString(char *pBuffer)
{
	int index = 0;

	// Read 1 byte of data which is the first letter of the string
	fread(pBuffer, 1, 1, g_File3DSPointer);

	// Loop until we get NULL
	while (*(pBuffer + index++) != 0) {

		// Read in a character at a time until we hit NULL.
		fread(pBuffer + index, 1, 1, g_File3DSPointer);
	}

	// Return the string length, which is how many bytes we read in (including the NULL)
	return strlen(pBuffer) + 1;
}


//*****************************************************************************
//
//*****************************************************************************
void ReadColorChunk_3DS(BYTE *pColor, t3DSChunk *pChunk)
{
	t3DSChunk tempChunk;
	memset( &tempChunk, 0, sizeof(t3DSChunk));

	// Read the color chunk info
	ReadChunk_3DS(&tempChunk);

	// Read in the R G B color (3 bytes - 0 through 255)
	tempChunk.bytesRead += fread(pColor, 1, tempChunk.length - tempChunk.bytesRead, g_File3DSPointer);

	// Add the bytes read to our chunk
	pChunk->bytesRead += tempChunk.bytesRead;
}

//*****************************************************************************
//
//*****************************************************************************
void ReadPercentageChunk_3DS(FLOAT32 *pPercentage, t3DSChunk *pChunk)
{
	t3DSChunk tempChunk;
	memset( &tempChunk, 0, sizeof(t3DSChunk));

	// Read the color chunk info
	ReadChunk_3DS(&tempChunk);

	// Read the percentage chunk
	int wTemp = 0;
	tempChunk.bytesRead += fread(&wTemp, 1, tempChunk.length - tempChunk.bytesRead, g_File3DSPointer);

	*pPercentage = wTemp;

	// Add the bytes read to our chunk
	pChunk->bytesRead += tempChunk.bytesRead;
}

//*****************************************************************************
//
//*****************************************************************************
void Read3DCoordinates_3DS (FLOAT32 pPosition[3], t3DSChunk *pChunk)
{
	pChunk->bytesRead += fread(pPosition, 3, sizeof(FLOAT32), g_File3DSPointer);
}



//*****************************************************************************
//
//*****************************************************************************
void ReadVertexIndices_3DS(t3DSObject *pObject, t3DSChunk *pPreviousChunk)
{
	unsigned short index = 0; // This is used to read in the current face index

	// Read in the number of faces that are in this object (int)
	pPreviousChunk->bytesRead += fread(&pObject->numOfFaces, 1, 2, g_File3DSPointer);

	// Alloc enough memory for the faces and initialize the structure
	pObject->pFaces = new t3DSFace [pObject->numOfFaces];
	memset(pObject->pFaces, 0, sizeof(t3DSFace) * pObject->numOfFaces);

	// Go through all of the faces in this object
	//FILE *ptr = fopen ("toto.obj", "a");
	for(int i = 0; i < pObject->numOfFaces; i++)
	{
		// Next, we read in the A then B then C index for the face, but ignore the 4th value.
		// The fourth value is a visibility flag for 3D Studio Max.
		for(int j = 0; j < 4; j++)
		{
			// Read the first vertex index for the current face 
			pPreviousChunk->bytesRead += fread(&index, 1, sizeof(index), g_File3DSPointer);

			if(j < 3)
			{
				// Store the index in our face structure.
				pObject->pFaces[i].vertIndex[j] = index;
			}
		}

		pObject->pFaces[i].materialID = -1;
		//fprintf (ptr, "f %d %d %d\n",
		//	1+pObject->pFaces[i].vertIndex[0], 1+pObject->pFaces[i].vertIndex[1], 1+pObject->pFaces[i].vertIndex[2]);
	}
	//fclose (ptr);
}

//*****************************************************************************
//
//*****************************************************************************
void ReadUVCoordinates_3DS(t3DSObject *pObject, t3DSChunk *pPreviousChunk)
{
	// In order to read in the UV indices for the object, we need to first
	// read in the amount there are, then read them in.

	// Read in the number of UV coordinates there are (int)
	pPreviousChunk->bytesRead += fread(&pObject->numTexVertex, 1, 2, g_File3DSPointer);

	// Allocate memory to hold the UV coordinates
	pObject->pTexVerts = new VECTOR2f [pObject->numTexVertex];

	// Read in the texture coodinates (an array 2 float)
	pPreviousChunk->bytesRead += fread(pObject->pTexVerts, 1, pPreviousChunk->length - pPreviousChunk->bytesRead, g_File3DSPointer);
}


//*****************************************************************************
//
//*****************************************************************************
void ReadVertices_3DS(t3DSObject *pObject, t3DSChunk *pPreviousChunk)
{
	// Like most chunks, before we read in the actual vertices, we need
	// to find out how many there are to read in.  Once we have that number
	// we then fread() them into our vertice array.

	// Read in the number of vertices (int)
	pPreviousChunk->bytesRead += fread(&(pObject->numOfVerts), 1, 2, g_File3DSPointer);

	// Allocate the memory for the verts and initialize the structure
	pObject->pVerts = new VECTOR3f [pObject->numOfVerts];
	memset(pObject->pVerts, 0, sizeof(VECTOR3f) * pObject->numOfVerts);

	// Read in the array of vertices (an array of 3 floats)
	pPreviousChunk->bytesRead += fread(pObject->pVerts, 1, pPreviousChunk->length - pPreviousChunk->bytesRead, g_File3DSPointer);

	// Now we should have all of the vertices read in.  Because 3D Studio Max
	// Models with the Z-Axis pointing up (strange and ugly I know!), we need
	// to flip the y values with the z values in our vertices.  That way it
	// will be normal, with Y pointing up.  If you prefer to work with Z pointing
	// up, then just delete this next loop.  Also, because we swap the Y and Z
	// we need to negate the Z to make it come out correctly.

	// Go through all of the vertices that we just read and swap the Y and Z values
	//FILE *ptr = fopen ("toto.obj", "w");
	for(int i = 0; i < pObject->numOfVerts; i++)
	{
		// Store off the Y value
		float fTempY = pObject->pVerts[i].fY;

		// Set the Y value to the Z value
		pObject->pVerts[i].fY = pObject->pVerts[i].fZ;

		// Set the Z value to the Y value, 
		// but negative Z because 3D Studio max does the opposite.
		pObject->pVerts[i].fZ = -fTempY;
//		pObject->pVerts[i].fZ = fTempY;
		//fprintf (ptr, "v %f %f %f\n", pObject->pVerts[i].fX, pObject->pVerts[i].fY, pObject->pVerts[i].fZ);
	}
	//fclose (ptr);
}


//*****************************************************************************
//
//*****************************************************************************
void ReadObjectMaterial_3DS(t3DSModel *pModel, t3DSObject *pObject, t3DSChunk *pPreviousChunk)
{
	char strMaterial[255] = {0};			// This is used to hold the objects material name

	// A material is either the color or the texture map of the object.
	// It can also hold other information like the brightness, shine, etc... 

	// material name that is assigned to the current object.
	pPreviousChunk->bytesRead += GetString(strMaterial);

	// Now that we have a material name, we need to go through all of the materials
	// and check the name against each material.  When we find a material in our material
	// list that matches this name we just read in, then we assign the materialID
	// of the object to that material index.

	// Go through all of the textures
	bool materialIDFound = FALSE;
	for(int i = 0; i < pModel->numOfMaterials; i++)
	{
		// If the material we just read in matches the current texture name
		if(strcmp(strMaterial, pModel->pMaterials[i].strName) == 0)
		{
			materialIDFound = TRUE;
			// Set the material ID to the current index 'i' and stop checking
			pObject->pFacesMaterialList[pObject->numOfMaterials - 1].materialID = i;

			// Now that we found the material, check if it's a texture map.
			// If the strFile has a string length of 1 and over it's a texture
			if(strlen(pModel->pMaterials[i].strFile) > 0) 
			{				
				// Set the object's flag to say it has a texture map to bind.
				pObject->bHasTexture = TRUE;
			}	
			break;
		}
		/*
		else
		{
			// Here we check first to see if there is a texture already assigned to this object
			if(pObject->bHasTexture != TRUE)
			{
				// Set the ID to -1 to show there is no material for this object
				pObject->materialID = -1;
			}
		}
		*/
	}
	if (materialIDFound = FALSE)
	{
		//problem
		printf("No material ID found");
	}


	// Read in the number of faces (short int)
	UINT16 uiNbrFaces;
	pPreviousChunk->bytesRead += fread(&uiNbrFaces, 1, 2, g_File3DSPointer);
	pObject->pFacesMaterialList[pObject->numOfMaterials - 1].numOfFaces = uiNbrFaces;
	pObject->pFacesMaterialList[pObject->numOfMaterials - 1].pFacesMaterialsList = (UINT16*)malloc(uiNbrFaces*sizeof(UINT16));

	//Loop through all the faces
	for (UINT16 j=0; j<uiNbrFaces; j++)
	{
		UINT16 uiFacesID = 0;
		pPreviousChunk->bytesRead += fread(&uiFacesID, 1, sizeof(INT16), g_File3DSPointer);
		//pObject->pFaces[uiFacesID].materialID = pObject->materialID;
		pObject->pFacesMaterialList[pObject->numOfMaterials - 1].pFacesMaterialsList[j] = uiFacesID;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void ReadLocalCoordinateSystem_3DS (t3DSObject *pObject, t3DSChunk *pPreviousChunk)
{
	FLOAT32 x1[3], x2[3], x3[3], O[3];
	ReadVector (x1, pPreviousChunk);
	ReadVector (x2, pPreviousChunk);
	ReadVector (x3, pPreviousChunk);
	ReadVector (O,  pPreviousChunk);

	pObject->LocalCoordinateSystem[0][0] = x1[0];	pObject->LocalCoordinateSystem[0][1] = x1[2];	pObject->LocalCoordinateSystem[0][2] = -x1[1];
	pObject->LocalCoordinateSystem[1][0] = x2[0];	pObject->LocalCoordinateSystem[1][1] = x2[2];	pObject->LocalCoordinateSystem[1][2] = -x2[1];
	pObject->LocalCoordinateSystem[2][0] = x3[0];	pObject->LocalCoordinateSystem[2][1] = x3[2];	pObject->LocalCoordinateSystem[2][2] = -x3[1];
	pObject->LocalCoordinateSystem[3][0] = O[0];	pObject->LocalCoordinateSystem[3][1] = O[2];	pObject->LocalCoordinateSystem[3][2] = -O[1];
}

//*****************************************************************************
//
//*****************************************************************************
void StoreUnknownChunk (t3DSModel *pModel, UINT16 fatherID, UINT16 chunkID)
{
	t3DSUnknownChunk unknownChunk;
	memset (&unknownChunk, 0, sizeof(t3DSUnknownChunk));

	// is it already present ?
	for (int i=0; i<pModel->numOfUnknownChunks;i++)
	{
		if (	pModel->pUnknownChunks[i].fatherID	==	fatherID
			&&	pModel->pUnknownChunks[i].chunkID	==	chunkID		)
		{
			return;
		}
	}

	// insert
	pModel->numOfUnknownChunks++;					// Increase the number of unknown chunks
	pModel->pUnknownChunks.push_back(unknownChunk);	// Add an empty animation structure to our animations list

	// init
	pModel->pUnknownChunks[pModel->numOfUnknownChunks - 1].chunkID	= chunkID;
	pModel->pUnknownChunks[pModel->numOfUnknownChunks - 1].fatherID	= fatherID;
}

/**
                     ____________________
                    /                   /
    ,-~~-.___.     |   SAVING PART !!! /
   / |  '     \   _|  ________________/
  (  )        0  /___/           
   \_/-, ,----'		         
      ====           //                    
     /  \-'~;    /~~~(O)
    /  __/~|   /       |    
  =(  _____| (_________|
			-Paul Hickey 
*/

UINT32 WriteChunkHeader (UINT16 id_header, fpos_t *position)
{
	UINT32 chunkSize = sizeof(UINT16) + sizeof(UINT32);

	fwrite (&id_header, 1, sizeof(UINT16), g_File3DSPointer);	// write the chunk ID for the object
	fgetpos (g_File3DSPointer, position);						// get the position
	fwrite (&chunkSize, 1, sizeof(UINT32), g_File3DSPointer);	// write the size

	return chunkSize;
}

void WriteChunkFooter (fpos_t position, UINT32 chunkSize)
{
	fsetpos (g_File3DSPointer, &position);						// update the size of the current chunk
	fwrite (&chunkSize, 1, sizeof(UINT32), g_File3DSPointer);	// write the size of the chunk
	fseek (g_File3DSPointer, 0, SEEK_END);						// go to the end of the file
}

UINT32 WriteColorRGBFLOAT32 (FLOAT32 r, FLOAT32 g, FLOAT32 b)
{
	UINT16 chunkID		= CHK3DS_0_COLOR_F;
	UINT32 chunkSize	= sizeof(UINT16) + sizeof(UINT32) + 3*sizeof(FLOAT32);

	fwrite (&chunkID, 1, sizeof(UINT16), g_File3DSPointer); // Write the chunk ID for the object
	fwrite (&chunkSize, 1, sizeof(UINT32), g_File3DSPointer); // Write the size

	fwrite (&r, 1, sizeof(FLOAT32), g_File3DSPointer);
	fwrite (&g, 1, sizeof(FLOAT32), g_File3DSPointer);
	fwrite (&b, 1, sizeof(FLOAT32), g_File3DSPointer);

	return chunkSize;
}

UINT32 WriteColorRGB (BYTE r, BYTE g, BYTE b)
{
	UINT16 chunkID		= CHK3DS_0_COLOR_24;
	UINT32 chunkSize	= sizeof(UINT16) + sizeof(UINT32) + 3*sizeof(BYTE);

	fwrite (&chunkID, 1, sizeof(UINT16), g_File3DSPointer); // Write the chunk ID for the object
	fwrite (&chunkSize, 1, sizeof(UINT32), g_File3DSPointer); // Write the size

	fwrite (&r, 1, sizeof(BYTE), g_File3DSPointer);
	fwrite (&g, 1, sizeof(BYTE), g_File3DSPointer);
	fwrite (&b, 1, sizeof(BYTE), g_File3DSPointer);

	return chunkSize;
}

UINT32 WriteColorRGBA (BYTE r, BYTE g, BYTE b, BYTE a)
{
	UINT16 chunkID		= CHK3DS_0_LIN_COLOR_24;
	UINT32 chunkSize	= sizeof(UINT16) + sizeof(UINT32) + 4*sizeof(BYTE);

	fwrite (&chunkID, 1, sizeof(UINT16), g_File3DSPointer); // Write the chunk ID for the object
	fwrite (&chunkSize, 1, sizeof(UINT32), g_File3DSPointer); // Write the size

	fwrite (&r, 1, sizeof(BYTE), g_File3DSPointer);
	fwrite (&g, 1, sizeof(BYTE), g_File3DSPointer);
	fwrite (&b, 1, sizeof(BYTE), g_File3DSPointer);
	fwrite (&a, 1, sizeof(BYTE), g_File3DSPointer);

	return chunkSize;
}

UINT32 WriteColorRGBAFLOAT32 (FLOAT32 r, FLOAT32 g, FLOAT32 b, FLOAT32 a)
{
	UINT16 chunkID		= CHK3DS_0_LIN_COLOR_F;
	UINT32 chunkSize	= sizeof(UINT16) + sizeof(UINT32) + 4*sizeof(FLOAT32);

	fwrite (&chunkID, 1, sizeof(UINT16), g_File3DSPointer);		// Write the chunk ID for the object
	fwrite (&chunkSize, 1, sizeof(UINT32), g_File3DSPointer);	// Write the size

	fwrite (&r, 1, sizeof(FLOAT32), g_File3DSPointer);
	fwrite (&g, 1, sizeof(FLOAT32), g_File3DSPointer);
	fwrite (&b, 1, sizeof(FLOAT32), g_File3DSPointer);
	fwrite (&a, 1, sizeof(FLOAT32), g_File3DSPointer);

	return chunkSize;
}

//*****************************************************************************
// Save a 3DS structure into a file
// @param t3DSModel* - An OS independant structure describing the 3D model
// @param szFile - The path to the file to open
// @param ExtraParameters - Some extra parameters
// @return int - 0, an error code otherwise
//*****************************************************************************
int Write3DSFile (t3DSModel *pModel, char *szFile, void *ExtraParameters )
{
	int hr = 0;

	// Check the file to save the 3DS structure :
	// if the given filename is not equal to NULL,
	// we consider the path stored in the 3DS structure
	if (szFile == NULL)
	{
		szFile = pModel->strPathToModel;
	}

	// Open the 3DS file
	g_File3DSPointer = fopen(szFile, "wb");

	// Make sure we have a valid file pointer (we found the file)
	char strMessage[255];
	if(!g_File3DSPointer) 
	{
		printf("Unable to find the file: %s!", szFile);
		return false;
	}

	// write the content of CHK3DS_4_M3DMAGIC
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_M3DMAGIC, &position);
	chunkSize += WriteEditor (pModel); // write the 3D editor chunk
	WriteChunkFooter (position, chunkSize);
	CleanUp_3DS(); // Clean up after everything

	return hr;
}

//*****************************************************************************
// Write the 3D editor chunk
//*****************************************************************************
UINT32 WriteEditor (t3DSModel *pModel)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_3_MDATA, &position);

	// Write the content of CHK3DS_3_MDATA
	chunkSize += WriteMaterials (pModel);	// write the materials
	chunkSize += WriteObjects (pModel);		// write the objects
	chunkSize += WriteAnimations (pModel);	// write the animations

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

//*****************************************************************************
//
// Write the materials in the file
//
//*****************************************************************************
UINT32 WriteMaterials (t3DSModel *pModel)
{
	UINT32 chunkSize = 0;

	// write the material
	std::vector<t3DSMaterialInfo>::iterator iteratorVectorMaterial;
	for(iteratorVectorMaterial = pModel->pMaterials.begin();
		iteratorVectorMaterial != pModel->pMaterials.end();
		iteratorVectorMaterial++)
	{
		t3DSMaterialInfo material = *(iteratorVectorMaterial);
		chunkSize += WriteMaterial (&material);
	}

	return chunkSize;
}

//*****************************************************************************
// Write the materials in the file
//*****************************************************************************
UINT32 WriteMaterial (t3DSMaterialInfo *pMaterial)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_A_MAT_ENTRY, &position);

	// write the material
	chunkSize += WriteMaterialName (pMaterial);
	chunkSize += WriteMaterialAmbientColor (pMaterial);
	chunkSize += WriteMaterialDiffuseColor (pMaterial);
	chunkSize += WriteMaterialSpecularColor (pMaterial);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

UINT32 WriteMaterialName (t3DSMaterialInfo *pMaterial)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_A_MAT_NAME, &position);

	// Write the content of CHK3DS_A_MAT_NAME
	int strLength = (strlen(pMaterial->strName)+1);
	fwrite (&pMaterial->strName, 1, strLength*sizeof(char), g_File3DSPointer);

	// update the chunk size
	chunkSize += strLength;

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

UINT32 WriteMaterialAmbientColor (t3DSMaterialInfo *pMaterial)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_A_MAT_AMBIENT, &position);

	// Write the content of CHK3DS_A_MAT_AMBIENT and update the chunk size
	chunkSize += WriteColorRGB (pMaterial->sMaterial.Ambient.r,
								pMaterial->sMaterial.Ambient.g,
								pMaterial->sMaterial.Ambient.b);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

UINT32 WriteMaterialDiffuseColor (t3DSMaterialInfo *pMaterial)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_A_MAT_DIFFUSE, &position);

	// Write the content of CHK3DS_A_MAT_AMBIENT and update the chunk size
	chunkSize += WriteColorRGB (pMaterial->sMaterial.Diffuse.r,
								pMaterial->sMaterial.Diffuse.g,
								pMaterial->sMaterial.Diffuse.b);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

UINT32 WriteMaterialSpecularColor (t3DSMaterialInfo *pMaterial)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_A_MAT_SPECULAR, &position);

	// Write the content of CHK3DS_A_MAT_AMBIENT and update the chunk size
	chunkSize += WriteColorRGB (pMaterial->sMaterial.Specular.r,
								pMaterial->sMaterial.Specular.g,
								pMaterial->sMaterial.Specular.b);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

//*****************************************************************************
//
// Write the objects in the file
//
//*****************************************************************************
UINT32 WriteObjects (t3DSModel *pModel)
{
	UINT32 chunkSize = 0;

	// write the lights
	std::vector<t3DSLight>::iterator iteratorVectorLight;
	for(iteratorVectorLight = pModel->pLights.begin();
		iteratorVectorLight != pModel->pLights.end();
		iteratorVectorLight++)
	{
		t3DSLight light = *(iteratorVectorLight);
		chunkSize += WriteObject (pModel, &light, CHK3DS_4_N_DIRECT_LIGHT);
	}

	// write the cameras

	// write the trimesh
	std::vector<t3DSObject>::iterator iteratorVectorObject;
	for(iteratorVectorObject = pModel->pObject.begin();
		iteratorVectorObject != pModel->pObject.end();
		iteratorVectorObject++)
	{
		t3DSObject object = *(iteratorVectorObject);
		chunkSize += WriteObject (pModel, &object, CHK3DS_4_N_TRI_OBJECT);
	}
	return chunkSize;
}

// Write the object in the file
UINT32 WriteObject (t3DSModel *pModel, void *pObject, UINT16 objectType)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_NAMED_OBJECT, &position);

	UINT32  nextChunkSize = 0;
	switch (objectType)
	{
	case CHK3DS_4_N_DIRECT_LIGHT:
		{
			t3DSLight* light = (t3DSLight*)pObject;

			// Write the content of CHK3DS_4_N_DIRECT_LIGHT
			int strLength = (strlen(light->strName)+1);
			fwrite (&light->strName, 1, strLength*sizeof(char), g_File3DSPointer);

			// update the chunk size
			chunkSize += strLength;

			// write the trimesh
			nextChunkSize = WriteLight (light);
		}
		break;
	case CHK3DS_4_N_TRI_OBJECT: // it's a trimesh
		{
			t3DSObject* trimesh = (t3DSObject*)pObject;

			// Write the content of CHK3DS_4_NAMED_OBJECT
			int strLength = (strlen(trimesh->strName)+1);
			fwrite (&trimesh->strName, 1, strLength*sizeof(char), g_File3DSPointer);

			// update the chunk size
			chunkSize += strLength;

			// write the trimesh
			nextChunkSize = WriteTrimesh (pModel, trimesh);
		}
		break;
		
	default:
		assert (0);
		break;
	}

	// update the chunk size
	chunkSize += nextChunkSize;

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

// Write the lights in the file
UINT32 WriteLight (t3DSLight *pLight)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_N_DIRECT_LIGHT, &position);

	fwrite (&pLight->position, 1, 3*sizeof(FLOAT32), g_File3DSPointer);
	chunkSize += 3*sizeof(FLOAT32);

	chunkSize += WriteColorRGBFLOAT32 (pLight->color[0], pLight->color[1], pLight->color[2]);
	if (pLight->isSpotLight) chunkSize += WriteLightSpotLight (pLight);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

UINT32 WriteLightSpotLight (t3DSLight *pLight)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_DL_SPOTLIGHT, &position);

	fwrite (&pLight->spotLight_target, 1, 3*sizeof(FLOAT32), g_File3DSPointer);
	fwrite (&pLight->spotLight_hotSpot, 1, sizeof(FLOAT32), g_File3DSPointer);
	fwrite (&pLight->spotLight_fallOff, 1, sizeof(FLOAT32), g_File3DSPointer);
	chunkSize += 5*sizeof(FLOAT32);

	// ... go on with :
	// CHK3DS_4_DL_SPOT_RECTANGULAR
	// CHK3DS_4_DL_SPOT_PROJECTOR
	// CHK3DS_4_DL_SPOT_ROLL

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

// Write the cameras in the file
UINT32 WriteCamera (t3DSCamera *pCamera)
{
	return 0;
}

// Write the trimeshes in the file
UINT32 WriteTrimesh (t3DSModel *pModel, t3DSObject *pObject)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_N_TRI_OBJECT, &position);

	// write the vertices
	if (pObject->numOfVerts)
	{
		chunkSize += WriteVertices (pObject->numOfVerts, pObject->pVerts);
	}

	// write the faces
	if (pObject->numOfFaces)
	{
		chunkSize += WriteFaces (pModel, pObject);
	}

	// write the tex coords
	if (pObject->numTexVertex)
	{
		chunkSize += WriteUVCoordinates (pObject->numTexVertex, pObject->pTexVerts);
	}


	/*
		cout << "<br>number of texture coordinates : " << pObject->numTexVertex << endl;
		cout << "<br>texture ID : " << pObject->materialID << endl;
		cout << "<br>has a texture : " << pObject->bHasTexture << endl;
		cout << "<br>Vertices : ";
		cout << "<br>Texture's UV coordinates : ";
		if (pObject->pTexVerts)
		{
			cout << "..." << endl;
			//cout << endl;
			//for (int i=0; i<object->numOfVerts; i++)
			//	cout << "<br> " << pObject->pTexVerts[i].fU << " " << pObject->pTexVerts[i].fV << endl;
		}
		else
		{
			cout << "not present" << endl;
		}
		cout << "<br>Faces : ";
		*/
	
	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

/**
* Write the vertices
*/
UINT32 WriteVertices (UINT32 nVertices, VECTOR3f* vertices)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_POINT_ARRAY, &position);

	// Write the content of CHK3DS_4_POINT_ARRAY
	fwrite (&nVertices, 1, sizeof(UINT16), g_File3DSPointer);
	chunkSize += sizeof(UINT16);

	FLOAT32 fTempY, fTempZ;
	for(unsigned int i=0; i<nVertices; i++)
	{
		fTempY = -vertices[i].fZ;
		fTempZ =  vertices[i].fY;
		fwrite (&vertices[i].fX, 1, sizeof(FLOAT32), g_File3DSPointer);
		fwrite (&fTempY, 1, sizeof(FLOAT32), g_File3DSPointer);
		fwrite (&fTempZ, 1, sizeof(FLOAT32), g_File3DSPointer);
	}
	chunkSize += 3*nVertices*sizeof(FLOAT32);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}

/**
* Write the faces
*/
UINT32 WriteFaces (t3DSModel *pModel, t3DSObject *pObject)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_FACE_ARRAY, &position);

	// Write the content of CHK3DS_4_POINT_ARRAY
	UINT32 nFaces = pObject->numOfFaces;
	t3DSFace* faces = pObject->pFaces;

	fwrite (&nFaces, 1, sizeof(UINT16), g_File3DSPointer);
	chunkSize += sizeof(UINT16);

	INT16 faceFlag = 3;
	for(unsigned int i=0; i<nFaces; i++)
	{
		t3DSFace face = faces[i];
		fwrite (&face.vertIndex[0], 1, sizeof(INT16), g_File3DSPointer);
		fwrite (&face.vertIndex[1], 1, sizeof(INT16), g_File3DSPointer);
		fwrite (&face.vertIndex[2], 1, sizeof(INT16), g_File3DSPointer);
		fwrite (&faceFlag, 1, sizeof(INT16), g_File3DSPointer);
	}
	chunkSize += 4*nFaces*sizeof(INT16);

	// write the materials used by the object
	if (pObject->numOfMaterials > 0)
	{
		for (int i=0; i<pObject->numOfMaterials; i++)
			chunkSize += WriteFacesMaterialList (pModel, pObject, i);
	}

	WriteChunkFooter (position, chunkSize);

	return chunkSize;
}

/**
* Write the faces material list
*/
UINT32 WriteFacesMaterialList (t3DSModel *pModel, t3DSObject *pObject, int index)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_MSH_MAT_GROUP, &position);

	// Write the content of CHK3DS_4_MSH_MAT_GROUP
	UINT16 materialID = pObject->pFacesMaterialList[index].materialID;
	fwrite (&pModel->pMaterials[materialID].strName, 1, (strlen(pModel->pMaterials[materialID].strName)+1)*sizeof(char), g_File3DSPointer);
	chunkSize += (strlen(pModel->pMaterials[materialID].strName)+1)*sizeof(char);

	fwrite (&pObject->pFacesMaterialList[index].numOfFaces, 1, sizeof(UINT16), g_File3DSPointer);
	chunkSize += sizeof(UINT16);

	UINT16 uiNbrFaces = pObject->pFacesMaterialList[index].numOfFaces;
	for (UINT16 i=0; i<uiNbrFaces; i++)
	{
		fwrite (&pObject->pFacesMaterialList[index].pFacesMaterialsList[i], 1, sizeof(UINT16), g_File3DSPointer);
	}
	chunkSize += uiNbrFaces*sizeof(UINT16);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}


/**
* Write the UV coordinates
*/
UINT32 WriteUVCoordinates (INT32 numTexVertex, VECTOR2f* pTexVerts)
{
	fpos_t position;
	UINT32 chunkSize = WriteChunkHeader (CHK3DS_4_TEX_VERTS, &position);

	fwrite (&numTexVertex, 1, sizeof(INT16), g_File3DSPointer);
	chunkSize += sizeof(INT16);

	fwrite (pTexVerts, 1, 2*numTexVertex*sizeof(float), g_File3DSPointer);
	chunkSize += 2*numTexVertex*sizeof(float);

	WriteChunkFooter (position, chunkSize);
	return chunkSize;
}


// Write the animations in the file
UINT32 WriteAnimations (t3DSModel *pModel)
{
	return 0;
}


/*
// *Note* 
//
// Below are some math functions for calculating vertex normals.  We want vertex normals
// because it makes the lighting look really smooth and life like.  You probably already
// have these functions in the rest of your engine, so you can delete these and call
// your own.  I wanted to add them so I could show how to calculate vertex normals.

//////////////////////////////	Math Functions  ////////////////////////////////*

// This computes the magnitude of a normal.   (magnitude = sqrt(x^2 + y^2 + z^2)
#define Mag(Normal) (sqrt(Normal.fX*Normal.fX + Normal.fY*Normal.fY + Normal.fZ*Normal.fZ))

#define Vector(vVector, vPoint1, vPoint2)\
{\
vVector.fX = vPoint1.fX - vPoint2.fX;\
vVector.fY = vPoint1.fY - vPoint2.fY;\
vVector.fZ = vPoint1.fZ - vPoint2.fZ;\
}

// This adds 2 vectors together and returns the result
#define AddVector(vResult, vVector1, vVector2)\
{\
vResult.fX = vVector2.fX + vVector1.fX;\
vResult.fY = vVector2.fY + vVector1.fY;\
vResult.fZ = vVector2.fZ + vVector1.fZ;\
}

#define DivideVectorByScaler(vResult, vVector1, Scaler)\
{\
vResult.fX = vVector1.fX / Scaler;\
vResult.fY = vVector1.fY / Scaler;\
vResult.fZ = vVector1.fZ / Scaler;\
}

// This returns the cross product between 2 vectors
#define Cross(vCross, vVector1, vVector2)\
{\
vCross.fX = ((vVector1.fY * vVector2.fZ) - (vVector1.fZ * vVector2.fY));\
vCross.fY = ((vVector1.fZ * vVector2.fX) - (vVector1.fX * vVector2.fZ));\
vCross.fZ = ((vVector1.fX * vVector2.fY) - (vVector1.fY * vVector2.fX));\
}

#define Normalize(vNormal)\
{\
double Magnitude;\
Magnitude = Mag(vNormal);\
vNormal.fX /= (float)Magnitude;\
vNormal.fY /= (float)Magnitude;\
vNormal.fZ /= (float)Magnitude;\
}
*/

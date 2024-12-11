/*****************************************************************************/
//
// File: Structures_3DS.h
//
// Desc: Load/Save 3D model from a 3DS file (3D STUDIO File Format)
//
/*****************************************************************************/

#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include <fstream>
using namespace std;

#include "mesh_io_3ds_structures.h"
#include "mesh_io_3ds_defines.h"

// common functions
static char* getHexColor (FLOAT32 red, FLOAT32 green, FLOAT32 blue)
{
	char *res = (char*)malloc(7*sizeof(char));
	sprintf (res, "#%02x%02x%02x", (int)(red*255), (int)(green*255), (int)(blue*255));
	return res;
}

static char* getHexColor (INT32 red, INT32 green, INT32 blue)
{
	char *res = (char*)malloc(7*sizeof(char));
	sprintf (res, "#%02x%02x%02x", red, green, blue);
	return res;
}

//*****************************************************************************
// Dump the structure of the 3D model into a html file
// @param pModel : pointer to the structure of the 3D model
// @param pFilename : filename where the information will be written
//*****************************************************************************
void Dump3DSFile (t3DSModel *pModel, char* pFilename)
{
	assert (pModel);
	ofstream ofs;
	ofs.open (pFilename);
	ofs << "<html>" << endl;
	ofs << "<body bgcolor=#EEEEEE>" << endl;

	// global structure of the 3D model
	ofs << "<br><b>Object</b> : " << "<i>" << pModel->strPathToModel << "</i>" << endl;
	ofs << "<br>" << endl;
	ofs << "<br>file version : " << pModel->fileVersion << endl;
	ofs << "<br>mesh version : " << pModel->meshVersion << endl;
	ofs << "<br>number of materials : " << pModel->numOfMaterials << endl;
	ofs << "<br>number of objects : " << pModel->numOfObjects << endl;
	ofs << "<br>number of lights : " << pModel->numOfLights << endl;
	ofs << "<br>number of cameras : " << pModel->numOfCameras << endl;
	ofs << "<br>number of animations : " << pModel->numOfAnimations << endl;
	ofs << "<br><br>" << endl;

	// Materials
	ofs << "<br><table border=1 bgcolor=\"FFFFFF\" width=\"100%\"><tr><td align=\"center\"><b>MATERIALS</b></td></tr></table><br>" << endl;
	int iMaterial = 1;
	std::vector<t3DSMaterialInfo>::iterator iteratorVectorMaterialInfo;
	for(iteratorVectorMaterialInfo = pModel->pMaterials.begin();
		iteratorVectorMaterialInfo != pModel->pMaterials.end();
		iteratorVectorMaterialInfo++)
	{
		t3DSMaterialInfo material = *(iteratorVectorMaterialInfo);

		ofs << "<br><b>Material : " << iMaterial << "/" << pModel->numOfMaterials << "</b>" << endl;
		iMaterial++;
		ofs << "<br>name of the texture : " << material.strName << endl;
		ofs << "<br>texture file name (for texture map) : " << material.strFile << endl;
		ofs << "<br>color of the object : " << material.color[0] << "," << material.color[1] << "," << material.color[2] << endl;
		//ofs << "<table bgcolor=\"" << getHexColor (material.color[0], material.color[1], material.color[2]) << "\"><tr><td></td></tr></table>" << endl;
		ofs << "<br>texure Id : " << material.texureId << endl;
		ofs << "<br>u/v tiling of the texture : " << material.uScale << " / " << material.vScale << endl;
		ofs << "<br>u/v offset of the texture : " << material.uOffset << " / " << material.vOffset << endl;
		ofs << "<br>tile mapping value : " << material.wTiling << endl;
		ofs << "<br>text blur value : " << material.fTexBlur << endl;
		ofs << "<br>Material : " << endl;
		ofs << "<table>" << endl;

		INT32 r, g, b,a ;

		// diffuse
		r = (INT32)material.sMaterial.Diffuse.r;
		g = (INT32)material.sMaterial.Diffuse.g;
		b = (INT32)material.sMaterial.Diffuse.b;
		a = (INT32)material.sMaterial.Diffuse.a;
		ofs << "<tr><td>" << endl;
		ofs << "Diffuse</td>" << endl;
		ofs << "<td width=\"50\" bgcolor=\"" << getHexColor (r, g, b) << "\"></td><td>(" << endl;
		ofs << r << " " << g << " " << b << " " << a << ")</td></tr>" << endl;
	
		// ambient
		r = (INT32)material.sMaterial.Ambient.r;
		g = (INT32)material.sMaterial.Ambient.g;
		b = (INT32)material.sMaterial.Ambient.b;
		a = (INT32)material.sMaterial.Ambient.a;
		ofs << "<tr><td>" << endl;
		ofs << "Ambient</td>" << endl;
		ofs << "<td width=\"50\" bgcolor=\"" << getHexColor (r, g, b) << "\"></td><td>(" << endl;
		ofs << r << " " << g << " " << b << " " << a << ")</td></tr>" << endl;

		// specular
		r = (INT32)material.sMaterial.Specular.r;
		g = (INT32)material.sMaterial.Specular.g;
		b = (INT32)material.sMaterial.Specular.b;
		a = (INT32)material.sMaterial.Specular.a;
		ofs << "<tr><td>" << endl;
		ofs << "Specular</td>" << endl;
		ofs << "<td width=\"50\" bgcolor=\"" << getHexColor (r, g, b) << "\"></td><td>(" << endl;
		ofs << r << " " << g << " " << b << " " << a << ")</td></tr>" << endl;

		// emissive
		r = (INT32)material.sMaterial.Emissive.r;
		g = (INT32)material.sMaterial.Emissive.g;
		b = (INT32)material.sMaterial.Emissive.b;
		a = (INT32)material.sMaterial.Emissive.a;
		ofs << "<tr><td>" << endl;
		ofs << "Emissive</td>" << endl;
		ofs << "<td width=\"50\" bgcolor=\"" << getHexColor (r, g, b) << "\"></td><td>(" << endl;
		ofs << r << " " << g << " " << b << " " << a << ")</td></tr>" << endl;

		ofs << "</table>" << endl;

		//ofs << "<br>&nbsp;&nbsp;&nbsp;Diffuse : " << (int)material.sMaterial.Diffuse.r << " " << (int)material.sMaterial.Diffuse.g << " " << (int)material.sMaterial.Diffuse.b << " " << (int)material.sMaterial.Diffuse.a << endl;
		//ofs << "<br>&nbsp;&nbsp;&nbsp;Ambient : " << (int)material.sMaterial.Ambient.r << " " << (int)material.sMaterial.Ambient.g << " " << (int)material.sMaterial.Ambient.b << " " << (int)material.sMaterial.Ambient.a << endl;
		//ofs << "<br>&nbsp;&nbsp;&nbsp;Specular : " << (int)material.sMaterial.Specular.r << " " << (int)material.sMaterial.Specular.g << " " << (int)material.sMaterial.Specular.b << " " << (int)material.sMaterial.Specular.a << endl;
		//ofs << "<br>&nbsp;&nbsp;&nbsp;Emissive : " << (int)material.sMaterial.Emissive.r << " " << (int)material.sMaterial.Emissive.g << " " << (int)material.sMaterial.Emissive.b << " " << (int)material.sMaterial.Emissive.a << endl;
		
		ofs << "&nbsp;&nbsp;&nbsp;Power : " << material.sMaterial.Power << endl;
		ofs << "<br>fTransparency : " << material.fTransparency << endl;
		ofs << "<br>" << endl;
	}

	// Objects
	ofs << "<br><table border=1 bgcolor=\"FFFFFF\" width=\"100%\"><tr><td align=\"center\"><b>OBJECTS</b></td></tr></table><br>" << endl;
	int iObject = 1;
	std::vector<t3DSObject>::iterator iteratorVectorObject;
	for(iteratorVectorObject = pModel->pObject.begin();
		iteratorVectorObject != pModel->pObject.end();
		iteratorVectorObject++)
	{
		t3DSObject object = *(iteratorVectorObject);

		ofs << "<br><b>Object : " << iObject << "/" << pModel->numOfObjects << "</b>" << endl;
		iObject++;
		ofs << "<br>name of the object : " << object.strName << endl;
		ofs << "<br>number of vertices : " << object.numOfVerts << endl;
		ofs << "<br>number of faces : " << object.numOfFaces << endl;
		ofs << "<br>number of texture coordinates : " << object.numTexVertex << endl;
		ofs << "<br>has a texture : " << object.bHasTexture << endl;
		ofs << "<br>Local Coordinate System : " << endl;
		ofs << "<table border=\"1\">" << endl;
		for (int i=0; i<4; i++)
		{
			ofs << "<tr>" << endl;
			for (int j=0; j<3; j++)
			{
				ofs << "<td>" << object.LocalCoordinateSystem[i][j] << "</td>";
			}
			ofs << "</tr>" << endl;
		}
		ofs << "</table>" << endl;

		ofs << "<br>Vertices : ";
		if (object.pVerts)
		{
			ofs << "<b><span style=\"color:green\">yes</span></b>" << endl;
		}
		else
		{
			ofs << "<b><span style=\"color:red\">no</span></b>" << endl;
		}
		ofs << "<br>Normals : ";
		if (object.pNormals)
		{
			ofs << "<b><span style=\"color:green\">yes</span></b>" << endl;
			//ofs << endl;
			//for (int i=0; i<object.numOfVerts; i++)
			//	ofs << "<br> " << object.pNormals[i].fX << " " << object.pNormals[i].fY << " " << object.pNormals[i].fZ << " " << endl;
		}
		else
		{
			ofs << "<b><span style=\"color:red\">no</span></b>" << endl;
		}
		ofs << "<br>Texture's UV coordinates : ";
		if (object.pTexVerts)
		{
			ofs << "<b><span style=\"color:green\">yes</span></b>" << endl;
			//ofs << endl;
			//for (int i=0; i<object.numOfVerts; i++)
			//	ofs << "<br> " << object.pTexVerts[i].fU << " " << object.pTexVerts[i].fV << endl;
		}
		else
		{
			ofs << "<b><span style=\"color:red\">no</span></b>" << endl;
		}
		ofs << "<br>Faces : ";
		if (object.pFaces)
		{
			//ofs << "..." << endl;
			ofs << endl;
			for (int i=0; i<object.numOfFaces; i++)
			{
				//ofs << "<br> index : " << i << " / " << object.numOfFaces << endl;
				//ofs << "<br> " << object.pFaces[i].vertIndex[0] << " " << object.pFaces[i].vertIndex[1] << " " << object.pFaces[i].vertIndex[2] << endl;
				//ofs << "<br> " << object.pFaces[i].coordIndex[0] << " " << object.pFaces[i].coordIndex[1] << " " << object.pFaces[i].coordIndex[2] << endl;
			}
			for (int i=0; i<object.numOfMaterials; i++)
			{
				ofs << "<br> materialID : " << pModel->pMaterials[object.pFacesMaterialList[i].materialID].strName << endl;
				ofs << "<br> numOfFaces : " << object.pFacesMaterialList[i].numOfFaces << endl;
				ofs << "<br> faces : " << endl;
				//for (int j=0; j<object.pFacesMaterialList[i].numOfFaces; j++)
				//	ofs << "<br>   " << object.pFacesMaterialList[i].pFacesMaterialsList[j] << endl;
			}
		}
		else
		{
			ofs << "<b><span style=\"color:red\">no</span></b>" << endl;
		}
		ofs << "<br>" << endl;
	}

	// Lights
	ofs << "<br><table border=1 bgcolor=\"FFFFFF\" width=\"100%\"><tr><td align=\"center\"><b>LIGHTS</b></td></tr></table><br>" << endl;
	int iLight = 1;
	std::vector<t3DSLight>::iterator iteratorVectorLight;
	for(iteratorVectorLight = pModel->pLights.begin();
		iteratorVectorLight != pModel->pLights.end();
		iteratorVectorLight++)
	{
		t3DSLight light = *(iteratorVectorLight);

		ofs << "<br><b>Light : " << iLight << "/" << pModel->numOfLights << "</b>" << endl;
		iLight++;
		ofs << "<br>name of the light : " << light.strName << endl;
		ofs << "<br>position : " << light.position[0] << " "<< light.position[1] << " " << light.position[2] << endl;

		ofs << "<br>color : " << light.color[0] << " " << light.color[1] << " " << light.color[2] << endl;
		ofs << "<br>outerRange : " << light.outerRange << endl;
		ofs << "<br>innerRange : " << light.innerRange << endl;
		ofs << "<br>multiplier : " << light.multiplier << endl;
		if (light.isSpotLight)
		{
			ofs << "<br>SpotLight :" << endl;
			ofs << "<br>&nbsp;target : " << light.spotLight_target[0] << " " << light.spotLight_target[1] << " " << light.spotLight_target[2] << endl;
			ofs << "<br>&nbsp;hotSpot : " << light.spotLight_hotSpot << endl;
			ofs << "<br>&nbsp;fallOff : " << light.spotLight_fallOff << endl;
		}
		ofs << "<br>" << endl;
	}

	// Cameras
	ofs << "<br><table border=1 bgcolor=\"FFFFFF\" width=\"100%\"><tr><td align=\"center\"><b>CAMERAS</b></td></tr></table><br>" << endl;
	int iCamera = 1;
	std::vector<t3DSCamera>::iterator iteratorVectorCamera;
	for(iteratorVectorCamera = pModel->pCameras.begin();
		iteratorVectorCamera != pModel->pCameras.end();
		iteratorVectorCamera++)
	{
		t3DSCamera camera = *(iteratorVectorCamera);

		ofs << "<br><b>Camera : " << iCamera << "/" << pModel->numOfCameras << "</b>" << endl;
		iCamera++;
		ofs << "<br>name of the camera : " << camera.strName << endl;
		ofs << "<br>position : " << camera.position[0] << " "<< camera.position[1] << " " << camera.position[2] << endl;
		ofs << "<br>target : " << camera.target[0] << " "<< camera.target[1] << " " << camera.target[2] << endl;
		ofs << "<br>bank : " << camera.bank << endl;
		ofs << "<br>lens : " << camera.lens << endl;
		ofs << "<br>" << endl;
	}

	
	// Animations
	ofs << "<br><table border=1 bgcolor=\"FFFFFF\" width=\"100%\"><tr><td align=\"center\"><b>ANIMATIONS</b></td></tr></table><br>" << endl;
	if (0)
	{
		int iAnimation = 1;
		std::vector<t3DSAnimation>::iterator iteratorVectorAnimation;
		for(iteratorVectorAnimation = pModel->pAnimations.begin();
			iteratorVectorAnimation != pModel->pAnimations.end();
			iteratorVectorAnimation++)
		{
			t3DSAnimation animation = *(iteratorVectorAnimation);

			ofs << "<br>Animation : " << iAnimation << "/" << pModel->numOfAnimations << endl;
			iAnimation++;
			ofs << "<br>objectType : " << hex << animation.objectType << dec << endl;

			for (int j=0; j<animation.nTracks; j++)
			{
				switch (animation.trackType[j])
				{
				case CHK3DS_B_POS_TRACK_TAG:
					{
						ofs << "<br>CHK3DS_B_POS_TRACK_TAG" << endl;
						t3DSPositionTrack *positionTrack = (t3DSPositionTrack*)(animation.track[j]);
						ofs << "<br>" << positionTrack->header.numOfKeys << endl;
						for (int i=0; i<positionTrack->header.numOfKeys; i++)
						{
							ofs << "<br> " << positionTrack->position[3*i] << " " << positionTrack->position[3*i+1] << " " << positionTrack->position[3*i+2] << endl;
						}
					}
					break;
				case CHK3DS_B_ROT_TRACK_TAG:
					{
						ofs << "<br>CHK3DS_B_ROT_TRACK_TAG" << endl;
						t3DSRotationTrack *rotationTrack = (t3DSRotationTrack*)(animation.track[j]);
						ofs << "<br>" << rotationTrack->header.numOfKeys << endl;
						ofs << "<table>" << endl;
						for (int i=0; i<rotationTrack->header.numOfKeys; i++)
						{
							ofs << "<tr>" << endl;
							ofs << "<td>" << rotationTrack->axis[3*i] << "</td>";
							ofs << "<td>" << rotationTrack->axis[3*i+1] << "</td>";
							ofs << "<td>" << rotationTrack->axis[3*i+2] << "</td>";
							ofs << "<td>" << rotationTrack->angle[i] << "</td>";
							ofs << "</tr>" << endl;
						}
						ofs << "</table>" << endl;
					}
					break;
				case CHK3DS_B_SCL_TRACK_TA:
					{
						ofs << "<br>CHK3DS_B_SCL_TRACK_TA" << endl;
						t3DSScaleTrack *scaleTrack = (t3DSScaleTrack*)(animation.track[j]);
						ofs << "<br>" << scaleTrack->header.numOfKeys << endl;
						ofs << "<table>" << endl;
						for (int i=0; i<scaleTrack->header.numOfKeys; i++)
						{
							ofs << "<tr>" << endl;
							ofs << "<td>" << scaleTrack->size[3*i] << "</td>";
							ofs << "<td>" << scaleTrack->size[3*i+1] << "</td>";
							ofs << "<td>" << scaleTrack->size[3*i+2] << "</td>";
							ofs << "</tr>" << endl;
						}
						ofs << "</table>" << endl;
					}
					break;
				default:
					{
						ofs << "<br> unknown track" << endl;
					}
					break;
				}
			}
		}
	}

	// Unknown chunks
	ofs << "<br><table border=1 bgcolor=\"FFFFFF\" width=\"100%\"><tr><td align=\"center\"><b>UNKNOWN CHUNKS</b></td></tr></table><br>" << endl;
	int iUnknownChunk = 0;
	std::vector<t3DSUnknownChunk>::iterator iteratorVectortUnknownChunk;
	ofs << "<br><table border=1>" << endl;
	ofs << "<tr>" << endl;
	ofs << "<td>index / total</td>" << endl;
	ofs << "<td>father Id</td>" << endl;
	ofs << "<td>chunk Id</td>" << endl;
	ofs << "</tr>" << endl;
	for(iteratorVectortUnknownChunk = pModel->pUnknownChunks.begin();
		iteratorVectortUnknownChunk != pModel->pUnknownChunks.end();
		iteratorVectortUnknownChunk++)
	{
		t3DSUnknownChunk unknownChunk = *(iteratorVectortUnknownChunk);

		iUnknownChunk++;
		ofs << "<tr>" << endl;
		ofs << "<td align=center>" << iUnknownChunk << "/" << pModel->numOfUnknownChunks << "</td>" << endl;
		ofs << "<td>0x" << hex << unknownChunk.fatherID << dec << "</td>" << endl;
		ofs << "<td>0x" << hex << unknownChunk.chunkID << dec << "</td>" << endl;
		ofs << "</tr>" << endl;
	}
	ofs << "</table>" << endl;

	ofs << "</body>" << endl;
	ofs << "</html>" << endl;
	ofs.close ();
}

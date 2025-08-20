#include "mesh.h"
#if 0
#include "io/FileFormat_U3D.h"
#endif

int Mesh::import_u3d (const char *filename)
{
	if (filename == NULL)
		return -1;
#if 0
	U3DModel *pU3DModel = LoadU3DFile(filename);
	DumpU3DFile (pU3DModel, "u3d_output.html");
	FreeU3DModel(pU3DModel);
#endif

	return 0;
}

int Mesh::export_u3d (const char *filename)
{
	/*
	if (filename == NULL)
		return -1;

	IFXWriteManager* pWriteManager = NULL;
	IFXWriteBuffer* pWriteBuffer = NULL;
	IFXStdio* pStdio = NULL;

	// Create Write Manager component
	if (IFXSUCCESS(result))
		result = IFXCreateComponent(CID_IFXWriteManager, IID_IFXWriteManager,
		(void**)&pWriteManager);

	// Initialize WriteManeger
	if (IFXSUCCESS(result))
		result = pWriteManager->Initialize(pCoreServices);

	// Create an IFXWriteBuffer object
	if (IFXSUCCESS(result))
		result = IFXCreateComponent(CID_IFXStdioWriteBuffer, IID_IFXWriteBuffer,
		(void**)&pWriteBuffer);

	// Get the objects's IFXStdio interface.
	if (IFXSUCCESS(result))
		result = pWriteBuffer->QueryInterface(IID_IFXStdio, (void**)&pStdio);

	// Open file to store scene
	if (IFXSUCCESS(result))
		result = pStdio->Open(filename);

	// Mark all subnodes to store. (Recursively marks a subset of the database)
	if (IFXSUCCESS(result))
		result = pSceneGraph->Mark();

	// Write an SceneGraph out to an WriteBuffer, based on the options supplied
	// in exportOptions.
	if (IFXSUCCESS(result))
		result = pWriteManager->Write(pWriteBuffer, IFXEXPORT_EVERYTHING);

	// Close the file
	if (IFXSUCCESS(result))
		result = pStdio->Close();

	IFXRELEASE(pStdio)
		IFXRELEASE(pWriteBuffer)
		IFXRELEASE(pWriteManager)
		*/
	return 0;
}

//----------------------------------------------------------------------------
// File: PRTSim.h
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
struct SETTINGS
{
    bool bSubDirs;
    bool bUserAbort;
    bool bVerbose;
    CGrowableArray <WCHAR*> aFiles;
};


//-----------------------------------------------------------------------------
struct CONCAT_MESH
{
    ID3DXMesh* pMesh;
    DWORD dwNumMaterials;
    CGrowableArray <DWORD> numMaterialsArray;
    CGrowableArray <D3DXMATERIAL> materialArray;
    CGrowableArray <D3DXSHMATERIAL> shMaterialArray;
};

//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
HRESULT RunPRTSimulator( IDirect3DDevice9* pd3dDevice, SIMULATOR_OPTIONS* pOptions,
                         CONCAT_MESH* pPRTMesh, CONCAT_MESH* pBlockerMesh, SETTINGS* pSettings );



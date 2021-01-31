//--------------------------------------------------------------------------------------
// File: SkinMesh.cpp
//
// Desc: Skinned mesh loader
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "skinmesh.h"


//--------------------------------------------------------------------------------------
// Name: AllocateName()
// Desc: Allocates memory for a string to hold the name of a frame or mesh
//--------------------------------------------------------------------------------------
HRESULT AllocateName( LPCSTR Name, char** ppNameOut )
{
    HRESULT hr = S_OK;
    char* pNewName = NULL;

    // Start clean
    ( *ppNameOut ) = NULL;

    // No work to be done if name is NULL
    if( NULL == Name )
    {
        hr = S_OK;
        goto e_Exit;
    }

    // Allocate a new buffer
    const UINT BUFFER_LENGTH = ( UINT )strlen( Name ) + 1;
    pNewName = new CHAR[BUFFER_LENGTH];
    if( NULL == pNewName )
    {
        hr = E_OUTOFMEMORY;
        goto e_Exit;
    }

    // Copy the string and return
    strcpy_s( pNewName, BUFFER_LENGTH, Name );
    ( *ppNameOut ) = pNewName;
    pNewName = NULL;
    hr = S_OK;

e_Exit:
    SAFE_DELETE_ARRAY( pNewName );
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::CreateFrame()
// Desc: Create a new derived D3DXFRAME object
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::CreateFrame( LPCSTR Name, D3DXFRAME** ppFrameOut )
{
    HRESULT hr = S_OK;

    // Start clean
    ( *ppFrameOut ) = NULL;

    // Create a new frame
    D3DXFRAME_DERIVED* pNewFrame = new D3DXFRAME_DERIVED;
    if( NULL == pNewFrame )
    {
        hr = E_OUTOFMEMORY;
        goto e_Exit;
    }

    // Clear the new frame
    ZeroMemory( pNewFrame, sizeof( D3DXFRAME_DERIVED ) );

    // Duplicate the name string
    hr = AllocateName( Name, &pNewFrame->Name );
    if( FAILED( hr ) )
        goto e_Exit;

    // Initialize other data members of the frame
    D3DXMatrixIdentity( &pNewFrame->TransformationMatrix );
    D3DXMatrixIdentity( &pNewFrame->CombinedTransformationMatrix );

    // Copy the frame to the output and return
    ( *ppFrameOut ) = pNewFrame;
    pNewFrame = NULL;
    hr = S_OK;

e_Exit:
    SAFE_DELETE( pNewFrame );
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::CreateMeshContainer()
// Desc: Create a new derived D3DXMESHCONTAINER object
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::CreateMeshContainer( const char* Name,
                                                 const D3DXMESHDATA* pMeshData,
                                                 const D3DXMATERIAL* pMaterials,
                                                 const D3DXEFFECTINSTANCE* pEffectInstances,
                                                 DWORD NumMaterials,
                                                 const DWORD* pAdjacency,
                                                 ID3DXSkinInfo* pSkinInfo,
                                                 D3DXMESHCONTAINER** ppMeshContainerOut )
{
    HRESULT hr = S_OK;
    ID3DXMesh* pNewMesh = NULL;
    D3DXMESHCONTAINER_DERIVED* pNewMeshContainer = NULL;
    DWORD* pNewAdjacency = NULL;
    D3DXMATRIX* pNewBoneOffsetMatrices = NULL;
    IDirect3DDevice9* pd3dDevice = NULL;

    // Start clean
    ( *ppMeshContainerOut ) = NULL;

    // This sample does not handle patch meshes, so fail when one is found
    if( D3DXMESHTYPE_MESH != pMeshData->Type )
    {
        hr = E_FAIL;
        goto e_Exit;
    }

    // Get the pMesh interface pointer out of the mesh data structure
    ID3DXMesh* pMesh = pMeshData->pMesh;
    if( NULL == pMesh )
    {
        hr = E_FAIL;
        goto e_Exit;
    }

    // Get the device
    hr = pMesh->GetDevice( &pd3dDevice );
    if( FAILED( hr ) )
        goto e_Exit;

    // Allocate the overloaded structure to return as a D3DXMESHCONTAINER
    pNewMeshContainer = new D3DXMESHCONTAINER_DERIVED;
    if( NULL == pNewMeshContainer )
    {
        hr = E_OUTOFMEMORY;
        goto e_Exit;
    }

    // Clear the new mesh container
    ZeroMemory( pNewMeshContainer, sizeof( D3DXMESHCONTAINER_DERIVED ) );

    //---------------------------------
    // Name
    //---------------------------------
    // Copy the name.  All memory as input belongs to caller, interfaces can be addref'd though
    hr = AllocateName( Name, &pNewMeshContainer->Name );
    if( FAILED( hr ) )
        goto e_Exit;

    //---------------------------------
    // MeshData
    //---------------------------------
    // Rearrange the mesh as desired
    hr = FilterMesh( pd3dDevice, pMeshData->pMesh, &pNewMesh );
    if( FAILED( hr ) )
        goto e_Exit;

    // Copy the pointer
    pNewMeshContainer->MeshData.Type = D3DXMESHTYPE_MESH;
    pNewMeshContainer->MeshData.pMesh = pNewMesh;
    pNewMesh = NULL;

    //---------------------------------
    // Materials (disabled)
    //---------------------------------
    pNewMeshContainer->NumMaterials = 0;
    pNewMeshContainer->pMaterials = NULL;

    //---------------------------------
    // Adjacency
    //---------------------------------
    ID3DXMesh* pTempMesh = pNewMeshContainer->MeshData.pMesh;
    pNewAdjacency = new DWORD[pTempMesh->GetNumFaces() * 3];
    if( NULL == pNewAdjacency )
    {
        hr = E_OUTOFMEMORY;
        goto e_Exit;
    }

    hr = pTempMesh->GenerateAdjacency( 1e-6f, pNewAdjacency );
    if( FAILED( hr ) )
        goto e_Exit;

    // Copy the pointer
    pNewMeshContainer->pAdjacency = pNewAdjacency;
    pNewAdjacency = NULL;

    //---------------------------------
    // SkinInfo
    //---------------------------------
    // if there is skinning information, save off the required data and then setup for HW skinning
    if( pSkinInfo )
    {
        // first save off the SkinInfo and original mesh data
        pNewMeshContainer->pSkinInfo = pSkinInfo;
        pSkinInfo->AddRef();

        // Will need an array of offset matrices to move the vertices from the figure space to the 
        // bone's space
        const DWORD NumBones = pSkinInfo->GetNumBones();
        pNewBoneOffsetMatrices = new D3DXMATRIX[ NumBones ];
        if( NULL == pNewBoneOffsetMatrices )
        {
            hr = E_OUTOFMEMORY;
            goto e_Exit;
        }

        // Get each of the bone offset matrices so that we don't need to get them later
        for( UINT iBone = 0; iBone < NumBones; iBone++ )
        {
            pNewBoneOffsetMatrices[iBone] = *( pSkinInfo->GetBoneOffsetMatrix( iBone ) );
        }

        // Copy the pointer
        pNewMeshContainer->pBoneOffsetMatrices = pNewBoneOffsetMatrices;
        pNewBoneOffsetMatrices = NULL;

        // GenerateSkinnedMesh will take the general skinning information and transform it to a 
        // HW friendly version
        hr = GenerateSkinnedMesh( pd3dDevice, pNewMeshContainer );
        if( FAILED( hr ) )
            goto e_Exit;

    }

    // Copy the mesh container and return
    ( *ppMeshContainerOut ) = pNewMeshContainer;
    pNewMeshContainer = NULL;
    hr = S_OK;

e_Exit:
    SAFE_DELETE_ARRAY( pNewBoneOffsetMatrices );
    SAFE_DELETE_ARRAY( pNewAdjacency );
    SAFE_RELEASE( pNewMesh );
    SAFE_RELEASE( pd3dDevice );

    // Call Destroy function to properly clean up the memory allocated 
    if( pNewMeshContainer )
        DestroyMeshContainer( pNewMeshContainer );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::FilterMesh
// Desc: Alter or optimize the mesh before adding it to the new mesh container
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::FilterMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMeshIn, ID3DXMesh** ppMeshOut )
{
    HRESULT hr = S_OK;
    ID3DXMesh* pTempMesh = NULL;
    ID3DXMesh* pNewMesh = NULL;
    DWORD* pAdjacency = NULL;

    // Start clean
    ( *ppMeshOut ) = NULL;

    // Create a new vertex declaration to hold all the required data
    const D3DVERTEXELEMENT9 VertexDecl[] =
    {
        { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT,  0 },
        { 0, 36, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 44, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
        { 0, 60, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
        D3DDECL_END()
    };

    // Clone mesh to the new vertex format
    hr = pMeshIn->CloneMesh( pMeshIn->GetOptions(), VertexDecl, pd3dDevice, &pTempMesh );
    if( FAILED( hr ) )
        goto e_Exit;


    // Compute tangents, which are required for normal mapping
    hr = D3DXComputeTangentFrameEx( pTempMesh, D3DDECLUSAGE_TEXCOORD, 0, D3DDECLUSAGE_TANGENT, 0,
                                    D3DX_DEFAULT, 0, D3DDECLUSAGE_NORMAL, 0,
                                    0, NULL, -1, 0, -1, &pNewMesh, NULL );
    if( FAILED( hr ) )
        goto e_Exit;

    // Copy the mesh and return
    ( *ppMeshOut ) = pNewMesh;
    pNewMesh = NULL;
    hr = S_OK;

e_Exit:
    SAFE_DELETE_ARRAY( pAdjacency );
    SAFE_RELEASE( pNewMesh );
    SAFE_RELEASE( pTempMesh );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Called either by CreateMeshContainer when loading a skin mesh, or when 
// changing methods.  This function uses the pSkinInfo of the mesh 
// container to generate the desired drawable mesh and bone combination 
// table.
//--------------------------------------------------------------------------------------
HRESULT GenerateSkinnedMesh( IDirect3DDevice9* pd3dDevice, D3DXMESHCONTAINER_DERIVED* pMeshContainer )
{
    HRESULT hr = S_OK;

    // Save off the mesh
    ID3DXMesh* pMeshStart = pMeshContainer->MeshData.pMesh;
    pMeshContainer->MeshData.pMesh = NULL;

    // Start clean
    SAFE_RELEASE( pMeshContainer->pBoneCombinationBuf );

    // Get the SkinInfo
    ID3DXSkinInfo* pSkinInfo = pMeshContainer->pSkinInfo;
    if( pSkinInfo == NULL )
    {
        hr = E_INVALIDARG;
        goto e_Exit;
    }

    // Use ConvertToIndexedBlendedMesh to generate drawable mesh 
    pMeshContainer->NumPaletteEntries = min( MAX_BONES, pSkinInfo->GetNumBones() );

    hr = pSkinInfo->ConvertToIndexedBlendedMesh( pMeshStart,
                                                 D3DXMESHOPT_VERTEXCACHE | D3DXMESH_MANAGED,
                                                 pMeshContainer->NumPaletteEntries,
                                                 pMeshContainer->pAdjacency,
                                                 NULL, NULL, NULL,
                                                 &pMeshContainer->NumInfl,
                                                 &pMeshContainer->NumAttributeGroups,
                                                 &pMeshContainer->pBoneCombinationBuf,
                                                 &pMeshContainer->MeshData.pMesh );
    if( FAILED( hr ) )
        goto e_Exit;

    hr = S_OK;

e_Exit:
    SAFE_RELEASE( pMeshStart );
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::DestroyFrame()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::DestroyFrame( LPD3DXFRAME pFrameToFree )
{
    SAFE_DELETE_ARRAY( pFrameToFree->Name );
    SAFE_DELETE( pFrameToFree );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::DestroyMeshContainer()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::DestroyMeshContainer( LPD3DXMESHCONTAINER pMeshContainerBase )
{
    D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;

    SAFE_DELETE_ARRAY( pMeshContainer->Name );
    SAFE_DELETE_ARRAY( pMeshContainer->pAdjacency );
    SAFE_DELETE_ARRAY( pMeshContainer->pMaterials );
    SAFE_DELETE_ARRAY( pMeshContainer->pBoneOffsetMatrices );
    SAFE_DELETE_ARRAY( pMeshContainer->ppBoneMatrixPtrs );
    SAFE_RELEASE( pMeshContainer->pBoneCombinationBuf );
    SAFE_RELEASE( pMeshContainer->MeshData.pMesh );
    SAFE_RELEASE( pMeshContainer->pSkinInfo );
    SAFE_DELETE( pMeshContainer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Update the frame matrices
//--------------------------------------------------------------------------------------
void UpdateFrameMatrices( LPD3DXFRAME pFrameBase, const D3DXMATRIX* pParentMatrix )
{
    D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;

    // Concatenate all matrices in the chain
    if( pParentMatrix != NULL )
        D3DXMatrixMultiply( &pFrame->CombinedTransformationMatrix, &pFrame->TransformationMatrix, pParentMatrix );
    else
        pFrame->CombinedTransformationMatrix = pFrame->TransformationMatrix;

    // Call siblings
    if( pFrame->pFrameSibling )
        UpdateFrameMatrices( pFrame->pFrameSibling, pParentMatrix );

    // Call children
    if( pFrame->pFrameFirstChild )
        UpdateFrameMatrices( pFrame->pFrameFirstChild, &pFrame->CombinedTransformationMatrix );
}


//--------------------------------------------------------------------------------------
// Called to setup the pointers for a given bone to its transformation matrix
//--------------------------------------------------------------------------------------
HRESULT SetupBoneMatrixPointersOnMesh( LPD3DXMESHCONTAINER pMeshContainerBase, LPD3DXFRAME pFrameRoot )
{
    UINT iBone, cBones;
    D3DXFRAME_DERIVED* pFrame;

    D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;

    // if there is a skinmesh, then setup the bone matrices
    if( pMeshContainer->pSkinInfo != NULL )
    {
        cBones = pMeshContainer->pSkinInfo->GetNumBones();

        pMeshContainer->ppBoneMatrixPtrs = new D3DXMATRIX*[cBones];
        if( pMeshContainer->ppBoneMatrixPtrs == NULL )
            return E_OUTOFMEMORY;

        for( iBone = 0; iBone < cBones; iBone++ )
        {
            pFrame = ( D3DXFRAME_DERIVED* )D3DXFrameFind( pFrameRoot,
                                                          pMeshContainer->pSkinInfo->GetBoneName( iBone ) );
            if( pFrame == NULL )
                return E_FAIL;

            pMeshContainer->ppBoneMatrixPtrs[iBone] = &pFrame->CombinedTransformationMatrix;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Called to setup the pointers for a given bone to its transformation matrix
//--------------------------------------------------------------------------------------
HRESULT SetupBoneMatrixPointers( LPD3DXFRAME pFrame, LPD3DXFRAME pFrameRoot )
{
    HRESULT hr;

    if( pFrame->pMeshContainer != NULL )
    {
        hr = SetupBoneMatrixPointersOnMesh( pFrame->pMeshContainer, pFrameRoot );
        if( FAILED( hr ) )
            return hr;
    }

    if( pFrame->pFrameSibling != NULL )
    {
        hr = SetupBoneMatrixPointers( pFrame->pFrameSibling, pFrameRoot );
        if( FAILED( hr ) )
            return hr;
    }

    if( pFrame->pFrameFirstChild != NULL )
    {
        hr = SetupBoneMatrixPointers( pFrame->pFrameFirstChild, pFrameRoot );
        if( FAILED( hr ) )
            return hr;
    }

    return S_OK;
}

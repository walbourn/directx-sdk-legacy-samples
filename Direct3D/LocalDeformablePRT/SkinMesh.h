//-----------------------------------------------------------------------------
// File: SkinMesh.h
//
// Desc: Skinned mesh loader
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef _SKINMESH_H
#define _SKINMESH_H

const int MAX_BONES = 26;


//--------------------------------------------------------------------------------------
// Name: struct D3DXFRAME_DERIVED
// Desc: Structure derived from D3DXFRAME so we can add some app-specific
//       info that will be stored with each frame
//--------------------------------------------------------------------------------------
struct D3DXFRAME_DERIVED : public D3DXFRAME
{
    D3DXMATRIXA16 CombinedTransformationMatrix;
};


//--------------------------------------------------------------------------------------
// Name: struct D3DXMESHCONTAINER_DERIVED
// Desc: Structure derived from D3DXMESHCONTAINER so we can add some app-specific
//       info that will be stored with each mesh
//--------------------------------------------------------------------------------------
struct D3DXMESHCONTAINER_DERIVED : public D3DXMESHCONTAINER
{
    // SkinMesh info             
    LPD3DXATTRIBUTERANGE pAttributeTable;
    DWORD NumAttributeGroups;
    DWORD NumInfl;
    LPD3DXBUFFER pBoneCombinationBuf;
    D3DXMATRIX** ppBoneMatrixPtrs;
    D3DXMATRIX* pBoneOffsetMatrices;
    DWORD NumPaletteEntries;
};


//--------------------------------------------------------------------------------------
// Name: class CAllocateHierarchy
// Desc: Custom version of ID3DXAllocateHierarchy with custom methods to create
//       frames and meshcontainers.
//--------------------------------------------------------------------------------------
class CAllocateHierarchy : public ID3DXAllocateHierarchy
{
public:
    STDMETHOD( CreateFrame )( THIS_ LPCSTR Name, LPD3DXFRAME *ppNewFrame );
    STDMETHOD( CreateMeshContainer )( THIS_
        LPCSTR Name,
        CONST D3DXMESHDATA *pMeshData,
        CONST D3DXMATERIAL *pMaterials,
        CONST D3DXEFFECTINSTANCE *pEffectInstances,
        DWORD NumMaterials,
        CONST DWORD *pAdjacency,
        LPD3DXSKININFO pSkinInfo,
        LPD3DXMESHCONTAINER *ppNewMeshContainer );
    STDMETHOD( DestroyFrame )( THIS_ LPD3DXFRAME pFrameToFree );
    STDMETHOD( DestroyMeshContainer )( THIS_ LPD3DXMESHCONTAINER pMeshContainerBase );

            CAllocateHierarchy()
            {
            }
    HRESULT FilterMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMeshIn, ID3DXMesh** ppMeshOut );
};


// Public functions
HRESULT SetupBoneMatrixPointersOnMesh( LPD3DXMESHCONTAINER pMeshContainer, D3DXFRAME* pFrameRoot );
HRESULT SetupBoneMatrixPointers( LPD3DXFRAME pFrame, D3DXFRAME* pFrameRoot );
void UpdateFrameMatrices( LPD3DXFRAME pFrameBase, const D3DXMATRIX* pParentMatrix );
void UpdateSkinningMethod( LPD3DXFRAME pFrameBase );
HRESULT GenerateSkinnedMesh( IDirect3DDevice9* pd3dDevice, D3DXMESHCONTAINER_DERIVED* pMeshContainer );


#endif //_SKINMESH_H

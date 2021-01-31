//--------------------------------------------------------------------------------------
// File: GenShadowMesh.h
//
// Functions to help generate a mesh with degenerate triangles between edges for shadow volumes
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"

#define ADJACENCY_EPSILON 0.0001f
#define EXTRUDE_EPSILON 0.1f

struct POSTPROCVERT
{
    float x, y, z;
    float rhw;

    const static D3DVERTEXELEMENT9 Decl[2];
};

struct SHADOWVERT
{
    D3DXVECTOR3 Position;
    D3DXVECTOR3 Normal;

    const static D3DVERTEXELEMENT9 Decl[3];
};

struct MESHVERT
{
    D3DXVECTOR3 Position;
    D3DXVECTOR3 Normal;
    D3DXVECTOR2 Tex;

    const static D3DVERTEXELEMENT9 Decl[4];
};

HRESULT GenerateShadowMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMesh, ID3DXMesh** ppOutMesh );

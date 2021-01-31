//--------------------------------------------------------------------------------------
// File: SubDMesh.h
//
// This class encapsulates the mesh loading and housekeeping functions for a SubDMesh.
// The mesh loads .obj files from disk and converts them to a Catmull-Clark patch based
// format.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"

// Maximum number of points that can be part of a subd quad.
// This includes the 4 interior points of the quad, plus the 1-ring neighborhood.
// This value must be divisible by 4.
#define MAX_POINTS 32

// We special case regular patches since they are cheaper to compute and more common.
#define NUM_REGULAR_POINTS 16

// Define the patch stride in terms of float4s 
#define PATCH_STRIDE 16	

// Maximum valence we expect to encounter for extraordinary vertices
#define MAX_VALENCE 16

struct SUBDPATCH
{
    // First 4 points are the inner quad
    // The remaining points are the 1-ring neighborhood in counter clockwise order
    UINT    m_Points[MAX_POINTS];

    // Valences for the first 4 points
    BYTE    m_Valences[4];

    // Precalculated prefixes for the first 4 points
    BYTE    m_Prefixes[4];
};

struct SUBDPATCHREGULAR
{
    // First 4 points are the inner quad
    // The remaining points are the 1-ring neighborhood in counter clockwise order
    D3DXVECTOR4 m_Points[NUM_REGULAR_POINTS];
};

// Vertex format
struct VERTEX
{
    D3DXVECTOR4 m_Position;
    D3DXVECTOR3 m_Normal;
    D3DXVECTOR2 m_Texcoord;

    BYTE    m_Bones[2];
    BYTE    m_Weights[2];
};

// For extraordinary patches, we store the bones in a separate buffer since they won't fit
// in with the regular vertex data.
struct BONESSTRUCT
{
    BYTE    m_Bones[2];
    BYTE    m_Weights[2];
};


// Used for a hashtable vertex cache when creating the mesh from a .obj file
struct CacheEntry
{
    UINT m_Index;
    CacheEntry* m_pNext;
};

struct BONETRANSFORM
{
    D3DXVECTOR3 m_Translation;
    D3DXVECTOR3 m_Rotation;
};

//--------------------------------------------------------------------------------------
// This class handles most of the loading and conversion for a subd mesh.  It also
// creates and tracks buffers used by the mesh.
//--------------------------------------------------------------------------------------
class CSubDMesh
{
private:
    CGrowableArray <SUBDPATCH*> m_QuadArray;        // Array of quads (regular and extraordinary)
    CGrowableArray <SUBDPATCHREGULAR*> m_RegularQuadArray; // Just the regular quads

    CGrowableArray <CacheEntry*> m_VertexCache;   // Hashtable cache for locating duplicate vertices
    CGrowableArray <VERTEX> m_Vertices;      // Filled and copied to the vertex buffer
    CGrowableArray <DWORD> m_Indices;       // Filled and copied to the index buffer
    CGrowableArray <D3DXVECTOR4> m_Tangents;      // Texture space tangents: we use 4 components here because we're fetching them from a buffer in hlsl.
    CGrowableArray <BONETRANSFORM*> m_Bones;         // Bones for skinning
    D3DXMATRIX* m_pTransforms;   // Skeletal transforms

    D3DXVECTOR3 m_vMeshExtentsMax;      // Min and Max <x,y,z> for the mesh
    D3DXVECTOR3 m_vMeshExtentsMin;

    // Mesh objects
    ID3D10Buffer* m_pPatchesBufferB;      // Stores the bicubic patch coefficients
    ID3D10Buffer* m_pPatchesBufferUV;     // Stores the tangent UV patch coefficients
    ID3D10ShaderResourceView* m_pPatchesBufferBSRV;
    ID3D10ShaderResourceView* m_pPatchesBufferUVSRV;

    ID3D10Buffer* m_pControlPointVB;      // Stores the control points
    ID3D10Buffer* m_pControlPointBonesVB; // Stores the bones number and weights
    ID3D10Buffer* m_pControlPointUV;      // Stores UV coordinates
    ID3D10ShaderResourceView* m_pControlPointSRV;
    ID3D10ShaderResourceView* m_pControlPointBonesSRV;
    ID3D10ShaderResourceView* m_pControlPointUVSRV;
    ID3D10Buffer* m_pSubDPatchVB;	        // Stores the actual patch representation (4 verts and 1-ring neighborhood)
    ID3D10Buffer* m_pSubDPatchRegVB;      // Same thing, but just for regular patches
    ID3D10ShaderResourceView* m_pHeightSRV;

private:
    // Loading helpers
    DWORD       AddVertex( UINT hash, VERTEX* pVertex );
    void        DeleteCache();

    // Conditioning helpers
    bool        QuadContainsPoint( SUBDPATCH* pQuad, D3DXVECTOR4* pV );
    int         FindLocalIndexForPointInQuad( SUBDPATCH* pQuad, D3DXVECTOR4* pV );
    SUBDPATCH* FindQuadWithPointsABButNotC( D3DXVECTOR4* pA, D3DXVECTOR4* pB, D3DXVECTOR4* pC );
    BYTE        ConditionPoint( D3DXVECTOR4* pV, D3DXVECTOR4* vOtherPatchV, CGrowableArray <int>* pNeighborPoints );
    bool        ConditionPatch( SUBDPATCH* pQuad );

public:
                CSubDMesh();
                ~CSubDMesh();

    // Loading
    HRESULT     LoadSubDFromObj( const WCHAR* strFileName );
    void        Destroy();

    // Conditioning the mesh and getting it ready for the conversion process
    bool        ConditionMesh();
    HRESULT     ComputeTangents();
    void        SortPatchesBySize();
    HRESULT     CreatePatchesBuffer( ID3D10Device* pd3dDevice );
    HRESULT     CreateBuffersFromSubDMesh( ID3D10Device* pd3dDevice );
    HRESULT     LoadHeightTexture( ID3D10Device* pd3dDevice, WCHAR* strfile );

    // Programatic skinning
    void        AddBones( UINT NumBones, D3DXVECTOR3 vDir, float falloff );
    void        RotateBone( UINT iBone, D3DXVECTOR3 vRot );
    void        CreateSkinningMatrices();

    // Various housekeeping
    int         GetNumPatches()
    {
        return m_QuadArray.GetSize();
    }
    int         GetNumExtraordinaryPatches()
    {
        return m_QuadArray.GetSize() - m_RegularQuadArray.GetSize();
    }
    int         GetNumRegularPatches()
    {
        return m_RegularQuadArray.GetSize();
    }
    int         GetNumVertices()
    {
        return m_Vertices.GetSize();
    }
    SUBDPATCH* GetPatch( int i )
    {
        return m_QuadArray.GetAt( i );
    }
    SUBDPATCH* GetPatchExtraordinary( int i )
    {
        return m_QuadArray.GetAt( i );
    }
    SUBDPATCHREGULAR* GetPatchRegular( int i )
    {
        return m_RegularQuadArray.GetAt( i );
    }
    VERTEX      GetVertex( int i )
    {
        return m_Vertices.GetAt( i );
    }
    VERTEX* GetVertices()
    {
        return m_Vertices.GetData();
    }
    D3DXVECTOR4 GetTangent( int i )
    {
        return m_Tangents.GetAt( i );
    }

    D3DXMATRIX* GetTransformMatrices()
    {
        return m_pTransforms;
    }
    int         GetNumTransformMatrices()
    {
        return m_Bones.GetSize();
    }

    ID3D10Buffer* GetPatchesBufferB()
    {
        return m_pPatchesBufferB;
    }
    ID3D10Buffer* GetPatchesBufferUV()
    {
        return m_pPatchesBufferUV;
    }
    ID3D10ShaderResourceView* GetPatchesBufferBSRV()
    {
        return m_pPatchesBufferBSRV;
    }
    ID3D10ShaderResourceView* GetPatchesBufferUVSRV()
    {
        return m_pPatchesBufferUVSRV;
    }

    ID3D10ShaderResourceView* GetControlPointSRV()
    {
        return m_pControlPointSRV;
    }
    ID3D10ShaderResourceView* GetControlPointBonesSRV()
    {
        return m_pControlPointBonesSRV;
    }
    ID3D10ShaderResourceView* GetControlPointUVSRV()
    {
        return m_pControlPointUVSRV;
    }
    ID3D10Buffer* GetSubDPatchVB()
    {
        return m_pSubDPatchVB;
    }
    ID3D10Buffer* GetSubDPatchRegVB()
    {
        return m_pSubDPatchRegVB;
    }
    ID3D10ShaderResourceView* GetHeightSRV()
    {
        return m_pHeightSRV;
    }
};

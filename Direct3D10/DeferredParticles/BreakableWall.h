//--------------------------------------------------------------------------------------
// File: BreakableWall.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SDKMesh.h"

#define NUM_CHUNKS 9

struct BOUNDING_SPHERE
{
    D3DXVECTOR3 vCenter;
    float fRadius;
};

struct MESH_INSTANCE
{
    CDXUTSDKMesh* pMesh;
    BOUNDING_SPHERE BS;
    D3DXVECTOR3 vPosition;
    D3DXVECTOR3 vRotation;
    D3DXVECTOR3 vRotationOrig;

    bool bDynamic;
    bool bVisible;
    D3DXVECTOR3 vVelocity;
    D3DXVECTOR3 vRotationSpeed;
};

//--------------------------------------------------------------------------------------
class CBreakableWall
{
private:
    MESH_INSTANCE m_BaseMesh;
    MESH_INSTANCE   m_ChunkMesh[NUM_CHUNKS];
    bool m_bBroken;

public:
                    CBreakableWall();
                    ~CBreakableWall();

    void            SetPosition( D3DXVECTOR3 vPosition );
    void            SetRotation( D3DXVECTOR3 vRotation );
    void            SetBaseMesh( CDXUTSDKMesh* pMesh );
    void            SetChunkMesh( UINT iChunk, CDXUTSDKMesh* pMesh, D3DXVECTOR3 vOffset, float fWallScale );
    void            RenderInstance( MESH_INSTANCE* pInstance, ID3D10Device* pd3dDevice,
                                    ID3D10EffectTechnique* pTechnique, float fWallScale, D3DXMATRIX* pmViewProj,
                                    ID3D10EffectMatrixVariable* pWVP, ID3D10EffectMatrixVariable* pWorld );
    void            Render( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, float fWallScale,
                            D3DXMATRIX* pmViewProj, ID3D10EffectMatrixVariable* pWVP,
                            ID3D10EffectMatrixVariable* pWorld );

    D3DXMATRIX      GetMeshMatrix( MESH_INSTANCE* pInstance, float fWallScale );
    D3DXMATRIX      GetBaseMeshMatrix( float fWallScale );
    D3DXMATRIX      GetChunkMeshMatrix( UINT iChunk, float fWallScale );

    bool            IsBaseMeshVisible();
    bool            IsChunkMeshVisible( UINT iChunk );

    void            CreateExplosion( D3DXVECTOR3 vCenter, D3DXVECTOR3 vDirMul, float fRadius, float fMinPower,
                                     float fMaxPower, float fWallScale );
    void            AdvancePieces( float fElapsedTime, D3DXVECTOR3 vGravity );
};

//--------------------------------------------------------------------------------------
class CBuilding
{
private:
    UINT m_WidthWalls;
    UINT m_HeightWalls;
    UINT m_DepthWalls;

    UINT m_NumWalls;
    CBreakableWall* m_pWallList;

    BOUNDING_SPHERE m_BS;
    float m_fWallScale;

public:
            CBuilding();
            ~CBuilding();

    bool    CreateBuilding( D3DXVECTOR3 vCenter, float fWallScale, UINT WidthWalls, UINT HeightWalls,
                            UINT DepthWalls );
    void    SetBaseMesh( CDXUTSDKMesh* pMesh );
    void    SetChunkMesh( UINT iChunk, CDXUTSDKMesh* pMesh, D3DXVECTOR3 vOffset );
    void    Render( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, D3DXMATRIX* pmViewProj,
                    ID3D10EffectMatrixVariable* pWVP, ID3D10EffectMatrixVariable* pWorld );

    void    CollectBaseMeshMatrices( CGrowableArray <D3DXMATRIX>* pMatrixArray );
    void    CollectChunkMeshMatrices( UINT iChunk, CGrowableArray <D3DXMATRIX>* pMatrixArray );

    void    CreateExplosion( D3DXVECTOR3 vCenter, D3DXVECTOR3 vDirMul, float fRadius, float fMinPower,
                             float fMaxPower );
    void    AdvancePieces( float fElapsedTime, D3DXVECTOR3 vGravity );
};

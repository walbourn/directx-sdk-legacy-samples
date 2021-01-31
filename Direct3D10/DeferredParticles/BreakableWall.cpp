//--------------------------------------------------------------------------------------
// File: BreakableWall.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "BreakableWall.h"

//--------------------------------------------------------------------------------------
CBreakableWall::CBreakableWall()
{
    m_bBroken = false;
}

//--------------------------------------------------------------------------------------
CBreakableWall::~CBreakableWall()
{
}

//--------------------------------------------------------------------------------------
void CBreakableWall::SetPosition( D3DXVECTOR3 vPosition )
{
    m_BaseMesh.vPosition = vPosition;
    for( UINT i = 0; i < NUM_CHUNKS; i++ )
    {
        m_ChunkMesh[i].vPosition = vPosition;
    }
}

//--------------------------------------------------------------------------------------
void CBreakableWall::SetRotation( D3DXVECTOR3 vRotation )
{
    m_BaseMesh.vRotation = vRotation;
    m_BaseMesh.vRotationOrig = vRotation;
    for( UINT i = 0; i < NUM_CHUNKS; i++ )
    {
        m_ChunkMesh[i].vRotation = vRotation;
        m_ChunkMesh[i].vRotationOrig = vRotation;
    }
}

//--------------------------------------------------------------------------------------
void CBreakableWall::SetBaseMesh( CDXUTSDKMesh* pMesh )
{
    m_BaseMesh.pMesh = pMesh;

    D3DXVECTOR3 vExtents = pMesh->GetMeshBBoxExtents( 0 );
    m_BaseMesh.BS.vCenter = pMesh->GetMeshBBoxCenter( 0 );
    m_BaseMesh.BS.fRadius = D3DXVec3Length( &vExtents );

    m_BaseMesh.bVisible = true;
}

//--------------------------------------------------------------------------------------
void CBreakableWall::SetChunkMesh( UINT iChunk, CDXUTSDKMesh* pMesh, D3DXVECTOR3 vOffset, float fWallScale )
{
    m_ChunkMesh[iChunk].pMesh = pMesh;

    D3DXMATRIX mRot;
    m_ChunkMesh[iChunk].vRotation = m_ChunkMesh[iChunk].vRotationOrig;
    D3DXMatrixRotationYawPitchRoll( &mRot, m_ChunkMesh[iChunk].vRotation.y, m_ChunkMesh[iChunk].vRotation.x,
                                    m_ChunkMesh[iChunk].vRotation.z );

    vOffset *= fWallScale;
    D3DXVECTOR3 vRotOffset;
    D3DXVec3TransformNormal( &vRotOffset, &vOffset, &mRot );

    m_ChunkMesh[iChunk].vPosition = m_BaseMesh.vPosition + vRotOffset;

    D3DXVECTOR3 vExtents = pMesh->GetMeshBBoxExtents( 0 );
    m_ChunkMesh[iChunk].BS.vCenter = pMesh->GetMeshBBoxCenter( 0 );
    m_ChunkMesh[iChunk].BS.fRadius = D3DXVec3Length( &vExtents );

    m_ChunkMesh[iChunk].bDynamic = false;
    m_ChunkMesh[iChunk].bVisible = true;
}

//--------------------------------------------------------------------------------------
void CBreakableWall::RenderInstance( MESH_INSTANCE* pInstance, ID3D10Device* pd3dDevice,
                                     ID3D10EffectTechnique* pTechnique, float fWallScale, D3DXMATRIX* pmViewProj,
                                     ID3D10EffectMatrixVariable* pWVP, ID3D10EffectMatrixVariable* pWorld )
{
    if( !pInstance->bVisible )
        return;

    D3DXMATRIX mWorld = GetMeshMatrix( pInstance, fWallScale );
    D3DXMATRIX mWVP = mWorld * ( *pmViewProj );

    pWVP->SetMatrix( ( float* )&mWVP );
    pWorld->SetMatrix( ( float* )&mWorld );

    pInstance->pMesh->Render( pd3dDevice, pTechnique );
}

//--------------------------------------------------------------------------------------
void CBreakableWall::Render( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, float fWallScale,
                             D3DXMATRIX* pmViewProj, ID3D10EffectMatrixVariable* pWVP,
                             ID3D10EffectMatrixVariable* pWorld )
{
    if( m_bBroken )
    {
        for( UINT i = 0; i < NUM_CHUNKS; i++ )
        {
            RenderInstance( &m_ChunkMesh[i], pd3dDevice, pTechnique, fWallScale, pmViewProj, pWVP, pWorld );
        }
    }
    else
    {
        RenderInstance( &m_BaseMesh, pd3dDevice, pTechnique, fWallScale, pmViewProj, pWVP, pWorld );
    }
}

//--------------------------------------------------------------------------------------
D3DXMATRIX CBreakableWall::GetMeshMatrix( MESH_INSTANCE* pInstance, float fWallScale )
{
    D3DXMATRIX mScale;
    D3DXMATRIX mRot;
    D3DXMATRIX mTrans;

    D3DXMatrixScaling( &mScale, fWallScale, fWallScale, fWallScale );
    D3DXMatrixRotationYawPitchRoll( &mRot, pInstance->vRotation.y, pInstance->vRotation.x, pInstance->vRotation.z );
    D3DXMatrixTranslation( &mTrans, pInstance->vPosition.x, pInstance->vPosition.y, pInstance->vPosition.z );

    D3DXMATRIX mWorld = mScale * mRot * mTrans;

    return mWorld;
}

//--------------------------------------------------------------------------------------
D3DXMATRIX CBreakableWall::GetBaseMeshMatrix( float fWallScale )
{
    return GetMeshMatrix( &m_BaseMesh, fWallScale );
}

//--------------------------------------------------------------------------------------
D3DXMATRIX CBreakableWall::GetChunkMeshMatrix( UINT iChunk, float fWallScale )
{
    return GetMeshMatrix( &m_ChunkMesh[iChunk], fWallScale );
}

//--------------------------------------------------------------------------------------
bool CBreakableWall::IsBaseMeshVisible()
{
    return !m_bBroken;
}

//--------------------------------------------------------------------------------------
bool CBreakableWall::IsChunkMeshVisible( UINT iChunk )
{
    return m_ChunkMesh[iChunk].bVisible;
}

//--------------------------------------------------------------------------------------
float RPercent();
void CBreakableWall::CreateExplosion( D3DXVECTOR3 vCenter, D3DXVECTOR3 vDirMul, float fRadius, float fMinPower,
                                      float fMaxPower, float fWallScale )
{
    D3DXVECTOR3 vDelta = vCenter - ( m_BaseMesh.vPosition + m_BaseMesh.BS.vCenter * fWallScale );
    float fDist = D3DXVec3LengthSq( &vDelta );

    float f2Rad = fRadius + m_BaseMesh.BS.fRadius * fWallScale;
    if( fDist > f2Rad * f2Rad )
        return;

    // We're broken
    m_bBroken = true;

    for( UINT i = 0; i < NUM_CHUNKS; i++ )
    {
        // center of gravity
        D3DXVECTOR3 vCOG = m_ChunkMesh[i].vPosition;// + m_ChunkMesh[i].BS.vCenter*fWallScale;
        vDelta = vCOG - vCenter;
        float fDist = D3DXVec3LengthSq( &vDelta );
        float fChunkRad = m_ChunkMesh[i].BS.fRadius * fWallScale;
        f2Rad = fRadius + fChunkRad;

        if( fDist < f2Rad * f2Rad )
        {
            // We're in motion
            m_ChunkMesh[i].bDynamic = true;

            // Set velocity
            D3DXVec3Normalize( &vDelta, &vDelta );

            fDist -= fChunkRad * fChunkRad;

            float fPowerLerp = fabs( RPercent() );
            float fPower = ( fMaxPower * fPowerLerp + fMinPower * ( 1.0f - fPowerLerp ) );// / sqrt(fDist);

            m_ChunkMesh[i].vVelocity = vDelta * fPower;
            m_ChunkMesh[i].vVelocity.x *= vDirMul.x;
            m_ChunkMesh[i].vVelocity.y *= vDirMul.y;
            m_ChunkMesh[i].vVelocity.z *= vDirMul.z;

            // Set rotation speed
            float fRotationScalar = 3.0f;
            m_ChunkMesh[i].vRotationSpeed.x = RPercent() * fRotationScalar;
            m_ChunkMesh[i].vRotationSpeed.y = RPercent() * fRotationScalar;
            m_ChunkMesh[i].vRotationSpeed.z = RPercent() * fRotationScalar;
        }
    }
}

//--------------------------------------------------------------------------------------
void CBreakableWall::AdvancePieces( float fElapsedTime, D3DXVECTOR3 vGravity )
{
    if( !m_bBroken )
        return;

    for( UINT i = 0; i < NUM_CHUNKS; i++ )
    {
        if( m_ChunkMesh[i].bDynamic )
        {
            m_ChunkMesh[i].vVelocity += fElapsedTime * vGravity;
            m_ChunkMesh[i].vPosition += fElapsedTime * m_ChunkMesh[i].vVelocity;
            m_ChunkMesh[i].vRotation += fElapsedTime * m_ChunkMesh[i].vRotationSpeed;

            if( m_ChunkMesh[i].vPosition.y < -10.0f )
                m_ChunkMesh[i].bVisible = false;
        }
    }
}

//----------------------------------------------------
CBuilding::CBuilding()
{
    m_WidthWalls = 0;
    m_HeightWalls = 0;
    m_DepthWalls = 0;

    m_NumWalls = 0;
    m_pWallList = NULL;

    m_fWallScale = 1.0f;
}

//--------------------------------------------------------------------------------------
CBuilding::~CBuilding()
{
    SAFE_DELETE_ARRAY( m_pWallList );
}

//--------------------------------------------------------------------------------------
bool CBuilding::CreateBuilding( D3DXVECTOR3 vCenter, float fWallScale, UINT WidthWalls, UINT HeightWalls,
                                UINT DepthWalls )
{
    m_WidthWalls = WidthWalls;
    m_HeightWalls = HeightWalls;
    m_DepthWalls = DepthWalls;
    m_fWallScale = fWallScale;

    UINT NumNSWalls = WidthWalls * HeightWalls; // +z -z
    UINT NumEWWalls = DepthWalls * HeightWalls; // +x -x
    UINT NumRoofWalls = WidthWalls * DepthWalls;// +y

    m_NumWalls = NumNSWalls * 2 + NumEWWalls * 2 + NumRoofWalls;

    m_pWallList = new CBreakableWall[ m_NumWalls ];

    float f90Degrees = D3DX_PI / 2.0f;
    float f180Degrees = D3DX_PI;
    float fWallSize = 2.0f * m_fWallScale;

    float X, Y, Z;

    D3DXVECTOR3 vRotations( 0,0,0 );

    UINT index = 0;

    // North wall
    Z = ( fWallSize * m_DepthWalls ) / 2.0f;
    Y = fWallSize / 2.0f;
    for( UINT h = 0; h < m_HeightWalls; h++ )
    {
        X = -( fWallSize * m_WidthWalls - fWallSize ) / 2.0f;
        for( UINT w = 0; w < m_WidthWalls; w++ )
        {
            m_pWallList[index].SetPosition( D3DXVECTOR3( X, Y, Z ) + vCenter );
            m_pWallList[index].SetRotation( vRotations );
            index ++;

            X += fWallSize;
        }
        Y += fWallSize;
    }

    // South wall
    vRotations.y = f180Degrees;
    Z = -( fWallSize * m_DepthWalls ) / 2.0f;
    Y = fWallSize / 2.0f;
    for( UINT h = 0; h < m_HeightWalls; h++ )
    {
        X = -( fWallSize * m_WidthWalls - fWallSize ) / 2.0f;
        for( UINT w = 0; w < m_WidthWalls; w++ )
        {
            m_pWallList[index].SetPosition( D3DXVECTOR3( X, Y, Z ) + vCenter );
            m_pWallList[index].SetRotation( vRotations );
            index ++;

            X += fWallSize;
        }
        Y += fWallSize;
    }

    // East wall
    vRotations.y = -f90Degrees;
    X = ( fWallSize * m_WidthWalls ) / 2.0f;
    Y = fWallSize / 2.0f;
    for( UINT h = 0; h < m_HeightWalls; h++ )
    {
        Z = -( fWallSize * m_DepthWalls - fWallSize ) / 2.0f;
        for( UINT w = 0; w < m_DepthWalls; w++ )
        {
            m_pWallList[index].SetPosition( D3DXVECTOR3( X, Y, Z ) + vCenter );
            m_pWallList[index].SetRotation( vRotations );
            index ++;

            Z += fWallSize;
        }
        Y += fWallSize;
    }

    // West wall
    vRotations.y = f90Degrees;
    X = -( fWallSize * m_WidthWalls ) / 2.0f;
    Y = fWallSize / 2.0f;
    for( UINT h = 0; h < m_HeightWalls; h++ )
    {
        Z = -( fWallSize * m_DepthWalls - fWallSize ) / 2.0f;
        for( UINT w = 0; w < m_DepthWalls; w++ )
        {
            m_pWallList[index].SetPosition( D3DXVECTOR3( X, Y, Z ) + vCenter );
            m_pWallList[index].SetRotation( vRotations );
            index ++;

            Z += fWallSize;
        }
        Y += fWallSize;
    }

    // Roof wall
    vRotations.y = 0;
    vRotations.x = -f90Degrees;
    Y = ( fWallSize * m_HeightWalls );
    Z = -( fWallSize * m_DepthWalls - fWallSize ) / 2.0f;
    for( UINT h = 0; h < m_DepthWalls; h++ )
    {
        X = -( fWallSize * m_WidthWalls - fWallSize ) / 2.0f;
        for( UINT w = 0; w < m_WidthWalls; w++ )
        {
            m_pWallList[index].SetPosition( D3DXVECTOR3( X, Y, Z ) + vCenter );
            m_pWallList[index].SetRotation( vRotations );
            index ++;

            X += fWallSize;
        }
        Z += fWallSize;
    }

    // Bounding sphere
    m_BS.vCenter = vCenter;
    m_BS.vCenter.y += ( fWallSize * m_HeightWalls ) / 2.0f;

    D3DXVECTOR3 vCorner;
    vCorner.x = ( fWallSize * m_WidthWalls + fWallSize ) / 2.0f;
    vCorner.y = ( fWallSize * m_HeightWalls + fWallSize ) / 2.0f;
    vCorner.z = ( fWallSize * m_DepthWalls + fWallSize ) / 2.0f;

    m_BS.fRadius = D3DXVec3Length( &vCorner );

    return true;
}

//--------------------------------------------------------------------------------------
void CBuilding::SetBaseMesh( CDXUTSDKMesh* pMesh )
{
    for( UINT i = 0; i < m_NumWalls; i++ )
    {
        m_pWallList[i].SetBaseMesh( pMesh );
    }
}

//--------------------------------------------------------------------------------------
void CBuilding::SetChunkMesh( UINT iChunk, CDXUTSDKMesh* pMesh, D3DXVECTOR3 vOffset )
{
    for( UINT i = 0; i < m_NumWalls; i++ )
    {
        m_pWallList[i].SetChunkMesh( iChunk, pMesh, vOffset, m_fWallScale );
    }
}

//--------------------------------------------------------------------------------------
void CBuilding::Render( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, D3DXMATRIX* pmViewProj,
                        ID3D10EffectMatrixVariable* pWVP, ID3D10EffectMatrixVariable* pWorld )
{
    for( UINT i = 0; i < m_NumWalls; i++ )
    {
        m_pWallList[i].Render( pd3dDevice, pTechnique, m_fWallScale, pmViewProj, pWVP, pWorld );
    }
}

//--------------------------------------------------------------------------------------
void CBuilding::CollectBaseMeshMatrices( CGrowableArray <D3DXMATRIX>* pMatrixArray )
{
    for( UINT i = 0; i < m_NumWalls; i++ )
    {
        if( m_pWallList[i].IsBaseMeshVisible() )
        {
            D3DXMATRIX m = m_pWallList[i].GetBaseMeshMatrix( m_fWallScale );
            pMatrixArray->Add( m );
        }
    }
}

//--------------------------------------------------------------------------------------
void CBuilding::CollectChunkMeshMatrices( UINT iChunk, CGrowableArray <D3DXMATRIX>* pMatrixArray )
{
    for( UINT i = 0; i < m_NumWalls; i++ )
    {
        if( !m_pWallList[i].IsBaseMeshVisible() && m_pWallList[i].IsChunkMeshVisible( iChunk ) )
        {
            D3DXMATRIX m = m_pWallList[i].GetChunkMeshMatrix( iChunk, m_fWallScale );
            pMatrixArray->Add( m );
        }
    }
}

//--------------------------------------------------------------------------------------
void CBuilding::CreateExplosion( D3DXVECTOR3 vCenter, D3DXVECTOR3 vDirMul, float fRadius, float fMinPower,
                                 float fMaxPower )
{
    D3DXVECTOR3 vDelta = vCenter - m_BS.vCenter;
    float fDist = D3DXVec3LengthSq( &vDelta );

    float f2Rad = fRadius + m_BS.fRadius;
    if( fDist > f2Rad * f2Rad )
        return;

    for( UINT i = 0; i < m_NumWalls; i++ )
    {
        m_pWallList[i].CreateExplosion( vCenter, vDirMul, fRadius, fMinPower, fMaxPower, m_fWallScale );
    }
}

//--------------------------------------------------------------------------------------
void CBuilding::AdvancePieces( float fElapsedTime, D3DXVECTOR3 vGravity )
{
    for( UINT i = 0; i < m_NumWalls; i++ )
    {
        m_pWallList[i].AdvancePieces( fElapsedTime, vGravity );
    }
}

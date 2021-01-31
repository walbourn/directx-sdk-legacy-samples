//--------------------------------------------------------------------------------------
// Player.cpp
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Player.h"
#include "Terrain.h"
#include "Sim.h"

CPlayer::CPlayer() : m_pTerrain( NULL ),
                     m_fRadius( 0.0f ),
                     m_vDirection( 0,0,1 ),
                     m_vGravity( 0,-1.0f,0 ),
                     m_fDrag( 0.2f )
{
    m_vPosition[0] = m_vPosition[1] = D3DXVECTOR3( 0, 0, 0 );
    m_vVelocity[0] = m_vVelocity[1] = D3DXVECTOR3( 0, 0, 0 );
    m_vAcceleration[0] = m_vAcceleration[1] = D3DXVECTOR3( 0, 0, 0 );
}

CPlayer::~CPlayer()
{
}

void CPlayer::CollideWithPlayer( CPlayer* pPlayer )
{
    D3DXVECTOR3 vLocalPos = GetPosition();
    D3DXVECTOR3 vLocalVel = GetVelocity();
    float fLocalRad = GetRadius();

    D3DXVECTOR3 vPos = pPlayer->GetPosition();
    D3DXVECTOR3 vVel = pPlayer->GetVelocity();
    float fRad = pPlayer->GetRadius();

    // Find the normal of collision between the two balls
    D3DXVECTOR3 vNormal = vLocalPos - vPos;
    float length = D3DXVec3LengthSq( &vNormal );
    float fRadSq = fLocalRad + fRad;
    fRadSq *= fRadSq;
    if( length >= fRadSq )
        return;

    // NOW, do the sqrtf
    length = sqrtf( length );

    // normalize
    vNormal /= length;

    // Push the ball a little more than half way from the collision
    m_vPosition[1] += vNormal * ( ( fLocalRad + fRad ) - length ) / 1.99f;

    D3DXVECTOR3 Vab;
    Vab = vLocalVel - vVel;
    float j = -( 0.9f ) * D3DXVec3Dot( &Vab, &vNormal );

    m_vVelocity[1] = vLocalVel + j * vNormal;
}

void CPlayer::OnFrameMove( double fTime, float fElapsedTime, CSim* pSim )
{
    m_vPosition[1] = m_vPosition[0];
    m_vVelocity[1] = m_vVelocity[0];
    m_vAcceleration[1] = m_vAcceleration[0];

    // Collide with other balls
    UINT NumGrids = m_GridArray.GetSize();
    for( UINT i = 0; i < NumGrids; i++ )
    {
        GRID_CELL* pCell = pSim->GetGridCell( m_GridArray.GetAt( i ) );
        UINT NumPlayers = pCell->BallArray.GetSize();
        for( UINT b = 0; b < NumPlayers; b++ )
        {
            CPlayer* pPlayer = pSim->GetPlayer( pCell->BallArray.GetAt( b ) );
            if( pPlayer->GetID() != m_ID )
            {
                CollideWithPlayer( pPlayer );
            }
        }
    }

    // Euler integration for now
    m_vPosition[1] = m_vPosition[1] + m_vVelocity[1] * fElapsedTime;
    D3DXVECTOR3 accel = m_vAcceleration[0] + m_vGravity;
    m_vVelocity[1] = m_vVelocity[1] + accel * fElapsedTime;
    D3DXVECTOR3 vMotion;
    D3DXVec3Normalize( &vMotion, &m_vVelocity[1] );
    m_vAcceleration[1] = m_vAcceleration[1] - vMotion * m_fDrag;
    if( D3DXVec3Dot( &m_vAcceleration[1], &m_vVelocity[1] ) < 0 )
        m_vAcceleration[1] = D3DXVECTOR3( 0, 0, 0 );

    float fWorld = m_pTerrain->GetWorldScale() / 2.0f;
    if( m_vPosition[1].x < -fWorld )
    {
        m_vPosition[1].x = -fWorld;
        m_vVelocity[1].x = fabs( m_vVelocity[1].x );
    }
    if( m_vPosition[1].x > fWorld )
    {
        m_vPosition[1].x = fWorld;
        m_vVelocity[1].x = -fabs( m_vVelocity[1].x );
    }
    if( m_vPosition[1].z < -fWorld )
    {
        m_vPosition[1].z = -fWorld;
        m_vVelocity[1].z = fabs( m_vVelocity[1].z );
    }
    if( m_vPosition[1].z > fWorld )
    {
        m_vPosition[1].z = fWorld;
        m_vVelocity[1].z = -fabs( m_vVelocity[1].z );
    }

    // Perturb the velocity vector by the terrain
    float fHeight = m_pTerrain->GetHeightOnMap( &m_vPosition[1] );
    m_vPosition[1].y = fHeight + m_fRadius;
    D3DXVECTOR3 norm = m_pTerrain->GetNormalOnMap( &m_vPosition[1] );

    D3DXVECTOR3 right;
    D3DXVECTOR3 tan;
    D3DXVec3Cross( &right, &norm, &m_vVelocity[1] );
    D3DXVec3Cross( &tan, &right, &norm );
    D3DXVec3Normalize( &tan, &tan );
    float fMag = D3DXVec3Dot( &tan, &m_vVelocity[1] );
    m_vVelocity[1] = fMag * tan;
}

void CPlayer::UpdatePostPhysicsValues()
{
    m_vPosition[0] = m_vPosition[1];
    m_vVelocity[0] = m_vVelocity[1];
    m_vAcceleration[0] = m_vAcceleration[1];
}

void CPlayer::Turn( float fRadians )
{
    D3DXMATRIX mRot;
    D3DXMatrixRotationY( &mRot, fRadians );
    D3DXVECTOR3 vNewDir;
    D3DXVec3TransformNormal( &vNewDir, &m_vDirection, &mRot );
    D3DXVec3Normalize( &m_vDirection, &vNewDir );
}

#define MAX_VELOCITY 10.0f
void CPlayer::MoveForward( float fSpeed )
{
    m_vVelocity[0] += fSpeed * m_vDirection;
    float fMag = D3DXVec3Length( &m_vVelocity[0] );
    if( fMag > MAX_VELOCITY )
        m_vVelocity[0] *= MAX_VELOCITY / fMag;
}

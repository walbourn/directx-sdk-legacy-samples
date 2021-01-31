//--------------------------------------------------------------------------------------
// Player.h
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

class CTerrain;
class CSim;
class CPlayer
{
private:
    CTerrain* m_pTerrain;
    float m_fRadius;
    UINT m_ID;

    D3DXVECTOR3 m_vPosition[2];
    D3DXVECTOR3 m_vDirection;
    D3DXVECTOR3 m_vVelocity[2];
    D3DXVECTOR3 m_vAcceleration[2];
    D3DXVECTOR3 m_vGravity;
    float m_fDrag;

    CGrowableArray <UINT> m_GridArray;

public:
                CPlayer();
                ~CPlayer();

    void        SetTerrain( CTerrain* pTerrain )
    {
        m_pTerrain = pTerrain;
    }
    void        SetRadius( float fRadius )
    {
        m_fRadius = fRadius;
    }
    void        SetID( UINT id )
    {
        m_ID = id;
    }

    void        CollideWithPlayer( CPlayer* pPlayer );
    void        OnFrameMove( double fTime, float fElapsedTime, CSim* pSim );
    void        UpdatePostPhysicsValues();

    void        Turn( float fRadians );
    void        MoveForward( float fSpeed );

    void        SetPosition( D3DXVECTOR3* pPosition )
    {
        m_vPosition[0] = *pPosition;
    }
    void        SetDirection( D3DXVECTOR3* pDirection )
    {
        m_vDirection = *pDirection;
    }
    void        SetVelocity( D3DXVECTOR3* pVelocity )
    {
        m_vVelocity[0] = *pVelocity;
    }
    void        SetAcceleration( D3DXVECTOR3* pAcceleration )
    {
        m_vAcceleration[0] = *pAcceleration;
    }
    void        SetGravity( D3DXVECTOR3* pGravity )
    {
        m_vGravity = *pGravity;
    }
    void        SetDrag( float fDrag )
    {
        m_fDrag = fDrag;
    }
    D3DXVECTOR3 GetPosition()
    {
        return m_vPosition[0];
    }
    D3DXVECTOR3 GetDirection()
    {
        return m_vDirection;
    }
    D3DXVECTOR3 GetVelocity()
    {
        return m_vVelocity[0];
    }
    D3DXVECTOR3 GetAcceleration()
    {
        return m_vAcceleration[0];
    }
    D3DXVECTOR3 GetGravity()
    {
        return m_vGravity;
    }
    float       GetDrag()
    {
        return m_fDrag;
    }
    float       GetRadius()
    {
        return m_fRadius;
    }
    UINT        GetID()
    {
        return m_ID;
    }
    void        ResetGridArray()
    {
        m_GridArray.Reset();
    }
    void        AddGrid( UINT iGrid )
    {
        m_GridArray.Add( iGrid );
    }
};

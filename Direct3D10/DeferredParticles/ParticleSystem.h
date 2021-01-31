//--------------------------------------------------------------------------------------
// File: ParticleSystem.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

struct PARTICLE_VERTEX
{
    D3DXVECTOR3 vPos;
    D3DXVECTOR2 vUV;
    float fLife;
    float fRot;
    DWORD Color;
};

struct PARTICLE
{
    D3DXVECTOR3 vPos;
    D3DXVECTOR3 vDir;
    D3DXVECTOR3 vMass;
    DWORD Color;
    float fRadius;
    float fLife;
    float fFade;
    float fRot;
    float fRotRate;
    bool bVisible;
};

enum PARTICLE_SYSTEM_TYPE
{
    PST_DEFAULT = 0,
    PST_MUSHROOM,
    PST_STALK,
    PST_GROUNDEXP,
    PST_LANDMIND,
};

HRESULT CreateParticleArray( UINT MaxParticles );
void DestroyParticleArray();
void CopyParticlesToVertexBuffer( PARTICLE_VERTEX* pVB, D3DXVECTOR3 vEye, D3DXVECTOR3 vRight, D3DXVECTOR3 vUp );
float RPercent();
void InitParticleArray( UINT NumParticles );
UINT GetNumActiveParticles();

//-----------------------------------------------------------------------
class CParticleSystem
{
protected:
    UINT m_NumParticles;
    PARTICLE* m_pParticles;

    float m_fSpread;
    float m_fLifeSpan;
    float m_fStartSize;
    float m_fEndSize;
    float m_fSizeExponent;
    float m_fStartSpeed;
    float m_fEndSpeed;
    float m_fSpeedExponent;
    float m_fFadeExponent;
    float m_fRollAmount;
    float m_fWindFalloff;

    UINT m_NumStreamers;
    float m_fSpeedVariance;
    D3DXVECTOR3 m_vDirection;
    D3DXVECTOR3 m_vDirVariance;

    D3DXVECTOR4 m_vColor0;
    D3DXVECTOR4 m_vColor1;

    D3DXVECTOR3 m_vPosMul;
    D3DXVECTOR3 m_vDirMul;

    float m_fCurrentTime;
    D3DXVECTOR3 m_vCenter;
    float m_fStartTime;

    D3DXVECTOR4 m_vFlashColor;

    bool m_bStarted;

    PARTICLE_SYSTEM_TYPE m_PST;

public:
                    CParticleSystem();
    virtual         ~CParticleSystem();

    HRESULT         CreateParticleSystem( UINT NumParticles );
    void            SetSystemAttributes( D3DXVECTOR3 vCenter,
                                         float fSpread, float fLifeSpan, float fFadeExponent,
                                         float fStartSize, float fEndSize, float fSizeExponent,
                                         float fStartSpeed, float fEndSpeed, float fSpeedExponent,
                                         float fRollAmount, float fWindFalloff,
                                         UINT NumStreamers, float fSpeedVariance,
                                         D3DXVECTOR3 vDirection, D3DXVECTOR3 vDirVariance,
                                         D3DXVECTOR4 vColor0, D3DXVECTOR4 vColor1,
                                         D3DXVECTOR3 vPosMul, D3DXVECTOR3 vDirMul );

    void            SetCenter( D3DXVECTOR3 vCenter );
    void            SetStartTime( float fStartTime );
    void            SetStartSpeed( float fStartSpeed );
    void            SetFlashColor( D3DXVECTOR4 vFlashColor );
    D3DXVECTOR4     GetFlashColor();
    float           GetCurrentTime();
    float           GetLifeSpan();
    UINT            GetNumParticles();
    D3DXVECTOR3     GetCenter();

    virtual void    Init();
    virtual void    AdvanceSystem( float fTime, float fElapsedTime, D3DXVECTOR3 vRight, D3DXVECTOR3 vUp,
                                   D3DXVECTOR3 vWindVel, D3DXVECTOR3 vGravity );
};

//-----------------------------------------------------------------------
class CMushroomParticleSystem : public CParticleSystem
{
public:
                    CMushroomParticleSystem();
    virtual         ~CMushroomParticleSystem();

    virtual void    Init();
    virtual void    AdvanceSystem( float fTime, float fElapsedTime, D3DXVECTOR3 vRight, D3DXVECTOR3 vUp,
                                   D3DXVECTOR3 vWindVel, D3DXVECTOR3 vGravity );
};

//-----------------------------------------------------------------------
class CStalkParticleSystem : public CParticleSystem
{
public:
                    CStalkParticleSystem();
    virtual         ~CStalkParticleSystem();

    virtual void    Init();
    virtual void    AdvanceSystem( float fTime, float fElapsedTime, D3DXVECTOR3 vRight, D3DXVECTOR3 vUp,
                                   D3DXVECTOR3 vWindVel, D3DXVECTOR3 vGravity );
};

//-----------------------------------------------------------------------
class CGroundBurstParticleSystem : public CParticleSystem
{
public:
                    CGroundBurstParticleSystem();
    virtual         ~CGroundBurstParticleSystem();

    virtual void    Init();
    virtual void    AdvanceSystem( float fTime, float fElapsedTime, D3DXVECTOR3 vRight, D3DXVECTOR3 vUp,
                                   D3DXVECTOR3 vWindVel, D3DXVECTOR3 vGravity );
};

//-----------------------------------------------------------------------
class CLandMineParticleSystem : public CParticleSystem
{
public:
                    CLandMineParticleSystem();
    virtual         ~CLandMineParticleSystem();

    virtual void    Init();
    virtual void    AdvanceSystem( float fTime, float fElapsedTime, D3DXVECTOR3 vRight, D3DXVECTOR3 vUp,
                                   D3DXVECTOR3 vWindVel, D3DXVECTOR3 vGravity );
};


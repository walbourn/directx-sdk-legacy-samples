//-----------------------------------------------------------------------------
// File: Tiny.cpp
//
// Desc: Defines the character class, CTiny, for the MultipleAnimation sample.
//       The class does some basic things to make the character aware of
//       its surroundings, as well as implements all actions and movements.
//       CTiny shows a full-featured example of this.  These
//       classes use the MultiAnimation class library to control the
//       animations.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#pragma warning(disable: 4995)
#include "MultiAnimation.h"
#include "Tiny.h"
#pragma warning(default: 4995)

using namespace std;


CSound* g_apSoundsTiny[ 2 ];




//-----------------------------------------------------------------------------
// Name: CTiny::CTiny
// Desc: Constructor for CTiny
//-----------------------------------------------------------------------------
CTiny::CTiny() : m_pMA( NULL ),
                 m_dwMultiAnimIdx( 0 ),
                 m_pAI( NULL ),
                 m_pv_pChars( NULL ),
                 m_pSM( NULL ),

                 m_dTimePrev( 0.0 ),
                 m_dTimeCurrent( 0.0 ),
                 m_bUserControl( false ),
                 m_bPlaySounds( true ),
                 m_dwCurrentTrack( 0 ),

                 m_fSpeed( 0.f ),
                 m_fSpeedTurn( 0.f ),
                 m_pCallbackHandler( NULL ),
                 m_fPersonalRadius( 0.f ),

                 m_fSpeedWalk( 1.f / 5.7f ),
                 m_fSpeedJog( 1.f / 2.3f ),

                 m_bIdle( false ),
                 m_bWaiting( false ),
                 m_dTimeIdling( 0.0 ),
                 m_dTimeWaiting( 0.0 ),
                 m_dTimeBlocked( 0.0 )
{
    D3DXMatrixIdentity( &m_mxOrientation );

    m_fSpeedTurn = 1.3f;
    m_pCallbackHandler = NULL;
    m_fPersonalRadius = .035f;

    m_szASName[0] = '\0';
}




//-----------------------------------------------------------------------------
// Name: CTiny::~CTiny
// Desc: Destructor for CTiny
//-----------------------------------------------------------------------------
CTiny::~CTiny()
{
    Cleanup();
}




//-----------------------------------------------------------------------------
// Name: CTiny::Setup
// Desc: Initializes the class and readies it for animation
//-----------------------------------------------------------------------------
HRESULT CTiny::Setup( CMultiAnim* pMA,
                      vector <CTiny*>* pv_pChars,
                      CSoundManager* pSM,
                      double dTimeCurrent )
{
    m_pMA = pMA;
    m_pv_pChars = pv_pChars;
    m_pSM = pSM;

    m_dTimeCurrent = m_dTimePrev = dTimeCurrent;

    HRESULT hr;
    hr = m_pMA->CreateNewInstance( &m_dwMultiAnimIdx );
    if( FAILED( hr ) )
        return E_OUTOFMEMORY;

    m_pAI = m_pMA->GetInstance( m_dwMultiAnimIdx );

    // set initial position
    bool bBlocked = true;
    DWORD dwAttempts;
    for( dwAttempts = 0; dwAttempts < 1000 && bBlocked; ++ dwAttempts )
    {
        ChooseNewLocation( &m_vPos );
        bBlocked = IsBlockedByCharacter( &m_vPos );
    }

    m_fFacing = 0.0f;

    // set up anim indices
    m_dwAnimIdxLoiter = GetAnimIndex( "Loiter" );
    m_dwAnimIdxWalk = GetAnimIndex( "Walk" );
    m_dwAnimIdxJog = GetAnimIndex( "Jog" );
    if( m_dwAnimIdxLoiter == ANIMINDEX_FAIL ||
        m_dwAnimIdxWalk == ANIMINDEX_FAIL ||
        m_dwAnimIdxJog == ANIMINDEX_FAIL )
        return E_FAIL;

    // set up callback key data
    m_CallbackData[ 0 ].m_dwFoot = 0;
    m_CallbackData[ 0 ].m_pvTinyPos = &m_vPos;
    m_CallbackData[ 1 ].m_dwFoot = 1;
    m_CallbackData[ 1 ].m_pvTinyPos = &m_vPos;

    // set up footstep callbacks
    SetupCallbacksAndCompression();
    m_pCallbackHandler = new CBHandlerTiny;
    if( m_pCallbackHandler == NULL )
        return E_OUTOFMEMORY;

    // set up footstep sounds
    WCHAR sPath[ MAX_PATH ];
    if( g_apSoundsTiny[ 0 ] == NULL )
    {
        hr = DXUTFindDXSDKMediaFileCch( sPath, MAX_PATH, FOOTFALLSOUND00 );
        if( FAILED( hr ) )
            wcscpy_s( sPath, MAX_PATH, FOOTFALLSOUND00 );

        hr = m_pSM->Create( &g_apSoundsTiny[ 0 ], sPath, DSBCAPS_CTRLVOLUME );
        if( FAILED( hr ) )
        {
            OutputDebugString( FOOTFALLSOUND00 L" not found; continuing without sound.\n" );
            m_bPlaySounds = false;
        }
    }

    if( g_apSoundsTiny[ 1 ] == NULL )
    {
        hr = DXUTFindDXSDKMediaFileCch( sPath, MAX_PATH, FOOTFALLSOUND01 );
        if( FAILED( hr ) )
            wcscpy_s( sPath, MAX_PATH, FOOTFALLSOUND01 );

        hr = m_pSM->Create( &g_apSoundsTiny[ 1 ], sPath, DSBCAPS_CTRLVOLUME );
        if( FAILED( hr ) )
        {
            OutputDebugString( FOOTFALLSOUND01 L" not found; continuing without sound.\n" );
            m_bPlaySounds = false;
        }
    }

    // compute reorientation matrix based on default orientation and bounding radius
    D3DXMATRIX mx;
    float fScale = 1.f / m_pMA->GetBoundingRadius() / 7.f;
    D3DXMatrixScaling( &mx, fScale, fScale, fScale );
    m_mxOrientation = mx;
    D3DXMatrixRotationX( &mx, -D3DX_PI / 2.0f );
    D3DXMatrixMultiply( &m_mxOrientation, &m_mxOrientation, &mx );
    D3DXMatrixRotationY( &mx, D3DX_PI / 2.0f );
    D3DXMatrixMultiply( &m_mxOrientation, &m_mxOrientation, &mx );

    // set starting target
    SetSeekingState();
    ComputeFacingTarget();

    LPD3DXANIMATIONCONTROLLER pAC;
    m_pAI->GetAnimController( &pAC );
    pAC->AdvanceTime( m_dTimeCurrent, NULL );
    pAC->Release();

    // Add this instance to the list
    try
    {
        pv_pChars->push_back( this );
    }
    catch( ... )
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CTiny::Cleanup()
// Desc: Performs cleanup tasks for CTiny
//-----------------------------------------------------------------------------
void CTiny::Cleanup()
{
    delete m_pCallbackHandler; m_pCallbackHandler = NULL;
}




//-----------------------------------------------------------------------------
// Name: CTiny::GetAnimInstance()
// Desc: Returns the CAnimInstance object that this instance of CTiny
//       embeds.
//-----------------------------------------------------------------------------
CAnimInstance* CTiny::GetAnimInstance()
{
    return m_pAI;
}




//-----------------------------------------------------------------------------
// Name: CTiny::GetPosition()
// Desc: Returns the position of this instance.
//-----------------------------------------------------------------------------
void CTiny::GetPosition( D3DXVECTOR3* pV )
{
    *pV = m_vPos;
}




//-----------------------------------------------------------------------------
// Name: CTiny::GetFacing()
// Desc: Returns a unit vector representing the direction this CTiny
//       instance is facing.
//-----------------------------------------------------------------------------
void CTiny::GetFacing( D3DXVECTOR3* pV )
{
    D3DXMATRIX m;
    *pV = D3DXVECTOR3( 1.0f, 0.0f, 0.0f );
    D3DXVec3TransformCoord( pV, pV, D3DXMatrixRotationY( &m, -m_fFacing ) );
}




//-----------------------------------------------------------------------------
// Name: CTiny::Animate()
// Desc: Advances the local time by dTimeDelta. Determine an action for Tiny,
//       then update the animation controller's tracks to reflect the action.
//-----------------------------------------------------------------------------
void CTiny::Animate( double dTimeDelta )
{
    // adjust position and facing based on movement mode
    if( m_bUserControl )
        AnimateUserControl( dTimeDelta );  // user-controlled
    else if( m_bIdle )
        AnimateIdle( dTimeDelta );         // idling - not turning toward
    else
        AnimateMoving( dTimeDelta );       // moving or waiting - turning toward

    // loop the loiter animation back on itself to avoid the end-to-end jerk
    SmoothLoiter();

    D3DXMATRIX mxWorld, mx;

    // compute world matrix based on pos/face
    D3DXMatrixRotationY( &mxWorld, -m_fFacing );
    D3DXMatrixTranslation( &mx, m_vPos.x, m_vPos.y, m_vPos.z );
    D3DXMatrixMultiply( &mxWorld, &mxWorld, &mx );
    D3DXMatrixMultiply( &mxWorld, &m_mxOrientation, &mxWorld );
    m_pAI->SetWorldTransform( &mxWorld );
}




//-----------------------------------------------------------------------------
// Name: CTiny::ResetTime()
// Desc: Resets the local time for this CTiny instance.
//-----------------------------------------------------------------------------
HRESULT CTiny::ResetTime()
{
    m_dTimeCurrent = m_dTimePrev = 0.0;
    return m_pAI->ResetTime();
}




//-----------------------------------------------------------------------------
// Name: CTiny::AdvanceTime()
// Desc: Advances the local animation time by dTimeDelta, and call
//       CAnimInstance to set up its frames to reflect the time advancement.
//-----------------------------------------------------------------------------
HRESULT CTiny::AdvanceTime( double dTimeDelta, D3DXVECTOR3* pvEye )
{
    // if we're playing sounds, set the sound source position
    if( m_bPlaySounds )
    {
        m_CallbackData[ 0 ].m_pvCameraPos = pvEye;
        m_CallbackData[ 1 ].m_pvCameraPos = pvEye;
    }
    else    // else, set it to null to let the handler know to be quiet
    {
        m_CallbackData[ 0 ].m_pvCameraPos = NULL;
        m_CallbackData[ 1 ].m_pvCameraPos = NULL;
    }

    m_dTimePrev = m_dTimeCurrent;
    m_dTimeCurrent += dTimeDelta;
    return m_pAI->AdvanceTime( dTimeDelta, m_pCallbackHandler );
}




//-----------------------------------------------------------------------------
// Name: CTiny::Draw()
// Desc: Renders this CTiny instace using the current animation frames.
//-----------------------------------------------------------------------------
HRESULT CTiny::Draw()
{
    return m_pAI->Draw();
}




//-----------------------------------------------------------------------------
// Name: CTiny::Report()
// Desc: Add to the vector of strings, v_sReport, with useful information
//       about this instance of CTiny.
//-----------------------------------------------------------------------------
void CTiny::Report( vector <String>& v_sReport )
{
    WCHAR s[ 256 ];

    try
    {
        swprintf_s( s, 256, L"Pos: %f, %f", m_vPos.x, m_vPos.z );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"Facing: %f", m_fFacing );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"Local time: %f", m_dTimeCurrent );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"Pos Target: %f, %f", m_vPosTarget.x, m_vPosTarget.z );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"Facing Target: %f", m_fFacingTarget );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"Status: %s", m_bIdle ? L"Idle" : ( m_bWaiting ? L"Waiting" : L"Moving" ) );
        v_sReport.push_back( String( s ) );

        // report track data
        LPD3DXANIMATIONCONTROLLER pAC;
        LPD3DXANIMATIONSET pAS;
        D3DXTRACK_DESC td;
        m_pAI->GetAnimController( &pAC );

        pAC->GetTrackAnimationSet( 0, &pAS );
        WCHAR wstr[256];
        MultiByteToWideChar( CP_ACP, 0, pAS->GetName(), -1, wstr, 256 );
        swprintf_s( s, 256, L"Track 0: %s%s", wstr, m_dwCurrentTrack == 0 ? L" (current)" : L"" );
        v_sReport.push_back( String( s ) );
        pAS->Release();

        pAC->GetTrackDesc( 0, &td );
        swprintf_s( s, 256, L"  Weight: %f", td.Weight );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"  Speed: %f", td.Speed );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"  Position: %f", td.Position );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"  Enable: %s", td.Enable ? L"true" : L"false" );
        v_sReport.push_back( String( s ) );

        pAC->GetTrackAnimationSet( 1, &pAS );
        if( pAS )
        {
            MultiByteToWideChar( CP_ACP, 0, pAS->GetName(), -1, wstr, 256 );
            pAS->Release();
        }
        else
        {
            swprintf_s( wstr, 256, L"n/a" );
        }
        swprintf_s( s, 256, L"Track 1: %s%s", wstr, m_dwCurrentTrack == 1 ? L" (current)" : L"" );
        v_sReport.push_back( String( s ) );

        pAC->GetTrackDesc( 1, &td );
        swprintf_s( s, 256, L"  Weight: %f", td.Weight );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"  Speed: %f", td.Speed );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"  Position: %f", td.Position );
        v_sReport.push_back( String( s ) );
        swprintf_s( s, 256, L"  Enable: %s", td.Enable ? L"true" : L"false" );
        v_sReport.push_back( String( s ) );

        if( m_bUserControl )
        {
            swprintf_s( s, 256, L"Control: USER" );
            v_sReport.push_back( String( s ) );
        }
        else
        {
            swprintf_s( s, 256, L"Control: AUTO" );
            v_sReport.push_back( String( s ) );
        }

        pAC->Release();
    }
    catch( ... )
    {
    }
}




//-----------------------------------------------------------------------------
// Specifies that this instance of CTiny is controlled by the user.
void CTiny::SetUserControl()
{
    m_bUserControl = true;
}


//-----------------------------------------------------------------------------
// Specifies that this instance of CTiny is controlled by the application.
void CTiny::SetAutoControl()
{
    if( m_bUserControl )
    {
        m_bUserControl = false;
        SetSeekingState();
    }
}


//-----------------------------------------------------------------------------
// Name: CTiny::SetSounds()
// Desc: Enables or disables the sound support for this instance of CTiny.
//       In this case, whether we hear the footstep sound or not.
//-----------------------------------------------------------------------------
void CTiny::SetSounds( bool bSounds )
{
    m_bPlaySounds = bSounds;
}




//-----------------------------------------------------------------------------
// Name: CTiny::ChooseNewLocation()
// Desc: Determine a new location for this character to move to.  In this case
//       we simply randomly pick a spot on the floor as the new location.
//-----------------------------------------------------------------------------
void CTiny::ChooseNewLocation( D3DXVECTOR3* pV )
{
    pV->x = ( float )( rand() % 256 ) / 256.f;
    pV->y = 0.f;
    pV->z = ( float )( rand() % 256 ) / 256.f;
}




//-----------------------------------------------------------------------------
// Name: CTiny::IsBlockedByCharacter()
// Desc: Goes through the character list nad returns true if this instance is
//       blocked by any other such that it cannot move, or false otherwise.
//-----------------------------------------------------------------------------
bool CTiny::IsBlockedByCharacter( D3DXVECTOR3* pV )
{
    D3DXVECTOR3 vSub;

    // move through each character to see if it blocks this
    vector <CTiny*>::iterator itCur, itEnd = m_pv_pChars->end();
    for( itCur = m_pv_pChars->begin(); itCur != itEnd; ++ itCur )
    {
        CTiny* pChar = *itCur;
        if( pChar == this )      // don't test against ourselves
            continue;

        D3DXVECTOR3 vSub;
        D3DXVec3Subtract( &vSub, &pChar->m_vPos, pV );
        float fRadiiSq = pChar->m_fPersonalRadius + m_fPersonalRadius;
        fRadiiSq *= fRadiiSq;

        // test distance
        if( D3DXVec3LengthSq( &vSub ) < fRadiiSq )
        {
            return true;
        }
    }

    return false;
}




//-----------------------------------------------------------------------------
// Name: CTiny::IsOutOfBounds()
// Desc: Checks this instance against its bounds.  Returns true if it has moved
//       outside the boundaries, or false if not.  In this case, we check if
//       x and z are within the required range (0 to 1).
//-----------------------------------------------------------------------------
bool CTiny::IsOutOfBounds( D3DXVECTOR3* pV )
{
    if( pV->x < 0.0 || pV->x > 1.0 || pV->z < 0.0 || pV->z > 1.0 )
        return true;
    else
        return false;
}




//-----------------------------------------------------------------------------
// Name: CTiny::GetAnimIndex()
// Desc: Returns the index of an animation set within this animation instance's
//       animation controller given an animation set name.
//-----------------------------------------------------------------------------
DWORD CTiny::GetAnimIndex( char sString[] )
{
    HRESULT hr;
    LPD3DXANIMATIONCONTROLLER pAC;
    LPD3DXANIMATIONSET pAS;
    DWORD dwRet = ANIMINDEX_FAIL;

    m_pAI->GetAnimController( &pAC );

    for( DWORD i = 0; i < pAC->GetNumAnimationSets(); ++ i )
    {
        hr = pAC->GetAnimationSet( i, &pAS );
        if( FAILED( hr ) )
            continue;

        if( pAS->GetName() &&
            !strncmp( pAS->GetName(), sString, min( strlen( pAS->GetName() ), strlen( sString ) ) ) )
        {
            dwRet = i;
            pAS->Release();
            break;
        }

        pAS->Release();
    }

    pAC->Release();

    return dwRet;
}




//-----------------------------------------------------------------------------
// Name: CTiny::AddCallbackKeysAndCompress()
// Desc: Replaces an animation set in the animation controller with the
//       compressed version and callback keys added to it.
//-----------------------------------------------------------------------------
HRESULT CTiny::AddCallbackKeysAndCompress( LPD3DXANIMATIONCONTROLLER pAC,
                                           LPD3DXKEYFRAMEDANIMATIONSET pAS,
                                           DWORD dwNumCallbackKeys,
                                           D3DXKEY_CALLBACK aKeys[],
                                           DWORD dwCompressionFlags,
                                           FLOAT fCompression )
{
    HRESULT hr;
    LPD3DXCOMPRESSEDANIMATIONSET pASNew = NULL;
    LPD3DXBUFFER pBufCompressed = NULL;

    hr = pAS->Compress( dwCompressionFlags, fCompression, NULL, &pBufCompressed );
    if( FAILED( hr ) )
        goto e_Exit;

    hr = D3DXCreateCompressedAnimationSet( pAS->GetName(),
                                           pAS->GetSourceTicksPerSecond(),
                                           pAS->GetPlaybackType(),
                                           pBufCompressed,
                                           dwNumCallbackKeys,
                                           aKeys,
                                           &pASNew );
    pBufCompressed->Release();

    if( FAILED( hr ) )
        goto e_Exit;

    pAC->UnregisterAnimationSet( pAS );
    pAS->Release();

    hr = pAC->RegisterAnimationSet( pASNew );
    if( FAILED( hr ) )
        goto e_Exit;

    pASNew->Release();
    pASNew = NULL;


e_Exit:

    if( pASNew )
        pASNew->Release();

    return hr;
}




//-----------------------------------------------------------------------------
// Name: CTiny::SetupCallbacksAndCompression()
// Desc: Add callback keys to the walking and jogging animation sets in the
//       animation controller for playing footstepping sound.  Then compress
//       all animation sets in the animation controller.
//-----------------------------------------------------------------------------
HRESULT CTiny::SetupCallbacksAndCompression()
{
    LPD3DXANIMATIONCONTROLLER pAC;
    LPD3DXKEYFRAMEDANIMATIONSET pASLoiter, pASWalk, pASJog;

    m_pAI->GetAnimController( &pAC );
    pAC->GetAnimationSet( m_dwAnimIdxLoiter, ( LPD3DXANIMATIONSET* )&pASLoiter );
    pAC->GetAnimationSet( m_dwAnimIdxWalk, ( LPD3DXANIMATIONSET* )&pASWalk );
    pAC->GetAnimationSet( m_dwAnimIdxJog, ( LPD3DXANIMATIONSET* )&pASJog );

    D3DXKEY_CALLBACK aKeysWalk[ 2 ];
    aKeysWalk[ 0 ].Time = 0;
    aKeysWalk[ 0 ].pCallbackData = &m_CallbackData[ 0 ];
    aKeysWalk[ 1 ].Time = float( pASWalk->GetPeriod() / 2.0 * pASWalk->GetSourceTicksPerSecond() );
    aKeysWalk[ 1 ].pCallbackData = &m_CallbackData[ 1 ];

    D3DXKEY_CALLBACK aKeysJog[ 8 ];
    for( int i = 0; i < 8; ++ i )
    {
        aKeysJog[ i ].Time = float( pASJog->GetPeriod() / 8 * ( double )i * pASWalk->GetSourceTicksPerSecond() );
        aKeysJog[ i ].pCallbackData = &m_CallbackData[ ( i + 1 ) % 2 ];
    }

    AddCallbackKeysAndCompress( pAC, pASLoiter, 0, NULL, D3DXCOMPRESS_DEFAULT, .8f );
    AddCallbackKeysAndCompress( pAC, pASWalk, 2, aKeysWalk, D3DXCOMPRESS_DEFAULT, .4f );
    AddCallbackKeysAndCompress( pAC, pASJog, 8, aKeysJog, D3DXCOMPRESS_DEFAULT, .25f );

    m_dwAnimIdxLoiter = GetAnimIndex( "Loiter" );
    m_dwAnimIdxWalk = GetAnimIndex( "Walk" );
    m_dwAnimIdxJog = GetAnimIndex( "Jog" );
    if( m_dwAnimIdxLoiter == ANIMINDEX_FAIL ||
        m_dwAnimIdxWalk == ANIMINDEX_FAIL ||
        m_dwAnimIdxJog == ANIMINDEX_FAIL )
        return E_FAIL;

    pAC->Release();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CTiny::SmoothLoiter()
// Desc: If Tiny is loitering, check if we have reached the end of animation.
//       If so, set up a new track to play Loiter animation from the start and
//       smoothly transition to the track, so that Tiny can loiter more.
//-----------------------------------------------------------------------------
void CTiny::SmoothLoiter()
{
    LPD3DXANIMATIONCONTROLLER pAC;
    LPD3DXANIMATIONSET pASTrack, pASLoiter;
    m_pAI->GetAnimController( &pAC );

    // check if we're loitering
    pAC->GetTrackAnimationSet( m_dwCurrentTrack, &pASTrack );
    pAC->GetAnimationSet( m_dwAnimIdxLoiter, &pASLoiter );
    if( pASTrack && pASTrack == pASLoiter )
    {
        D3DXTRACK_DESC td;
        pAC->GetTrackDesc( m_dwCurrentTrack, &td );
        if( td.Position > pASTrack->GetPeriod() - IDLE_TRANSITION_TIME )  // come within the change delta of the end
            SetIdleKey( true );
    }

    SAFE_RELEASE( pASTrack );
    SAFE_RELEASE( pASLoiter );
    SAFE_RELEASE( pAC );
}




//-----------------------------------------------------------------------------
// Name: CTiny::SetNewTarget()
// Desc: This only applies when Tiny is under automatic movement control.  If
//       this instnace of Tiny is blocked by either the edge of another
//       instance of Tiny, find a new location to walk to.
//-----------------------------------------------------------------------------
void CTiny::SetNewTarget()
{
    // get new position
    bool bBlocked = true;
    DWORD dwAttempts;
    for( dwAttempts = 0; dwAttempts < 1000 && bBlocked; ++ dwAttempts )
    {
        ChooseNewLocation( &m_vPosTarget );
        bBlocked = IsBlockedByCharacter( &m_vPosTarget );
    }

    ComputeFacingTarget();
}




//-----------------------------------------------------------------------------
// Name: CTiny::GetSpeedScale()
// Desc: Returns the speed of the current track.
//-----------------------------------------------------------------------------
double CTiny::GetSpeedScale()
{
    LPD3DXANIMATIONCONTROLLER pAC;
    D3DXTRACK_DESC td;

    if( m_bIdle )
        return 1.0;
    else
    {
        m_pAI->GetAnimController( &pAC );
        pAC->GetTrackDesc( m_dwCurrentTrack, &td );
        pAC->Release();

        return td.Speed;
    }
}




//-----------------------------------------------------------------------------
// Name: CTiny::AnimateUserControl()
// Desc: Reads user input and update Tiny's state and animation accordingly.
//-----------------------------------------------------------------------------
void CTiny::AnimateUserControl( double dTimeDelta )
{
    // use keyboard controls to make Tiny move

    bool bCanMove;

    if( GetKeyState( 'V' ) < 0 )
    {
        m_bUserControl = false;
        SetSeekingState();
        return;
    }

    if( GetKeyState( 'W' ) < 0 )
    {
        bCanMove = true;

        if( GetAsyncKeyState( VK_SHIFT ) < 0 )
        {
            if( m_fSpeed == m_fSpeedWalk )
            {
                m_fSpeed = m_fSpeedJog;
                m_bIdle = true;  // Set idle to true so that we can reset the movement animation below
            }
        }
        else
        {
            if( m_fSpeed == m_fSpeedJog )
            {
                m_fSpeed = m_fSpeedWalk;
                m_bIdle = true;  // Set idle to true so that we can reset the movement animation below
            }
        }

        D3DXVECTOR3 vMovePos;
        GetFacing( &vMovePos );
        D3DXVec3Scale( &vMovePos, &vMovePos, float( m_fSpeed * GetSpeedScale() * dTimeDelta ) );
        D3DXVec3Add( &vMovePos, &vMovePos, &m_vPos );

        // is our step ahead going to take us out of bounds?
        if( IsOutOfBounds( &vMovePos ) )
            bCanMove = false;

        // are we stepping on someone else?
        if( IsBlockedByCharacter( &vMovePos ) )
            bCanMove = false;

        if( bCanMove )
            m_vPos = vMovePos;
    }
    else
        bCanMove = false;


    if( m_bIdle && bCanMove )
    {
        SetMoveKey();
        m_bIdle = false;
    }

    if( !m_bIdle && !bCanMove )
    {
        SetIdleKey( true );
        m_bIdle = true;
    }

    // turn
    if( GetKeyState( 'A' ) < 0 )
        m_fFacing = float( m_fFacing + m_fSpeedTurn * dTimeDelta );

    if( GetKeyState( 'D' ) < 0 )
        m_fFacing = float( m_fFacing - m_fSpeedTurn * dTimeDelta );
}




//-----------------------------------------------------------------------------
// Name: CTiny::AnimateIdle()
// Desc: Checks if Tiny has been idle for long enough.  If so, initialize Tiny
//       to move again to a new location.
//-----------------------------------------------------------------------------
void CTiny::AnimateIdle( double dTimeDelta )
{
    // count down the idle counters
    if( m_dTimeIdling > 0.0 )
        m_dTimeIdling -= dTimeDelta;

    // if idle time runs out, pick a new location
    if( m_dTimeIdling <= 0.0 )
        SetSeekingState();
}




//-----------------------------------------------------------------------------
// Name: CTiny::AnimateMoving()
// Desc: Here we try to figure out if we're moving and can keep moving, 
//       or if we're waiting / blocked and must keep waiting / blocked,
//       or if we have reached our destination.
//-----------------------------------------------------------------------------
void CTiny::AnimateMoving( double dTimeDelta )
{
    // move, then turn
    D3DXVECTOR3 vMovePos;
    D3DXVECTOR3 vSub;
    float fDist;

    if( m_bWaiting )
    {
        if( m_dTimeWaiting > 0.0 )
            m_dTimeWaiting -= dTimeDelta;

        if( m_dTimeWaiting <= 0.0 )
        {
            SetNewTarget();
            m_dTimeWaiting = 4.0;
        }
    }

    // get distance from target
    D3DXVec3Subtract( &vSub, &m_vPos, &m_vPosTarget );
    fDist = D3DXVec3LengthSq( &vSub );
    double dSpeedScale = GetSpeedScale();

    if( m_dTimeBlocked > 0.0 )                  // if we're supposed to wait, then turn only
    {
        m_dTimeBlocked -= dTimeDelta;
    }
        // TODO: help next line
    else if( m_fSpeed * dSpeedScale * dTimeDelta * m_fSpeed * dSpeedScale * dTimeDelta >= fDist )
    {
        // we're within reach; set the exact point
        m_vPos = m_vPosTarget;
        SetIdleState();
    }
    else
    {
        // moving forward
        GetFacing( &vMovePos );
        D3DXVec3Scale( &vMovePos, &vMovePos, float( m_fSpeed * dSpeedScale * dTimeDelta ) );
        D3DXVec3Add( &vMovePos, &vMovePos, &m_vPos );

        bool bCanMove = true;
        bool bOrbit = false;

        // is our step ahead going to take us out of bounds?
        if( IsOutOfBounds( &vMovePos ) )
            bCanMove = false;

        // are we stepping on someone else?
        if( IsBlockedByCharacter( &vMovePos ) )
            bCanMove = false;

        // are we orbiting our target?
        if( ( m_fFacing != m_fFacingTarget ) &&
            ( fDist <= ( ( m_fSpeed * m_fSpeed ) / ( m_fSpeedTurn * m_fSpeedTurn ) ) ) )
        {
            bOrbit = true;
            bCanMove = false;
        }

        // set keys if we have to
        if( bCanMove && m_bWaiting )
        {
            SetMoveKey();
            m_bWaiting = false;
        }

        if( !bCanMove && !m_bWaiting )
        {
            SetIdleKey( false );
            m_bWaiting = true;
            if( !bOrbit )
                m_dTimeBlocked = 1.0;
        }

        if( bCanMove )
            m_vPos = vMovePos;
    }

    // turning
    if( m_fFacingTarget != m_fFacing )
    {
        float fFacing = m_fFacingTarget;
        if( m_fFacingTarget > m_fFacing )
            fFacing -= 2 * D3DX_PI;

        float fDiff = m_fFacing - fFacing;
        if( fDiff < D3DX_PI )       // cw turn
        {
            // if we're overturning
            if( m_fFacing - m_fSpeedTurn * dTimeDelta <= fFacing )
                m_fFacing = m_fFacingTarget;
            else
                m_fFacing = float( m_fFacing - m_fSpeedTurn * dTimeDelta );
        }
        else                        // ccw turn
        {
            // if we're overturning
            if( m_fFacing + m_fSpeedTurn * dTimeDelta - 2 * D3DX_PI >= fFacing )
                m_fFacing = m_fFacingTarget;
            else
                m_fFacing = float( m_fFacing + m_fSpeedTurn * dTimeDelta );
        }

        ComputeFacingTarget();
    }
}




//-----------------------------------------------------------------------------
// Name: CTiny::ComputeFacingTarget()
// Desc: Computes the direction in forms of both an angle and a vector that
//       Tiny is facing.
//-----------------------------------------------------------------------------
void CTiny::ComputeFacingTarget()
{
    D3DXVECTOR3 vDiff;
    D3DXVec3Subtract( &vDiff, &m_vPosTarget, &m_vPos );
    D3DXVec3Normalize( &vDiff, &vDiff );

    if( vDiff.z == 0.f )
    {
        if( vDiff.x > 0.f )
            m_fFacingTarget = 0.0f;
        else
            m_fFacingTarget = D3DX_PI;
    }
    else if( vDiff.z > 0.f )
        m_fFacingTarget = acosf( vDiff.x );
    else
        m_fFacingTarget = acosf( - vDiff.x ) + D3DX_PI;
}




//-----------------------------------------------------------------------------
// Name: CTiny::SetIdleState()
// Desc: This only applies when Tiny is not controlled by the user.  Called
//       when Tiny has just reached a destination.  We let Tiny remain idle
//       for a period of time before a new action is taken.
//-----------------------------------------------------------------------------
void CTiny::SetIdleState()
{
    m_bIdle = true;
    m_dTimeIdling = 4.0;

    SetIdleKey( false );
}




//-----------------------------------------------------------------------------
// Name: CTiny::SetSeekingState()
// Desc: Used when the computer controls Tiny.  Find a new location to move
//       Tiny to, then set the animation controller's tracks to reflect the
//       change of action.
//-----------------------------------------------------------------------------
void CTiny::SetSeekingState()
{
    m_bIdle = false;
    m_bWaiting = false;

    SetNewTarget();

    if( rand() % 5 )
        m_fSpeed = m_fSpeedWalk;
    else
        m_fSpeed = m_fSpeedJog;

    SetMoveKey();
}




//-----------------------------------------------------------------------------
// Name: CTiny::SetMoveKey()
// Desc: Initialize a new track in the animation controller for the movement
//       animation (run or walk), and set up the smooth transition from the idle
//       animation (current track) to it (new track).
//-----------------------------------------------------------------------------
void CTiny::SetMoveKey()
{
    DWORD dwNewTrack = ( m_dwCurrentTrack == 0 ? 1 : 0 );
    LPD3DXANIMATIONCONTROLLER pAC;
    LPD3DXANIMATIONSET pAS;
    m_pAI->GetAnimController( &pAC );

    if( m_fSpeed == m_fSpeedWalk )
        pAC->GetAnimationSet( m_dwAnimIdxWalk, &pAS );
    else
        pAC->GetAnimationSet( m_dwAnimIdxJog, &pAS );

    pAC->SetTrackAnimationSet( dwNewTrack, pAS );
    pAS->Release();

    pAC->UnkeyAllTrackEvents( m_dwCurrentTrack );
    pAC->UnkeyAllTrackEvents( dwNewTrack );

    pAC->KeyTrackEnable( m_dwCurrentTrack, FALSE, m_dTimeCurrent + MOVE_TRANSITION_TIME );
    pAC->KeyTrackSpeed( m_dwCurrentTrack, 0.0f, m_dTimeCurrent, MOVE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );
    pAC->KeyTrackWeight( m_dwCurrentTrack, 0.0f, m_dTimeCurrent, MOVE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );
    pAC->SetTrackEnable( dwNewTrack, TRUE );
    pAC->KeyTrackSpeed( dwNewTrack, 1.0f, m_dTimeCurrent, MOVE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );
    pAC->KeyTrackWeight( dwNewTrack, 1.0f, m_dTimeCurrent, MOVE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );

    m_dwCurrentTrack = dwNewTrack;

    pAC->Release();
}




//-----------------------------------------------------------------------------
// Name: CTiny::SetIdleKey()
// Desc: Initialize a new track in the animation controller for the idle
//       (loiter ) animation, and set up the smooth transition from the
//       movement animation (current track) to it (new track).
//
//       bResetPosition controls whether we start the Loiter animation from
//       its beginning or current position.
//-----------------------------------------------------------------------------
void CTiny::SetIdleKey( bool bResetPosition )
{
    DWORD dwNewTrack = ( m_dwCurrentTrack == 0 ? 1 : 0 );
    LPD3DXANIMATIONCONTROLLER pAC;
    LPD3DXANIMATIONSET pAS;
    m_pAI->GetAnimController( &pAC );

    pAC->GetAnimationSet( m_dwAnimIdxLoiter, &pAS );
    pAC->SetTrackAnimationSet( dwNewTrack, pAS );
    pAS->Release();

    pAC->UnkeyAllTrackEvents( m_dwCurrentTrack );
    pAC->UnkeyAllTrackEvents( dwNewTrack );

    pAC->KeyTrackEnable( m_dwCurrentTrack, FALSE, m_dTimeCurrent + IDLE_TRANSITION_TIME );
    pAC->KeyTrackSpeed( m_dwCurrentTrack, 0.0f, m_dTimeCurrent, IDLE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );
    pAC->KeyTrackWeight( m_dwCurrentTrack, 0.0f, m_dTimeCurrent, IDLE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );
    pAC->SetTrackEnable( dwNewTrack, TRUE );
    pAC->KeyTrackSpeed( dwNewTrack, 1.0f, m_dTimeCurrent, IDLE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );
    pAC->KeyTrackWeight( dwNewTrack, 1.0f, m_dTimeCurrent, IDLE_TRANSITION_TIME, D3DXTRANSITION_LINEAR );
    if( bResetPosition )
        pAC->SetTrackPosition( dwNewTrack, 0.0 );

    m_dwCurrentTrack = dwNewTrack;

    pAC->Release();
}




//-----------------------------------------------------------------------------
// Name: CTiny::RestoreDeviceObjects()
// Desc: Reinitialize necessary objects
//-----------------------------------------------------------------------------
HRESULT CTiny::RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice )
{
    // Compress the animation sets in the new animation controller
    SetupCallbacksAndCompression();

    LPD3DXANIMATIONCONTROLLER pAC;
    m_pAI->GetAnimController( &pAC );
    pAC->ResetTime();
    pAC->AdvanceTime( m_dTimeCurrent, NULL );

    // Initialize current track
    if( m_szASName[0] != '\0' )
    {
        DWORD dwActiveSet = GetAnimIndex( m_szASName );
        LPD3DXANIMATIONSET pAS = NULL;
        pAC->GetAnimationSet( dwActiveSet, &pAS );
        pAC->SetTrackAnimationSet( m_dwCurrentTrack, pAS );
        SAFE_RELEASE( pAS );
    }

    pAC->SetTrackEnable( m_dwCurrentTrack, TRUE );
    pAC->SetTrackWeight( m_dwCurrentTrack, 1.0f );
    pAC->SetTrackSpeed( m_dwCurrentTrack, 1.0f );

    SAFE_RELEASE( pAC );

    // Call animate to initialize the tracks.
    Animate( 0.0 );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CTiny::RestoreDeviceObjects()
// Desc: Free D3D objects so that the device can be reset.
//-----------------------------------------------------------------------------
HRESULT CTiny::InvalidateDeviceObjects()
{
    // Save the current track's animation set name
    // so we can reset it again in RestoreDeviceObjects later.
    LPD3DXANIMATIONCONTROLLER pAC = NULL;
    m_pAI->GetAnimController( &pAC );
    if( pAC )
    {
        LPD3DXANIMATIONSET pAS = NULL;
        pAC->GetTrackAnimationSet( m_dwCurrentTrack, &pAS );
        if( pAS )
        {
            if( pAS->GetName() )
                strcpy_s( m_szASName, 64, pAS->GetName() );
            SAFE_RELEASE( pAS );
        }
        SAFE_RELEASE( pAC );
    }

    return S_OK;
}

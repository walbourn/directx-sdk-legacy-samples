//--------------------------------------------------------------------------------------
// File: XInputGame.cpp
//
// Basic starting point for new Direct3D samples
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"
#include <XInput.h>


//--------------------------------------------------------------------------------------
// Structs/defines
//--------------------------------------------------------------------------------------
inline int RectWidth( RECT& rc )
{
    return ( ( rc ).right - ( rc ).left );
}
inline int RectHeight( RECT& rc )
{
    return ( ( rc ).bottom - ( rc ).top );
}
#define MAX_AMMO 100
#define MAX_ASTEROID 100

struct AMMO_STATE
{
    bool bActive;
    double fTimeCreated;
    float fLifeCountdown;
    D3DXVECTOR2 vPosition;
    D3DXVECTOR2 vVelocity;
    int iFromPlayer;
    D3DCOLOR clr;
    RECT rcTexture;
    float fSizeInPercent;
    D3DXVECTOR2 vSizeInPixels;
};

struct ASTEROID_STATE
{
    bool bActive;
    bool bDead;
    float fDeathCountdown;
    int nAnimationState;
    D3DXVECTOR2 vPosition;
    D3DXVECTOR2 vVelocity;
    RECT rcTexture[6];
    float fSizeInPercent;
    D3DXVECTOR2 vSizeInPixels;
    D3DCOLOR clr;
};

struct PLAYER_STATE
{
    int controllerIndex;
    float fFireReloadCountdown;
    bool bActive;
    D3DXVECTOR2 vPosition;
    RECT rcTexture;
    D3DXVECTOR2 vSizeInPixels;
    int nScore;
    bool bFireRumbleInEffect;
    float fFireRumbleCountdown;
    bool bDamangeRumbleInEffect;
    float fDamageRumbleCountdown;
    D3DCOLOR clr;
};

enum GAME_MODE
{
    GAME_MAIN_MENU = 1,
    GAME_RUNNING,
    GAME_CONTROLLER_UNPLUGGED,
};

struct GAME_STATE
{
    int nNumActivePlayers;
    PLAYER_STATE    player[DXUT_MAX_CONTROLLERS];
    AMMO_STATE      AmmoQ[MAX_AMMO];
    ASTEROID_STATE  AsteroidQ[MAX_ASTEROID];
    int nAsteroidCount;
    GAME_MODE gameMode;
    int nUnpluggedPlayer;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;             // Font for drawing text
ID3DXFont*                  g_pSmallFont = NULL;             // Font for drawing text
ID3DXSprite*                g_pSprite = NULL;           // Sprite for batching draw text calls
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CDXUTDialog                 g_SampleUI;                 // dialog for sample specific controls
IDirect3DTexture9*          g_pSpriteTexture = NULL;
IDirect3DTexture9*          g_pStarTexture = NULL;
DXUT_GAMEPAD g_GamePads[DXUT_MAX_CONTROLLERS];
GAME_STATE                  g_GameState;



//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
void RenderText();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Init the app
    InitApp();

    // Init DXUT, and create the window and D3D device
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTInit();
    DXUTSetHotkeyHandling();
    DXUTCreateWindow( L"XInputGame" );
    DXUTCreateDevice( false, 0, 0 );

    // Start the message loop
    DXUTMainLoop();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    ZeroMemory( &g_GameState, sizeof( GAME_STATE ) );
    g_GameState.gameMode = GAME_MAIN_MENU;

    // Initialize dialogs
    g_SampleUI.Init( &g_DialogResourceManager );
    g_SampleUI.SetCallback( OnGUIEvent );
}



//--------------------------------------------------------------------------------------
// Create device resources 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;
    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( D3DXCreateFont( pd3dDevice, 30, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );
    V_RETURN( D3DXCreateFont( pd3dDevice, 20, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pSmallFont ) );

    // Load the environment texture
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"sprites.dds" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pSpriteTexture ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"stars.dds" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pStarTexture ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
void SetAnimationRect( RECT* pRect, int nX, int nY )
{
    SetRect( pRect, nX * 128, nY * 128, nX * 128 + 128, nY * 128 + 128 );
}


//--------------------------------------------------------------------------------------
// Create device resources that don't live through pd3dDevice->Reset()
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );
    if( g_pSmallFont )
        V_RETURN( g_pSmallFont->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite ) );

    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    {
        float fSizeInPixels = pBackBufferSurfaceDesc->Width / 15.0f;
        for (int iPlayer = 0; iPlayer < DXUT_MAX_CONTROLLERS; iPlayer++)
        {
            g_GameState.player[iPlayer].vSizeInPixels = D3DXVECTOR2(fSizeInPixels, fSizeInPixels);
            SetAnimationRect(&g_GameState.player[iPlayer].rcTexture, iPlayer, 0);
            g_GameState.player[iPlayer].clr = 0xFFFFFFFF;
        }
        g_GameState.player[0].vPosition = D3DXVECTOR2(pBackBufferSurfaceDesc->Width / 2.0f,
            pBackBufferSurfaceDesc->Height / 2.0f);
        g_GameState.player[1].vPosition = D3DXVECTOR2(pBackBufferSurfaceDesc->Width / 2.0f - fSizeInPixels * 4,
            pBackBufferSurfaceDesc->Height / 2.0f - fSizeInPixels * 4);
        g_GameState.player[2].vPosition = D3DXVECTOR2(pBackBufferSurfaceDesc->Width / 2.0f - fSizeInPixels * 4,
            pBackBufferSurfaceDesc->Height / 2.0f + fSizeInPixels * 4);
        g_GameState.player[3].vPosition = D3DXVECTOR2(pBackBufferSurfaceDesc->Width / 2.0f + fSizeInPixels * 4,
            pBackBufferSurfaceDesc->Height / 2.0f - fSizeInPixels * 4);
    }

    for( int iAmmo = 0; iAmmo < MAX_AMMO; iAmmo++ )
    {
        if( !g_GameState.AmmoQ[iAmmo].bActive )
            continue;
        float fSizeInPixels = pBackBufferSurfaceDesc->Width * g_GameState.AmmoQ[iAmmo].fSizeInPercent;
        g_GameState.AmmoQ[iAmmo].vSizeInPixels = D3DXVECTOR2( fSizeInPixels, fSizeInPixels );
    }

    for( int iAsteroid = 0; iAsteroid < MAX_ASTEROID; iAsteroid++ )
    {
        if( !g_GameState.AsteroidQ[iAsteroid].bActive )
            continue;
        float fSizeInPixels = pBackBufferSurfaceDesc->Width * g_GameState.AsteroidQ[iAsteroid].fSizeInPercent;
        g_GameState.AsteroidQ[iAsteroid].vSizeInPixels = D3DXVECTOR2( fSizeInPixels, fSizeInPixels );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
void FireAmmo( float fPosX, float fPosY, float fVelX, float fVelY, int iFromPlayer )
{
    // Check to see if there are already MAX_AMMO balls in the world.
    // If there are, remove the oldest ammo to make room for the newest if necessary.
    double fOldest = g_GameState.AmmoQ[0].fTimeCreated;
    int nOldestIndex = 0;
    int nInactiveIndex = -1;
    for( int iAmmo = 0; iAmmo < MAX_AMMO; iAmmo++ )
    {
        if( !g_GameState.AmmoQ[iAmmo].bActive )
        {
            nInactiveIndex = iAmmo;
            break;
        }
        if( g_GameState.AmmoQ[iAmmo].fTimeCreated < fOldest )
        {
            fOldest = g_GameState.AmmoQ[iAmmo].fTimeCreated;
            nOldestIndex = iAmmo;
        }
    }

    if( nInactiveIndex < 0 )
    {
        g_GameState.AmmoQ[nOldestIndex].bActive = false;
        nInactiveIndex = nOldestIndex;
    }

    int nNewIndex = nInactiveIndex;
    g_GameState.AmmoQ[nNewIndex].bActive = true;
    g_GameState.AmmoQ[nNewIndex].fTimeCreated = DXUTGetTime();
    g_GameState.AmmoQ[nNewIndex].fLifeCountdown = 1.0f;
    g_GameState.AmmoQ[nNewIndex].iFromPlayer = iFromPlayer;
    g_GameState.AmmoQ[nNewIndex].vVelocity = D3DXVECTOR2( fVelX, fVelY );
    SetAnimationRect( &g_GameState.AmmoQ[nNewIndex].rcTexture, iFromPlayer, 3 );
    g_GameState.AmmoQ[nNewIndex].fSizeInPercent = 1.0f / 20.0f;
    float fSizeInPixels = DXUTGetD3D9BackBufferSurfaceDesc()->Width * g_GameState.AmmoQ[nNewIndex].fSizeInPercent;
    g_GameState.AmmoQ[nNewIndex].vSizeInPixels = D3DXVECTOR2( fSizeInPixels, fSizeInPixels );
    g_GameState.AmmoQ[nNewIndex].vPosition = D3DXVECTOR2( fPosX - fSizeInPixels / 2, fPosY - fSizeInPixels / 2 );
    g_GameState.AmmoQ[nNewIndex].clr = g_GameState.player[iFromPlayer].clr;
}


//--------------------------------------------------------------------------------------
void CreateAsteroid( bool bSmall )
{
    // Check to see if there are already MAX_ASTEROID in the world.
    if( g_GameState.nAsteroidCount >= MAX_ASTEROID )
        return;

    int nInactiveIndex = -1;
    for( int iAsteroid = 0; iAsteroid < MAX_ASTEROID; iAsteroid++ )
    {
        if( !g_GameState.AsteroidQ[iAsteroid].bActive )
        {
            nInactiveIndex = iAsteroid;
            break;
        }
    }

    if( nInactiveIndex < 0 )
        return;

    g_GameState.nAsteroidCount++;

    float fPosX = ( rand() % 1000 ) / 1000.0f * DXUTGetD3D9BackBufferSurfaceDesc()->Width;
    float fPosY = ( rand() % 1000 ) / 1000.0f * DXUTGetD3D9BackBufferSurfaceDesc()->Height;

    float fVelX = ( rand() % 2000 - 1000 ) / 1000.0f * 0.1f;
    float fVelY = ( rand() % 2000 - 1000 ) / 1000.0f * 0.1f;

    int nNewIndex = nInactiveIndex;
    g_GameState.AsteroidQ[nNewIndex].bActive = true;
    g_GameState.AsteroidQ[nNewIndex].bDead = false;
    g_GameState.AsteroidQ[nNewIndex].vVelocity = D3DXVECTOR2( fVelX, fVelY );

    SetAnimationRect( &g_GameState.AsteroidQ[nNewIndex].rcTexture[0], 0, 1 ); // alive
    SetAnimationRect( &g_GameState.AsteroidQ[nNewIndex].rcTexture[1], 1, 1 ); // death 1
    SetAnimationRect( &g_GameState.AsteroidQ[nNewIndex].rcTexture[2], 2, 1 ); // death 2
    SetAnimationRect( &g_GameState.AsteroidQ[nNewIndex].rcTexture[3], 3, 1 ); // death 3
    SetAnimationRect( &g_GameState.AsteroidQ[nNewIndex].rcTexture[4], 0, 2 ); // death 4
    SetAnimationRect( &g_GameState.AsteroidQ[nNewIndex].rcTexture[5], 1, 2 ); // death 5
    g_GameState.AsteroidQ[nNewIndex].fSizeInPercent = 1.0f / 10.0f;

    float fSizeInPixels = DXUTGetD3D9BackBufferSurfaceDesc()->Width * g_GameState.AsteroidQ[nNewIndex].fSizeInPercent;
    g_GameState.AsteroidQ[nNewIndex].vSizeInPixels = D3DXVECTOR2( fSizeInPixels, fSizeInPixels );
    g_GameState.AsteroidQ[nNewIndex].vPosition = D3DXVECTOR2( fPosX - fSizeInPixels / 2, fPosY - fSizeInPixels / 2 );
    g_GameState.AsteroidQ[nNewIndex].clr = 0xFFFFFFFF;
    g_GameState.AsteroidQ[nNewIndex].nAnimationState = 0;
}


//--------------------------------------------------------------------------------------
void UpdateGame( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    // Update active ammo
    for( int iAmmo = 0; iAmmo < MAX_AMMO; iAmmo++ )
    {
        if( !g_GameState.AmmoQ[iAmmo].bActive )
            continue;

        AMMO_STATE* pAmmo = &g_GameState.AmmoQ[iAmmo];

        pAmmo->fLifeCountdown -= fElapsedTime;
        if( pAmmo->fLifeCountdown < 0.0f )
        {
            pAmmo->bActive = false;
            continue;
        }

        pAmmo->vPosition.x += pAmmo->vVelocity.x * pAmmo->vSizeInPixels.x * fElapsedTime * 15.0f;
        pAmmo->vPosition.y += pAmmo->vVelocity.y * pAmmo->vSizeInPixels.y * fElapsedTime * 15.0f;

        // Bounce ammo against walls
        float fMaxBoardY = ( float )DXUTGetD3D9BackBufferSurfaceDesc()->Height - pAmmo->vSizeInPixels.y;
        float fMaxBoardX = ( float )DXUTGetD3D9BackBufferSurfaceDesc()->Width - pAmmo->vSizeInPixels.x;
        if( pAmmo->vPosition.x > fMaxBoardX )
        {
            pAmmo->vPosition.x = fMaxBoardX;
            pAmmo->vVelocity.x = -pAmmo->vVelocity.x;
        }
        if( pAmmo->vPosition.x < 0 )
        {
            pAmmo->vPosition.x = 0;
            pAmmo->vVelocity.x = -pAmmo->vVelocity.x;
        }
        if( pAmmo->vPosition.y > fMaxBoardY )
        {
            pAmmo->vPosition.y = fMaxBoardY;
            pAmmo->vVelocity.y = -pAmmo->vVelocity.y;
        }
        if( pAmmo->vPosition.y < 0 )
        {
            pAmmo->vPosition.y = 0;
            pAmmo->vVelocity.y = -pAmmo->vVelocity.y;
        }

        float fAmmoCenterX = pAmmo->vPosition.x + pAmmo->vSizeInPixels.x / 2.0f;
        float fAmmoCenterY = pAmmo->vPosition.y + pAmmo->vSizeInPixels.y / 2.0f;

        // Check if hit other player
        for( int iPlayer = 0; iPlayer < DXUT_MAX_CONTROLLERS; iPlayer++ )
        {
            if( iPlayer == pAmmo->iFromPlayer )
                continue;
            if( !g_GameState.player[iPlayer].bActive )
                continue;

            float fPlayerCenterX = g_GameState.player[iPlayer].vPosition.x +
                g_GameState.player[iPlayer].vSizeInPixels.x / 2.0f;
            float fPlayerCenterY = g_GameState.player[iPlayer].vPosition.y +
                g_GameState.player[iPlayer].vSizeInPixels.y / 2.0f;
            if( fabsf( fAmmoCenterX - fPlayerCenterX ) < g_GameState.player[iPlayer].vSizeInPixels.x / 2.0f &&
                fabsf( fAmmoCenterY - fPlayerCenterY ) < g_GameState.player[iPlayer].vSizeInPixels.y / 2.0f )
            {
                g_GameState.player[pAmmo->iFromPlayer].nScore++;
                pAmmo->bActive = false;

                g_GameState.player[iPlayer].nScore -= 1;
                g_GameState.player[iPlayer].bDamangeRumbleInEffect = true;
                g_GameState.player[iPlayer].fDamageRumbleCountdown = 0.50f;

                XINPUT_VIBRATION vibration;
                vibration.wLeftMotorSpeed = 0xFFFF;
                vibration.wRightMotorSpeed = 0;
                XInputSetState( g_GameState.player[iPlayer].controllerIndex, &vibration );
                continue;
            }
        }

        for( int iAsteroid = 0; iAsteroid < MAX_ASTEROID; iAsteroid++ )
        {
            if( !g_GameState.AsteroidQ[iAsteroid].bActive || g_GameState.AsteroidQ[iAsteroid].bDead )
                continue;

            float fAsterCenterX = g_GameState.AsteroidQ[iAsteroid].vPosition.x +
                g_GameState.AsteroidQ[iAsteroid].vSizeInPixels.x / 2.0f;
            float fAsterCenterY = g_GameState.AsteroidQ[iAsteroid].vPosition.y +
                g_GameState.AsteroidQ[iAsteroid].vSizeInPixels.y / 2.0f;
            if( fabsf( fAmmoCenterX - fAsterCenterX ) < g_GameState.AsteroidQ[iAsteroid].vSizeInPixels.x / 4.0f &&
                fabsf( fAmmoCenterY - fAsterCenterY ) < g_GameState.AsteroidQ[iAsteroid].vSizeInPixels.y / 4.0f )
            {
                g_GameState.player[pAmmo->iFromPlayer].nScore++;
                pAmmo->bActive = false;
                g_GameState.AsteroidQ[iAsteroid].bDead = true;
                g_GameState.AsteroidQ[iAsteroid].nAnimationState = 2;
                g_GameState.AsteroidQ[iAsteroid].fDeathCountdown = 1.1f;
            }
        }
    }

    // Update active asteroids
    for( int iAsteroid = 0; iAsteroid < MAX_ASTEROID; iAsteroid++ )
    {
        if( !g_GameState.AsteroidQ[iAsteroid].bActive )
            continue;

        ASTEROID_STATE* pAsteroid = &g_GameState.AsteroidQ[iAsteroid];

        if( pAsteroid->bDead )
        {
            pAsteroid->fDeathCountdown -= fElapsedTime;
            g_GameState.AsteroidQ[iAsteroid].nAnimationState = min( ( int )( ( 1.1f - pAsteroid->fDeathCountdown ) /
                                                                             0.1f ) + 1, 5 );
            if( pAsteroid->fDeathCountdown < 0.5f )
            {
                pAsteroid->bActive = false;
                g_GameState.nAsteroidCount--;
            }
            continue;
        }

        pAsteroid->vPosition.x += pAsteroid->vVelocity.x * pAsteroid->vSizeInPixels.x * fElapsedTime * 15.0f;
        pAsteroid->vPosition.y += pAsteroid->vVelocity.y * pAsteroid->vSizeInPixels.y * fElapsedTime * 15.0f;

        // Bounce asteroid against walls
        float fMaxBoardY = ( float )DXUTGetD3D9BackBufferSurfaceDesc()->Height - pAsteroid->vSizeInPixels.y;
        float fMaxBoardX = ( float )DXUTGetD3D9BackBufferSurfaceDesc()->Width - pAsteroid->vSizeInPixels.x;
        if( pAsteroid->vPosition.x > fMaxBoardX )
        {
            pAsteroid->vPosition.x = fMaxBoardX;
            pAsteroid->vVelocity.x = -pAsteroid->vVelocity.x;
        }
        if( pAsteroid->vPosition.x < 0 )
        {
            pAsteroid->vPosition.x = 0;
            pAsteroid->vVelocity.x = -pAsteroid->vVelocity.x;
        }
        if( pAsteroid->vPosition.y > fMaxBoardY )
        {
            pAsteroid->vPosition.y = fMaxBoardY;
            pAsteroid->vVelocity.y = -pAsteroid->vVelocity.y;
        }
        if( pAsteroid->vPosition.y < 0 )
        {
            pAsteroid->vPosition.y = 0;
            pAsteroid->vVelocity.y = -pAsteroid->vVelocity.y;
        }

        float fAsteroidCenterX = pAsteroid->vPosition.x + pAsteroid->vSizeInPixels.x / 2.0f;
        float fAsteroidCenterY = pAsteroid->vPosition.y + pAsteroid->vSizeInPixels.y / 2.0f;

        // Check if asteroid hit player
        for( int iPlayer = 0; iPlayer < DXUT_MAX_CONTROLLERS; iPlayer++ )
        {
            if( !g_GameState.player[iPlayer].bActive )
                continue;

            float fPlayerCenterX = g_GameState.player[iPlayer].vPosition.x +
                g_GameState.player[iPlayer].vSizeInPixels.x / 2.0f;
            float fPlayerCenterY = g_GameState.player[iPlayer].vPosition.y +
                g_GameState.player[iPlayer].vSizeInPixels.y / 2.0f;
            if( fabsf( fAsteroidCenterX - fPlayerCenterX ) < g_GameState.player[iPlayer].vSizeInPixels.x / 2.0f &&
                fabsf( fAsteroidCenterY - fPlayerCenterY ) < g_GameState.player[iPlayer].vSizeInPixels.y / 2.0f )
            {
                pAsteroid->bActive = false;
                g_GameState.nAsteroidCount--;

                g_GameState.player[iPlayer].nScore -= 10;
                g_GameState.player[iPlayer].bDamangeRumbleInEffect = true;
                g_GameState.player[iPlayer].fDamageRumbleCountdown = 0.50f;

                XINPUT_VIBRATION vibration;
                vibration.wLeftMotorSpeed = 0xFFFF;
                vibration.wRightMotorSpeed = 0;
                XInputSetState( g_GameState.player[iPlayer].controllerIndex, &vibration );
                continue;
            }
        }
    }

    // Update active players
    for( int iPlayer = 0; iPlayer < DXUT_MAX_CONTROLLERS; iPlayer++ )
    {
        if( !g_GameState.player[iPlayer].bActive )
            continue;

        // Check controller index assigned to this player
        int iUserIndex = g_GameState.player[iPlayer].controllerIndex;
        if( !g_GamePads[iUserIndex].bConnected )
            continue; // unplugged?

        if( g_GameState.player[iPlayer].bFireRumbleInEffect )
        {
            g_GameState.player[iPlayer].fFireRumbleCountdown -= fElapsedTime;
            if( g_GameState.player[iPlayer].fFireRumbleCountdown < 0.0f )
            {
                g_GameState.player[iPlayer].bFireRumbleInEffect = false;

                // Turn off rumble if damage rumble not happening
                if( !g_GameState.player[iPlayer].bDamangeRumbleInEffect )
                {
                    XINPUT_VIBRATION vibration;
                    vibration.wLeftMotorSpeed = 0;
                    vibration.wRightMotorSpeed = 0;
                    XInputSetState( iUserIndex, &vibration );
                }
            }
        }

        if( g_GameState.player[iPlayer].bDamangeRumbleInEffect )
        {
            g_GameState.player[iPlayer].fDamageRumbleCountdown -= fElapsedTime;
            if( g_GameState.player[iPlayer].fDamageRumbleCountdown < 0.0f )
            {
                g_GameState.player[iPlayer].fDamageRumbleCountdown = false;

                // Turn off rumble if fire rumble not happening
                if( !g_GameState.player[iPlayer].bFireRumbleInEffect )
                {
                    XINPUT_VIBRATION vibration;
                    vibration.wLeftMotorSpeed = 0;
                    vibration.wRightMotorSpeed = 0;
                    XInputSetState( iUserIndex, &vibration );
                }
            }
        }

        // Move player based on left thumbstick
        g_GameState.player[iPlayer].vPosition.x += g_GamePads[iUserIndex].fThumbLX * fElapsedTime *
            g_GameState.player[iPlayer].vSizeInPixels.x * 10.0f;
        g_GameState.player[iPlayer].vPosition.y += -g_GamePads[iUserIndex].fThumbLY * fElapsedTime *
            g_GameState.player[iPlayer].vSizeInPixels.y * 10.0f;

        // Keep player within map bounds
        float fMaxBoardY = ( float )DXUTGetD3D9BackBufferSurfaceDesc()->Height -
            g_GameState.player[iPlayer].vSizeInPixels.y;
        float fMaxBoardX = ( float )DXUTGetD3D9BackBufferSurfaceDesc()->Width -
            g_GameState.player[iPlayer].vSizeInPixels.x;
        if( g_GameState.player[iPlayer].vPosition.x > fMaxBoardX )
            g_GameState.player[iPlayer].vPosition.x = fMaxBoardX;
        if( g_GameState.player[iPlayer].vPosition.x < 0 )
            g_GameState.player[iPlayer].vPosition.x = 0;
        if( g_GameState.player[iPlayer].vPosition.y > fMaxBoardY )
            g_GameState.player[iPlayer].vPosition.y = fMaxBoardY;
        if( g_GameState.player[iPlayer].vPosition.y < 0 )
            g_GameState.player[iPlayer].vPosition.y = 0;

        g_GameState.player[iPlayer].fFireReloadCountdown -= fElapsedTime;
        if( g_GameState.player[iPlayer].fFireReloadCountdown < 0.0f )
        {
            // Fire ammo if right thumbstick non-zero
            if( g_GamePads[iUserIndex].sThumbRX != 0 || g_GamePads[iUserIndex].sThumbRY != 0 )
            {
                g_GameState.player[iPlayer].fFireReloadCountdown = 0.1f;
                float fVelX = g_GamePads[iUserIndex].fThumbRX;
                float fVelY = -g_GamePads[iUserIndex].fThumbRY;
                FireAmmo( g_GameState.player[iPlayer].vPosition.x + g_GameState.player[iPlayer].vSizeInPixels.x / 2,
                          g_GameState.player[iPlayer].vPosition.y + g_GameState.player[iPlayer].vSizeInPixels.y / 2,
                          fVelX, fVelY, iPlayer );

                g_GameState.player[iPlayer].bFireRumbleInEffect = true;
                g_GameState.player[iPlayer].fFireRumbleCountdown = 0.15f;
                XINPUT_VIBRATION vibration;
                vibration.wLeftMotorSpeed = 0;
                vibration.wRightMotorSpeed = 15000;
                XInputSetState( iUserIndex, &vibration );
            }
        }
    }

    // Check if another player is trying to join
    for( int iUserIndex = 0; iUserIndex < DXUT_MAX_CONTROLLERS; iUserIndex++ )
    {
        // Skip if controller not connected
        if( !g_GamePads[iUserIndex].bConnected )
            continue;

        // Check if this controller is already being used by another player
        bool bControllerInUse = false;
        int nPlayerUsingThisController = -1;
        for( int iPlayer = 0; iPlayer < DXUT_MAX_CONTROLLERS; iPlayer++ )
        {
            if( !g_GameState.player[iPlayer].bActive )
                continue;
            if( g_GameState.player[iPlayer].controllerIndex == iUserIndex )
            {
                bControllerInUse = true;
                nPlayerUsingThisController = iPlayer;
            }
        }

        if( !bControllerInUse )
        {
            if( g_GamePads[iUserIndex].wPressedButtons != 0 )
            {
                // If a button is pressed on an used controller, assign it
                // to the first inactive player
                for( int iPlayer = 0; iPlayer < DXUT_MAX_CONTROLLERS; iPlayer++ )
                {
                    if( !g_GameState.player[iPlayer].bActive )
                    {
                        g_GameState.player[iPlayer].bActive = true;
                        g_GameState.nNumActivePlayers++;
                        g_GameState.player[iPlayer].controllerIndex = iUserIndex;

                        // Reset all score when a new player joins
                        for( int iPlayer2 = 0; iPlayer2 < DXUT_MAX_CONTROLLERS; iPlayer2++ )
                            g_GameState.player[iPlayer2].nScore = 0;
                        break;
                    }
                }
            }
        }
        else
        {
            if( ( g_GamePads[iUserIndex].wPressedButtons & XINPUT_GAMEPAD_START ) )
            {
                if( nPlayerUsingThisController >= 0 && nPlayerUsingThisController < DXUT_MAX_CONTROLLERS )
                {
                    g_GameState.player[nPlayerUsingThisController].bActive = false;
                    g_GameState.nNumActivePlayers--;
                    if( g_GameState.nNumActivePlayers == 0 )
                    {
                        DXUTStopRumbleOnAllControllers();
                        g_GameState.gameMode = GAME_MAIN_MENU;
                    }

                    // Turn off rumble
                    XINPUT_VIBRATION vibration;
                    vibration.wLeftMotorSpeed = 0;
                    vibration.wRightMotorSpeed = 0;
                    XInputSetState( g_GameState.player[nPlayerUsingThisController].controllerIndex, &vibration );
                }
            }
        }
    }

}


//--------------------------------------------------------------------------------------
// Update scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    if( g_GameState.nAsteroidCount < 10 * g_GameState.nNumActivePlayers )
        CreateAsteroid( ( rand() % 2 ) == 0 );

    // Get the state of the gamepads
    for( DWORD iUserIndex = 0; iUserIndex < DXUT_MAX_CONTROLLERS; iUserIndex++ )
        DXUTGetGamepadState( iUserIndex, &g_GamePads[iUserIndex], true, false );

    if( g_GameState.gameMode == GAME_RUNNING )
    {
        // Detect if any controller became unplugged.  If it did
        // pause the game and display error text
        for( int iPlayer = 0; iPlayer < DXUT_MAX_CONTROLLERS; iPlayer++ )
        {
            if( !g_GameState.player[iPlayer].bActive )
                continue;

            // Check controller index assigned to this player
            int iUserIndex = g_GameState.player[iPlayer].controllerIndex;
            if( !g_GamePads[iUserIndex].bConnected )
            {
                // Controller is unplugged
                DXUTStopRumbleOnAllControllers();
                g_GameState.gameMode = GAME_CONTROLLER_UNPLUGGED;
                g_GameState.nUnpluggedPlayer = iPlayer;
                return;
            }
        }
    }
    else if( g_GameState.gameMode == GAME_CONTROLLER_UNPLUGGED )
    {
        // Check to see if player's unplugged controller is reconnect and the 
        // player has pressed Start yet.
        int iUserIndex = g_GameState.player[g_GameState.nUnpluggedPlayer].controllerIndex;
        if( g_GamePads[iUserIndex].bConnected )
        {
            if( ( g_GamePads[iUserIndex].wPressedButtons & XINPUT_GAMEPAD_START ) )
            {
                g_GameState.gameMode = GAME_RUNNING;
                return;
            }
        }
    }

    if( g_GameState.gameMode == GAME_MAIN_MENU )
    {
        for( int iUserIndex = 0; iUserIndex < DXUT_MAX_CONTROLLERS; iUserIndex++ )
        {
            if( !g_GamePads[iUserIndex].bConnected )
                continue;

            // If any button pressed
            if( g_GamePads[iUserIndex].wPressedButtons != 0 )
            {
                g_GameState.player[0].bActive = true;
                g_GameState.player[0].controllerIndex = iUserIndex;
                g_GameState.nNumActivePlayers = 1;
                g_GameState.gameMode = GAME_RUNNING;
            }
        }
    }
    else if( g_GameState.gameMode == GAME_RUNNING )
    {
        UpdateGame( DXUTGetD3D9Device(), fTime, fElapsedTime );
    }
}


//--------------------------------------------------------------------------------------
// Render scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 0, 140 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        if( g_GameState.gameMode == GAME_MAIN_MENU )
        {
            CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 25 );
            txtHelper.Begin();
            txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 256,
                                       DXUTGetD3D9BackBufferSurfaceDesc()->Height / 2 - 50 );
            txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );

            bool bIsAnyControllerConnected = false;
            for( int iUserIndex = 0; iUserIndex < DXUT_MAX_CONTROLLERS; iUserIndex++ )
            {
                if( g_GamePads[iUserIndex].bConnected )
                    bIsAnyControllerConnected = true;
            }

            if( bIsAnyControllerConnected )
            {
                txtHelper.DrawTextLine( L"Press any controller button to start the game" );
                txtHelper.DrawTextLine( L"Press Escape to quit" );
            }
            else
            {
                txtHelper.DrawTextLine( L"No XInput compatible controllers found." );
                txtHelper.DrawTextLine( L"Plug in a 'Xbox 360 Controller For Windows'" );
                txtHelper.DrawTextLine( L"to run this sample. Press Escape to quit" );
            }
            txtHelper.End();
        }
        else
        {
            g_pSprite->Begin( 0 );

            D3DXMATRIXA16 matTransform;
            pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
            pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
            pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

            // Render background star field
            pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
            pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
            RECT rcTexture;
            SetRect( &rcTexture, 0, 0, DXUTGetD3D9BackBufferSurfaceDesc()->Width,
                     DXUTGetD3D9BackBufferSurfaceDesc()->Height );
            D3DXMatrixScaling( &matTransform, 1.0f, 1.0f, 1.0f );
            g_pSprite->SetTransform( &matTransform );
            {
                D3DXVECTOR3 vPos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
                g_pSprite->Draw(g_pStarTexture, &rcTexture, NULL, &vPos, 0xFFFFFFFF);
            }

            for( int i = 0; i < MAX_AMMO; i++ )
            {
                if( g_GameState.AmmoQ[i].bActive )
                {
                    float fScaleX = ( float )g_GameState.AmmoQ[i].vSizeInPixels.x /
                        RectWidth( g_GameState.AmmoQ[i].rcTexture );
                    float fScaleY = ( float )g_GameState.AmmoQ[i].vSizeInPixels.y /
                        RectHeight( g_GameState.AmmoQ[i].rcTexture );
                    D3DXMatrixScaling( &matTransform, fScaleX, fScaleY, 1.0f );
                    g_pSprite->SetTransform( &matTransform );
                    D3DXVECTOR3 vPos( g_GameState.AmmoQ[i].vPosition.x / fScaleX, g_GameState.AmmoQ[i].vPosition.y /
                                      fScaleY, 0.0f );
                    g_pSprite->Draw( g_pSpriteTexture, &g_GameState.AmmoQ[i].rcTexture, NULL, &vPos,
                                     g_GameState.AmmoQ[i].clr );
                }
            }

            for( int i = 0; i < MAX_ASTEROID; i++ )
            {
                if( g_GameState.AsteroidQ[i].bActive )
                {
                    RECT* pTexRect = &g_GameState.AsteroidQ[i].rcTexture[g_GameState.AsteroidQ[i].nAnimationState];
                    float fScaleX = ( float )g_GameState.AsteroidQ[i].vSizeInPixels.x / RectWidth( *pTexRect );
                    float fScaleY = ( float )g_GameState.AsteroidQ[i].vSizeInPixels.y / RectHeight( *pTexRect );
                    D3DXMatrixScaling( &matTransform, fScaleX, fScaleY, 1.0f );
                    g_pSprite->SetTransform( &matTransform );
                    D3DXVECTOR3 vPos( g_GameState.AsteroidQ[i].vPosition.x / fScaleX,
                                      g_GameState.AsteroidQ[i].vPosition.y / fScaleY, 0.0f );
                    g_pSprite->Draw( g_pSpriteTexture, pTexRect, NULL, &vPos, g_GameState.AsteroidQ[i].clr );
                }
            }

            for( int i = 0; i < DXUT_MAX_CONTROLLERS; i++ )
            {
                if( g_GameState.player[i].bActive )
                {
                    float fScaleX = ( float )g_GameState.player[i].vSizeInPixels.x /
                        RectWidth( g_GameState.player[i].rcTexture );
                    float fScaleY = ( float )g_GameState.player[i].vSizeInPixels.y /
                        RectHeight( g_GameState.player[i].rcTexture );
                    D3DXMatrixScaling( &matTransform, fScaleX, fScaleY, 1.0f );
                    g_pSprite->SetTransform( &matTransform );
                    D3DXVECTOR3 vPos( g_GameState.player[i].vPosition.x / fScaleX, g_GameState.player[i].vPosition.y /
                                      fScaleY, 0.0f );
                    g_pSprite->Draw( g_pSpriteTexture, &g_GameState.player[i].rcTexture, NULL, &vPos,
                                     g_GameState.player[i].clr );
                }
            }

            g_pSprite->End();

            D3DXMatrixScaling( &matTransform, 1.0f, 1.0f, 1.0f );
            g_pSprite->SetTransform( &matTransform );

            RenderText();
        }

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 25 );
    txtHelper.Begin();

    for( int i = 0; i < DXUT_MAX_CONTROLLERS; i++ )
    {
        if( !g_GameState.player[i].bActive )
            continue;

        switch( g_GameState.player[i].controllerIndex )
        {
            case 0:
                txtHelper.SetInsertionPos( 5, 5 ); break;
            case 1:
                txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width - 230, 5 ); break;
            case 2:
                txtHelper.SetInsertionPos( 5, DXUTGetD3D9BackBufferSurfaceDesc()->Height - 70 ); break;
            case 3:
                txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width - 230,
                                           DXUTGetD3D9BackBufferSurfaceDesc()->Height - 30 ); break;
        }

        WCHAR strName[256];
        switch( i )
        {
            case 0:
                txtHelper.SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
                wcscpy_s( strName, 256, L"Green" ); break;
            case 1:
                txtHelper.SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 1.0f, 1.0f ) );
                wcscpy_s( strName, 256, L"Blue" ); break;
            case 2:
                txtHelper.SetForegroundColor( D3DXCOLOR( 0.8f, 0.0f, 0.8f, 1.0f ) );
                wcscpy_s( strName, 256, L"Purple" ); break;
            case 3:
                txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.5f, 0.0f, 1.0f ) );
                wcscpy_s( strName, 256, L"Red" ); break;
        }

        txtHelper.DrawFormattedTextLine( L"%s Player: %d", strName, g_GameState.player[i].nScore );
    }

    // Display reconnect message if controller came unplugged
    if( g_GameState.gameMode == GAME_CONTROLLER_UNPLUGGED )
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
        txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 512,
                                   DXUTGetD3D9BackBufferSurfaceDesc()->Height / 2 );
        txtHelper.DrawFormattedTextLine( L"Player %d's controller is unplugged.  Please reconnect and push Start",
                                         g_GameState.nUnpluggedPlayer + 1 );
    }
    txtHelper.End();

    CDXUTTextHelper txtHelper2( g_pSmallFont, g_pSprite, 20 );
    txtHelper2.Begin();
    txtHelper2.SetForegroundColor( D3DXCOLOR( 0.135f, 0.55f, 0.93f, 1.0f ) );
    txtHelper2.SetInsertionPos( 5, DXUTGetD3D9BackBufferSurfaceDesc()->Height - 40 );
    txtHelper2.DrawTextLine( L"Press 'Start' to join or leave.  Supports up to 4 players" );
    txtHelper2.DrawTextLine( L"Left thumbstick to move.  Right thumbstick to fire" );
    txtHelper2.End();
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    return 0;
}



//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release resources created in the OnResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pSmallFont )
        g_pSmallFont->OnLostDevice();
    SAFE_RELEASE( g_pSprite );
}


//--------------------------------------------------------------------------------------
// Release resources created in the OnCreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pSpriteTexture );
    SAFE_RELEASE( g_pStarTexture );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pSmallFont );
}

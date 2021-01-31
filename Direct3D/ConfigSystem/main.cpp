//--------------------------------------------------------------------------------------
// File: ConfigSystem.cpp
//
// This sample demonstrates a configuration system implementation that customizes
// application behavior based on installed devices.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxut.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKsound.h"
#include "SDKmesh.h"
#include "SDKmisc.h"
#pragma warning(disable: 4995)
#include <stdio.h>
#include <shlobj.h>
#include "resource.h"
#include "ConfigDatabase.h"
#include "ConfigManager.h"
#pragma warning(default: 4995)


//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


// MAXANISOTROPY is the maximum anisotropy state value used when anisotropic filtering is enabled.
#define MAXANISOTROPY 8


// -GROUND_Y is the Y coordinate of the ground.
#define GROUND_Y 3.0f


// GROUND_ABSORBANCE is the percentage of the velocity absorbed by ground and walls when an ammo hits.
#define GROUND_ABSORBANCE 0.2f


// AMMO_ABSORBANCE is the percentage of the velocity absorbed by ammos when two collide.
#define AMMO_ABSORBANCE 0.1f


// MAX_AMMO is the maximum number of ammo that can exist in the world.
#define MAX_AMMO 50


// AMMO_SIZE is the diameter of the ball mesh in the world.
#define AMMO_SIZE 0.25f


// AUTOFIRE_DELAY is the period between two successive ammo firing.
#define AUTOFIRE_DELAY 0.1f


// CAMERA_SIZE is used for clipping camera movement
#define CAMERA_SIZE 0.2f


// GRAVITY defines the magnitude of the downward force applied to ammos.
#define GRAVITY 6.0f


// BOUNCE_TRANSFER is the proportion of velocity transferred during a collision between 2 ammos.
#define BOUNCE_TRANSFER 0.8f


// BOUNCE_LOST is the proportion of velocity lost during a collision between 2 ammos.
#define BOUNCE_LOST 0.1f


// REST_THRESHOLD is the energy below which the ball is flagged as laying on ground.
// It is defined as Gravity * Height_above_ground + 0.5 * Velocity * Velocity
#define REST_THRESHOLD 0.005f


// PHYSICS_FRAMELENGTH is the duration of a frame for physics handling
// when the graphics frame length is too long.
#define PHYSICS_FRAMELENGTH 0.003f


// MAX_SOUND_VELOCITY is the velocity at which the bouncing sound is
// played at maximum volume.  Higher velocity uses maximum volume.
#define MAX_SOUND_VELOCITY 1.0f


// MIN_SOUND_VELOCITY is the minimum contact velocity required to make a sound.
#define MIN_SOUND_VELOCITY 0.07f


// MIN_VOL_ADJUST is the minimum volume adjustment based on contact velocity.
#define MIN_VOL_ADJUST 0.8f


// MinBound and MaxBound are the bounding box representing the cell mesh.
const D3DXVECTOR3           g_MinBound( -4.0f, -GROUND_Y, -6.0f );
const D3DXVECTOR3           g_MaxBound( 4.0f, GROUND_Y, 6.0f );


struct CBall
{
    D3DXVECTOR3 vPosition;
    D3DXVECTOR3 vVelocity;
    bool bGround;    // Whether it is laying on the ground (resting or rolling)
    CSound* pSound;

            CBall() : bGround( false ),
                      pSound( NULL )
            {
            }
    void    PlaySound( float fImpactSpeed );
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                g_pEffect = NULL;       // D3DX effect interface
IDirect3DTexture9*          g_pDefaultTex;          // Default texture for un-textured geometry.
CFirstPersonCamera          g_Camera;               // Camera
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
CConfigManager*             g_pCMList = NULL;       // Array of CConfigManager, one for each display device on system
CConfigManager*             g_pCM = NULL;           // Active CM. Points to one of the elements in g_pCMList.
CDXUTXFileMesh              g_Cell;                 // Cell mesh object
D3DXMATRIXA16               g_mCellWorld;           // World matrix for cell mesh
CDXUTXFileMesh              g_Ball;                 // Ball mesh object
D3DXMATRIXA16               g_mBallWorld;           // World matrix for ball meshes (scaling)
CBall g_AmmoQ[MAX_AMMO];      // Queue of ammos in the world
int                         g_nAmmoCount = 0;       // Number of ammos that exist in the world
int                         g_nAmmoListHead = 0;    // Head node of the queue
int                         g_nAmmoListTail = 0;    // Tail node of the queue (1 past last instance)
D3DXHANDLE                  g_hShaderTech;          // Technique to use when using programmable rendering path
D3DXHANDLE                  g_hMatW;                // Handles to transformation matrices in effect
D3DXHANDLE                  g_hMatV;
D3DXHANDLE                  g_hMatP;
D3DXHANDLE                  g_hMatWV;
D3DXHANDLE                  g_hMatWVP;
D3DXHANDLE                  g_hDiffuse;             // Handles to material parameters in effect
D3DXHANDLE                  g_hSpecular;
D3DXHANDLE                  g_hTex;
D3DXHANDLE                  g_hPower;
CSoundManager               g_SndMgr;
bool                        g_bLeftMouseDown = false;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_USEANISO            4
#define IDC_FIXEDFUNC           5
#define IDC_DISABLESPECULAR     6
#define IDC_FORCESHADERTEXT     7
#define IDC_FORCESHADER         8


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
void RenderText();


//--------------------------------------------------------------------------------------
void CBall::PlaySound( float fImpactSpeed )
{
    // Determine the sound volume adjustment based on velocity.
    float fAdjustment;
    if( fImpactSpeed < MIN_SOUND_VELOCITY )
        fAdjustment = 0.0f;  // Too soft.  Don't play sound.
    else
    {
        fAdjustment = min( 1.0f, fImpactSpeed / MAX_SOUND_VELOCITY );
        // Rescale the adjustment so that its minimum is at 0.6.
        fAdjustment = MIN_VOL_ADJUST + fAdjustment * ( 1.0f - MIN_VOL_ADJUST );
    }
    // Play sound
    D3DXVECTOR3 vDistance = *g_Camera.GetEyePt() - vPosition;
    float fVolume = min( D3DXVec3LengthSq( &vDistance ), 1.0f );
    // Scale
    fVolume *= -1800.f;
    // Sound is proportional to how hard the ball is hitting.
    fVolume = fAdjustment * ( fVolume - DSBVOLUME_MIN ) + DSBVOLUME_MIN;

    // Check pSound because it could be NULL. We allow the sample to run
    // even if we cannot initialize sound.
    if( pSound )
    {
        pSound->Reset();
        pSound->Play( 0, 0, ( LONG )fVolume );
    }
}


//--------------------------------------------------------------------------------------
// CheckForSafeMode searches launch state files written by previous instances of the
// application. It is called during program launch and program shutdown. During
// initialization, it enables the safe mode flag if a previous instance did not shut
// down properly. When shutting down, it deletes those orphaned launch state files so
// that the next launch can go normally without entering safe mode.
//--------------------------------------------------------------------------------------
void CheckForSafeMode( WCHAR* wszLaunchFile, bool& bSafeMode, bool bInitialize )
{
    HRESULT hr;

    if( !bInitialize )
    {
        // If shutting down, remove the launch state file that this current instance wrote.
        ::DeleteFile( wszLaunchFile );
    }

    // Check for initialization flag from previous launch.
    // We use SHGetFolderPath with CSIDL_LOCAL_APPDATA to write to
    // a subdirectory of Documents and Settings\<username>\Local Settings\Application Data.
    // It is the best practice to write user-specific data under the user profile directory
    // because not every user on the system will always have write access to the application
    // executable directory.
    WCHAR wszPath[MAX_PATH];
    ::GetModuleFileName( NULL, wszPath, MAX_PATH );
    hr = ::SHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, wszPath );
    if( SUCCEEDED( hr ) )
    {
        // Create the directory for this application. We don't care
        // if the directory already exists as long as it exists after
        // this point.
        wcscat_s( wszPath, MAX_PATH, L"\\ConfigSystem" );
        ::CreateDirectory( wszPath, NULL );

        // Search for launch*.sta. For each file found, we extract the process ID
        // from the file name and search for the process. If we cannot find the
        // process with the same ID, then that instance of app did not shut down
        // properly, and we should enable safe mode.
        WCHAR wszSearch[MAX_PATH];
        wcscpy_s( wszSearch, MAX_PATH, wszPath );
        wcscat_s( wszSearch, MAX_PATH, L"\\launch*.sta" );
        WIN32_FIND_DATAW fd;
        HANDLE hFind = ::FindFirstFile( wszSearch, &fd );
        while( INVALID_HANDLE_VALUE != hFind )
        {
            // Extract the process ID from the file name.
            WCHAR wszPid[MAX_PATH];
            wcscpy_s( wszPid, MAX_PATH, fd.cFileName + 6 );
            WCHAR* pDot = wcsstr( wszPid, L".sta" );
            if( pDot )
            {
                *pDot = L'\0';

                // wszPid now holds the PID in string form.
                DWORD dwPid = _wtol( wszPid );
                HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, dwPid );
                DWORD dwExitCode = 0;
                if( hProcess )
                    GetExitCodeProcess( hProcess, &dwExitCode );
                if( !hProcess || STILL_ACTIVE != dwExitCode )
                {
                    // Launch state file exists but the process does not.
                    // Now check the state file again in the rare case
                    // that the process shuts down right after we check
                    // the existence of the file but before the call
                    // to OpenProcess().

                    wcscpy_s( wszSearch, MAX_PATH, wszPath );
                    wcscat_s( wszSearch, MAX_PATH, L"\\" );
                    wcscat_s( wszSearch, MAX_PATH, fd.cFileName );
                    if( ::GetFileAttributes( wszSearch ) != INVALID_FILE_ATTRIBUTES )
                    {
                        // File still exists. Process was not shut down properly.
                        if( bInitialize )
                        {
                            // When initializing, enable safe mode.
                            bSafeMode = true;
                            break;
                        }
                        else
                        {
                            // When shutting down, delete each orphaned launch state file.
                            ::DeleteFile( wszSearch );
                        }
                    }
                }
                else
                    CloseHandle( hProcess );
            }

            // Find the next file
            if( !::FindNextFile( hFind, &fd ) )
                break;
        }

        ::FindClose( hFind );

        if( bInitialize )
        {
            // When initializing, create Launch%PID%.sta in the ConfigSystem's app data folder.
            // This serves as a way to "register" this instance of the app.
            swprintf_s( wszLaunchFile, MAX_PATH, L"%s\\launch%u.sta", wszPath, GetCurrentProcessId() );
            HANDLE hFile = ::CreateFile( wszLaunchFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                         NULL );
            if( hFile != INVALID_HANDLE_VALUE )
                ::CloseHandle( hFile );
        }
    }
}


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

    HRESULT hr;
    bool bSafeMode = false;
    WCHAR wszLaunchFile[MAX_PATH] = L"";

    DXUTSetCallbackD3D9DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCursorSettings( true, true );
    InitApp();

    // Check if safe mode is desired.
    CheckForSafeMode( wszLaunchFile, bSafeMode, true );

    // When initializing, prompt the user to enter safe mode.
    if( bSafeMode )
    {
        if( IDNO == ::MessageBox( NULL, L"The application failed to initialize during "
                                  L"the previous launch.  It is recommended that "
                                  L"you run the application in safe mode.\n\n"
                                  L"Do you wish to run in safe mode?",
                                  L"ConfigSystem", MB_YESNO | MB_ICONQUESTION ) )
            bSafeMode = false;
    }

    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys

    // Initialize config information for all available 3D devices after DXUTInit is called
    // because we would like to do this as soon as we have a D3D object.
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );

    // Allocate the CM array to hold all devices on the system.
    g_pCMList = new CConfigManager[pD3D->GetAdapterCount()];
    if( !g_pCMList )
    {
        // If we cannot allocate memory for the CM array.  The system must
        // be in a critical state.  Do not proceed in this case.
        ::MessageBox( NULL, L"The system has insufficient memory to run the application.", L"ConfigSystem", MB_OK );
    }
    else
    {
        // Obtain sound information (device GUID)
        GUID guidDeviceId;
        GetDeviceID( &DSDEVID_DefaultPlayback, &guidDeviceId );

        WCHAR str[MAX_PATH];
        hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"config.txt" );

        if( FAILED( hr ) )
        {
            MessageBox( NULL, L"Config.txt is not found. All properties and requirements are set "
                        L"to the default.",
                        L"Configuration File Missing", MB_OK | MB_ICONEXCLAMATION );
        }

        // Initialize CM objects, one for each 3D device
        for( DWORD dev = 0; dev < pD3D->GetAdapterCount(); ++dev )
        {
            D3DADAPTER_IDENTIFIER9 id;
            D3DCAPS9 Caps;
            pD3D->GetAdapterIdentifier( dev, 0, &id );
            pD3D->GetDeviceCaps( dev, D3DDEVTYPE_HAL, &Caps );
            g_pCMList[dev].Initialize( str, id, Caps, guidDeviceId );
            // Propagate safe mode flag
            g_pCMList[dev].cf_SafeMode = bSafeMode;
        }

        // Make g_pCM point to the first element in g_pCMList by default.
        // OnCreateDevice will make it point to the correct element later.
        g_pCM = g_pCMList;

        // Handle window creation
        DXUTCreateWindow( L"ConfigSystem" );

        // Initialize sound after we have the window
        WCHAR wszBounce[MAX_PATH];
        g_SndMgr.Initialize( DXUTGetHWND(), DSSCL_PRIORITY );
        DXUTFindDXSDKMediaFileCch( wszBounce, MAX_PATH, L"bounce.wav" );
        for( int A = 0; A < MAX_AMMO; ++A )
        {
            g_SndMgr.Create( &g_AmmoQ[A].pSound, wszBounce, DSBCAPS_CTRLVOLUME );
        }

        hr = DXUTCreateDevice( true, 640, 480 );
        if( SUCCEEDED( hr ) )
        {
            // Pass control to DXUT for handling the message pump and 
            // dispatching render calls. DXUT will call your FrameMove 
            // and FrameRender callback when there is idle time between handling window messages.
            DXUTMainLoop();
        }
        delete[] g_pCMList;

        if( SUCCEEDED( hr ) )
        {
            // Program completed.  Do safe mode clean up.
            CheckForSafeMode( wszLaunchFile, bSafeMode, false );
        }
    }

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.
    for( int A = 0; A < MAX_AMMO; ++A )
        delete g_AmmoQ[A].pSound;

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 0;
    g_SampleUI.AddStatic( IDC_FORCESHADERTEXT, L"Force shader version:", 15, iY, 145, 22 );
    g_SampleUI.AddComboBox( IDC_FORCESHADER, 15, iY += 24, 145, 22 );
    g_SampleUI.AddCheckBox( IDC_USEANISO, L"Use anisotropic filtering", 15, iY += 24, 145, 22 );
    g_SampleUI.AddCheckBox( IDC_FIXEDFUNC, L"Use fixed-function", 15, iY += 24, 145, 22 );
    g_SampleUI.AddCheckBox( IDC_DISABLESPECULAR, L"Disable specular lighting", 15, iY += 24, 145, 22 );
    // Populate the combo box with shader rendering techniques
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"Card maximum", ( LPVOID )0 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"3.0", ( LPVOID )30 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"2.a", ( LPVOID )9998 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"2.b", ( LPVOID )9997 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"2.0", ( LPVOID )20 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"1.4", ( LPVOID )14 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"1.3", ( LPVOID )13 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"1.2", ( LPVOID )12 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"1.1", ( LPVOID )11 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->AddItem( L"Do not use shader", ( LPVOID )9999 );

    // Setup the camera
    D3DXVECTOR3 MinBound( g_MinBound.x + CAMERA_SIZE, g_MinBound.y + CAMERA_SIZE, g_MinBound.z + CAMERA_SIZE );
    D3DXVECTOR3 MaxBound( g_MaxBound.x - CAMERA_SIZE, g_MaxBound.y - CAMERA_SIZE, g_MaxBound.z - CAMERA_SIZE );
    g_Camera.SetClipToBoundary( true, &MinBound, &MaxBound );
    g_Camera.SetEnableYAxisMovement( false );
    g_Camera.SetRotateButtons( false, false, true );
    g_Camera.SetScalers( 0.01f, 2.0f );
    D3DXVECTOR3 vecEye( 0.0f, -GROUND_Y + 0.7f, 0.0f );
    D3DXVECTOR3 vecAt ( 0.0f, -GROUND_Y + 0.7f, 1.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // Initialize world matrices
    D3DXMatrixScaling( &g_mCellWorld, 1.0f, GROUND_Y / 3.0f, 1.0f );

    // Initialize some instances of ammo so it's easier for users to
    // figure out what to do at the start.
    D3DXVECTOR4 Pos( 0.0f, -GROUND_Y - 2 * AMMO_SIZE, AMMO_SIZE, 1.0f );
    D3DXVECTOR4 Vel( 0.0f, 4.25f, 4.25f, 1.0f );
    D3DXMATRIXA16 m;
    D3DXMatrixRotationY( &m, D3DX_PI / 6 );  // Matrix to rotate vel vector
    for( int A = 0; A < 12; ++A )
    {
        g_AmmoQ[A].bGround = false;
        g_AmmoQ[A].vPosition = ( D3DXVECTOR3 )Pos;
        g_AmmoQ[A].vVelocity = ( D3DXVECTOR3 )Vel;
        D3DXVec4Transform( &Pos, &Pos, &m );
        D3DXVec4Transform( &Vel, &Vel, &m );
    }
    g_nAmmoCount = 12;
    g_nAmmoListTail = 12;
}


//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    HRESULT hr;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 caps;

    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
                            pDeviceSettings->d3d9.DeviceType,
                            &caps ) );

    // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // Debugging vertex shaders requires either REF or software vertex processing 
    // and debugging pixel shaders requires REF.  
#ifdef DEBUG_VS
    if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
    {
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
        pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
#endif
#ifdef DEBUG_PS
    pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
#endif
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Compute a matrix that scales Mesh to a specified size and centers around origin
//--------------------------------------------------------------------------------------
void ComputeMeshScaling( CDXUTXFileMesh& Mesh, D3DXMATRIX* pmScalingCenter, float fNewRadius )
{
    LPVOID pVerts = NULL;
    D3DXMatrixIdentity( pmScalingCenter );
    if( SUCCEEDED( Mesh.GetMesh()->LockVertexBuffer( 0, &pVerts ) ) )
    {
        D3DXVECTOR3 vCtr;
        float fRadius;
        if( SUCCEEDED( D3DXComputeBoundingSphere( ( const D3DXVECTOR3* )pVerts,
                                                  Mesh.GetMesh()->GetNumVertices(),
                                                  Mesh.GetMesh()->GetNumBytesPerVertex(),
                                                  &vCtr, &fRadius ) ) )
        {
            D3DXMatrixTranslation( pmScalingCenter, -vCtr.x, -vCtr.y, -vCtr.z );
            D3DXMATRIXA16 m;
            D3DXMatrixScaling( &m, fNewRadius / fRadius,
                               fNewRadius / fRadius,
                               fNewRadius / fRadius );
            D3DXMatrixMultiply( pmScalingCenter, pmScalingCenter, &m );
        }
        Mesh.GetMesh()->UnlockVertexBuffer();
    }
}


//--------------------------------------------------------------------------------------
void SetEffectTechnique()
{
    if( g_pCM->cf_UseFixedFunction )
        g_pEffect->SetTechnique( "FFRenderScene" );
    else
    {
        // For programmable pipeline, check for shader version override (ForceShader).
        switch( g_pCM->cf_ForceShader )
        {
            case 9999:
                // 9999 is a code to mean "do not use shader"
                g_hShaderTech = g_pEffect->GetTechniqueByName( "FFRenderScene" );
                break;

            case 9998:
                // 9998 is a code to represent shader 2_a
                g_hShaderTech = g_pEffect->GetTechniqueByName( "RenderScenePS20A" );
                break;

            case 9997:
                // 9997 is a code to represent shader 2_b
                g_hShaderTech = g_pEffect->GetTechniqueByName( "RenderScenePS20B" );
                break;

            case 0:
                // 0 means use the highest valid technique.
                g_pEffect->FindNextValidTechnique( NULL, &g_hShaderTech );
                break;
                // In all other cases, g_pCM->cf_ForceShader defines a shader version
                // this way: g_pCM->cf_ForceShader = MAJOR * 10 + MINOR
            default:
            {
                char szTechName[50];

                sprintf_s( szTechName, 50, "RenderScenePS%d", g_pCM->cf_ForceShader );
                g_hShaderTech = g_pEffect->GetTechniqueByName( szTechName );
                break;
            }

        }

        g_pEffect->SetTechnique( g_hShaderTech );
    }
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;

    // Verify requirement and prompt the user if any requirement is not met.
    hr = g_pCM->VerifyRequirements();
    if( FAILED( hr ) )
    {
        DXUTShutdown( 0 );
        return S_OK;
    }

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
    //
    // Obtain the CConfigManager object for the 3D device created.
    // Point g_pCM to the CConfigManager object.
    D3DCAPS9 Caps;
    pd3dDevice->GetDeviceCaps( &Caps );
    g_pCM = g_pCMList + Caps.AdapterOrdinal;

    // Check for unsupported display device.
    if( g_pCM->cf_UnsupportedCard )
    {
        ::MessageBox( NULL, L"The display device is not supported by this application. "
                      L"The program will now exit.", L"ConfigSystem", MB_OK | MB_ICONERROR );
        return E_FAIL;
    }

    // Check for invalid display driver
    if( g_pCM->cf_InvalidDriver )
    {
        ::MessageBox( NULL, L"The display driver detected is incompatible with this application. "
                      L"The program will now exit.", L"ConfigSystem", MB_OK | MB_ICONERROR );
        return E_FAIL;
    }

    // Check for old display driver
    if( g_pCM->cf_OldDriver )
    {
        ::MessageBox( NULL, L"The display driver is out-of-date. We recommend that you "
                      L"install the latest driver for this display device.",
                      L"ConfigSystem", MB_OK | MB_ICONWARNING );
    }

    // Check for known prototype card
    if( g_pCM->cf_PrototypeCard )
    {
        ::MessageBox( NULL, L"A prototype display card is detected.  We recommend using "
                      L"only supported retail display cards to run this application.",
                      L"ConfigSystem", MB_OK | MB_ICONWARNING );
    }

    // Check for invalid sound driver
    if( g_pCM->cf_InvalidSoundDriver )
    {
        ::MessageBox( NULL, L"The sound driver detected is incompatible with this application. "
                      L"The program will now exit.", L"ConfigSystem", MB_OK | MB_ICONERROR );
        return E_FAIL;
    }

    // Check for old sound driver
    if( g_pCM->cf_OldSoundDriver )
    {
        ::MessageBox( NULL, L"The sound driver is out-of-date. We recommend that you "
                      L"install the latest driver for this sound device.",
                      L"ConfigSystem", MB_OK | MB_ICONWARNING );
    }

    // Go through the list of resolution and remove those that are excluded for this card.
    CD3D9EnumAdapterInfo* pAdapterInfo = DXUTGetD3D9Enumeration()->GetAdapterInfo( Caps.AdapterOrdinal );
    for( int idm = pAdapterInfo->displayModeList.GetSize() - 1; idm >= 0; --idm )
    {
        D3DDISPLAYMODE DisplayMode = pAdapterInfo->displayModeList.GetAt( idm );

        if( DisplayMode.Width > g_pCM->cf_MaximumResolution )
            pAdapterInfo->displayModeList.Remove( idm );
    }

    // Update the UI elements to reflect correct config flag values.
    g_SampleUI.GetCheckBox( IDC_USEANISO )->SetChecked( g_pCM->cf_UseAnisotropicFilter != 0 );
    g_SampleUI.GetCheckBox( IDC_FIXEDFUNC )->SetChecked( g_pCM->cf_UseFixedFunction != 0 );
    g_SampleUI.GetComboBox( IDC_FORCESHADER )->SetSelectedByData( ( LPVOID )( size_t )g_pCM->cf_ForceShader );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;
#if defined( DEBUG ) || defined( _DEBUG )
        dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Main.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    dwShaderFlags |= D3DXSHADER_USE_LEGACY_D3DX9_31_DLL;
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    // Obtain handles
    g_hMatW = g_pEffect->GetParameterByName( NULL, "g_mWorld" );
    g_hMatV = g_pEffect->GetParameterByName( NULL, "g_mView" );
    g_hMatP = g_pEffect->GetParameterByName( NULL, "g_mProj" );
    g_hMatWV = g_pEffect->GetParameterByName( NULL, "g_mWorldView" );
    g_hMatWVP = g_pEffect->GetParameterByName( NULL, "g_mWorldViewProj" );
    g_hDiffuse = g_pEffect->GetParameterByName( NULL, "g_matDiffuse" );
    g_hSpecular = g_pEffect->GetParameterByName( NULL, "g_matSpecular" );
    g_hTex = g_pEffect->GetParameterByName( NULL, "g_txScene" );
    g_hPower = g_pEffect->GetParameterByName( NULL, "g_matPower" );

    // Create mesh
    WCHAR wsz[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( wsz, MAX_PATH, L"misc\\cell.x" ) );
    g_Cell.Create( pd3dDevice, wsz );
    // Tessellate the mesh for better per-vertex lighting result.
    ID3DXMesh* pTessMesh;
    D3DXTessellateNPatches( g_Cell.m_pMesh, NULL, 8, false, &pTessMesh, NULL );
    g_Cell.m_pMesh->Release();
    g_Cell.m_pMesh = pTessMesh;

    V_RETURN( DXUTFindDXSDKMediaFileCch( wsz, MAX_PATH, L"ball.x" ) );
    g_Ball.Create( pd3dDevice, wsz );

    // Create the 1x1 white default texture
    V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, 0, D3DFMT_A8R8G8B8,
                                         D3DPOOL_MANAGED, &g_pDefaultTex, NULL ) );
    D3DLOCKED_RECT lr;
    V_RETURN( g_pDefaultTex->LockRect( 0, &lr, NULL, 0 ) );
    *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 255, 255, 255, 255 );
    V_RETURN( g_pDefaultTex->UnlockRect( 0 ) );

    // Compute the scaling matrix for the ball mesh.
    ComputeMeshScaling( g_Ball, &g_mBallWorld, AMMO_SIZE * 0.5f );

    //
    // Initialize feature usage settings in the effect object according to the config flags.
    //

    // Rendering technique (FF or shader)
    SetEffectTechnique();

    // Specular lighting
    g_pEffect->SetBool( "g_bUseSpecular", !g_pCM->cf_DisableSpecular );

    // Anisotropic filtering
    g_pEffect->SetBool( "g_bUseAnisotropic", g_pCM->cf_UseAnisotropicFilter );
    g_pEffect->SetInt( "g_MinFilter", g_pCM->cf_UseAnisotropicFilter ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR );
    g_pEffect->SetInt( "g_MaxAnisotropy", g_pCM->cf_UseAnisotropicFilter ? 8 : 1 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );
    if( g_pEffect )
        V_RETURN( g_pEffect->OnResetDevice() );

    g_Cell.RestoreDeviceObjects( pd3dDevice );
    g_Ball.RestoreDeviceObjects( pd3dDevice );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 230 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    // Check mouse and shoot ammo
    if( g_bLeftMouseDown )
    {
        static float fLastFired;  // Timestamp of last ammo fired

        if( fTime - fLastFired >= AUTOFIRE_DELAY )
        {
            // Check to see if there are already MAX_AMMO balls in the world.
            // Remove the oldest ammo to make room for the newest if necessary.
            if( g_nAmmoCount == MAX_AMMO )
            {
                g_nAmmoListHead = ( g_nAmmoListHead + 1 ) % MAX_AMMO;
                --g_nAmmoCount;
            }

            // Get inverse view matrix
            D3DXMATRIXA16 mInvView;
            D3DXMatrixInverse( &mInvView, NULL, g_Camera.GetViewMatrix() );

            // Compute initial velocity in world space from camera space
            D3DXVECTOR4 InitialVelocity( 0.0f, 0.0f, 6.0f, 0.0f );
            D3DXVec4Transform( &InitialVelocity, &InitialVelocity, &mInvView );

            // Populate velocity
            g_AmmoQ[g_nAmmoListTail].vVelocity.x = InitialVelocity.x;
            g_AmmoQ[g_nAmmoListTail].vVelocity.y = InitialVelocity.y;
            g_AmmoQ[g_nAmmoListTail].vVelocity.z = InitialVelocity.z;

            // Populate position
            D3DXVECTOR4 InitialPosition( 0.0f, -0.15f, 0.0f, 1.0f );
            D3DXVec4Transform( &InitialPosition, &InitialPosition, &mInvView );
            g_AmmoQ[g_nAmmoListTail].vPosition.x = InitialPosition.x;
            g_AmmoQ[g_nAmmoListTail].vPosition.y = InitialPosition.y;
            g_AmmoQ[g_nAmmoListTail].vPosition.z = InitialPosition.z;

            // Initially not laying on ground
            g_AmmoQ[g_nAmmoListTail].bGround = false;

            // Increment tail
            g_nAmmoListTail = ( g_nAmmoListTail + 1 ) % MAX_AMMO;
            ++g_nAmmoCount;

            fLastFired = ( float )fTime;
        }
    }

    // Animate the ammo
    if( g_nAmmoCount > 0 )
    {
        // Check for inter-ammo collision
        if( g_nAmmoCount > 1 )
        {
            for( int One = g_nAmmoListHead; ; )
            {
                for( int Two = ( One + 1 ) % MAX_AMMO; ; )
                {
                    // Check collision between instances One and Two.
                    // vOneToTwo is the collision normal vector.
                    D3DXVECTOR3 vOneToTwo = g_AmmoQ[Two].vPosition - g_AmmoQ[One].vPosition;
                    float DistSq = D3DXVec3LengthSq( &vOneToTwo );
                    if( DistSq < AMMO_SIZE * AMMO_SIZE )
                    {
                        D3DXVec3Normalize( &vOneToTwo, &vOneToTwo );

                        // Check if the two instances are already moving away from each other.
                        // If so, skip collision.  This can happen when a lot of instances are
                        // bunched up next to each other.
                        float fImpact = D3DXVec3Dot( &vOneToTwo, &g_AmmoQ[One].vVelocity ) -
                            D3DXVec3Dot( &vOneToTwo, &g_AmmoQ[Two].vVelocity );
                        if( fImpact > 0.0f )
                        {
                            // Compute the normal and tangential components of One's velocity.
                            D3DXVECTOR3 vVelocityOneN = ( 1 - BOUNCE_LOST ) *
                                D3DXVec3Dot( &vOneToTwo, &g_AmmoQ[One].vVelocity ) * vOneToTwo;
                            D3DXVECTOR3 vVelocityOneT = ( 1 - BOUNCE_LOST ) * g_AmmoQ[One].vVelocity - vVelocityOneN;
                            // Compute the normal and tangential components of Two's velocity.
                            D3DXVECTOR3 vVelocityTwoN = ( 1 - BOUNCE_LOST ) *
                                D3DXVec3Dot( &vOneToTwo, &g_AmmoQ[Two].vVelocity ) * vOneToTwo;
                            D3DXVECTOR3 vVelocityTwoT = ( 1 - BOUNCE_LOST ) * g_AmmoQ[Two].vVelocity - vVelocityTwoN;

                            // Compute post-collision velocity
                            g_AmmoQ[One].vVelocity = vVelocityOneT - vVelocityOneN * ( 1 - BOUNCE_TRANSFER ) +
                                vVelocityTwoN * BOUNCE_TRANSFER;
                            g_AmmoQ[Two].vVelocity = vVelocityTwoT - vVelocityTwoN * ( 1 - BOUNCE_TRANSFER ) +
                                vVelocityOneN * BOUNCE_TRANSFER;

                            // Fix the positions so that the two balls are exactly AMMO_SIZE apart.
                            float fDistanceToMove = ( AMMO_SIZE - sqrtf( DistSq ) ) * 0.5f;
                            g_AmmoQ[One].vPosition -= vOneToTwo * fDistanceToMove;
                            g_AmmoQ[Two].vPosition += vOneToTwo * fDistanceToMove;

                            // Flag the two instances so that they are not laying on ground.
                            g_AmmoQ[One].bGround = g_AmmoQ[Two].bGround = false;

                            // Play sound
                            g_AmmoQ[One].PlaySound( fImpact );
                            g_AmmoQ[Two].PlaySound( fImpact );
                        }
                    }

                    Two = ( Two + 1 ) % MAX_AMMO;
                    if( Two == g_nAmmoListTail )
                        break;
                }

                One = ( One + 1 ) % MAX_AMMO;
                if( ( One + 1 ) % MAX_AMMO == g_nAmmoListTail )
                    break;
            }
        }

        // Apply gravity and check for collision against ground and walls.
        for( int A = g_nAmmoListHead; ; )
        {
            // If the elapsed time is too long, we slice up the time and
            // handle physics over several passes.
            float fTimeLeft = fElapsedTime;
            float fElapsedFrameTime;

            while( fTimeLeft > 0.0f )
            {
                fElapsedFrameTime = min( fTimeLeft, PHYSICS_FRAMELENGTH );
                fTimeLeft -= fElapsedFrameTime;

                g_AmmoQ[A].vPosition += g_AmmoQ[A].vVelocity * fElapsedFrameTime;

                g_AmmoQ[A].vVelocity.x -= g_AmmoQ[A].vVelocity.x * 0.1f * fElapsedFrameTime;
                g_AmmoQ[A].vVelocity.z -= g_AmmoQ[A].vVelocity.z * 0.1f * fElapsedFrameTime;
                // Apply gravity if the ball is not resting on the ground.
                if( !g_AmmoQ[A].bGround )
                {
                    g_AmmoQ[A].vVelocity.y -= GRAVITY * fElapsedFrameTime;
                }

                // Check bounce on ground
                if( !g_AmmoQ[A].bGround )
                {
                    if( g_AmmoQ[A].vPosition.y < -GROUND_Y + ( AMMO_SIZE * 0.5f ) )
                    {
                        // Align the ball with the ground.
                        g_AmmoQ[A].vPosition.y = -GROUND_Y + ( AMMO_SIZE * 0.5f );

                        // Play sound
                        g_AmmoQ[A].PlaySound( -g_AmmoQ[A].vVelocity.y );

                        // Invert the Y velocity
                        g_AmmoQ[A].vVelocity.y = -g_AmmoQ[A].vVelocity.y * ( 1 - GROUND_ABSORBANCE );

                        // X and Z velocity are reduced because of friction.
                        g_AmmoQ[A].vVelocity.x *= 0.9f;
                        g_AmmoQ[A].vVelocity.z *= 0.9f;
                    }
                }
                else
                {
                    // Ball is resting or rolling on ground.
                    // X and Z velocity are reduced because of friction.
                    g_AmmoQ[A].vVelocity.x *= 0.9f;
                    g_AmmoQ[A].vVelocity.z *= 0.9f;
                }

                // Check bounce on ceiling
                if( g_AmmoQ[A].vPosition.y > GROUND_Y - ( AMMO_SIZE * 0.5f ) )
                {
                    // Align the ball with the ground.
                    g_AmmoQ[A].vPosition.y = GROUND_Y - ( AMMO_SIZE * 0.5f );

                    // Play sound
                    g_AmmoQ[A].PlaySound( g_AmmoQ[A].vVelocity.y );

                    // Invert the Y velocity
                    g_AmmoQ[A].vVelocity.y = -g_AmmoQ[A].vVelocity.y * ( 1 - GROUND_ABSORBANCE );

                    // X and Z velocity are reduced because of friction.
                    g_AmmoQ[A].vVelocity.x *= 0.9f;
                    g_AmmoQ[A].vVelocity.z *= 0.9f;
                }

                // If the Y direction motion is below a certain threshold, flag the instance as
                // laying on the ground.
                if( GRAVITY * ( g_AmmoQ[A].vPosition.y + GROUND_Y - ( AMMO_SIZE * 0.5f ) ) + 0.5f *
                    g_AmmoQ[A].vVelocity.y * g_AmmoQ[A].vVelocity.y
                    < REST_THRESHOLD )
                {
                    // Align the ball with the ground.
                    g_AmmoQ[A].vPosition.y = -GROUND_Y + ( AMMO_SIZE * 0.5f );

                    // Y direction velocity becomes 0.
                    g_AmmoQ[A].vVelocity.y = 0.0f;

                    // Flag it
                    g_AmmoQ[A].bGround = true;
                }

                // Check bounce on front and back walls
                if( g_AmmoQ[A].vPosition.z < g_MinBound.z + ( AMMO_SIZE * 0.5f ) )
                {
                    // Align the ball with the wall.
                    g_AmmoQ[A].vPosition.z = g_MinBound.z + ( AMMO_SIZE * 0.5f );

                    // Play sound
                    g_AmmoQ[A].PlaySound( -g_AmmoQ[A].vVelocity.z );

                    // Invert the Z velocity
                    g_AmmoQ[A].vVelocity.z = -g_AmmoQ[A].vVelocity.z * ( 1 - GROUND_ABSORBANCE );
                }
                if( g_AmmoQ[A].vPosition.z > g_MaxBound.z - ( AMMO_SIZE * 0.5f ) )
                {
                    // Align the ball with the wall.
                    g_AmmoQ[A].vPosition.z = g_MaxBound.z - ( AMMO_SIZE * 0.5f );

                    // Play sound
                    g_AmmoQ[A].PlaySound( g_AmmoQ[A].vVelocity.z );

                    // Invert the Z velocity
                    g_AmmoQ[A].vVelocity.z = -g_AmmoQ[A].vVelocity.z * ( 1 - GROUND_ABSORBANCE );
                }

                // Check bounce on left and right walls
                if( g_AmmoQ[A].vPosition.x < g_MinBound.x + ( AMMO_SIZE * 0.5f ) )
                {
                    // Align the ball with the wall.
                    g_AmmoQ[A].vPosition.x = g_MinBound.x + ( AMMO_SIZE * 0.5f );

                    // Play sound
                    g_AmmoQ[A].PlaySound( -g_AmmoQ[A].vVelocity.x );

                    // Invert the X velocity
                    g_AmmoQ[A].vVelocity.x = -g_AmmoQ[A].vVelocity.x * ( 1 - GROUND_ABSORBANCE );
                }
                if( g_AmmoQ[A].vPosition.x > g_MaxBound.x - ( AMMO_SIZE * 0.5f ) )
                {
                    // Align the ball with the wall.
                    g_AmmoQ[A].vPosition.x = g_MaxBound.x - ( AMMO_SIZE * 0.5f );

                    // Play sound
                    g_AmmoQ[A].PlaySound( g_AmmoQ[A].vVelocity.x );

                    // Invert the X velocity
                    g_AmmoQ[A].vVelocity.x = -g_AmmoQ[A].vVelocity.x * ( 1 - GROUND_ABSORBANCE );
                }
            }

            A = ( A + 1 ) % MAX_AMMO;
            if( A == g_nAmmoListTail ) break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldView;
    D3DXMATRIXA16 mWorldViewProjection;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Get the projection & view matrix from the camera class
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();

        mWorldViewProjection = g_mCellWorld * mView * mProj;
        mWorldView = g_mCellWorld * mView;

        // Update the effect's variables.  Instead of using strings, it would 
        // be more efficient to cache a handle to the parameter by calling 
        // ID3DXEffect::GetParameterByName
        V( g_pEffect->SetMatrix( g_hMatV, &mView ) );
        V( g_pEffect->SetMatrix( g_hMatP, &mProj ) );
        V( g_pEffect->SetMatrix( g_hMatWVP, &mWorldViewProjection ) );
        V( g_pEffect->SetMatrix( g_hMatWV, &mWorldView ) );
        V( g_pEffect->SetMatrix( g_hMatW, &g_mCellWorld ) );

        pd3dDevice->SetFVF( g_Cell.m_pMesh->GetFVF() );
        g_Cell.Render( g_pEffect, g_hTex, g_hDiffuse, 0, g_hSpecular, 0, g_hPower, true, false );

        pd3dDevice->SetFVF( g_Ball.m_pMesh->GetFVF() );
        if( g_nAmmoCount > 0 )
            for( int A = g_nAmmoListHead; ; )
            {
                D3DXMATRIXA16 mWorld;
                D3DXMatrixTranslation( &mWorld, g_AmmoQ[A].vPosition.x,
                                       g_AmmoQ[A].vPosition.y,
                                       g_AmmoQ[A].vPosition.z );
                mWorld = g_mBallWorld * mWorld;
                mWorldViewProjection = mWorld * mView * mProj;
                mWorldView = mWorld * mView;
                V( g_pEffect->SetMatrix( g_hMatWVP, &mWorldViewProjection ) );
                V( g_pEffect->SetMatrix( g_hMatWV, &mWorldView ) );
                V( g_pEffect->SetMatrix( g_hMatW, &mWorld ) );

                g_pEffect->SetTexture( g_hTex, g_pDefaultTex );
                g_Ball.Render( g_pEffect, 0, g_hDiffuse, 0, g_hSpecular, 0, g_hPower, true, false );

                A = ( A + 1 ) % MAX_AMMO;
                if( A == g_nAmmoListTail ) break;
            }

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );
        DXUT_EndPerfEvent();

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    txtHelper.DrawTextLine( L"This sample is rendering with features enabled/disabled based\n"
                            L"on the display card model on the system.  Please see the documentation\n"
                            L"for complete information." );

    // List properties that have been applied.
    // Since IConfigDatabase only lives within CConfigManager::Initialize,
    // we check all properties and print them one by one.  This is ok since
    // this code is usually only used for informational purposes only.
    txtHelper.SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
    int iY = pd3dsdBackBuffer->Height - 24;
    if( g_pCM->cf_ForceShader )
    {
        txtHelper.SetInsertionPos( 10, iY );
        switch( g_pCM->cf_ForceShader )
        {
            case 9999:
                txtHelper.DrawTextLine( L"ForceShader=0 (Shader is disabled)" ); break;
            case 9998:
                txtHelper.DrawTextLine( L"ForceShader=A2 (Forcing ps_2_a target)" ); break;
            case 9997:
                txtHelper.DrawTextLine( L"ForceShader=B2 (Forcing ps_2_b target)" ); break;
            default:
                txtHelper.DrawFormattedTextLine( L"ForceShader=%d (Forcing ps_%d_%d target)", g_pCM->cf_ForceShader,
                                                 g_pCM->cf_ForceShader / 10, g_pCM->cf_ForceShader % 10 );
        }
        iY -= 16;
    }
    if( g_pCM->cf_InvalidSoundDriver )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"InvalidSoundDriver (Sound driver has problem with this application)" );
        iY -= 16;
    }
    if( g_pCM->cf_InvalidDriver )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"InvalidDriver (Display driver has problem with this application)" );
        iY -= 16;
    }
    if( g_pCM->cf_OldSoundDriver )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"OldSoundDriver (Sound driver is older than versions tested)" );
        iY -= 16;
    }
    if( g_pCM->cf_OldDriver )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"OldDriver (Display driver is older than versions tested)" );
        iY -= 16;
    }
    if( g_pCM->cf_UnsupportedCard )  // Should never see this. Included here anyways.
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"UnsupportedCard (Display card is below requirement)" );
        iY -= 16;
    }
    if( g_pCM->cf_PrototypeCard )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"PrototypeCard (Display card is known prototype card)" );
        iY -= 16;
    }
    if( g_pCM->cf_DisableSpecular )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"DisableSpecular (Specular lighting is disabled)" );
        iY -= 16;
    }
    if( g_pCM->cf_UseFixedFunction )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"UseFixedFunction (Forcing fixed-function rendering path)" );
        iY -= 16;
    }
    if( g_pCM->cf_UseAnisotropicFilter )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawTextLine( L"UseAnisotropicFilter (Anisotropic filtering for textures is enabled)" );
        iY -= 16;
    }
    if( g_pCM->cf_MaximumResolution )
    {
        txtHelper.SetInsertionPos( 10, iY );
        txtHelper.DrawFormattedTextLine( L"MaximumResolution=%d (Maximum resolution width is limited to %d)",
                                         g_pCM->cf_MaximumResolution, g_pCM->cf_MaximumResolution );
        iY -= 16;
    }

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    txtHelper.SetInsertionPos( 5, iY - 4 );
    txtHelper.DrawTextLine( L"Config properties found:" );

    iY -= 24;
    txtHelper.SetInsertionPos( 5, iY );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
    if( g_pCM->cf_SafeMode )
        txtHelper.DrawTextLine( L"SAFE MODE\n" );

    // Draw help
    if( g_bShowHelp )
    {
        txtHelper.SetInsertionPos( pd3dsdBackBuffer->Width - 150, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( pd3dsdBackBuffer->Width - 130, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Move: W,S,A,D\n"
                                L"Shoot: Left mouse\n"
                                L"Rotate: Right mouse\n"
                                L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetInsertionPos( pd3dsdBackBuffer->Width - 150, pd3dsdBackBuffer->Height - 15 * 2 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }
    txtHelper.End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    switch( uMsg )
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            g_bLeftMouseDown = true;
            break;
        case WM_LBUTTONUP:
            g_bLeftMouseDown = false;
            break;
        case WM_CAPTURECHANGED:
            if( ( HWND )lParam != hWnd )
                g_bLeftMouseDown = false;
            break;
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_FIXEDFUNC:
        {
            g_pCM->cf_UseFixedFunction = ( ( CDXUTCheckBox* )pControl )->GetChecked();
            if( g_pCM->cf_UseFixedFunction )
                g_pEffect->SetTechnique( "FFRenderScene" );
            else
                g_pEffect->SetTechnique( g_hShaderTech );
            break;
        }
        case IDC_USEANISO:
        {
            g_pCM->cf_UseAnisotropicFilter = ( ( CDXUTCheckBox* )pControl )->GetChecked();
            g_pEffect->SetBool( "g_bUseAnisotropic", g_pCM->cf_UseAnisotropicFilter );
            g_pEffect->SetInt( "g_MinFilter", g_pCM->cf_UseAnisotropicFilter ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR );
            g_pEffect->SetInt( "g_MaxAnisotropy", g_pCM->cf_UseAnisotropicFilter ? 8 : 1 );
            break;
        }
        case IDC_DISABLESPECULAR:
        {
            g_pCM->cf_DisableSpecular = ( ( CDXUTCheckBox* )pControl )->GetChecked();
            g_pEffect->SetBool( "g_bUseSpecular", !g_pCM->cf_DisableSpecular );
            break;
        }
        case IDC_FORCESHADER:
        {
            g_pCM->cf_ForceShader = ( DWORD )( size_t )( ( CDXUTComboBox* )pControl )->GetSelectedData();
            SetEffectTechnique();
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
    g_Cell.InvalidateDeviceObjects();
    g_Ball.InvalidateDeviceObjects();
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pDefaultTex );
    g_Cell.Destroy();
    g_Ball.Destroy();
}



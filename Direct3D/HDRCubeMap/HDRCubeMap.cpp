//--------------------------------------------------------------------------------------
// File: HDRCubeMap.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmesh.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Defines, and constants
//--------------------------------------------------------------------------------------
#define ENVMAPSIZE 256
#define NUM_LIGHTS 4  // Currently, 4 is the only number of lights supported.
#define LIGHTMESH_RADIUS 0.2f
#define HELPTEXTCOLOR D3DXCOLOR( 0.0f, 1.0f, 0.3f, 1.0f )


//--------------------------------------------------------------------------------------
D3DVERTEXELEMENT9 g_aVertDecl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
LPCTSTR g_atszMeshFileName[] =
{
    L"misc\\teapot.x",
    L"misc\\skullocc.x",
    L"misc\\car.x"
};

#define NUM_MESHES ( sizeof(g_atszMeshFileName) / sizeof(g_atszMeshFileName[0]) )

//--------------------------------------------------------------------------------------
struct ORBITDATA
{
    LPCTSTR tszXFile;  // X file
    D3DVECTOR vAxis;     // Axis of rotation
    float fRadius;   // Orbit radius
    float fSpeed;    // Orbit speed in radians per second
};


//--------------------------------------------------------------------------------------
// Mesh file to use for orbiter objects
// These don't have to use the same mesh file.
//--------------------------------------------------------------------------------------
ORBITDATA g_OrbitData[] =
{
    {L"airplane\\airplane 2.x", { -1.0f, 0.0f, 0.0f }, 2.0f, D3DX_PI * 1.0f },
    {L"airplane\\airplane 2.x", { 0.3f,  1.0f, 0.3f }, 2.5f, D3DX_PI / 2.0f }
};


//--------------------------------------------------------------------------------------
HRESULT ComputeBoundingSphere( CDXUTXFileMesh& Mesh, D3DXVECTOR3* pvCtr, float* pfRadius )
{
    HRESULT hr;

    // Obtain the bounding sphere
    LPD3DXMESH pMeshObj = Mesh.GetMesh();
    if( !pMeshObj )
        return E_FAIL;

    // Obtain the declaration
    D3DVERTEXELEMENT9 decl[MAX_FVF_DECL_SIZE];
    if( FAILED( hr = pMeshObj->GetDeclaration( decl ) ) )
        return hr;

    // Lock the vertex buffer
    LPVOID pVB;
    if( FAILED( hr = pMeshObj->LockVertexBuffer( D3DLOCK_READONLY, &pVB ) ) )
        return hr;

    // Compute the bounding sphere
    UINT uStride = D3DXGetDeclVertexSize( decl, 0 );
    hr = D3DXComputeBoundingSphere( ( const D3DXVECTOR3* )pVB, pMeshObj->GetNumVertices(),
                                    uStride, pvCtr, pfRadius );
    pMeshObj->UnlockVertexBuffer();

    return hr;
}




//--------------------------------------------------------------------------------------
// Encapsulate an object in the scene with its world transformation
// matrix.
//--------------------------------------------------------------------------------------
struct CObj
{
    D3DXMATRIXA16 m_mWorld;  // World transformation
    CDXUTXFileMesh m_Mesh;    // Mesh object

public:
            CObj()
            {
                D3DXMatrixIdentity( &m_mWorld );
            }
            ~CObj()
            {
            }
    HRESULT WorldCenterAndScaleToRadius( float fRadius )
    {
        //
        // Compute the world transformation matrix
        // to center the mesh at origin in world space
        // and scale its size to the specified radius.
        //
        HRESULT hr;

        float fRadiusBound;
        D3DXVECTOR3 vCtr;
        if( FAILED( hr = ::ComputeBoundingSphere( m_Mesh, &vCtr, &fRadiusBound ) ) )
            return hr;

        D3DXMatrixTranslation( &m_mWorld, -vCtr.x, -vCtr.y, -vCtr.z );
        D3DXMATRIXA16 mScale;
        D3DXMatrixScaling( &mScale, fRadius / fRadiusBound,
                           fRadius / fRadiusBound,
                           fRadius / fRadiusBound );
        D3DXMatrixMultiply( &m_mWorld, &m_mWorld, &mScale );

        return hr;
    }  // WorldCenterAndScaleToRadius
};


//--------------------------------------------------------------------------------------
// Encapsulate an orbiter object in the scene with related data
//--------------------------------------------------------------------------------------
class COrbiter : public CObj
{
public:
    D3DXVECTOR3 m_vAxis;       // orbit axis
    float m_fRadius;     // orbit radius
    float m_fSpeed;      // Speed, angle in radian per second

public:
            COrbiter() : m_vAxis( 0.0f, 1.0f, 0.0f ),
                         m_fRadius( 1.0f ),
                         m_fSpeed( D3DX_PI )
            {
            }
    void    SetOrbit( D3DXVECTOR3& vAxis, float fRadius, float fSpeed )
    {
        // Call this after m_mWorld is initialized
        m_vAxis = vAxis; m_fRadius = fRadius; m_fSpeed = fSpeed;
        D3DXVec3Normalize( &m_vAxis, &m_vAxis );

        // Translate by m_fRadius in -Y direction
        D3DXMATRIXA16 m;
        D3DXMatrixTranslation( &m, 0.0f, -m_fRadius, 0.0f );
        D3DXMatrixMultiply( &m_mWorld, &m_mWorld, &m );

        // Apply rotation from X axis to the orbit axis
        D3DXVECTOR3 vX( 1.0f, 0.0f, 0.0f );
        D3DXVECTOR3 vRot;
        D3DXVec3Cross( &vRot, &m_vAxis, &vX );  // Axis of rotation
        // If the cross product is 0, m_vAxis is on the X axis
        // So we either rotate 0 or PI.
        if( D3DXVec3LengthSq( &vRot ) == 0 )
        {
            if( m_vAxis.x < 0.0f )
                D3DXMatrixRotationY( &m, D3DX_PI ); // PI
            else
                D3DXMatrixIdentity( &m );           // 0
        }
        else
        {
            float fAng = ( float )acos( D3DXVec3Dot( &m_vAxis, &vX ) );  // Angle to rotate
            // Find out direction to rotate
            D3DXVECTOR3 vXxRot;  // X cross vRot
            D3DXVec3Cross( &vXxRot, &vX, &vRot );
            if( D3DXVec3Dot( &vXxRot, &m_vAxis ) >= 0 )
                fAng = -fAng;
            D3DXMatrixRotationAxis( &m, &vRot, fAng );
        }
        D3DXMatrixMultiply( &m_mWorld, &m_mWorld, &m );
    }

    void    Orbit( float fElapsedTime )
    {
        // Compute the orbit transformation and apply to m_mWorld
        D3DXMATRIXA16 m;

        D3DXMatrixRotationAxis( &m, &m_vAxis, m_fSpeed * fElapsedTime );
        D3DXMatrixMultiply( &m_mWorld, &m_mWorld, &m );
    }
};


//--------------------------------------------------------------------------------------
#pragma warning( disable : 4324 )
struct CLight
{
    D3DXVECTOR4 vPos;      // Position in world space
    D3DXVECTOR4 vMoveDir;  // Direction in which it moves
    float fMoveDist; // Maximum distance it can move
    D3DXMATRIXA16 mWorld;    // World transform matrix for the light before animation
    D3DXMATRIXA16 mWorking;  // Working matrix (world transform after animation)
};


//--------------------------------------------------------------------------------------
struct CTechniqueGroup
{
    D3DXHANDLE hRenderScene;
    D3DXHANDLE hRenderLight;
    D3DXHANDLE hRenderEnvMap;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                      g_pFont = NULL;            // Font for drawing text
ID3DXSprite*                    g_pTextSprite = NULL;      // Sprite for batching draw text calls
ID3DXEffect*                    g_pEffect = NULL;          // D3DX effect interface
CModelViewerCamera              g_Camera;                  // A model viewing camera
bool                            g_bShowHelp = true;        // If true, it renders the UI control text
CDXUTDialogResourceManager      g_DialogResourceManager;   // manager for shared resources of dialogs
CD3DSettingsDlg                 g_SettingsDlg;             // Device settings dialog
CDXUTDialog                     g_HUD;                     // dialog for standard controls
CLight g_aLights[NUM_LIGHTS];     // Parameters of light objects
D3DXVECTOR4                     g_vLightIntensity;         // Light intensity
CObj g_EnvMesh[NUM_MESHES];     // Mesh to receive environment mapping
int                             g_nCurrMesh = 0;           // Index of the element in m_EnvMesh we are currently displaying
CDXUTXFileMesh                  g_Room;                    // Mesh representing room (wall, floor, ceiling)
COrbiter g_Orbiter[sizeof( g_OrbitData ) / sizeof( g_OrbitData[0] )]; // Orbiter meshes
CDXUTXFileMesh                  g_LightMesh;               // Mesh for the light object
IDirect3DVertexDeclaration9*    g_pVertDecl = NULL;        // Vertex decl for the sample
IDirect3DCubeTexture9*          g_apCubeMapFp[2];          // Floating point format cube map
IDirect3DCubeTexture9*          g_pCubeMap32 = NULL;       // 32-bit cube map (for fallback)
IDirect3DSurface9*              g_pDepthCube = NULL;       // Depth-stencil buffer for rendering to cube texture
int                             g_nNumFpCubeMap = 0;       // Number of cube maps required for using floating point
CTechniqueGroup g_aTechGroupFp[2];         // Group of techniques to use with floating pointcubemaps (2 cubes max)
CTechniqueGroup                 g_TechGroup32;             // Group of techniques to use with integer cubemaps
D3DXHANDLE                      g_hWorldView = NULL;       // Handle for world+view matrix in effect
D3DXHANDLE                      g_hProj = NULL;            // Handle for projection matrix in effect
D3DXHANDLE g_ahtxCubeMap[2];          // Handle for the cube texture in effect
D3DXHANDLE                      g_htxScene = NULL;         // Handle for the scene texture in effect
D3DXHANDLE                      g_hLightIntensity = NULL;  // Handle for the light intensity in effect
D3DXHANDLE                      g_hLightPosView = NULL;    // Handle for view space light positions in effect
D3DXHANDLE                      g_hReflectivity = NULL;    // Handle for reflectivity in effect

int                             g_nNumCubes;               // Number of cube maps used based on current cubemap format
IDirect3DCubeTexture9**         g_apCubeMap = g_apCubeMapFp;// Cube map(s) to use based on current cubemap format
CTechniqueGroup*                g_pTech = g_aTechGroupFp; // Techniques to be used based on cubemap format

float                           g_fWorldRotX = 0.0f;       // World rotation state X-axis
float                           g_fWorldRotY = 0.0f;       // World rotation state Y-axis
bool                            g_bUseFloatCubeMap;        // Whether we use floating point format cube map
float                           g_fReflectivity;           // Reflectivity value. Ranges from 0 to 1.


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN  1
#define IDC_TOGGLEREF         3
#define IDC_CHANGEDEVICE      4
#define IDC_CHANGEMESH        5
#define IDC_RESETPARAM        6
#define IDC_SLIDERLIGHTTEXT   7
#define IDC_SLIDERLIGHT       8
#define IDC_SLIDERREFLECTTEXT 9
#define IDC_SLIDERREFLECT     10
#define IDC_CHECKHDR          11


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
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, LPCWSTR wszName, CDXUTXFileMesh& Mesh );
void RenderText();
void ResetParameters();
void RenderSceneIntoCubeMap( IDirect3DDevice9* pd3dDevice, double fTime );
void RenderScene( IDirect3DDevice9* pd3dDevice, const D3DXMATRIX* pmView, const D3DXMATRIX* pmProj,
                  CTechniqueGroup* pTechGroup, bool bRenderEnvMappedMesh, double fTime );
void UpdateUiWithChanges();


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

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
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
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys
    DXUTCreateWindow( L"HDRCubeMap" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Change CheckBox default visual style
    CDXUTElement* pElement = g_HUD.GetDefaultElement( DXUT_CONTROL_CHECKBOX, 0 );
    if( pElement )
    {
        pElement->FontColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 255, 0, 255, 0 );
    }

    // Change Static default visual style
    pElement = g_HUD.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->dwTextFormat = DT_LEFT | DT_VCENTER;
        pElement->FontColor.States[ DXUT_STATE_NORMAL ] = D3DCOLOR_ARGB( 255, 0, 255, 0 );
    }

    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_HUD.AddButton( IDC_CHANGEMESH, L"Change Mesh (N)", 35, iY += 24, 125, 22, L'N' );
    g_HUD.AddButton( IDC_RESETPARAM, L"Reset Parameters (R)", 35, iY += 24, 125, 22, L'R' );
    g_HUD.AddCheckBox( IDC_CHECKHDR, L"Use HDR Texture (F)", 35, iY += 24, 130, 22, true, L'F' );
    g_HUD.AddStatic( IDC_SLIDERLIGHTTEXT, L"Light Intensity", 35, iY += 24, 125, 16 );
    g_HUD.AddSlider( IDC_SLIDERLIGHT, 35, iY += 17, 125, 22, 0, 1500 );
    g_HUD.AddStatic( IDC_SLIDERREFLECTTEXT, L"Reflectivity", 35, iY += 24, 125, 16 );
    g_HUD.AddSlider( IDC_SLIDERREFLECT, 35, iY += 17, 125, 22, 0, 100 );

    ResetParameters();

    // Initialize camera parameters
    g_Camera.SetModelCenter( D3DXVECTOR3( 0.0f, 0.0f, 0.0f ) );
    g_Camera.SetRadius( 3.0f );
    g_Camera.SetEnablePositionMovement( false );

    // Set the light positions
    g_aLights[0].vPos = D3DXVECTOR4( -3.5f, 2.3f, -4.0f, 1.0f );
    g_aLights[0].vMoveDir = D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 0.0f );
    g_aLights[0].fMoveDist = 8.0f;
#if NUM_LIGHTS > 1
    g_aLights[1].vPos = D3DXVECTOR4( 3.5f, 2.3f, 4.0f, 1.0f );
    g_aLights[1].vMoveDir = D3DXVECTOR4( 0.0f, 0.0f, -1.0f, 0.0f );
    g_aLights[1].fMoveDist = 8.0f;
#endif
#if NUM_LIGHTS > 2
    g_aLights[2].vPos = D3DXVECTOR4( -3.5f, 2.3f, 4.0f, 1.0f );
    g_aLights[2].vMoveDir = D3DXVECTOR4( 1.0f, 0.0f, 0.0f, 0.0f );
    g_aLights[2].fMoveDist = 7.0f;
#endif
#if NUM_LIGHTS > 3
    g_aLights[3].vPos = D3DXVECTOR4( 3.5f, 2.3f, -4.0f, 1.0f );
    g_aLights[3].vMoveDir = D3DXVECTOR4( -1.0f, 0.0f, 0.0f, 0.0f );
    g_aLights[3].fMoveDist = 7.0f;
#endif
}


//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    // Must support cube textures
    if( !( pCaps->TextureCaps & D3DPTEXTURECAPS_CUBEMAP ) )
        return false;

    // Must support pixel shader 2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // need to support D3DFMT_A16B16G16R16F render target
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F ) ) )
    {
        // If not, need to support D3DFMT_G16R16F render target as fallback
        if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                             AdapterFormat, D3DUSAGE_RENDERTARGET,
                                             D3DRTYPE_CUBETEXTURE, D3DFMT_G16R16F ) ) )
            return false;
    }

    // need to support D3DFMT_A8R8G8B8 render target
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_CUBETEXTURE, D3DFMT_A8R8G8B8 ) ) )
    {
        return false;
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    // Initialize the number of cube maps required when using floating point format
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );
    D3DCAPS9 caps;
    HRESULT hr;
    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &caps ) );

    if( FAILED( pD3D->CheckDeviceFormat( caps.AdapterOrdinal, caps.DeviceType,
                                         pDeviceSettings->d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F ) ) )
    {
        g_nNumCubes = g_nNumFpCubeMap = 2;
    }
    else
    {
        g_nNumCubes = g_nNumFpCubeMap = 1;
    }

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


//-----------------------------------------------------------------------------
// Reset light and material parameters to default values
//-----------------------------------------------------------------------------
void ResetParameters()
{
    HRESULT hr;

    g_bUseFloatCubeMap = true;
    g_fReflectivity = 0.4f;
    g_vLightIntensity = D3DXVECTOR4( 24.0f, 24.0f, 24.0f, 24.0f );

    if( g_pEffect )
    {
        V( g_pEffect->SetFloat( g_hReflectivity, g_fReflectivity ) );
        V( g_pEffect->SetVector( g_hLightIntensity, &g_vLightIntensity ) );
    }

    UpdateUiWithChanges();
}


//--------------------------------------------------------------------------------------
// Write the updated values to the static UI controls
void UpdateUiWithChanges()
{
    CDXUTStatic* pStatic = g_HUD.GetStatic( IDC_SLIDERLIGHTTEXT );
    if( pStatic )
    {
        WCHAR wszText[128];
        swprintf_s( wszText, 128, L"Light intensity: %0.2f", g_vLightIntensity.x );
        pStatic->SetText( wszText );
    }
    pStatic = g_HUD.GetStatic( IDC_SLIDERREFLECTTEXT );
    if( pStatic )
    {
        WCHAR wszText[128];
        swprintf_s( wszText, 128, L"Reflectivity: %0.2f", g_fReflectivity );
        pStatic->SetText( wszText );
    }
    CDXUTSlider* pSlider = g_HUD.GetSlider( IDC_SLIDERLIGHT );
    if( pSlider )
    {
        pSlider->SetValue( int( g_vLightIntensity.x * 10.0f + 0.5f ) );
    }
    pSlider = g_HUD.GetSlider( IDC_SLIDERREFLECT );
    if( pSlider )
    {
        pSlider->SetValue( int( g_fReflectivity * 100.0f + 0.5f ) );
    }
    CDXUTCheckBox* pCheck = g_HUD.GetCheckBox( IDC_CHECKHDR );
    if( pCheck )
        pCheck->SetChecked( g_bUseFloatCubeMap );
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;


    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
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
#ifdef D3DXFX_LARGEADDRESS_HANDLE
    dwShaderFlags |= D3DXFX_LARGEADDRESSAWARE;
#endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"HDRCubeMap.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    g_hWorldView = g_pEffect->GetParameterByName( NULL, "g_mWorldView" );
    g_hProj = g_pEffect->GetParameterByName( NULL, "g_mProj" );
    g_ahtxCubeMap[0] = g_pEffect->GetParameterByName( NULL, "g_txCubeMap" );
    g_ahtxCubeMap[1] = g_pEffect->GetParameterByName( NULL, "g_txCubeMap2" );
    g_htxScene = g_pEffect->GetParameterByName( NULL, "g_txScene" );
    g_hLightIntensity = g_pEffect->GetParameterByName( NULL, "g_vLightIntensity" );
    g_hLightPosView = g_pEffect->GetParameterByName( NULL, "g_vLightPosView" );
    g_hReflectivity = g_pEffect->GetParameterByName( NULL, "g_fReflectivity" );

    // Determine the technique to render with

    // Integer cube map
    g_TechGroup32.hRenderScene = g_pEffect->GetTechniqueByName( "RenderScene" );
    g_TechGroup32.hRenderLight = g_pEffect->GetTechniqueByName( "RenderLight" );
    g_TechGroup32.hRenderEnvMap = g_pEffect->GetTechniqueByName( "RenderHDREnvMap" );

    ZeroMemory( g_aTechGroupFp, sizeof( g_aTechGroupFp ) );
    if( g_nNumFpCubeMap == 2 )
    {
        // Two floating point G16R16F cube maps
        g_aTechGroupFp[0].hRenderScene = g_pEffect->GetTechniqueByName( "RenderSceneFirstHalf" );
        g_aTechGroupFp[0].hRenderLight = g_pEffect->GetTechniqueByName( "RenderLightFirstHalf" );
        g_aTechGroupFp[0].hRenderEnvMap = g_pEffect->GetTechniqueByName( "RenderHDREnvMap2Tex" );
        g_aTechGroupFp[1].hRenderScene = g_pEffect->GetTechniqueByName( "RenderSceneSecondHalf" );
        g_aTechGroupFp[1].hRenderLight = g_pEffect->GetTechniqueByName( "RenderLightSecondHalf" );
        g_aTechGroupFp[1].hRenderEnvMap = g_pEffect->GetTechniqueByName( "RenderHDREnvMap2Tex" );
    }
    else
    {
        // Single floating point cube map
        g_aTechGroupFp[0].hRenderScene = g_pEffect->GetTechniqueByName( "RenderScene" );
        g_aTechGroupFp[0].hRenderLight = g_pEffect->GetTechniqueByName( "RenderLight" );
        g_aTechGroupFp[0].hRenderEnvMap = g_pEffect->GetTechniqueByName( "RenderHDREnvMap" );
    }

    // Initialize reflectivity
    V_RETURN( g_pEffect->SetFloat( g_hReflectivity, g_fReflectivity ) );

    // Initialize light intensity
    V_RETURN( g_pEffect->SetVector( g_hLightIntensity, &g_vLightIntensity ) );

    // Create vertex declaration
    V_RETURN( pd3dDevice->CreateVertexDeclaration( g_aVertDecl, &g_pVertDecl ) );

    // Load the mesh
    int i;
    for( i = 0; i < NUM_MESHES; ++i )
    {
        if( FAILED( LoadMesh( pd3dDevice, g_atszMeshFileName[i], g_EnvMesh[i].m_Mesh ) ) )
            return DXUTERR_MEDIANOTFOUND;
        g_EnvMesh[i].WorldCenterAndScaleToRadius( 1.0f );  // Scale to radius of 1
    }
    // Load the room object
    if( FAILED( LoadMesh( pd3dDevice, L"room.x", g_Room ) ) )
        return DXUTERR_MEDIANOTFOUND;
    // Load the light object
    if( FAILED( LoadMesh( pd3dDevice, L"misc\\sphere0.x", g_LightMesh ) ) )
        return DXUTERR_MEDIANOTFOUND;
    // Initialize the world matrix for the lights
    D3DXVECTOR3 vCtr;
    float fRadius;
    if( FAILED( ComputeBoundingSphere( g_LightMesh, &vCtr, &fRadius ) ) )
        return E_FAIL;
    D3DXMATRIXA16 mWorld, m;
    D3DXMatrixTranslation( &mWorld, -vCtr.x, -vCtr.y, -vCtr.z );
    D3DXMatrixScaling( &m, LIGHTMESH_RADIUS / fRadius,
                       LIGHTMESH_RADIUS / fRadius,
                       LIGHTMESH_RADIUS / fRadius );
    D3DXMatrixMultiply( &mWorld, &mWorld, &m );
    for( i = 0; i < NUM_LIGHTS; ++i )
    {
        D3DXMatrixTranslation( &m, g_aLights[i].vPos.x,
                               g_aLights[i].vPos.y,
                               g_aLights[i].vPos.z );
        D3DXMatrixMultiply( &g_aLights[i].mWorld, &mWorld, &m );
    }

    // Orbiter
    for( i = 0; i < sizeof( g_Orbiter ) / sizeof( g_Orbiter[0] ); ++i )
    {
        if( FAILED( LoadMesh( pd3dDevice, g_OrbitData[i].tszXFile, g_Orbiter[i].m_Mesh ) ) )
            return DXUTERR_MEDIANOTFOUND;

        g_Orbiter[i].WorldCenterAndScaleToRadius( 0.7f );
        D3DXVECTOR3 vAxis( g_OrbitData[i].vAxis );
        g_Orbiter[i].SetOrbit( vAxis, g_OrbitData[i].fRadius, g_OrbitData[i].fSpeed );
    }

    // World transform to identity
    D3DXMATRIXA16 mIdent;
    D3DXMatrixIdentity( &mIdent );
    V_RETURN( pd3dDevice->SetTransform( D3DTS_WORLD, &mIdent ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vFromPt( 0.0f, 0.0f, -2.5f );
    D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vFromPt, &vLookatPt );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Load mesh from file and convert vertices to our format
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, LPCWSTR wszName, CDXUTXFileMesh& Mesh )
{
    HRESULT hr;

    if( FAILED( hr = Mesh.Create( pd3dDevice, wszName ) ) )
        return hr;
    hr = Mesh.SetVertexDecl( pd3dDevice, g_aVertDecl );

    return hr;
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

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    int iY = pBackBufferSurfaceDesc->Height - 170;
    CDXUTControl* pControl;
    if( ( pControl = g_HUD.GetControl( IDC_CHECKHDR ) ) != NULL )
        pControl->SetLocation( 35, iY );
    if( ( pControl = g_HUD.GetControl( IDC_CHANGEMESH ) ) != NULL )
        pControl->SetLocation( 35, iY += 24 );
    if( ( pControl = g_HUD.GetControl( IDC_RESETPARAM ) ) != NULL )
        pControl->SetLocation( 35, iY += 24 );
    if( ( pControl = g_HUD.GetControl( IDC_SLIDERLIGHTTEXT ) ) != NULL )
        pControl->SetLocation( 35, iY += 35 );
    if( ( pControl = g_HUD.GetControl( IDC_SLIDERLIGHT ) ) != NULL )
        pControl->SetLocation( 35, iY += 17 );
    if( ( pControl = g_HUD.GetControl( IDC_SLIDERREFLECTTEXT ) ) != NULL )
        pControl->SetLocation( 35, iY += 24 );
    if( ( pControl = g_HUD.GetControl( IDC_SLIDERREFLECT ) ) != NULL )
        pControl->SetLocation( 35, iY += 17 );

    // Restore meshes
    for( int i = 0; i < NUM_MESHES; ++i )
        g_EnvMesh[i].m_Mesh.RestoreDeviceObjects( pd3dDevice );
    g_Room.RestoreDeviceObjects( pd3dDevice );
    g_LightMesh.RestoreDeviceObjects( pd3dDevice );
    for( int i = 0; i < sizeof( g_Orbiter ) / sizeof( g_Orbiter[0] ); ++i )
        g_Orbiter[i].m_Mesh.RestoreDeviceObjects( pd3dDevice );

    // Create the cube textures
    ZeroMemory( g_apCubeMapFp, sizeof( g_apCubeMapFp ) );
    hr = pd3dDevice->CreateCubeTexture( ENVMAPSIZE,
                                        1,
                                        D3DUSAGE_RENDERTARGET,
                                        D3DFMT_A16B16G16R16F,
                                        D3DPOOL_DEFAULT,
                                        &g_apCubeMapFp[0],
                                        NULL );

    if( FAILED( hr ) )
    {
        // Create 2 G16R16 textures as fallback
        V_RETURN( pd3dDevice->CreateCubeTexture( ENVMAPSIZE,
                                                 1,
                                                 D3DUSAGE_RENDERTARGET,
                                                 D3DFMT_G16R16F,
                                                 D3DPOOL_DEFAULT,
                                                 &g_apCubeMapFp[0],
                                                 NULL ) );
        V_RETURN( pd3dDevice->CreateCubeTexture( ENVMAPSIZE,
                                                 1,
                                                 D3DUSAGE_RENDERTARGET,
                                                 D3DFMT_G16R16F,
                                                 D3DPOOL_DEFAULT,
                                                 &g_apCubeMapFp[1],
                                                 NULL ) );
    }

    V_RETURN( pd3dDevice->CreateCubeTexture( ENVMAPSIZE,
                                             1,
                                             D3DUSAGE_RENDERTARGET,
                                             D3DFMT_A8R8G8B8,
                                             D3DPOOL_DEFAULT,
                                             &g_pCubeMap32,
                                             NULL ) );

    // Create the stencil buffer to be used with the cube textures
    // Create the depth-stencil buffer to be used with the shadow map
    // We do this to ensure that the depth-stencil buffer is large
    // enough and has correct multisample type/quality when rendering
    // the shadow map.  The default depth-stencil buffer created during
    // device creation will not be large enough if the user resizes the
    // window to a very small size.  Furthermore, if the device is created
    // with multisampling, the default depth-stencil buffer will not
    // work with the shadow map texture because texture render targets
    // do not support multisample.
    DXUTDeviceSettings d3dSettings = DXUTGetDeviceSettings();
    V_RETURN( pd3dDevice->CreateDepthStencilSurface( ENVMAPSIZE,
                                                     ENVMAPSIZE,
                                                     d3dSettings.d3d9.pp.AutoDepthStencilFormat,
                                                     D3DMULTISAMPLE_NONE,
                                                     0,
                                                     TRUE,
                                                     &g_pDepthCube,
                                                     NULL ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    for( int i = 0; i < sizeof( g_Orbiter ) / sizeof( g_Orbiter[0] ); ++i )
        g_Orbiter[i].Orbit( fElapsedTime );
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

    RenderSceneIntoCubeMap( pd3dDevice, fTime );

    // Clear the render buffers
    V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                          0x000000ff, 1.0f, 0L ) );

    // Begin the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        RenderScene( pd3dDevice, g_Camera.GetViewMatrix(), g_Camera.GetProjMatrix(), g_pTech, true, fTime );

        // Render stats and help text
        RenderText();

        g_HUD.OnRender( fElapsedTime );

        pd3dDevice->EndScene();
    }
}


//--------------------------------------------------------------------------------------
// Set up the cube map by rendering the scene into it.
//--------------------------------------------------------------------------------------
void RenderSceneIntoCubeMap( IDirect3DDevice9* pd3dDevice, double fTime )
{
    HRESULT hr;

    // The projection matrix has a FOV of 90 degrees and asp ratio of 1
    D3DXMATRIXA16 mProj;
    D3DXMatrixPerspectiveFovLH( &mProj, D3DX_PI * 0.5f, 1.0f, 0.01f, 100.0f );

    D3DXMATRIXA16 mViewDir( *g_Camera.GetViewMatrix() );
    mViewDir._41 = mViewDir._42 = mViewDir._43 = 0.0f;

    LPDIRECT3DSURFACE9 pRTOld = NULL;
    V( pd3dDevice->GetRenderTarget( 0, &pRTOld ) );
    LPDIRECT3DSURFACE9 pDSOld = NULL;
    if( SUCCEEDED( pd3dDevice->GetDepthStencilSurface( &pDSOld ) ) )
    {
        // If the device has a depth-stencil buffer, use
        // the depth stencil buffer created for the cube textures.
        V( pd3dDevice->SetDepthStencilSurface( g_pDepthCube ) );
    }

    for( int nCube = 0; nCube < g_nNumCubes; ++nCube )
        for( int nFace = 0; nFace < 6; ++nFace )
        {
            LPDIRECT3DSURFACE9 pSurf;

            V( g_apCubeMap[nCube]->GetCubeMapSurface( ( D3DCUBEMAP_FACES )nFace, 0, &pSurf ) );
            V( pd3dDevice->SetRenderTarget( 0, pSurf ) );
            SAFE_RELEASE( pSurf );

            D3DXMATRIXA16 mView = DXUTGetCubeMapViewMatrix( nFace );
            D3DXMatrixMultiply( &mView, &mViewDir, &mView );

            V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER,
                                  0x000000ff, 1.0f, 0L ) );

            // Begin the scene
            if( SUCCEEDED( pd3dDevice->BeginScene() ) )
            {
                RenderScene( pd3dDevice, &mView, &mProj, &g_pTech[nCube], false, fTime );

                // End the scene.
                pd3dDevice->EndScene();
            }
        }

    // Restore depth-stencil buffer and render target
    if( pDSOld )
    {
        V( pd3dDevice->SetDepthStencilSurface( pDSOld ) );
        SAFE_RELEASE( pDSOld );
    }
    V( pd3dDevice->SetRenderTarget( 0, pRTOld ) );
    SAFE_RELEASE( pRTOld );
}


//--------------------------------------------------------------------------------------
// Renders the scene with a specific view and projection matrix.
//--------------------------------------------------------------------------------------
void RenderScene( IDirect3DDevice9* pd3dDevice, const D3DXMATRIX* pmView, const D3DXMATRIX* pmProj,
                  CTechniqueGroup* pTechGroup, bool bRenderEnvMappedMesh, double fTime )
{
    HRESULT hr;
    UINT p, cPass;
    D3DXMATRIXA16 mWorldView;

    V( g_pEffect->SetMatrix( g_hProj, pmProj ) );

    // Write camera-space light positions to effect
    D3DXVECTOR4 avLightPosView[NUM_LIGHTS];
    for( int i = 0; i < NUM_LIGHTS; ++i )
    {
        // Animate the lights
        float fDisp = ( 1.0f + cosf( fmodf( ( float )fTime, D3DX_PI ) ) ) * 0.5f * g_aLights[i].fMoveDist; // Distance to move
        D3DXVECTOR4 vMove = g_aLights[i].vMoveDir * fDisp;  // In vector form
        D3DXMatrixTranslation( &g_aLights[i].mWorking, vMove.x, vMove.y, vMove.z ); // Matrix form
        D3DXMatrixMultiply( &g_aLights[i].mWorking, &g_aLights[i].mWorld, &g_aLights[i].mWorking );
        vMove += g_aLights[i].vPos;  // Animated world coordinates
        D3DXVec4Transform( &avLightPosView[i], &vMove, pmView );
    }
    V( g_pEffect->SetVectorArray( g_hLightPosView, avLightPosView, NUM_LIGHTS ) );

    //
    // Render the environment-mapped mesh if specified
    //

    if( bRenderEnvMappedMesh )
    {
        V( g_pEffect->SetTechnique( pTechGroup->hRenderEnvMap ) );

        // Combine the offset and scaling transformation with
        // rotation from the camera to form the final
        // world transformation matrix.
        D3DXMatrixMultiply( &mWorldView, &g_EnvMesh[g_nCurrMesh].m_mWorld, g_Camera.GetWorldMatrix() );
        D3DXMatrixMultiply( &mWorldView, &mWorldView, pmView );
        V( g_pEffect->SetMatrix( g_hWorldView, &mWorldView ) );

        V( g_pEffect->Begin( &cPass, 0 ) );

        for( int i = 0; i < g_nNumCubes; ++i )
            V( g_pEffect->SetTexture( g_ahtxCubeMap[i], g_apCubeMap[i] ) );

        for( p = 0; p < cPass; ++p )
        {
            V( g_pEffect->BeginPass( p ) );
            LPD3DXMESH pMesh = g_EnvMesh[g_nCurrMesh].m_Mesh.GetMesh();
            for( DWORD i = 0; i < g_EnvMesh[g_nCurrMesh].m_Mesh.m_dwNumMaterials; ++i )
                V( pMesh->DrawSubset( i ) );
            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );
    }

    //
    // Render light spheres
    //

    V( g_pEffect->SetTechnique( pTechGroup->hRenderLight ) );

    V( g_pEffect->Begin( &cPass, 0 ) );
    for( p = 0; p < cPass; ++p )
    {
        V( g_pEffect->BeginPass( p ) );

        for( int i = 0; i < NUM_LIGHTS; ++i )
        {
            D3DXMatrixMultiply( &mWorldView, &g_aLights[i].mWorking, pmView );
            V( g_pEffect->SetMatrix( g_hWorldView, &mWorldView ) );
            V( g_pEffect->CommitChanges() );
            V( g_LightMesh.Render( pd3dDevice ) );
        }
        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );

    //
    // Render the rest of the scene
    //

    V( g_pEffect->SetTechnique( pTechGroup->hRenderScene ) );

    V( g_pEffect->Begin( &cPass, 0 ) );
    for( p = 0; p < cPass; ++p )
    {
        LPD3DXMESH pMeshObj;
        V( g_pEffect->BeginPass( p ) );

        //
        // Orbiters
        //
        for( int i = 0; i < sizeof( g_Orbiter ) / sizeof( g_Orbiter[0] ); ++i )
        {
            D3DXMatrixMultiply( &mWorldView, &g_Orbiter[i].m_mWorld, pmView );
            V( g_pEffect->SetMatrix( g_hWorldView, &mWorldView ) );
            // Obtain the D3DX mesh object
            pMeshObj = g_Orbiter[i].m_Mesh.GetMesh();
            // Iterate through each subset and render with its texture
            for( DWORD m = 0; m < g_Orbiter[i].m_Mesh.m_dwNumMaterials; ++m )
            {
                V( g_pEffect->SetTexture( g_htxScene, g_Orbiter[i].m_Mesh.m_pTextures[m] ) );
                V( g_pEffect->CommitChanges() );
                V( pMeshObj->DrawSubset( m ) );
            }
        }

        //
        // The room object (walls, floor, ceiling)
        //

        V( g_pEffect->SetMatrix( g_hWorldView, pmView ) );
        pMeshObj = g_Room.GetMesh();
        // Iterate through each subset and render with its texture
        for( DWORD m = 0; m < g_Room.m_dwNumMaterials; ++m )
        {
            V( g_pEffect->SetTexture( g_htxScene, g_Room.m_pTextures[m] ) );
            V( g_pEffect->CommitChanges() );
            V( pMeshObj->DrawSubset( m ) );
        }
        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );
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
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );

    // Draw help
    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
    if( g_bShowHelp )
    {
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 12 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls:" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 11 );
        txtHelper.DrawTextLine( L"Rotate object: Left drag mouse\n"
                                L"Adjust camera: Right drag mouse\nZoom In/Out: Mouse wheel\n"
                                L"Adjust reflectivity: E,D\nAdjust light intensity: W,S\n"
                                L"Hide help: F1\n"
                                L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }

    txtHelper.SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 48 );
    txtHelper.DrawTextLine( L"Cube map format\n"
                            L"Material reflectivity ( e/E, d/D )\n"
                            L"Light intensity ( w/W, s/S )\n" );
    txtHelper.SetInsertionPos( 190, pd3dsdBackBuffer->Height - 48 );
    txtHelper.DrawFormattedTextLine( L"%s\n%.2f\n%.1f",
                                     g_bUseFloatCubeMap ?
                                     L"Floating-point ( D3D9 / HDR )" :
                                     L"Integer ( D3D8 )",
                                     g_fReflectivity, g_vLightIntensity.x );

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

    HRESULT hr;

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    switch( uMsg )
    {
            //
            // Use WM_CHAR to handle parameter adjustment so
            // that we can control the granularity based on
            // the letter cases.
        case WM_CHAR:
        {
            switch( wParam )
            {
                case 'W':
                case 'w':
                    if( 'w' == wParam )
                        g_vLightIntensity += D3DXVECTOR4( 0.1f, 0.1f, 0.1f, 0.0f );
                    else
                        g_vLightIntensity += D3DXVECTOR4( 10.0f, 10.0f, 10.0f, 0.0f );
                    if( g_vLightIntensity.x > 150.0f )
                    {
                        g_vLightIntensity.x =
                            g_vLightIntensity.y =
                            g_vLightIntensity.z = 150.0f;
                    }
                    V( g_pEffect->SetVector( g_hLightIntensity, &g_vLightIntensity ) );
                    UpdateUiWithChanges();
                    break;

                case 'S':
                case 's':
                    if( 's' == wParam )
                        g_vLightIntensity -= D3DXVECTOR4( 0.1f, 0.1f, 0.1f, 0.0f );
                    else
                        g_vLightIntensity -= D3DXVECTOR4( 10.0f, 10.0f, 10.0f, 0.0f );
                    if( g_vLightIntensity.x < 0.0f )
                        g_vLightIntensity.x =
                            g_vLightIntensity.y =
                            g_vLightIntensity.z = 0.0f;
                    V( g_pEffect->SetVector( g_hLightIntensity, &g_vLightIntensity ) );
                    UpdateUiWithChanges();
                    break;

                case 'D':
                case 'd':
                    if( 'd' == wParam )
                        g_fReflectivity -= 0.01f;
                    else
                        g_fReflectivity -= 0.1f;
                    if( g_fReflectivity < 0 )
                        g_fReflectivity = 0;
                    V( g_pEffect->SetFloat( g_hReflectivity, g_fReflectivity ) );
                    UpdateUiWithChanges();
                    break;

                case 'E':
                case 'e':
                    if( 'e' == wParam )
                        g_fReflectivity += 0.01f;
                    else
                        g_fReflectivity += 0.1f;
                    if( g_fReflectivity > 1.0f )
                        g_fReflectivity = 1.0f;
                    V( g_pEffect->SetFloat( g_hReflectivity, g_fReflectivity ) );
                    UpdateUiWithChanges();
                    break;
            }

            return 0;
        }
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
        case IDC_CHECKHDR:
            g_bUseFloatCubeMap = ( ( CDXUTCheckBox* )pControl )->GetChecked();
            if( g_bUseFloatCubeMap )
            {
                g_nNumCubes = g_nNumFpCubeMap;
                g_apCubeMap = g_apCubeMapFp;
                g_pTech = g_aTechGroupFp;
            }
            else
            {
                g_nNumCubes = 1;
                g_apCubeMap = &g_pCubeMap32;
                g_pTech = &g_TechGroup32;
            }

            break;
        case IDC_CHANGEMESH:
            if( ++g_nCurrMesh == NUM_MESHES )
                g_nCurrMesh = 0;
            break;

        case IDC_RESETPARAM:
            ResetParameters();
            break;

        case IDC_SLIDERLIGHT:
            if( nEvent == EVENT_SLIDER_VALUE_CHANGED )
            {
                CDXUTSlider* pSlider = ( CDXUTSlider* )pControl;
                g_vLightIntensity.x =
                    g_vLightIntensity.y =
                    g_vLightIntensity.z = pSlider->GetValue() * 0.1f;
                if( g_pEffect )
                    g_pEffect->SetVector( g_hLightIntensity, &g_vLightIntensity );
                CDXUTStatic* pStatic = g_HUD.GetStatic( IDC_SLIDERLIGHTTEXT );
                if( pStatic )
                {
                    WCHAR wszText[128];
                    swprintf_s( wszText, 128, L"Light intensity: %0.2f", g_vLightIntensity.x );
                    pStatic->SetText( wszText );
                }
            }
            break;

        case IDC_SLIDERREFLECT:
            if( nEvent == EVENT_SLIDER_VALUE_CHANGED )
            {
                CDXUTSlider* pSlider = ( CDXUTSlider* )pControl;
                g_fReflectivity = pSlider->GetValue() * 0.01f;
                if( g_pEffect )
                    g_pEffect->SetFloat( g_hReflectivity, g_fReflectivity );
                UpdateUiWithChanges();
                CDXUTStatic* pStatic = g_HUD.GetStatic( IDC_SLIDERREFLECTTEXT );
                if( pStatic )
                {
                    WCHAR wszText[128];
                    swprintf_s( wszText, 128, L"Reflectivity: %0.2f", g_fReflectivity );
                    pStatic->SetText( wszText );
                }
            }
            break;
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnResetDevice callback 
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

    for( int i = 0; i < NUM_MESHES; ++i )
        g_EnvMesh[i].m_Mesh.InvalidateDeviceObjects();
    g_Room.InvalidateDeviceObjects();
    g_LightMesh.InvalidateDeviceObjects();
    for( int i = 0; i < sizeof( g_Orbiter ) / sizeof( g_Orbiter[0] ); ++i )
        g_Orbiter[i].m_Mesh.InvalidateDeviceObjects();

    SAFE_RELEASE( g_apCubeMapFp[0] );
    SAFE_RELEASE( g_apCubeMapFp[1] );
    SAFE_RELEASE( g_pCubeMap32 );
    SAFE_RELEASE( g_pDepthCube );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnCreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pVertDecl );

    g_Room.Destroy();
    g_LightMesh.Destroy();
    for( int i = 0; i < sizeof( g_Orbiter ) / sizeof( g_Orbiter[0] ); ++i )
        g_Orbiter[i].m_Mesh.Destroy();
    for( int i = 0; i < NUM_MESHES; ++i )
        g_EnvMesh[i].m_Mesh.Destroy();
}


//--------------------------------------------------------------------------------------
// File: EffectParam.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "resource.h"
#include <crtdbg.h>

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

// SCROLL_TIME dictates the time one scroll op takes, in seconds.
#define SCROLL_TIME 0.5f


//--------------------------------------------------------------------------------------
struct MESHLISTDATA
{
    WCHAR   wszName[MAX_PATH];
    WCHAR   wszFile[MAX_PATH];
    DWORD dwNumMat;  // Number of materials.  To be filled in when loading this mesh.
} g_MeshListData[] =
{
    { L"Car", L"car2.x", 0 },
    { L"Banded Earth", L"sphereband.x", 0 },
    { L"Dwarf", L"dwarf\\DwarfWithEffectInstance.x", 0 },
    { L"Virus", L"cytovirus.x", 0 },
    { L"Car", L"car2.x", 0 },
    { L"Banded Earth", L"sphereband.x", 0 },
    { L"Dwarf", L"dwarf\\DwarfWithEffectInstance.x", 0 },
};


//--------------------------------------------------------------------------------------
struct MESHVERTEX
{
    D3DXVECTOR3 Position;
    D3DXVECTOR3 Normal;
    D3DXVECTOR2 Tex;

    const static D3DVERTEXELEMENT9 Decl[4];
};


//--------------------------------------------------------------------------------------
const D3DVERTEXELEMENT9 MESHVERTEX::Decl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
struct CMeshMaterial
{
    ID3DXEffect* m_pEffect;
    D3DXHANDLE m_hParam;
    IDirect3DTexture9* m_pTexture;

public:
CMeshMaterial()
{
    m_pEffect = NULL;
    m_hParam = NULL;
    m_pTexture = NULL;
}
~CMeshMaterial()
{
    SAFE_RELEASE( m_pEffect );
    SAFE_RELEASE( m_pTexture );
}
    const CMeshMaterial& operator=( const CMeshMaterial& rhs )
    {
        m_pEffect = rhs.m_pEffect;
        m_hParam = rhs.m_hParam;
        m_pTexture = rhs.m_pTexture;
        if( m_pEffect ) m_pEffect->AddRef();
        if( m_pTexture ) m_pTexture->AddRef();
        return *this;
    }
};


extern ID3DXEffect*             g_pEffect;
extern IDirect3DTexture9*       g_pDefaultTex;
extern IDirect3DCubeTexture9*   g_pEnvMapTex;


//--------------------------------------------------------------------------------------
class CEffectMesh
{
    WCHAR   m_wszMeshFile[MAX_PATH];
    ID3DXMesh* m_pMesh;
    CMeshMaterial* m_pMaterials;
    DWORD m_dwNumMaterials;

public:
            CEffectMesh()
            {
                m_pMesh = NULL;
                m_pMaterials = NULL;
                m_dwNumMaterials = 0;
            }
            CEffectMesh( const CEffectMesh& old )
            {
                m_pMesh = NULL; m_pMaterials = NULL;
                *this = old;
            }
            ~CEffectMesh()
            {
                Destroy();
            }
    const CEffectMesh& operator=( const CEffectMesh& rhs )
    {
        if( this == &rhs )
            return *this;

        wcscpy_s( m_wszMeshFile, MAX_PATH, rhs.m_wszMeshFile );
        m_dwNumMaterials = rhs.m_dwNumMaterials;
        SAFE_RELEASE( m_pMesh );
        if( ( m_pMesh = rhs.m_pMesh ) != 0 ) m_pMesh->AddRef();
        delete[] m_pMaterials;
        m_pMaterials = new CMeshMaterial[m_dwNumMaterials];
        if( m_pMaterials )
        {
            for( UINT i = 0; i < m_dwNumMaterials; ++i )
                m_pMaterials[i] = rhs.m_pMaterials[i];
        }
        return *this;
    }
    DWORD   GetNumMaterials() const
    {
        return m_dwNumMaterials;
    }
    ID3DXMesh* GetMesh()
    {
        return m_pMesh;
    }

    HRESULT Create( LPCWSTR wszFileName, IDirect3DDevice9* pd3dDevice );
    HRESULT Destroy()
    {
        delete[] m_pMaterials;
        m_pMaterials = NULL;
        m_dwNumMaterials = 0;
        SAFE_RELEASE( m_pMesh );
        return S_OK;
    }
    void    Render( IDirect3DDevice9* pd3dDevice )
    {
        HRESULT hr;

        for( UINT i = 0; i < m_dwNumMaterials; ++i )
        {
            CMeshMaterial* pMat = &m_pMaterials[i];
            V( pMat->m_pEffect->ApplyParameterBlock( pMat->m_hParam ) );

            UINT cPasses;
            V( pMat->m_pEffect->Begin( &cPasses, 0 ) );
            for( UINT p = 0; p < cPasses; ++p )
            {
                V( pMat->m_pEffect->BeginPass( p ) );
                V( m_pMesh->DrawSubset( i ) );
                V( pMat->m_pEffect->EndPass() );
            }
            V( pMat->m_pEffect->End() );
        }
    }
};


//--------------------------------------------------------------------------------------
class CMeshArcBall : public CD3DArcBall
{
public:
    void    OnBegin( int nX, int nY, D3DXMATRIXA16* pmInvViewRotate )
    {
        m_bDrag = true;
        m_qDown = m_qNow;
        m_vDownPt = ScreenToVector( ( float )nX, ( float )nY );
        D3DXVECTOR4 v;
        D3DXVec3Transform( &v, &m_vDownPt, pmInvViewRotate );
        m_vDownPt = ( D3DXVECTOR3 )v;
    }
    void    OnMove( int nX, int nY, D3DXMATRIXA16* pmInvViewRotate )
    {
        if( m_bDrag )
        {
            m_vCurrentPt = ScreenToVector( ( float )nX, ( float )nY );
            D3DXVECTOR4 v;
            D3DXVec3Transform( &v, &m_vCurrentPt, pmInvViewRotate );
            m_vCurrentPt = ( D3DXVECTOR3 )v;
            m_qNow = m_qDown * QuatFromBallPoints( m_vDownPt, m_vCurrentPt );
        }
    }
    LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, D3DXMATRIXA16* pmInvViewRotate )
    {
        // Current mouse position
        int iMouseX = ( short )LOWORD( lParam );
        int iMouseY = ( short )HIWORD( lParam );

        switch( uMsg )
        {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
                SetCapture( hWnd );
                OnBegin( iMouseX, iMouseY, pmInvViewRotate );
                return TRUE;

            case WM_LBUTTONUP:
                ReleaseCapture();
                OnEnd();
                return TRUE;

            case WM_CAPTURECHANGED:
                if( ( HWND )lParam != hWnd )
                {
                    ReleaseCapture();
                    OnEnd();
                }
                return TRUE;

            case WM_MOUSEMOVE:
                if( MK_LBUTTON & wParam )
                {
                    OnMove( iMouseX, iMouseY, pmInvViewRotate );
                }
                return TRUE;
        }

        return FALSE;
    }
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                      g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                    g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                    g_pEffect = NULL;       // D3DX effect interface
IDirect3DVertexDeclaration9*    g_pDecl = NULL;    // Vertex decl for meshes
IDirect3DTexture9*              g_pDefaultTex = NULL;   // Default texture
IDirect3DCubeTexture9*          g_pEnvMapTex = NULL;    // Environment map texture
DWORD                           g_dwShaderFlags = D3DXFX_NOT_CLONEABLE; // Shader creation flag for all effects
CModelViewerCamera              g_Camera;               // Camera for navigation
CGrowableArray <CMeshArcBall>   g_ArcBall;
bool                            g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager      g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                 g_SettingsDlg;          // Device settings dialog
CDXUTDialog                     g_HUD;                  // dialog for standard controls
CDXUTDialog                     g_SampleUI;             // dialog for sample specific controls
ID3DXEffectPool*                g_pEffectPool = NULL;   // Effect pool for sharing parameters
CGrowableArray <CEffectMesh>    g_Meshes;           // List of meshes being rendered
CGrowableArray <D3DXMATRIXA16>  g_amWorld;        // World transform for the meshes
int                             g_nActiveMesh = 0;
D3DXMATRIXA16                   g_mScroll;              // Scroll matrix
float                           g_fAngleToScroll = 0.0f;// Total angle to scroll the meshes in radian, in current scroll op
float                           g_fAngleLeftToScroll = 0.0f;// Angle left to scroll the meshes in radian


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_SHARE               5
#define IDC_SCROLLLEFT          6
#define IDC_SCROLLRIGHT         7
#define IDC_MESHNAME            8
#define IDC_MATCOUNT            9


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
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText();


//--------------------------------------------------------------------------------------
HRESULT CEffectMesh::Create( LPCWSTR wszFileName, IDirect3DDevice9* pd3dDevice )
{
    HRESULT hr;

    WCHAR str[MAX_PATH];
    WCHAR wszMeshPath[MAX_PATH];
    ID3DXBuffer* pAdjacency;
    ID3DXBuffer* pMaterials;
    ID3DXBuffer* pEffectInstance;
    DWORD dwNumMaterials;

    // Save the mesh filename
    wcscpy_s( m_wszMeshFile, MAX_PATH, wszFileName );

    V_RETURN( DXUTFindDXSDKMediaFileCch( wszMeshPath, MAX_PATH, m_wszMeshFile ) );
    V_RETURN( D3DXLoadMeshFromX( wszMeshPath, D3DXMESH_MANAGED, pd3dDevice,
                                 &pAdjacency, &pMaterials, &pEffectInstance,
                                 &dwNumMaterials, &m_pMesh ) );
    bool bHasNormals = ( m_pMesh->GetFVF() & D3DFVF_NORMAL ) != 0;
    ID3DXMesh* pCloneMesh;
    V_RETURN( m_pMesh->CloneMesh( m_pMesh->GetOptions(), MESHVERTEX::Decl, pd3dDevice, &pCloneMesh ) );
    m_pMesh->Release();
    m_pMesh = pCloneMesh;

    // Extract the path of the mesh file
    WCHAR* pLastBSlash = wcsrchr( wszMeshPath, L'\\' );
    if( pLastBSlash )
    {
        *( pLastBSlash + 1 ) = L'\0';
    }
    else
        wcscpy_s( wszMeshPath, MAX_PATH, L".\\" );

    // Ensure the mesh has correct normals.
    if( !bHasNormals )
        D3DXComputeNormals( m_pMesh, ( DWORD* )pAdjacency->GetBufferPointer() );

    // Allocate material array
    m_pMaterials = new CMeshMaterial[dwNumMaterials];
    if( !m_pMaterials )
        return E_OUTOFMEMORY;

    D3DXMATERIAL* pXMats = ( D3DXMATERIAL* )pMaterials->GetBufferPointer();
    D3DXEFFECTINSTANCE* pEI = ( D3DXEFFECTINSTANCE* )pEffectInstance->GetBufferPointer();
    for( UINT i = 0; i < dwNumMaterials; ++i )
    {
        // Obtain the effect

        hr = S_OK;
        // Try the mesh's directory
        wcscpy_s( str, MAX_PATH, wszMeshPath );
        MultiByteToWideChar( CP_ACP, 0, pEI[i].pEffectFilename, -1, str + lstrlenW( str ), MAX_PATH );

        if( pEI[i].pEffectFilename == NULL )
            hr = E_FAIL;

        WCHAR wszFxName[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, pEI[i].pEffectFilename, -1, wszFxName, MAX_PATH );

        if( SUCCEEDED( hr ) )
        {
            WCHAR szTmp[MAX_PATH];
            wcscpy_s( szTmp, MAX_PATH, L"SharedFx\\" );
            wcscat_s( szTmp, MAX_PATH, wszFxName );
            hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szTmp );
            if( FAILED( hr ) )
            {
                // Search the SDK paths
                hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, wszFxName );
            }
        }

        if( SUCCEEDED( hr ) )
            DXUTGetGlobalResourceCache().CreateEffectFromFile( pd3dDevice, str, NULL, NULL, g_dwShaderFlags,
                                                               g_pEffectPool,
                                                               &m_pMaterials[i].m_pEffect, NULL );
        if( !m_pMaterials[i].m_pEffect )
        {
            // No valid effect for this material.  Use the default.
            m_pMaterials[i].m_pEffect = g_pEffect;
            m_pMaterials[i].m_pEffect->AddRef();
        }

        // Set the technique this material should use
        D3DXHANDLE hTech;
        m_pMaterials[i].m_pEffect->FindNextValidTechnique( NULL, &hTech );
        m_pMaterials[i].m_pEffect->SetTechnique( hTech );

        // Create a parameter block to include all parameters for the effect.
        m_pMaterials[i].m_pEffect->BeginParameterBlock();
        for( UINT param = 0; param < pEI[i].NumDefaults; ++param )
        {
            D3DXHANDLE hHandle = m_pMaterials[i].m_pEffect->GetParameterByName( NULL,
                                                                                pEI[i].pDefaults[param].pParamName );
            D3DXPARAMETER_DESC desc;
            if( hHandle != NULL )
            {
                m_pMaterials[i].m_pEffect->GetParameterDesc( hHandle, &desc );
                if( desc.Type == D3DXPT_BOOL ||
                    desc.Type == D3DXPT_INT ||
                    desc.Type == D3DXPT_FLOAT ||
                    desc.Type == D3DXPT_STRING )
                {
                    m_pMaterials[i].m_pEffect->SetValue( pEI[i].pDefaults[param].pParamName,
                                                         pEI[i].pDefaults[param].pValue,
                                                         pEI[i].pDefaults[param].NumBytes );
                }
            }
        }

        // Obtain the texture
        hr = S_OK;

        if( pXMats[i].pTextureFilename )
        {
            // Try the mesh's directory first.
            wcscpy_s( str, MAX_PATH, wszMeshPath );
            MultiByteToWideChar( CP_ACP, 0, pXMats[i].pTextureFilename, -1, str + lstrlenW( str ), MAX_PATH );
            // If the texture file is not in the same directory as the mesh, search the SDK paths.
            if( INVALID_FILE_ATTRIBUTES == ::GetFileAttributesW( str ) )
            {
                WCHAR wszTexName[MAX_PATH];
                // Search the SDK paths
                MultiByteToWideChar( CP_ACP, 0, pXMats[i].pTextureFilename, -1, wszTexName, MAX_PATH );
                hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, wszTexName );
            }

            if( SUCCEEDED( hr ) )
                DXUTGetGlobalResourceCache().CreateTextureFromFile( pd3dDevice, str, &m_pMaterials[i].m_pTexture );
        }

        if( !m_pMaterials[i].m_pTexture )
        {
            // No texture or texture fails to load. Use the default texture.
            m_pMaterials[i].m_pTexture = g_pDefaultTex;
            m_pMaterials[i].m_pTexture->AddRef();
        }

        // Include the texture in the parameter block if the effect requires one.
        D3DXHANDLE hTexture = m_pMaterials[i].m_pEffect->GetParameterByName( NULL, "g_txScene" );
        if( hTexture )
            m_pMaterials[i].m_pEffect->SetTexture( hTexture, m_pMaterials[i].m_pTexture );

        // Include the environment map texture in the parameter block if the effect requires one.
        hTexture = m_pMaterials[i].m_pEffect->GetParameterByName( NULL, "g_txEnvMap" );
        if( hTexture )
            m_pMaterials[i].m_pEffect->SetTexture( hTexture, g_pEnvMapTex );

        // Save the parameter block
        m_pMaterials[i].m_hParam = m_pMaterials[i].m_pEffect->EndParameterBlock();
    }

    SAFE_RELEASE( pAdjacency );
    SAFE_RELEASE( pMaterials );
    SAFE_RELEASE( pEffectInstance );
    m_dwNumMaterials = dwNumMaterials;
    return S_OK;
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

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    InitApp();
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys
    DXUTCreateWindow( L"EffectParam" );
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
    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddStatic( IDC_MESHNAME, L"Mesh Name", 0, iY, 160, 20 );
    g_SampleUI.AddStatic( IDC_MATCOUNT, L"Number of materials: 0", 0, iY += 20, 160, 20 );
    g_SampleUI.AddButton( IDC_SCROLLLEFT, L"<<", 5, iY += 24, 70, 24 );
    g_SampleUI.AddButton( IDC_SCROLLRIGHT, L">>", 85, iY, 70, 24 );
    g_SampleUI.AddCheckBox( IDC_SHARE, L"Enable shared parameters", 0, iY += 32, 160, 24, true );

    // Initialize the arcball
    for( int i = 0; i < sizeof( g_MeshListData ) / sizeof( g_MeshListData[0] ); ++i )
    {
        g_ArcBall.Add( CMeshArcBall() );
        g_ArcBall[g_ArcBall.GetSize() - 1].SetTranslationRadius( 2.0f );
    }

    // Setup the cameras
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -3.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, 0 );
    g_Camera.SetRadius( 5.0f, 1.0f );
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

    // Must support pixel shader 1.1
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 1, 1 ) )
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

    // Turn vsync off
    pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    // If device doesn't support HW T&L or doesn't support 2.0 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 2, 0 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // If the hardware cannot do vertex blending, use software vertex processing.
    if( caps.MaxVertexBlendMatrices < 2 )
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    // If using hardware vertex processing, change to mixed vertex processing
    // so there is a fallback.
    if( pDeviceSettings->d3d9.BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_MIXED_VERTEXPROCESSING;

    // Add mixed vp to the available vp choices in device settings dialog.
    bool bSvp, bHvp, bPHvp, bMvp;
    DXUTGetD3D9Enumeration()->GetPossibleVertexProcessingList( &bSvp, &bHvp, &bPHvp, &bMvp );
    DXUTGetD3D9Enumeration()->SetPossibleVertexProcessingList( bSvp, false, false, true );

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
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;
    WCHAR str[MAX_PATH];

    DXUTGetGlobalResourceCache().OnCreateDevice( DXUTGetD3D9Device() );
    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    // Create the effect pool object if shared param is enabled
    if( g_SampleUI.GetCheckBox( IDC_SHARE )->GetChecked() )
        V_RETURN( D3DXCreateEffectPool( &g_pEffectPool ) );

    // Create the vertex decl
    V_RETURN( pd3dDevice->CreateVertexDeclaration( MESHVERTEX::Decl, &g_pDecl ) );
    pd3dDevice->SetVertexDeclaration( g_pDecl );

    // Create the 1x1 white default texture
    V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, 0, D3DFMT_A8R8G8B8,
                                         D3DPOOL_MANAGED, &g_pDefaultTex, NULL ) );
    D3DLOCKED_RECT lr;
    V_RETURN( g_pDefaultTex->LockRect( 0, &lr, NULL, 0 ) );
    *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 255, 255, 255, 255 );
    V_RETURN( g_pDefaultTex->UnlockRect( 0 ) );

    // Create the environment map texture
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"lobby\\lobbycube.dds" ) );
    V_RETURN( D3DXCreateCubeTextureFromFile( pd3dDevice, str, &g_pEnvMapTex ) );

    // Initialize the font
    V_RETURN( DXUTGetGlobalResourceCache().CreateFont( pd3dDevice, 15, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
                                                       OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                                       DEFAULT_PITCH | FF_DONTCARE,
                                                       L"Arial", &g_pFont ) );

#if defined( DEBUG ) || defined( _DEBUG )
        g_dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif

#ifdef DEBUG_VS
        g_dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        g_dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

    // Read the D3DX effect file
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"EffectParam.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( DXUTGetGlobalResourceCache().CreateEffectFromFile( pd3dDevice, str, NULL, NULL, g_dwShaderFlags,
                                                                 g_pEffectPool, &g_pEffect, NULL ) );

    // Create the meshes
    for( int i = 0; i < sizeof( g_MeshListData ) / sizeof( g_MeshListData[0] ); ++i )
    {
        CEffectMesh NewMesh;
        if( SUCCEEDED( NewMesh.Create( g_MeshListData[i].wszFile, DXUTGetD3D9Device() ) ) )
        {
            g_Meshes.Add( NewMesh );
            D3DXMATRIXA16 mW;
            LPVOID pVerts = NULL;
            D3DXMatrixIdentity( &mW );
            if( SUCCEEDED( NewMesh.GetMesh()->LockVertexBuffer( 0, &pVerts ) ) )
            {
                D3DXVECTOR3 vCtr;
                float fRadius;
                if( SUCCEEDED( D3DXComputeBoundingSphere( ( const D3DXVECTOR3* )pVerts,
                                                          NewMesh.GetMesh()->GetNumVertices(),
                                                          NewMesh.GetMesh()->GetNumBytesPerVertex(),
                                                          &vCtr, &fRadius ) ) )
                {
                    D3DXMatrixTranslation( &mW, -vCtr.x, -vCtr.y, -vCtr.z );
                    D3DXMATRIXA16 m;
                    D3DXMatrixScaling( &m, 1.0f / fRadius,
                                       1.0f / fRadius,
                                       1.0f / fRadius );
                    D3DXMatrixMultiply( &mW, &mW, &m );
                }
                NewMesh.GetMesh()->UnlockVertexBuffer();
            }

            g_MeshListData[i].dwNumMat = NewMesh.GetNumMaterials();
            g_amWorld.Add( mW );

            // Set the arcball window size
            const D3DSURFACE_DESC* pD3DSD = DXUTGetD3D9BackBufferSurfaceDesc();
            g_ArcBall[g_ArcBall.GetSize() - 1].SetWindow( pD3DSD->Width, pD3DSD->Height );
        }
    }

    g_SampleUI.GetStatic( IDC_MESHNAME )->SetText( g_MeshListData[g_nActiveMesh].wszName );
    WCHAR wsz[256];
    swprintf_s( wsz, 256, L"Number of materials: %u", g_MeshListData[g_nActiveMesh].dwNumMat );
    wsz[255] = L'\0';
    g_SampleUI.GetStatic( IDC_MATCOUNT )->SetText( wsz );

    D3DXMatrixIdentity( &g_mScroll );

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

    DXUTGetGlobalResourceCache().OnResetDevice( DXUTGetD3D9Device() );
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
    for( int i = 0; i < g_ArcBall.GetSize(); ++i )
        g_ArcBall[i].SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 170 ) / 2, pBackBufferSurfaceDesc->Height - 120 );
    g_SampleUI.SetSize( 170, 120 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the scroll matrix
    float fFrameAngleToScroll = g_fAngleToScroll * fElapsedTime / SCROLL_TIME;
    if( fabs( fFrameAngleToScroll ) > fabs( g_fAngleLeftToScroll ) )
        fFrameAngleToScroll = g_fAngleLeftToScroll;
    g_fAngleLeftToScroll -= fFrameAngleToScroll;
    D3DXMATRIXA16 m;
    D3DXMatrixRotationY( &m, fFrameAngleToScroll );
    g_mScroll *= m;
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
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
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mWorldView;
    D3DXMATRIXA16 mViewProj;
    D3DXMATRIXA16 mWorldViewProjection;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 66, 75, 121 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Get the projection & view matrix from the camera class
        mViewProj = *g_Camera.GetViewMatrix() * *g_Camera.GetProjMatrix();

        // Update the effect's variables.  Instead of using strings, it would 
        // be more efficient to cache a handle to the parameter by calling 
        // ID3DXEffect::GetParameterByName
        D3DXVECTOR4 vLightView( 0.0f, 0.0f, -10.0f, 1.0f );
        V( g_pEffect->SetVector( "g_vLight", &vLightView ) );

        float fAngleDelta = D3DX_PI * 2.0f / g_Meshes.GetSize();
        for( int i = 0; i < g_Meshes.GetSize(); ++i )
        {
            D3DXMATRIXA16 mWorld = *g_ArcBall[i].GetRotationMatrix() * *g_ArcBall[i].GetTranslationMatrix();
            mWorld = g_amWorld[i] * mWorld;
            D3DXMATRIXA16 mRot;
            D3DXMATRIXA16 mTrans;
            D3DXMatrixTranslation( &mTrans, 0.0f, 0.0f, -3.0f );
            D3DXMatrixRotationY( &mRot, fAngleDelta * ( i - g_nActiveMesh ) );
            mWorld *= mTrans * mRot * g_mScroll;
            mWorldView = mWorld * *g_Camera.GetViewMatrix();
            mWorldViewProjection = mWorld * mViewProj;
            V( g_pEffect->SetMatrix( "g_mWorld", &mWorld ) );
            V( g_pEffect->SetMatrix( "g_mView", g_Camera.GetViewMatrix() ) );
            V( g_pEffect->SetMatrix( "g_mProj", g_Camera.GetProjMatrix() ) );
            g_Meshes[i].Render( pd3dDevice );
        }

        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );

        V( pd3dDevice->EndScene() );
    }

    if( g_fAngleLeftToScroll == 0.0f )
    {
        D3DXMatrixIdentity( &g_mScroll );
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
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    txtHelper.DrawFormattedTextLine( L"Number of meshes: %d\n", g_Meshes.GetSize() );

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Rotate Mesh: Left mouse drag\n"
                                L"Zoom: Mouse wheel\n"
                                L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }

    // Draw shared param description
    txtHelper.SetInsertionPos( 5, 50 );
    if( g_pEffectPool )
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Shared parameters are enabled.  When updating transformation\n"
                                L"matrices on one effect object, all effect objects automatically\n"
                                L"see the updated values." );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Shared parameters are disabled.  When transformation matrices\n"
                                L"are updated on the default effect object (diffuse only), only that\n"
                                L"effect object has the up-to-date values.  All other effect objects\n"
                                L"do not have valid matrices for rendering." );
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
    D3DXMATRIXA16 mViewRotate = *g_Camera.GetViewMatrix();
    mViewRotate._41 = mViewRotate._42 = mViewRotate._43 = 0.0f;
    D3DXMatrixInverse( &mViewRotate, NULL, &mViewRotate );
    if( g_ArcBall.GetSize() > 0 )
        g_ArcBall[g_nActiveMesh].HandleMessages( hWnd, uMsg, wParam, lParam, &mViewRotate );

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

        case IDC_SHARE:
        {
            // Shared param status changed. Destroy and recreate everything
            // with or without effect pool as appropriate.
            if( DXUTGetD3D9Device() )
            {
                // We need to call the callbacks of the resource manager or the ref
                // count will not reach 0.
                OnLostDevice( NULL );
                OnDestroyDevice( NULL );
                OnCreateDevice( DXUTGetD3D9Device(), DXUTGetD3D9BackBufferSurfaceDesc(), NULL );
                OnResetDevice( DXUTGetD3D9Device(), DXUTGetD3D9BackBufferSurfaceDesc(), NULL );
            }
            break;
        }

        case IDC_SCROLLLEFT:
        case IDC_SCROLLRIGHT:
            {
                // Only scroll if we have more than one mesh
                if( g_Meshes.GetSize() <= 1 )
                    break;

                // Only scroll if we are not already scrolling
                if( g_fAngleLeftToScroll != 0.0f )
                    break;

                // Compute the angle to scroll
                g_fAngleToScroll = g_fAngleLeftToScroll = nControlID == IDC_SCROLLLEFT ? -D3DX_PI * 2.0f /
                    g_Meshes.GetSize() :
                    D3DX_PI * 2.0f / g_Meshes.GetSize();

                // Initialize the scroll matrix to be reverse full-angle rotation,
                // then gradually decrease to zero (identity).
                D3DXMatrixRotationY( &g_mScroll, -g_fAngleToScroll );
                // Update front mesh index
                if( nControlID == IDC_SCROLLLEFT )
                {
                    ++g_nActiveMesh;
                    if( g_nActiveMesh == g_Meshes.GetSize() )
                        g_nActiveMesh = 0;
                }
                else
                {
                    --g_nActiveMesh;
                    if( g_nActiveMesh < 0 )
                        g_nActiveMesh = g_Meshes.GetSize() - 1;
                }

                // Update mesh name and material count
                g_SampleUI.GetStatic( IDC_MESHNAME )->SetText( g_MeshListData[g_nActiveMesh].wszName );
                WCHAR wsz[256];
                swprintf_s( wsz, 256, L"Number of materials: %u", g_MeshListData[g_nActiveMesh].dwNumMat );
                wsz[255] = L'\0';
                g_SampleUI.GetStatic( IDC_MATCOUNT )->SetText( wsz );

                break;
            }
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    DXUTGetGlobalResourceCache().OnLostDevice();
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pDefaultTex );
    SAFE_RELEASE( g_pEffectPool );
    SAFE_RELEASE( g_pDecl );
    SAFE_RELEASE( g_pEnvMapTex );

    for( int i = 0; i < g_Meshes.GetSize(); ++i )
        g_Meshes[i].Destroy();

    g_Meshes.RemoveAll();
    g_amWorld.RemoveAll();
}

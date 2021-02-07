//-------------------------------------------------------------------------------------
// File: WindowsTouch.cpp
//
// This sample is a starting point for using the Multi - Touch APIs.  The sample
// registers a touch window.  This enables the sample to recieve WM_TOUCH messages.
// 
// The sample demonstrates how a camera can be controled with multi-touch.  It also 
// demonstrates how a game might select units.  
//
// The sample uses much of the same code as the Skinning10 sample to draw an animated  
// unit.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Multi-touch requires Windows 7 and the Windows SDK for Windows 7 or later
#include "sdkddkver.h"

#include "DXUT.h"
#include "ModelViewerTouchCamera.h"
#include "WinUser.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"


// animation data
#define MAX_BONE_MATRICES 255
#define MAX_INSTANCES 70
#define SPEED 1.0f;


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

INT g_nSoldierInstances = 10;
const float g_fUnitFormationSpacing = 0.4f;
const float g_fAnimationTime = 60.0f;
const float g_fWalkSpeed = 0.01f;
const float g_fBoundSphereSize = 0.4f;
const float g_fPixelScaler = 0.01f;

const D3DXVECTOR3 g_vCenterTranslation = D3DXVECTOR3( 0,-0.4f,0 );
const D3DXVECTOR3 g_vFacingDirection = D3DXVECTOR3( 0,0,1 );


CModelViewerTouchCamera                g_Camera;

bool                                g_bTouchCamera = true;

CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*                    g_pTxtHelper = NULL;
CDXUTDialog                         g_HUD;                  // dialog for standard controls
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls  

CDXUTSDKMesh                        g_SkinnedMesh;          

// Direct3D11 draw structures
ID3D11InputLayout*                  g_pSkinnedVertexLayout = NULL;
ID3D11InputLayout*                  g_pNonSkinnedVertexLayout = NULL;

ID3D11VertexShader*                 g_pRenderConstantBufferVS = NULL;
ID3D11PixelShader*                  g_pRenderPS = NULL;
ID3D11VertexShader*                 g_pRenderStillVS = NULL;

ID3D11VertexShader*                 g_pQuadVS = NULL;
ID3D11PixelShader*                  g_pQuadPS = NULL;

ID3D11Buffer*                       g_pCBMatrices = NULL;
ID3D11Buffer*                       g_pCBUserChange = NULL;
ID3D11Buffer*                       g_pCBConstBoneWorld = NULL;
ID3D11Buffer*                       g_pCBImmutable = NULL;

ID3D11SamplerState*                 g_pSamplerLinear = NULL;
bool                                g_bEnableDragSelect = false;
bool                                g_bShowHelp = false;
D3D_FEATURE_LEVEL                   g_FeatureLevel;

PTOUCHINPUT                         g_pInputs = NULL;
INT                                 g_iTouchInputArraySize = 3;

ID3D11BlendState*                   g_AlphaBlendState = NULL;
ID3D11RasterizerState*              g_QuadRasterizerState = NULL;

struct CBMatrices
{
    D3DXMATRIX                      m_matWorldViewProj;
    D3DXMATRIX                      m_matWorld;
};

struct CBImmutable 
{
    D3DXVECTOR4                     m_vDirectional;
    D3DXVECTOR4                     m_vAmbient;
    D3DXVECTOR4                     m_vSpecular;
};

struct CBUserChange
{
    D3DXVECTOR4                     m_vLightPos;
    D3DXVECTOR4                     m_vEyePt;
    D3DXVECTOR4                     m_vSelected ;
};

struct CBConstBoneWorld
{
    D3DXMATRIX                      g_mConstBoneWorld[MAX_BONE_MATRICES];
};

CBUserChange                        g_CBUserChange;
CBConstBoneWorld                    g_CBConstBoneWorld;
D3DXVECTOR3                         g_vLightPos = D3DXVECTOR3( 159.47f, 74.23f, 103.60f );
RECT                                g_Window;

void InitApp();
void RenderText();
void SetBoneMatrices( UINT iMesh );
void ToggleTouch () ;
void CalculateWindowOffsets ( INT x, INT y );

INT *g_iListOfSelectedUnitsByIndex;
INT INTersectRayUnits ( const D3DXVECTOR3 &vRayDirection, const D3DXVECTOR3 &vRayOrigin );
void CalculateScreenRayFromCoordinates ( float x, float y, D3DXVECTOR3 &vRayDirection );

// This class handles the animation and simulation of a unit.
// The selection of units is handled by the UnitControlManager.
class Unit {
public:
    Unit () {
        Init();
    };


    void Init () {
        m_vLocation.x = ( ( float )( rand ()%10000 - 5000 ) )*.001f;
        m_vLocation.y = 0;
        m_vLocation.z = ( ( float )( rand ()%10000 - 5000 ) )*.001f;
        m_vDestination = m_vLocation;
        m_bSelected = false;
        m_fWalkSpeed = g_fWalkSpeed;
        m_fBoundingSphereSquared = g_fBoundSphereSize;
        m_fBoundingSphereSquared *= m_fBoundingSphereSquared;
        D3DXMatrixIdentity (&m_matWalkRotation );    
    }
    ~Unit () {};

    bool InMotion () {
        if ( m_vLocation.x == m_vDestination.x && m_vLocation.y == m_vDestination.y ) return false;
        else return true;
    }

    void SetNewDestination ( float x, float y, float z ) {

        m_vDestination.x = x;
        m_vDestination.y = y;
        m_vDestination.z = z;       
        m_vWalkIncrement = m_vDestination - m_vLocation;
        D3DXVec3Normalize ( &m_vWalkIncrement, &m_vWalkIncrement );

        // Calculate the rotation angle before. Next, change the walk direction into 
        // an increment by multiplying by speed.
        float fAngle = D3DXVec3Dot( &m_vWalkIncrement, &g_vFacingDirection );
        D3DXVECTOR3 cross;
        D3DXVec3Cross( &cross, &m_vWalkIncrement, &g_vFacingDirection );
        fAngle = acosf( fAngle );
        if ( cross.y >  0.0f ) {
            fAngle *=-1.0f;
        }
        D3DXMatrixRotationY( &m_matWalkRotation, fAngle );

        m_vWalkIncrement *=m_fWalkSpeed;       
        
    }

    
    void Update (float fMoveIncrement) {
        if ( InMotion() ) {
            D3DXVECTOR3 update_delta = m_vWalkIncrement * fMoveIncrement;
            D3DXVECTOR3 location_vector = m_vDestination - m_vLocation;
            m_vLocation += update_delta;
            //determine  if we've moved past our target ( so we can stop ).
            float finished = D3DXVec3Dot( &m_vWalkIncrement, &location_vector );
            if ( finished < 0.0f ) m_vLocation = m_vDestination;
        }
    }


    void GetInstanceWorldMatrix(  D3DXMATRIX* pmatWorld )
    {
        D3DXMATRIX matTranslation;

        D3DXMatrixTranslation( 
            &matTranslation,
            m_vLocation.x + g_vCenterTranslation.x,
            m_vLocation.y + g_vCenterTranslation.y, 
            m_vLocation.z + g_vCenterTranslation.z);

        *pmatWorld = m_matWalkRotation ;
        *pmatWorld *= matTranslation;

    };

    const D3DXVECTOR3 &GetLocation () const { return m_vLocation; };
    float GetBoundingSphereSquared () const { return m_fBoundingSphereSquared; };
    bool IsSelected () const { return m_bSelected; };
    void DeSelect () { m_bSelected = false ;};
    void Select () { m_bSelected = true; };

private:

    float                       m_fWalkSpeed;
    float                       m_fBoundingSphereSquared;
    bool                        m_bSelected;
    D3DXVECTOR3                 m_vLocation;
    D3DXVECTOR3                 m_vDestination;
    D3DXVECTOR3                 m_vWalkIncrement;
    D3DXMATRIX                  m_matWalkRotation;
};

Unit *g_units = NULL;

class UIControlManager  {
public:

    enum UI_TOUCH_CONTROL { UI_NO_TOUCH_CONTROL, UI_CAMERA_TOUCH_CONTROL, UI_UNIT_SELECTION_TOUCH_CONTROL };

    UIControlManager () {
        m_bUnitsSelected = false;
        m_UIstate = UI_NO_TOUCH_CONTROL;
    };

    ~UIControlManager () {};

    void HandleTouchMessages (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

        if (uMsg == WM_MOVE) {
            INT xPos = (INT)LOWORD(lParam); 
            INT yPos = (INT)HIWORD(lParam);
            CalculateWindowOffsets (xPos, yPos) ;
        }
        else if (uMsg == WM_LBUTTONDOWN) {
            INT xPos = (INT)LOWORD(lParam); 
            INT yPos = (INT)HIWORD(lParam);
            TouchDown((float)xPos, (float)yPos);
        }

        if ( uMsg == WM_TOUCH ) {
                INT nInputs = (INT)wParam & 0xffff;
                HTOUCHINPUT handle = (HTOUCHINPUT)LOWORD( wParam );
                if ( nInputs > g_iTouchInputArraySize ) {
                    g_iTouchInputArraySize = nInputs;
                    SAFE_DELETE_ARRAY( g_pInputs );
                    g_pInputs = new TOUCHINPUT[nInputs];
                }

                if ( GetTouchInputInfo( ( HTOUCHINPUT )lParam, 
                    nInputs,
                    g_pInputs, 
                    sizeof( TOUCHINPUT ) ) )
                {
                          
                    TOUCHINPUT ti = g_pInputs[0];
                    // If the camera has not claimed the touch input, send it to the UI Manager
                    if ( m_UIstate == UI_NO_TOUCH_CONTROL || 
                        m_UIstate == UI_UNIT_SELECTION_TOUCH_CONTROL ) {
                        
                        if ( nInputs ==  1) {
                            
                            //truncate inputs and pass them INTo there respective functions
                            if ( ti.dwFlags & TOUCHEVENTF_DOWN )
                            {
                                TouchDown ( ti.x*g_fPixelScaler - g_Window.left, ti.y*g_fPixelScaler - g_Window.top );
                            }
                            else if ( ti.dwFlags & TOUCHEVENTF_MOVE )
                            {
                                TouchMove ( ti.x*g_fPixelScaler - g_Window.left, ti.y*g_fPixelScaler - g_Window.top );
                            }
                            else if ( ti.dwFlags & TOUCHEVENTF_UP )
                            {
                                TouchUp ( ti.x*g_fPixelScaler - g_Window.left, ti.y*g_fPixelScaler - g_Window.top );
                            }
                        }
                    }
                    // if the touch UI Manager hasn't claimed the camera send it to the camera.
                    if ( m_UIstate == UI_NO_TOUCH_CONTROL 
                        || m_UIstate == UI_CAMERA_TOUCH_CONTROL ) 
                    {
                        bool bCameraHandlingTouchInput = g_Camera.HandleTouchMessages( 
                            hWnd, uMsg, wParam, lParam, nInputs, g_pInputs, g_Window );
                        if ( bCameraHandlingTouchInput ) m_UIstate = UI_CAMERA_TOUCH_CONTROL;
                        else m_UIstate = UI_NO_TOUCH_CONTROL;
                    }
                    
                }
                CloseTouchInputHandle( handle );
        }

    }


    // Update the destination of selected units.
    void AssignSelectedUnitsToNewDestination ( const D3DXVECTOR3 &vec ) {

        INT nSelectedCount = 0 ;
        for ( INT i =0; i < g_nSoldierInstances; ++i ) {
            if (g_units[i].IsSelected()) {
                ++nSelectedCount;
            }
        }

        float fRowDimension = sqrt( ( float ) nSelectedCount );
        INT iRowDimension = ( INT )fRowDimension ;
        INT current = 0;
        float fRowOffset;
        fRowOffset = fRowDimension * 0.5f;
        
        if ( fRowDimension - ( float ) iRowDimension > 0.5f ) ++iRowDimension;
        // This code moves the units in a formation, so that they don't all move to the same location.
        for ( INT i =0; i < g_nSoldierInstances; ++i ) {
            if ( g_units[i].IsSelected() ) 
            {
                g_units[i].DeSelect();
                // move selected units in a formation
                g_units[i].SetNewDestination(
                    vec.x + ( g_fUnitFormationSpacing * ( float ) ( current % iRowDimension ) ) - fRowOffset, 
                    vec.y, 
                    vec.z + ( g_fUnitFormationSpacing * ( float ) ( current / iRowDimension ) ) - fRowOffset
                );
                ++current;
            }
        }
    };

    
    void TouchUp (float x, float y) {
        m_UIstate = UI_NO_TOUCH_CONTROL;
    }
    // When drag select is enabled we select units on touch move commands.
    void TouchMove ( float x, float y ) {

        if ( g_bEnableDragSelect ) {
            D3DXVECTOR3 vRayDirection;
            D3DXVECTOR3 vRayOrigin = *g_Camera.GetEyePt();
            CalculateScreenRayFromCoordinates ( x,y, vRayDirection );
            INT iSelected = INTersectRayUnits ( vRayDirection, vRayOrigin );
            if (iSelected != 0) {
                m_bUnitsSelected = true; 
                for (INT index=0; index < iSelected; ++index) {
                    g_units[g_iListOfSelectedUnitsByIndex[index]].Select();
                }
            }
        }

    }

    void TouchDown ( float x, float y )  {

        D3DXVECTOR3 vRayDirection;
        D3DXVECTOR3 vRayOrigin = *g_Camera.GetEyePt();
        CalculateScreenRayFromCoordinates( x,y, vRayDirection );
        INT nSelected = INTersectRayUnits( vRayDirection, vRayOrigin );
        D3DXVECTOR3 vGroundPlaneHit;
        
        // Select units based on intersection tests.
        if ( nSelected != 0 ) {
            for ( INT index=0; index < nSelected; ++index ) {
                g_units[ g_iListOfSelectedUnitsByIndex[ index ] ].Select();
                m_bUnitsSelected = true; 
            }
        }
        // Updated units targets based on ground plane intersection location.
        else 
        {
            D3DXVECTOR3 vPlaneNormal = D3DXVECTOR3(0,-1,0);
            float fpGroundPlaneINTersection = D3DXVec3Dot(&vPlaneNormal, &vRayDirection);
            if ( fpGroundPlaneINTersection > .001) {
                float fpINTersectionDistnace = -1* D3DXVec3Dot( &vPlaneNormal, &vRayOrigin );
                float fpINTersectionTime = fpINTersectionDistnace / fpGroundPlaneINTersection;
                if ( fpINTersectionTime > 0) {
                    vGroundPlaneHit = vRayOrigin;
                    vGroundPlaneHit += ( vRayDirection * fpINTersectionTime );
                    AssignSelectedUnitsToNewDestination ( vGroundPlaneHit );
                }
            }     
        } 

    };
private:
    UI_TOUCH_CONTROL m_UIstate;
    bool m_bUnitsSelected;
};

UIControlManager g_UIControlManager;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC             -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEWARP          2
#define IDC_CHANGEDEVICE        3
#define IDC_INSTANCES_STATIC    4
#define IDC_TOGGLETOUCH         5
#define IDC_TOGGLEDRAGSELECT    6
#define IDC_RESET               7
#define IDC_INSTANCES           8
#define IDC_TOGGLEHELP          9


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, INT nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable(  const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, 
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, 
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain(  ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, 
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                 double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"Windows Touch Input" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_9_1, true, 1280, 800);
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    INT iSpacing = 45;
    g_HUD.SetCallback( OnGUIEvent ); INT iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle Full Screen", 0, iY, 170, 40 );
    g_HUD.AddButton( IDC_TOGGLEHELP, L"Show Help (F1)", 0, iY += iSpacing, 170, 40, VK_F1 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += iSpacing,  170, 40, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F3)", 0, iY += iSpacing,  170, 40, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLETOUCH, L"Toggle Touch (F4)", 0, iY += iSpacing,  170, 40, VK_F4);
    g_HUD.AddButton(IDC_RESET, L"Reset", 0, iY+=iSpacing, 170, 40);
    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"Soldiers: %d", g_nSoldierInstances );
    g_HUD.AddStatic( IDC_INSTANCES_STATIC, str, 5, iY += iSpacing,  170, 402 );
    g_HUD.AddSlider(IDC_INSTANCES, 0, iY+=iSpacing, 170, 40,0, MAX_INSTANCES, g_nSoldierInstances);
    g_HUD.AddCheckBox( IDC_TOGGLEDRAGSELECT, L"Drag Select", 0, iY += iSpacing,  170, 40);

    iY = 220;

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
}
void ToggleTouch () {
    // This is the code that makes the applicaiton touch aware.  By registering a touch window,
    // the applicaiton opts in to recieving WM_TOUCH messages. 
    // When the window is not registered, the app will receive WM_GESTURE messages.
    // WM_GESTURE messages are an easy way to make an application multi-touch. 
    // However, games will want to use the multi-touch APIs and register a touch window
    // for the added flexability.

    BOOL rt;
    HWND hWnd = DXUTGetHWND();
    g_bTouchCamera  = !g_bTouchCamera ;
    if ( !IsTouchWindow( hWnd, 0 ) ) {
        rt = RegisterTouchWindow( hWnd, 0 );
    }else {
        rt = UnregisterTouchWindow( hWnd );    
    }
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    if ( DXUTGetDeviceSettings().d3d11.DeviceFeatureLevel < D3D_FEATURE_LEVEL_10_0 ) 
    {
        g_pTxtHelper->DrawFormattedTextLine( L"D3D10 Hardware is required for unit animation. " );
    }
    if ( g_bShowHelp ) {
        g_pTxtHelper->SetInsertionPos( 4, 70 );
        g_pTxtHelper->DrawTextLine( L"This sample requires a Windows 7 Multi Touch display." );
        g_pTxtHelper->DrawTextLine( L"Touch a unit to select." );
        g_pTxtHelper->DrawTextLine( L"Touch a spot on the ground plane to issue commands." );
        g_pTxtHelper->DrawTextLine( L"Rotate the camera by placing 3 fingers on the screen and dragging." );
        g_pTxtHelper->DrawTextLine( L"Pinch the screen to zoom in." );
    }

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, 
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Use this until D3DX11 comes online and we get some compilation helpers
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoINT, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoINT, szShaderModel, 
        dwShaderFlags, NULL, NULL, ppBlobOut, &pErrorBlob, NULL );

    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext  )
{


    HRESULT hr;
    ToggleTouch ();
    g_iListOfSelectedUnitsByIndex = new INT [MAX_INSTANCES];
    g_units = new Unit[MAX_INSTANCES];
    g_pInputs = new TOUCHINPUT[3];
    g_iTouchInputArraySize = 3;
    
    D3D11_BLEND_DESC dsc = 
    {
        false,//BOOL AlphaToCoverageEnable;
        false,//BOOL IndependentBlendEnable;
        {
        true, //BOOL BlendEnable;
        D3D11_BLEND_SRC_ALPHA, //D3D11_BLEND SrcBlend;
        D3D11_BLEND_INV_SRC_ALPHA ,    //D3D11_BLEND DestBlend;
        D3D11_BLEND_OP_ADD,    //D3D11_BLEND_OP BlendOp;
        D3D11_BLEND_ZERO ,    //D3D11_BLEND SrcBlendAlpha;
        D3D11_BLEND_ZERO ,    //D3D11_BLEND DestBlendAlpha;
        D3D11_BLEND_OP_ADD,    //D3D11_BLEND_OP BlendOpAlpha;
        D3D11_COLOR_WRITE_ENABLE_ALL //UINT8 RenderTargetWriteMask;    
        } 
    };
    pd3dDevice->CreateBlendState(&dsc, &g_AlphaBlendState );
    DXUT_SetDebugName( g_AlphaBlendState, "AlphaBlend" );

    D3D11_RASTERIZER_DESC r_dsc = {
    D3D11_FILL_SOLID, //D3D11_FILL_MODE FillMode;
    D3D11_CULL_NONE,//D3D11_CULL_MODE CullMode;
    false,//BOOL FrontCounterClockwise;
    0,//INT DepthBias;
    0.0f,//FLOAT DepthBiasClamp;
    0.0f,//FLOAT SlopeScaledDepthBias;
    true, //BOOL DepthClipEnable;
    false, //BOOL ScissorEnable;
    false,//BOOL MultisampleEnable;
    false, //BOOL AntialiasedLineEnable;   
    };

    pd3dDevice->CreateRasterizerState(&r_dsc, &g_QuadRasterizerState);
    DXUT_SetDebugName( g_QuadRasterizerState, "Solid" );

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    CalculateWindowOffsets(0,0);
    D3D11_SAMPLER_DESC s_dsc =  {
        D3D11_FILTER_MIN_MAG_MIP_LINEAR,    //D3D11_FILTER Filter;
        D3D11_TEXTURE_ADDRESS_CLAMP,        //D3D11_TEXTURE_ADDRESS_MODE AddressU;
        D3D11_TEXTURE_ADDRESS_CLAMP,        //D3D11_TEXTURE_ADDRESS_MODE AddressV;
        D3D11_TEXTURE_ADDRESS_CLAMP,        //D3D11_TEXTURE_ADDRESS_MODE AddressW;
        0,                                  //FLOAT MipLODBias;
        0,                                  //UINT MaxAnisotropy;
        D3D11_COMPARISON_NEVER,             //D3D11_COMPARISON_FUNC ComparisonFunc;
        {0.0f, 0.0f, 0.0f, 0.0f},           //FLOAT BorderColor[ 4 ];
        0.0f,                               //FLOAT MinLOD;
        FLT_MAX                             //FLOAT MaxLOD;
    };
    
    pd3dDevice->CreateSamplerState( &s_dsc, &g_pSamplerLinear );
    DXUT_SetDebugName( g_pSamplerLinear, "Linear" );

    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Soldier.fx" ) );

    g_FeatureLevel = DXUTGetD3D11DeviceFeatureLevel();

    ID3DBlob* VSSkinnedmain = NULL;
    if ( g_FeatureLevel > D3D_FEATURE_LEVEL_9_3 ) {
        V_RETURN( CompileShaderFromFile( L"Soldier.fx", "VSSkinnedmain", "vs_4_0", &VSSkinnedmain ) );
        V_RETURN( pd3dDevice->CreateVertexShader( VSSkinnedmain->GetBufferPointer(),
        VSSkinnedmain->GetBufferSize(), NULL,  &g_pRenderConstantBufferVS) );
        DXUT_SetDebugName( g_pRenderConstantBufferVS, "VSSkinnedmain" );
    }

    ID3DBlob* VSmain = NULL;   
    V_RETURN( CompileShaderFromFile( L"Soldier.fx", "VSmain", "vs_4_0_level_9_1", &VSmain ) );
    V_RETURN( pd3dDevice->CreateVertexShader( VSmain->GetBufferPointer(),
        VSmain->GetBufferSize(), NULL, &g_pRenderStillVS) );
    DXUT_SetDebugName( g_pRenderStillVS, "Soldier - VSmain" );

    ID3DBlob* PSSkinnedmain = NULL;
    V_RETURN( CompileShaderFromFile( L"Soldier.fx", "PSSkinnedmain", "ps_4_0_level_9_1", &PSSkinnedmain ) );
    V_RETURN( pd3dDevice->CreatePixelShader( PSSkinnedmain->GetBufferPointer(),
        PSSkinnedmain->GetBufferSize(), NULL, &g_pRenderPS) );
    DXUT_SetDebugName( g_pRenderPS, "PSSkinnedmain" );

    ID3DBlob* QuadVSBlob = NULL;
    ID3DBlob* QuadPSBlob = NULL;
    if ( g_FeatureLevel > D3D_FEATURE_LEVEL_9_3 ) {
        V_RETURN( CompileShaderFromFile( L"quad.hlsl", "VSMain", "vs_4_0", &QuadVSBlob ) );
        V_RETURN( pd3dDevice->CreateVertexShader( QuadVSBlob->GetBufferPointer(),
            QuadVSBlob->GetBufferSize(), NULL, &g_pQuadVS) );
        DXUT_SetDebugName( g_pQuadVS, "quad - VSMain" );

        V_RETURN( CompileShaderFromFile( L"quad.hlsl", "PSMain", "ps_4_0", &QuadPSBlob ) );
        V_RETURN( pd3dDevice->CreatePixelShader( QuadPSBlob->GetBufferPointer(),
            QuadPSBlob->GetBufferSize(), NULL, &g_pQuadPS) );
        DXUT_SetDebugName( g_pQuadPS, "quad - PSMain" ); 
    }
    SAFE_RELEASE( QuadPSBlob );
    SAFE_RELEASE( QuadVSBlob );

    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CBMatrices );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pCBMatrices ) );
    DXUT_SetDebugName( g_pCBMatrices, "CBMatrices" );

    Desc.ByteWidth = sizeof( CBUserChange );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pCBUserChange ) );
    DXUT_SetDebugName( g_pCBUserChange, "CBUserChange" );

    Desc.ByteWidth = sizeof( CBConstBoneWorld );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pCBConstBoneWorld) );
    DXUT_SetDebugName( g_pCBConstBoneWorld, "CBConstBoneWorld" );

    Desc.ByteWidth = sizeof( CBImmutable );
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.CPUAccessFlags = 0;
    CBImmutable im;
    im.m_vDirectional = D3DXVECTOR4( 1.0f, 1.0f, 1.0f, 1.0f );
    im.m_vAmbient = D3DXVECTOR4( 0.1f, 0.1f, 0.1f, 0.0f );
    im.m_vSpecular = D3DXVECTOR4( 1.0f, 1.0f, 1.0f, 1.0f );    
    D3D11_SUBRESOURCE_DATA dsd;
    dsd.pSysMem = &im;
    dsd.SysMemPitch = 0;
    dsd.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, &dsd, &g_pCBImmutable) );
    DXUT_SetDebugName( g_pCBImmutable, "CBImmutable" );

    // Define our vertex data layout for skinned objects
    if ( pd3dDevice->GetFeatureLevel() > D3D_FEATURE_LEVEL_9_3 ) { 
        const D3D11_INPUT_ELEMENT_DESC SkinnedLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,    0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "WEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,      12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BONES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0,         16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,      20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,       32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,     40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        INT numElements = sizeof( SkinnedLayout ) / sizeof( SkinnedLayout[0] );

        V_RETURN( pd3dDevice->CreateInputLayout( SkinnedLayout, numElements, VSSkinnedmain->GetBufferPointer(),
                                                 VSSkinnedmain->GetBufferSize(), &g_pSkinnedVertexLayout ) );
        DXUT_SetDebugName( g_pSkinnedVertexLayout, "Skinned Vertices" );
    }

    const D3D11_INPUT_ELEMENT_DESC UnSkinnedLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    INT numElements = sizeof( UnSkinnedLayout ) / sizeof( UnSkinnedLayout[0] );
    V_RETURN( pd3dDevice->CreateInputLayout( UnSkinnedLayout, numElements, VSmain->GetBufferPointer(),
                                             VSmain->GetBufferSize(), &g_pNonSkinnedVertexLayout ) );
    DXUT_SetDebugName( g_pNonSkinnedVertexLayout, "Vertices" );

    // Load the animated mesh
    V_RETURN( g_SkinnedMesh.Create( pd3dDevice, L"Soldier\\soldier.sdkmesh", true ) );
    V_RETURN( g_SkinnedMesh.LoadAnimation( L"Soldier\\soldier.sdkmesh_anim" ) );

    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity( &mIdentity );
    g_SkinnedMesh.TransformBindPose( &mIdentity );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 4.0f, 10.0f, 4.0f );
    //D3DXVECTOR3 vecAt ( 2.0f, 0.0f, 2.0f );
    D3DXVECTOR3 vecAt ( 0.1f, 0.5f, -1.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    SAFE_RELEASE(VSSkinnedmain);
    SAFE_RELEASE(PSSkinnedmain);
    SAFE_RELEASE(VSmain);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL,  MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Get the world matrix for this particular mesh instance
//--------------------------------------------------------------------------------------
void UpdateUnits ( float fElapsedTime ) {

    float fpMoveIncrement = fElapsedTime * g_fAnimationTime;      
    
    for ( INT i =0; i < g_nSoldierInstances; ++i ) {
        g_units[i].Update( fpMoveIncrement );
    }
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                 double fTime, float fElapsedTime, void* pUserContext )
{
    pd3dImmediateContext->PSSetSamplers(0,1, &g_pSamplerLinear);
    pd3dImmediateContext->RSSetState(0);

    static float colorhack = 0.2f;
    colorhack+= 0.02f;
    if (colorhack >0.7f) colorhack = 0.2f; 

    D3DXMATRIX mWorldViewProj;
    D3DXMATRIX mViewProj;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );

    // Clear the depth stencil
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Get the projection & view matrix from the camera class
    D3DXMatrixIdentity( &mWorld );
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mViewProj = mView * mProj;

    g_CBUserChange.m_vEyePt.x = g_Camera.GetEyePt()->x;
    g_CBUserChange.m_vEyePt.y = g_Camera.GetEyePt()->y;
    g_CBUserChange.m_vEyePt.z = g_Camera.GetEyePt()->z;
    g_CBUserChange.m_vLightPos.x = g_vLightPos.x;
    g_CBUserChange.m_vLightPos.y = g_vLightPos.y;
    g_CBUserChange.m_vLightPos.z = g_vLightPos.z;

    ID3D11Buffer* pBuffers[1];
    UINT stride[1]; 
    UINT offset[1] = { 0 };
    ID3D11VertexShader *p_vs;
    // Draw the individual soldiers.
    for( INT iInstance = 0; iInstance < g_nSoldierInstances; iInstance++ )
    {
        
        if (g_units[iInstance].InMotion() && g_FeatureLevel > D3D_FEATURE_LEVEL_9_3) 
        {
            p_vs = g_pRenderConstantBufferVS;
            pd3dImmediateContext->IASetInputLayout( g_pSkinnedVertexLayout );
        }
        else 
        {
            pd3dImmediateContext->IASetInputLayout( g_pNonSkinnedVertexLayout );
            p_vs = g_pRenderStillVS;
        }
        if (g_units[iInstance].IsSelected()) {
            D3DXVECTOR4 color = D3DXVECTOR4(0.0f,colorhack,0.0f,1.0f);
            g_CBUserChange.m_vSelected = color;
        }else {
            D3DXVECTOR4 color = D3DXVECTOR4(0.0f,0.0f,0.0f,1.0f);
            g_CBUserChange.m_vSelected = color;
        }

        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dImmediateContext->Map( g_pCBUserChange, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ); 
        memcpy(MappedResource.pData, &g_CBUserChange, sizeof (g_CBUserChange));
        pd3dImmediateContext->Unmap( g_pCBUserChange, 0 );

        g_units[iInstance].GetInstanceWorldMatrix( &mWorld );

        pd3dImmediateContext->Map( g_pCBMatrices, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ); 
        CBMatrices *pMatricesData = (CBMatrices*)MappedResource.pData;
        mWorldViewProj = mWorld * mViewProj;
        D3DXMatrixTranspose( &mWorld, &mWorld );
        D3DXMatrixTranspose( &mWorldViewProj, &mWorldViewProj );
        pMatricesData->m_matWorld = mWorld;
        pMatricesData->m_matWorldViewProj = mWorldViewProj;
        pd3dImmediateContext->Unmap(g_pCBMatrices, 0);
        // Render the meshes
        for( UINT m = 0; m < g_SkinnedMesh.GetNumMeshes(); m++ )
        {
            pBuffers[0] = g_SkinnedMesh.GetVB11( m, 0 );
            stride[0] = ( UINT )g_SkinnedMesh.GetVertexStride( m, 0 );
            offset[0] = 0;

            pd3dImmediateContext->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
            pd3dImmediateContext->IASetIndexBuffer( g_SkinnedMesh.GetIB11( m ), g_SkinnedMesh.GetIBFormat11( m ), 0 );

            SDKMESH_SUBSET* pSubset = NULL;
            SDKMESH_MATERIAL* pMat = NULL;
            D3D11_PRIMITIVE_TOPOLOGY PrimType;

            // Set the bone matrices
            SetBoneMatrices( m );
            D3D11_MAPPED_SUBRESOURCE MappedResource;
            pd3dImmediateContext->Map(g_pCBConstBoneWorld, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource); 
            memcpy(MappedResource.pData, &g_CBConstBoneWorld, sizeof (g_CBConstBoneWorld));
            pd3dImmediateContext->Unmap(g_pCBConstBoneWorld, 0);

            for( UINT subset = 0; subset < g_SkinnedMesh.GetNumSubsets( m ); subset++ )
            {
                pSubset = g_SkinnedMesh.GetSubset( m, subset );

                PrimType = g_SkinnedMesh.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )
                                                             pSubset->PrimitiveType );
                pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

                pMat = g_SkinnedMesh.GetMaterial( pSubset->MaterialID );
                if( pMat )
                {
                    pd3dImmediateContext->PSSetShaderResources(0, 1, &pMat->pDiffuseRV11);
                    pd3dImmediateContext->PSSetShaderResources(1, 1, &pMat->pNormalRV11);
                }

                pd3dImmediateContext->VSSetShader(p_vs, NULL, 0);
                pd3dImmediateContext->PSSetShader(g_pRenderPS, NULL, 0);
                pd3dImmediateContext->VSSetConstantBuffers(0, 1, &g_pCBMatrices); 
                pd3dImmediateContext->PSSetConstantBuffers(1, 1, &g_pCBUserChange); 
                pd3dImmediateContext->VSSetConstantBuffers(2, 1, &g_pCBConstBoneWorld); 
                pd3dImmediateContext->PSSetConstantBuffers(3, 1, &g_pCBImmutable);

                pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, ( UINT )pSubset->IndexStart,
                                         ( UINT )pSubset->VertexStart );
            }
        }//nummeshes
    }
    
    if ( pd3dDevice->GetFeatureLevel() > D3D_FEATURE_LEVEL_9_3 ) {
        float bf [4] = {1.0f, 1.0f, 1.0f, 1.0f};
        pd3dImmediateContext->RSSetState( g_QuadRasterizerState );
        pd3dImmediateContext->OMSetBlendState( g_AlphaBlendState, bf , 0xffffffff );
        pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBMatrices ); 
        pd3dImmediateContext->IASetInputLayout( NULL );
        pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
        pd3dImmediateContext->VSSetShader( g_pQuadVS, NULL, 0 );
        pd3dImmediateContext->PSSetShader( g_pQuadPS, NULL, 0 );
        pd3dImmediateContext->Draw(4, 0);
    }    
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    ToggleTouch ();

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    delete[] g_iListOfSelectedUnitsByIndex;
    delete[] g_units;
    SAFE_RELEASE( g_pSamplerLinear );
    SAFE_RELEASE( g_pSkinnedVertexLayout );
    SAFE_RELEASE( g_pNonSkinnedVertexLayout );
    SAFE_RELEASE( g_pRenderConstantBufferVS );
    SAFE_RELEASE( g_pRenderPS );
    SAFE_RELEASE( g_pRenderStillVS ); 
    SAFE_RELEASE( g_pCBMatrices );
    SAFE_RELEASE( g_pCBUserChange );
    SAFE_RELEASE( g_pCBImmutable );
    SAFE_RELEASE( g_pCBConstBoneWorld );   
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_QuadRasterizerState );
    SAFE_RELEASE( g_AlphaBlendState );
    SAFE_RELEASE( g_pQuadVS );
    SAFE_RELEASE( g_pQuadPS );
    g_SkinnedMesh.Destroy();

    SAFE_DELETE_ARRAY ( g_pInputs );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}



//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity( &mIdentity );
    g_SkinnedMesh.TransformMesh( &mIdentity, fTime );

    UpdateUnits( fElapsedTime );
}

float INTersectRaySphere(const D3DXVECTOR3 vRayDirection, D3DXVECTOR3 vRayOrigin, 
                         D3DXVECTOR3 vSphereOrigin,  float fpRadiusSquared) {
    D3DXVECTOR3 vRayToSphere = vSphereOrigin - vRayOrigin ;
    D3DXVECTOR3 vNormal = vRayToSphere;
    
    float fpB = D3DXVec3Dot( &vRayToSphere, &vRayDirection );
    float fpC = D3DXVec3Dot( &vRayToSphere, &vRayToSphere );
    float fpD = fpB * fpB - fpC + fpRadiusSquared; 
    return fpD > 0 ? sqrt(fpB) : -2e32f;

}

void CalculateScreenRayFromCoordinates( float x, float y, D3DXVECTOR3 &vRayDirection ) {

        RECT windowRect = DXUTGetWindowClientRect();
        const D3DXMATRIX* pmatProj = g_Camera.GetProjMatrix();

        // Compute the vector of the ray in screen space
        D3DXVECTOR3 v;
        v.x = ( ( ( 2.0f * x ) / windowRect.right ) - 1 ) / pmatProj->_11;
        v.y = -( ( ( 2.0f * y ) / windowRect.bottom) - 1 ) / pmatProj->_22;
        v.z = 1.0f;
        D3DXVECTOR3 vn = v;
        D3DXVec3Normalize(&vn, &vn);
        // Get the inverse view matrix
        const D3DXMATRIX matView = *g_Camera.GetViewMatrix();
        const D3DXMATRIX matWorld = *g_Camera.GetWorldMatrix();
        D3DXMATRIX mWorldView = matWorld * matView;
        D3DXMATRIX m; 
        D3DXMatrixInverse( &m, NULL, &mWorldView );

        vRayDirection.x = v.x * m._11 + v.y * m._21 + v.z * m._31;
        vRayDirection.y = v.x * m._12 + v.y * m._22 + v.z * m._32;
        vRayDirection.z = v.x * m._13 + v.y * m._23 + v.z * m._33;
        D3DXVec3Normalize( &vRayDirection, &vRayDirection );
    
}

void CalculateWindowOffsets ( INT x, INT y )  {
    RECT cl = DXUTGetWindowClientRect();
    g_Window.left=x;
    g_Window.right = x + cl.right;
    g_Window.top=y;
    g_Window.bottom = y + cl.bottom;
}

INT INTersectRayUnits ( const D3DXVECTOR3 &vRayDirection, const D3DXVECTOR3 &vRayOrigin ) {

    INT count = 0;
    for ( INT index=0; index < g_nSoldierInstances; ++index ) {

        D3DXMATRIX matProj = *g_Camera.GetProjMatrix();
        D3DXMATRIX matView = *g_Camera.GetViewMatrix();
        D3DXMATRIX matViewProj = matView * matProj;
        D3DXMATRIX matWorld ;
        g_units[index].GetInstanceWorldMatrix(&matWorld);                            

        float t_dist = INTersectRaySphere( vRayDirection, vRayOrigin, g_units[index].GetLocation(), 
            g_units[index].GetBoundingSphereSquared() );
        if ( t_dist >0) g_iListOfSelectedUnitsByIndex[count++] = index;
    }
    return count;
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
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
    g_UIControlManager.HandleTouchMessages (hWnd, uMsg, wParam, lParam);
    
    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, INT nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_TOGGLETOUCH :
            ToggleTouch ();    break;
        case IDC_TOGGLEDRAGSELECT :
            g_bEnableDragSelect  = !g_bEnableDragSelect;    
        break;
        case IDC_TOGGLEHELP: {
            g_bShowHelp = !g_bShowHelp;
        }
        break;
        case IDC_RESET : 
        {
            D3DXVECTOR3 vecEye( 4.0f, 10.0f, 4.0f );
            D3DXVECTOR3 vecAt ( 0.1f, 0.5f, -1.0f );
            g_Camera.SetViewParams( &vecEye, &vecAt );
            for (INT index=0; index < g_nSoldierInstances; ++index) {
                g_units[index].Init();
            }
        }
        break;
        
        case IDC_INSTANCES :
        {
            WCHAR str[MAX_PATH] = {0};
            g_nSoldierInstances = g_HUD.GetSlider( IDC_INSTANCES )->GetValue();
            g_iListOfSelectedUnitsByIndex = new INT [g_nSoldierInstances];
            swprintf_s( str, MAX_PATH, L"Soldlers: %d", g_nSoldierInstances );
            g_HUD.GetStatic( IDC_INSTANCES_STATIC )->SetText( str );

        }
        break;
    }
}

void SetBoneMatrices(  UINT iMesh )
{
    for( UINT i = 0; i < g_SkinnedMesh.GetNumInfluences( iMesh ); i++ )
    {
        const D3DXMATRIX* pMat = g_SkinnedMesh.GetMeshInfluenceMatrix( iMesh, i );
        D3DXMatrixTranspose(&g_CBConstBoneWorld.g_mConstBoneWorld[i], pMat);
    }
}


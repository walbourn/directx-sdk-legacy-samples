//--------------------------------------------------------------------------------------
// File: PIXGameDebugging.cpp
//
// This sample was used for the GDC 2008 presentation:
// "Beyond Printf: Debugging Games Through Tools"
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#pragma warning( disable:4995 ) // disable deprecated warning 
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"
#include <vector>       // stl vector header
#include <math.h>


using namespace std;    // saves us typing std:: before vector

// Uncomment below two lines to use PIX's HLSL shader tracing
// in addition to asm shader tracing
//#define DEBUG_VS   // Uncomment this line to debug D3D9 vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug D3D9 pixel shaders 

int                                 g_tex_cnt = 0;

// chess board and pieces
LPCWSTR g_aszMeshFile[] =
{
    L"ChessBoard.x",
    L"ChessBishop.x",
    L"ChessQueen.x",
    L"ChessRook.x",
    L"ChessPawn.x",
    L"ChessKing.x",
    L"ChessKnight.x",
};

// note: caustic texture filenames are dynamically generated in CreateTextures()
// the rest are below

// default textures
LPCWSTR g_aszMeshTexture[] =
{
    L"media\\ChessBoard.dds",
    L"media\\ChessBishop.dds",
    L"media\\ChessQueen.dds",
    L"media\\ChessRook.dds",
    L"media\\ChessPawn.dds",
    L"media\\ChessKing.dds",
    L"media\\ChessKnight.dds",
    L"media\\ChessColumn.dds",
};

// alternate textures
// same storage pattern as default
// but using only 1 texture file each
// to save on disk space
LPCWSTR g_aszMeshBigTexture[] =
{
    L"media\\LargeTexture.dds",
    L"media\\LargeTexture.dds",
    L"media\\LargeTexture.dds",
    L"media\\LargeTexture.dds",
    L"media\\LargeTexture.dds",
    L"media\\LargeTexture.dds",
    L"media\\LargeTexture.dds",
    L"media\\LargeTexture.dds",
};

LPCWSTR g_aszMeshSmallTexture[] =
{
    L"media\\SmallTexture.dds",
    L"media\\SmallTexture.dds",
    L"media\\SmallTexture.dds",
    L"media\\SmallTexture.dds",
    L"media\\SmallTexture.dds",
    L"media\\SmallTexture.dds",
    L"media\\SmallTexture.dds",
    L"media\\SmallTexture.dds",
};

#define NUM_OBJ (sizeof(g_aszMeshFile)/sizeof(g_aszMeshFile[0]))


D3DXMATRIXA16 g_amInitObjWorld[NUM_OBJ] =
{
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f,
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.0f, 0.0f, 0.0f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, //bishop
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.9f, 0.1f, 0.9f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, //queen
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   1.5f, 0.1f, 0.3f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, // rook
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   -0.3f, 0.1f, 1.5f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, // pawn
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   -0.9f, 0.1f, -0.9f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, // king
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.3f, 0.1f, -1.5f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, //knight
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   -1.5f, 0.1f, -0.3f, 1.0f ),

};

D3DXMATRIXA16 g_amInitObjWorldBadXForm[NUM_OBJ] =
{
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f,
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.0f, 0.0f, 0.0f, 1.0f ),
    D3DXMATRIXA16( 0.4f, 0.0f, 0.0f, 0.0f, //bishop
                   0.0f, 0.4f, 0.0f, 0.0f,
                   0.0f, 0.0f, 0.4f, 0.0f,
                   0.25f, 0.1f, -1.5f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, //queen
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   1.5f, 0.1f, 0.3f, 1.0f ),
    D3DXMATRIXA16( 45.0f, 0.0f, 0.0f, 0.0f, // rook
                   0.0f, 45.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 45.0f, 0.0f,
                   0.0f, -15.0f, 0.0f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, // pawn
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   -0.9f, 0.1f, -0.9f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, // king
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   0.3f, 0.1f, -1.5f, 1.0f ),
    D3DXMATRIXA16( 1.0f, 0.0f, 0.0f, 0.0f, //knight
                   0.0f, 1.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 1.0f, 0.0f,
                   -1.5f, 0.1f, -0.3f, 1.0f ),

};

D3DXMATRIXA16                       g_mScaler = D3DXMATRIXA16( 10.0f, 0.0f, 0.0f, 0.0f,
                                                               0.0f, 10.0f, 0.0f, 0.0f,
                                                               0.0f, 0.0f, 10.0f, 0.0f,
                                                               0.0f, 0.0f, 0.0f, 1.0f );

D3DVERTEXELEMENT9 g_aVertDecl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

struct LightShaftVertex
{
    float x, y, z;
    DWORD Color;
};

D3DVERTEXELEMENT9 g_aLightVertDecl[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    D3DDECL_END()
};

LightShaftVertex g_LightVertices[] = // triangle list for lightshafts
{
    { -1.0f,  -1.1f, 0.0f, 0x0066ccff },
    {  -1.0f, 1.5f, 0.0f, 0x1166ccff  }, // x, y, z, color	
    {  1.0f,  -1.10f, 0.0f, 0x0066ccff  },

    {  -1.0f, 1.5f, 0.0f, 0x1166ccff }, 	
    {  1.0f,  1.5f, 0.0f, 0x1166ccff  },
    {  1.0f,  -1.1f, 0.0f, 0x0066ccff  },
};

LPDIRECT3DVERTEXDECLARATION9        g_pVertDecl = NULL;// Vertex decl for the sample
LPDIRECT3DVERTEXDECLARATION9        g_pLightVertDecl = NULL;// Vertex decl for light shafts
LPDIRECT3DVERTEXBUFFER9             g_pVB = NULL; // Buffer to hold Vertices
CDXUTXFileMesh g_MeshObj[NUM_OBJ];		//scene object meshes

std::vector <LPDIRECT3DTEXTURE9>    g_pMeshTex;
std::vector <LPDIRECT3DTEXTURE9>    m_vecCausticTextures;

LPDIRECT3DTEXTURE9                  g_BlackTexture;
LPDIRECT3DTEXTURE9                  g_WhiteTexture;


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*                    g_pTxtHelper = NULL;
CDXUTDialog                         g_HUD;                  // dialog for standard controls
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

// UI control states
enum DrawSequenceType
{
    Standard,
    BadXForm,
    FrontToBack,
    BackToFront
};
DrawSequenceType                    g_DrawSequence = Standard;

bool                                g_bToggleHud = true;
bool                                g_bToggleZFight = false;
bool                                g_bToggleBadXForm = false;
bool                                g_bToggleCulling = false;
bool                                g_bToggleClear = false;
bool                                g_bToggleBlockyLight = false;
bool                                g_bToggleBlackClear = false;
bool                                g_bToggleTimeRender = true;
bool                                g_bToggleBadAlphaBlend = false;
bool                                g_bToggleAltTextures = false;
bool                                g_bToggleAltTexturesUpdate = false;
bool                                g_bToggleRefCountBug = false;
bool                                g_bToggleLightShaftsFirst = false;
bool                                g_bTriggerCrash = false;
int                                 g_uiMipLevel = 0;
int                                 g_iLightShaftCount = 1;
int                                 g_iDrawCount = 1;
int                                 g_iSelectCausticTexture = 0;
int                                 g_iSelectMeshTexture = 0;
int                                 g_iSelectFogState = 0;
int                                 g_iSelectScissorsState = 0;

// Direct3D 9 resources
ID3DXFont*                          g_pFont9 = NULL;
ID3DXSprite*                        g_pSprite9 = NULL;
ID3DXEffect*                        g_pEffect9 = NULL;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_TOGGLEHUD			4
#define IDC_TOGGLEZFIGHT		5
#define IDC_TOGGLEBADFOG		6
#define IDC_SELECTDRAWSEQUENCE	7
#define IDC_DRAWLABEL			8
#define IDC_TOGGLECULLING		9
#define IDC_TOGGLECLEAR			10
#define IDC_TOGGLEBLOCKYLIGHT   11
#define IDC_TOGGLEBLACKCLEAR	12
#define IDC_TOGGLETIMERENDER	13
#define IDC_TOGGLEBADALPHABLEND	14
#define IDC_SELECTDRAWCOUNT		15
#define IDC_SELECTMIPOVERRIDE	16
#define IDC_TOGGLEREFCOUNTBUG	17
#define IDC_SELECTFOG			18
#define IDC_SELECTTEXTURE		19
#define IDC_SELECTLIGHTSHAFTS	20
#define IDC_SELECTCAUSTICTEXTURE 21
#define IDC_TOGGLELIGHTSHAFTSFIRST	22
#define IDC_SELECTGAMECRASH			23
#define IDC_SELECTSCISSORSSTATE	24

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext );
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D9LostDevice( void* pUserContext );
void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

void InitApp();
void RenderText();
void DrawLightShafts( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
HRESULT CreateTextures( IDirect3DDevice9* pd3dDevice, LPCWSTR meshtextures[NUM_OBJ+1] );
HRESULT DestroyTextures();
HRESULT CreateMeshTextures( IDirect3DDevice9* pd3dDevice, LPCWSTR meshtextures[NUM_OBJ+1] );
HRESULT DestroyMeshTextures();
HRESULT ToggleAltTextures( IDirect3DDevice9* pd3dDevice, LPCWSTR meshtextures[NUM_OBJ+1] );


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below
    g_Camera.SetLimitPitch( false );

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );

    InitApp();

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"PIXGameDebugging" );
    DXUTCreateDevice( true, 640, 480 );
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

    g_HUD.SetCallback( OnGUIEvent ); int iY = 8;
    g_HUD.AddButton( IDC_TOGGLEHUD, L"HUD(F1)", 0, iY, 60, 22, VK_F1 );
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Full Screen(F2)", 70, iY, 90, 22, VK_F2 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Device (F3)", 0, iY += 24, 70, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"REF (F4)", 80, iY, 80, 22, VK_F4 );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"Crash App:", 30, iY += 24, 56, 22 );
    g_HUD.AddComboBox( IDC_SELECTGAMECRASH, 86, iY, 83, 22, 0, false );
    g_HUD.GetComboBox( IDC_SELECTGAMECRASH )->AddItem( L"No", ( LPVOID )0 );
    g_HUD.GetComboBox( IDC_SELECTGAMECRASH )->AddItem( L"Yes", ( LPVOID )1 );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"(F)og:", 54, iY += 22, 30, 22 );
    g_HUD.AddComboBox( IDC_SELECTFOG, 86, iY, 83, 22, 'F', false );
    g_HUD.GetComboBox( IDC_SELECTFOG )->AddItem( L"Default", ( LPVOID )0 );
    g_HUD.GetComboBox( IDC_SELECTFOG )->AddItem( L"None", ( LPVOID )1 );
    g_HUD.GetComboBox( IDC_SELECTFOG )->AddItem( L"Bad", ( LPVOID )2 );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"Sc(i)ssors:", 26, iY += 22, 60, 22 );
    g_HUD.AddComboBox( IDC_SELECTSCISSORSSTATE, 86, iY, 83, 22, 'I', false );
    g_HUD.GetComboBox( IDC_SELECTSCISSORSSTATE )->AddItem( L"Off", ( LPVOID )0 );
    g_HUD.GetComboBox( IDC_SELECTSCISSORSSTATE )->AddItem( L"Full", ( LPVOID )1 );
    g_HUD.GetComboBox( IDC_SELECTSCISSORSSTATE )->AddItem( L"1/2", ( LPVOID )2 );
    g_HUD.GetComboBox( IDC_SELECTSCISSORSSTATE )->AddItem( L"1/4", ( LPVOID )3 );
    g_HUD.GetComboBox( IDC_SELECTSCISSORSSTATE )->AddItem( L"Bad", ( LPVOID )4 );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"Multipl(y) Draws:", 0, iY += 22, 85, 22 );
    g_HUD.AddComboBox( IDC_SELECTDRAWCOUNT, 86, iY, 83, 22, 'Y', false );
    for( int x = 0; x <= 1024; x++ )
    {
        TCHAR strName[8];
        swprintf_s( strName, 8, L"%i", x );
        g_HUD.GetComboBox( IDC_SELECTDRAWCOUNT )->AddItem( strName, ( LPVOID )&x );
    }
    g_HUD.GetComboBox( IDC_SELECTDRAWCOUNT )->SetSelectedByText( L"1" );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"Mip O(v)erride:", 0, iY += 22, 72, 22 );
    g_HUD.AddComboBox( IDC_SELECTMIPOVERRIDE, 78, iY, 90, 22, 'V', false );
    g_HUD.GetComboBox( IDC_SELECTMIPOVERRIDE )->AddItem( L"Full chain", ( LPVOID )0 );
    for( int x = 1; x <= 64; x++ )
    {
        TCHAR strName[8];
        swprintf_s( strName, 8, L"%i", x );
        g_HUD.GetComboBox( IDC_SELECTMIPOVERRIDE )->AddItem( strName, ( LPVOID )&x );
    }


    g_HUD.AddStatic( IDC_DRAWLABEL, L"(T)exture:", 33, iY += 22, 48, 22 );
    g_HUD.AddComboBox( IDC_SELECTTEXTURE, 83, iY, 85, 22, 'T', false );
    g_HUD.GetComboBox( IDC_SELECTTEXTURE )->AddItem( L"Default", ( LPVOID )0 );
    g_HUD.GetComboBox( IDC_SELECTTEXTURE )->AddItem( L"Small", ( LPVOID )1 );
    g_HUD.GetComboBox( IDC_SELECTTEXTURE )->AddItem( L"Large", ( LPVOID )2 );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"Ca(u)stic Texture:", -10, iY += 22, 95, 22 );
    g_HUD.AddComboBox( IDC_SELECTCAUSTICTEXTURE, 83, iY, 85, 22, 'U', false );
    g_HUD.GetComboBox( IDC_SELECTCAUSTICTEXTURE )->AddItem( L"Default", ( LPVOID )0 );
    g_HUD.GetComboBox( IDC_SELECTCAUSTICTEXTURE )->AddItem( L"Black", ( LPVOID )1 );
    g_HUD.GetComboBox( IDC_SELECTCAUSTICTEXTURE )->AddItem( L"White", ( LPVOID )2 );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"(L)ight Layers:", 5, iY += 22, 85, 22 );
    g_HUD.AddComboBox( IDC_SELECTLIGHTSHAFTS, 83, iY, 85, 22, 'L', false );
    for( int x = 0; x <= 100; x++ )
    {
        TCHAR strName[8];
        swprintf_s( strName, 8, L"%i", x );
        g_HUD.GetComboBox( IDC_SELECTLIGHTSHAFTS )->AddItem( strName, ( LPVOID )&x );
    }
    g_HUD.GetComboBox( IDC_SELECTLIGHTSHAFTS )->SetSelectedByText( L"1" );

    g_HUD.AddStatic( IDC_DRAWLABEL, L"D(r)aw:", 0, iY += 22, 50, 22 );
    g_HUD.AddComboBox( IDC_SELECTDRAWSEQUENCE, 45, iY, 123, 22, 'R', false );
    g_HUD.GetComboBox( IDC_SELECTDRAWSEQUENCE )->SetDropHeight( 50 );
    g_HUD.GetComboBox( IDC_SELECTDRAWSEQUENCE )->AddItem( L"Standard", ( LPVOID )0 );
    g_HUD.GetComboBox( IDC_SELECTDRAWSEQUENCE )->AddItem( L"Bad XForm", ( LPVOID )1 );
    g_HUD.GetComboBox( IDC_SELECTDRAWSEQUENCE )->AddItem( L"Front Row 1st", ( LPVOID )2 );
    g_HUD.GetComboBox( IDC_SELECTDRAWSEQUENCE )->AddItem( L"Back Row 1st", ( LPVOID )3 );

    g_HUD.AddCheckBox( IDC_TOGGLEBADALPHABLEND, L"Disable Al(p)ha Blend", 0, iY += 24, 135, 22, g_bToggleBadAlphaBlend,
                       'P' );
    g_HUD.AddCheckBox( IDC_TOGGLECULLING, L"Disable (C)ulling", 0, iY += 22, 125, 22, g_bToggleCulling, 'C' );
    g_HUD.AddCheckBox( IDC_TOGGLECLEAR, L"Disable Cl(e)ar", 0, iY += 22, 125, 22, g_bToggleClear, 'E' );
    g_HUD.AddCheckBox( IDC_TOGGLEBLACKCLEAR, L"(B)lack Clear", 0, iY += 22, 125, 22, g_bToggleBlackClear, 'B' );
    g_HUD.AddCheckBox( IDC_TOGGLEZFIGHT, L"(Z)-Fighting", 0, iY += 22, 125, 22, g_bToggleZFight, 'Z' );
    g_HUD.AddCheckBox( IDC_TOGGLEBLOCKYLIGHT, L"Alt Lig(h)t Shader", 0, iY += 22, 125, 22, g_bToggleBlockyLight, 'H' );
    g_HUD.AddCheckBox( IDC_TOGGLELIGHTSHAFTSFIRST, L"Draw Li(g)ht Shafts 1st", 0, iY += 24, 140, 22,
                       g_bToggleLightShaftsFirst, 'G' );
    g_HUD.AddCheckBox( IDC_TOGGLETIMERENDER, L"Ti(m)e-Based Update", 0, iY += 22, 135, 22, g_bToggleTimeRender, 'M' );
    g_HUD.AddCheckBox( IDC_TOGGLEREFCOUNTBUG, L"Ref c(o)unt exit error", 0, iY += 22, 140, 22, g_bToggleRefCountBug,
                       'O' );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: Render Text" );
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->DrawTextLine( L"ESC to quit" );
    g_pTxtHelper->End();
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                      D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: IsD3D9DeviceAcceptable" );
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
    {
        DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Failed to find acceptable device" );
        DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Alpha blending unsupported" );
        DXUT_EndPerfEvent();
        return false;
    }

    // No fallback defined by this app, so reject any device that 
    // doesn't support at least ps2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
    {
        DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Failed to find acceptable device" );
        DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Pixel shader 2.0 unsupported" );
        DXUT_EndPerfEvent();
        return false;
    }

    DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Acceptable device found" );
    DXUT_EndPerfEvent();
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: ModifyDeviceSettings" );
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        D3DCAPS9 Caps;
        pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &Caps );

        // Turn vsync off
        pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

        // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
        // then switch to SWVP.
        if( ( Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
            Caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
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
    }

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D10_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE ) )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }
    DXUT_EndPerfEvent();
    return true;
}


//--------------------------------------------------------------------------------------
// Loads textures into memory. Used for device reset.
//--------------------------------------------------------------------------------------
HRESULT CreateTextures( IDirect3DDevice9* pd3dDevice, LPCWSTR meshtextures[NUM_OBJ+1] )
{
    HRESULT hr = S_OK;

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: CreateTextures" ); // These events are to help PIX identify what the code is doing

    hr = CreateMeshTextures( pd3dDevice, meshtextures );
    if( hr != S_OK ) // CreateMeshTextures failed, close off perf event hierarchy
    {
        DXUT_EndPerfEvent(); // needed 2x due to location of V_RETURN
        DXUT_EndPerfEvent();
    }

    D3DLOCKED_RECT lr;
    WCHAR str[MAX_PATH];

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Create Caustic Textures" );
    for( DWORD t = 0; t < 32; t++ )
    {
        TCHAR strTextureName[80];
        LPDIRECT3DTEXTURE9 pTexture;
        swprintf_s( strTextureName, 80, L"Caust%02ld.dds", t );
        hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strTextureName );
        hr = D3DXCreateTextureFromFile( pd3dDevice, str, &pTexture );
        if( FAILED( hr ) )
        {
            V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                                 D3DPOOL_DEFAULT, &pTexture, NULL ) );
            V_RETURN( pTexture->LockRect( 0, &lr, NULL, 0 ) );
            *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 255, 255, 255, 255 );
            V_RETURN( pTexture->UnlockRect( 0 ) );
        }
        m_vecCausticTextures.push_back( pTexture );
    }
    DXUT_EndPerfEvent();

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Create Black Texture" );
    V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_BlackTexture,
                                         NULL ) );
    V_RETURN( g_BlackTexture->LockRect( 0, &lr, NULL, 0 ) );
    *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 0, 0, 0, 255 );
    V_RETURN( g_BlackTexture->UnlockRect( 0 ) );
    DXUT_EndPerfEvent();

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Create White Texture" );
    V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_WhiteTexture,
                                         NULL ) );
    V_RETURN( g_WhiteTexture->LockRect( 0, &lr, NULL, 0 ) );
    *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 255, 255, 255, 255 );
    V_RETURN( g_WhiteTexture->UnlockRect( 0 ) );
    DXUT_EndPerfEvent();

    DXUT_EndPerfEvent();
    return hr;
}


HRESULT CreateMeshTextures( IDirect3DDevice9* pd3dDevice, LPCWSTR meshtextures[NUM_OBJ+1] )
{
    HRESULT hr = S_OK;
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: CreateMeshTextures" );
    D3DLOCKED_RECT lr;

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Mesh Texture setup" );
    for( int x = 0; x < NUM_OBJ + 1; x++ )
    {
        LPDIRECT3DTEXTURE9 pTexture;

        WCHAR str[MAX_PATH];
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, meshtextures[x] ) );

        hr = D3DXCreateTextureFromFileEx( pd3dDevice,
                                          str,
                                          D3DX_DEFAULT,
                                          D3DX_DEFAULT,
                                          g_uiMipLevel,
                                          0,
                                          D3DFMT_UNKNOWN,
                                          D3DPOOL_MANAGED,
                                          D3DX_FILTER_LINEAR,
                                          D3DX_FILTER_LINEAR,
                                          0,
                                          NULL,
                                          NULL,
                                          &pTexture );

        if( FAILED( hr ) )
        {
            V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                                 D3DPOOL_DEFAULT, &pTexture, NULL ) );
            V_RETURN( pTexture->LockRect( 0, &lr, NULL, 0 ) );
            *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 255, 255, 255, 255 );
            V_RETURN( pTexture->UnlockRect( 0 ) );
        }

        g_pMeshTex.push_back( pTexture );
    }

    DXUT_EndPerfEvent();

    DXUT_EndPerfEvent();
    return hr;
}


//--------------------------------------------------------------------------------------
// Used for device lost.
//--------------------------------------------------------------------------------------
HRESULT DestroyTextures()
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: DestroyTextures" );

    HRESULT hr = S_OK;

    hr = DestroyMeshTextures();

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Delete Caustic Textures" );
    while( !m_vecCausticTextures.empty() )
    {
        LPDIRECT3DTEXTURE9 pTexture = m_vecCausticTextures.back();
        SAFE_RELEASE( pTexture );
        SAFE_DELETE( pTexture );
        m_vecCausticTextures.pop_back();
    }
    DXUT_EndPerfEvent();

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Delete Black Texture" );
    SAFE_RELEASE( g_BlackTexture );
    SAFE_DELETE( g_BlackTexture );
    DXUT_EndPerfEvent();

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Delete White Texture" );
    SAFE_RELEASE( g_WhiteTexture );
    SAFE_DELETE( g_WhiteTexture );
    DXUT_EndPerfEvent();

    DXUT_EndPerfEvent();

    return hr;
}


HRESULT DestroyMeshTextures()
{
    HRESULT hr = S_OK;
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: DestroyMeshTextures" );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Purging Mesh Texture" );
    while( !g_pMeshTex.empty() )
    {
        LPDIRECT3DTEXTURE9 pTexture = g_pMeshTex.back();
        SAFE_RELEASE( pTexture );
        SAFE_DELETE( pTexture );
        g_pMeshTex.pop_back();
    }
    DXUT_EndPerfEvent();

    DXUT_EndPerfEvent();
    return hr;
}

//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: OnD3D9CreateDevice" );
    HRESULT hr;

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Create dialogs" );
    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
    DXUT_EndPerfEvent();

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Create font" );
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont9 ) );
    DXUT_EndPerfEvent();

    // Read the D3DX effect file
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Read D3DX effect file" );
    WCHAR str[MAX_PATH];
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXGameDebugging.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect9, NULL ) );
    DXUT_EndPerfEvent();

    // Setup the camera's view parameters
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Initialize camera" );
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, -2.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    DXUT_EndPerfEvent();

    // Create vertex declaration
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Declare and fill light shaft vertex buffer" );
    V_RETURN( pd3dDevice->CreateVertexBuffer( 6 * sizeof( LightShaftVertex ),
                                              D3DUSAGE_WRITEONLY,
                                              NULL,
                                              D3DPOOL_MANAGED,
                                              &g_pVB,
                                              NULL ) );
    // light shafts
    V_RETURN( pd3dDevice->CreateVertexDeclaration( g_aLightVertDecl, &g_pLightVertDecl ) );
    // Now we fill the vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the Vertices. This mechanism is required becuase vertex
    // buffers may be in device memory.

    LightShaftVertex* pVertices;
    hr = g_pVB->Lock( 0, sizeof( g_LightVertices ), ( void** )&pVertices, 0 );
    memcpy( pVertices, g_LightVertices, sizeof( g_LightVertices ) );
    hr = g_pVB->Unlock();
    DXUT_EndPerfEvent();

    // meshes
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Declare and fill mesh vertex buffer" );
    V_RETURN( pd3dDevice->CreateVertexDeclaration( g_aVertDecl, &g_pVertDecl ) );

    // Initialize the meshes
    for( int i = 0; i < NUM_OBJ; ++i )
    {
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_aszMeshFile[i] ) );
        if( FAILED( g_MeshObj[i].Create( pd3dDevice, str ) ) )
            return DXUTERR_MEDIANOTFOUND;
        V_RETURN( g_MeshObj[i].SetVertexDecl( pd3dDevice, g_aVertDecl ) );
    }
    DXUT_EndPerfEvent();

    DXUT_EndPerfEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice,
                                    const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: OnD3D9ResetDevice" );
    HRESULT hr;

    if( g_iSelectMeshTexture == 0 )
    {
        DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Using default mesh texture set" );
        hr = CreateTextures( pd3dDevice, g_aszMeshTexture );
    }
    else if( g_iSelectMeshTexture == 1 )
    {
        DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Using tiny mesh texture set" );
        hr = CreateTextures( pd3dDevice, g_aszMeshSmallTexture );
    }
    else
    {
        DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Using large mesh texture set" );
        hr = CreateTextures( pd3dDevice, g_aszMeshBigTexture );
    }
    if( hr != S_OK )
    {
        DXUT_EndPerfEvent();  // CreateTextures failed, the final end was not reached.
        DXUT_EndPerfEvent();
    }


    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Resetting dialogs, font, camera, hud" );
    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont9 ) V_RETURN( g_pFont9->OnResetDevice() );
    if( g_pEffect9 ) V_RETURN( g_pEffect9->OnResetDevice() );

    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite9 ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont9, g_pSprite9, NULL, NULL, 15 );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 480 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );
    DXUT_EndPerfEvent();


    DXUT_EndPerfEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: OnFrameMove" );
    g_Camera.FrameMove( fElapsedTime );
    DXUT_EndPerfEvent();
}

//--------------------------------------------------------------------------------------
// Dumps mesh textures and reloads using current settings
//--------------------------------------------------------------------------------------
HRESULT ToggleAltTextures( IDirect3DDevice9* pd3dDevice, LPCWSTR meshtextures[NUM_OBJ+1] )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: ToggleAltTextures" );

    HRESULT hr = S_OK;

    hr = DestroyMeshTextures();
    hr = CreateMeshTextures( pd3dDevice, meshtextures );

    g_bToggleAltTexturesUpdate = false; // set to false so this method is not called again until needed

    DXUT_EndPerfEvent();
    return S_OK;
}

void DrawLightShafts( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: DrawLightShafts" );
    HRESULT hr = S_OK;
    if( g_iLightShaftCount > 0 )
    {
        if( g_bToggleBadAlphaBlend )
        {
            pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        }
        else
        {
            pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        }


        if( !g_bToggleBlockyLight )
        {
            g_pEffect9->SetTechnique( "LightShaft" );
        }
        else
        {
            g_pEffect9->SetTechnique( "LightShaftAlternate" );
        }

        UINT cPasses;
        V( g_pEffect9->Begin( &cPasses, 0 ) );
        pd3dDevice->SetVertexDeclaration( g_pLightVertDecl );
        hr = pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( LightShaftVertex ) );
        g_pEffect9->SetFloat( "LightShaftZ", 0.5f );
        g_pEffect9->SetFloat( "LightShaftAlpha", 0.3f );
        V( g_pEffect9->BeginPass( 0 ) );
        if( g_iLightShaftCount == 1 )
        {
            hr = pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 2 );
        }
        else
        {
            float calc = 0;
            int step = ( 9850 - 9200 - 1 ) / g_iLightShaftCount; // zero case already checked
            if( step <= 0 )
                step = 1; // fail safe
            float alpha = 0.3f / ( ( float )g_iLightShaftCount );
            if( alpha <= 0.0f )
                alpha = 0.0f;
            g_pEffect9->SetFloat( "LightShaftAlpha", alpha );
            for( int level = 9850; level >= 9200; level -= step )
            {
                calc = ( ( float )level ) / 10000.0f;
                g_pEffect9->SetFloat( "LightShaftZ", calc );
                g_pEffect9->CommitChanges();
                hr = pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 2 );
            }
        }
        V( g_pEffect9->EndPass() );
        V( g_pEffect9->End() );
    }
    DXUT_EndPerfEvent();
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: OnD3D9FrameRender" );
    HRESULT hr;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;

    if( g_bToggleAltTexturesUpdate )
    {
        if( g_iSelectMeshTexture == 0 )
        {
            DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Using default mesh texture set" );
            ToggleAltTextures( pd3dDevice, g_aszMeshTexture );
        }
        else if( g_iSelectMeshTexture == 1 )
        {
            DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Using tiny mesh texture set" );
            ToggleAltTextures( pd3dDevice, g_aszMeshSmallTexture );
        }
        else
        {
            DXUT_SetPerfMarker( DXUT_PERFEVENTCOLOR2, L"Using large mesh texture set" );
            ToggleAltTextures( pd3dDevice, g_aszMeshBigTexture );
        }
    }



    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        DXUT_EndPerfEvent();
        return;
    }

    // Clear the render target and the zbuffer
    if( !g_bToggleClear )
    {	
        if( g_bToggleBlackClear )
        {
            V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f,
                                  0 ) );
        }
        else
        {
            V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 0, 102, 136 ), 1.0f,
                                  0 ) );
        }
    }

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        float time;
        if( g_bToggleTimeRender )
        {
            time = ( float )fTime;
        }
        else
        {
            time = 0;
        }

        V( g_pEffect9->SetFloat( "Time", time ) );
        // Get the projection & view matrix from the camera class
        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();


        // new pass

        mWorldViewProjection = mWorld * mView * mProj;

        g_pEffect9->SetTechnique( "Caustic" );

        D3DXVECTOR4 vLightAmbient( 0.2f, 0.2f, 0.2f, 0.2f );
        g_pEffect9->SetVector( "Light1Ambient", &vLightAmbient );

        D3DXVECTOR3 vLightToEye;
        D3DXVECTOR3 vLight( 0.0f, -1.0f, 0.0f );
        D3DXVECTOR4 vLightEye;

        // Transform direction vector into eye space
        D3DXVec3Normalize( &vLightToEye, &vLight );
        D3DXVec3TransformNormal( &vLightToEye, &vLightToEye, &mView );
        D3DXVec3Normalize( &vLightToEye, &vLightToEye );

        // Shader math requires that the vector is to the light
        vLightEye.x = -vLightToEye.x;
        vLightEye.y = -vLightToEye.y;
        vLightEye.z = -vLightToEye.z;
        vLightEye.w = 1.0f;

        g_pEffect9->SetVector( "Light1Dir", &vLightEye );

        D3DXVECTOR4 vFogData;
        if( g_iSelectFogState == 2 )
        {
            vFogData.x = 0.0f;    // Fog Start
            vFogData.y = 0.1f; // Fog End
        }
        else
        {
            vFogData.x = 1.5f;    // Fog Start
            vFogData.y = 8.5f; // Fog End
        }

        if( g_iSelectFogState == 1 )
        {
            pd3dDevice->SetRenderState( D3DRS_FOGENABLE, FALSE );
        }
        else
        {
            pd3dDevice->SetRenderState( D3DRS_FOGENABLE, TRUE );
        }

        vFogData.z = 1.0f / ( vFogData.y - vFogData.x ); // Fog range
        vFogData.w = 255.0f;
        g_pEffect9->SetVector( "FogData", &vFogData );

        DWORD tex = 0;
        g_tex_cnt++;

        if( g_bToggleTimeRender )
        {
            tex = ( ( DWORD )( fTime * 30.0 ) % 32 );
        }
        else
        {
            tex = ( ( DWORD )( g_tex_cnt ) % 32 );
        }

        if( g_bToggleCulling )
        {
            pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
        }
        else
        {
            pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
        }

        if( g_iSelectScissorsState )
        {
            pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
            RECT rc = DXUTGetWindowClientRect();
            int vOffset = 0, hOffset = 0;
            if( g_iSelectScissorsState == 2 )
            {
                // the following may be off by a pixel due to rounding error,
                // but close enough for purposes here
                vOffset = ( int )( ( ( float )( rc.bottom - rc.top ) ) * 0.1464466094f );
                hOffset = ( int )( ( ( float )( rc.right - rc.left ) ) * 0.1464466094f );
            }
            if( g_iSelectScissorsState == 3 )
            {
                vOffset = ( rc.bottom - rc.top ) >> 2;
                hOffset = ( rc.right - rc.left ) >> 2;
            }
            if( g_iSelectScissorsState == 2 || g_iSelectScissorsState == 3 )
            {
                rc.bottom -= vOffset;
                rc.top += vOffset;
                rc.right -= hOffset;
                rc.left += hOffset;
            }
            if( g_iSelectScissorsState == 4 )
            {
                rc.right = 1;
                rc.bottom = 1;
            }

            pd3dDevice->SetScissorRect( &rc );
        }
        else
        {
            pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
        }

        if( g_bTriggerCrash )
        {
            g_pVB->Release();
            DXUTToggleFullScreen();
        }

        if( ( m_vecCausticTextures.size() > tex ) && m_vecCausticTextures[tex] )
        {
            if( g_iSelectCausticTexture == 0 )
            {
                g_pEffect9->SetTexture( "CausticTexture", m_vecCausticTextures[tex] );
            }
            else if( g_iSelectCausticTexture == 1 )
            {
                g_pEffect9->SetTexture( "CausticTexture", g_BlackTexture );
            }
            else
            {
                g_pEffect9->SetTexture( "CausticTexture", g_WhiteTexture );
            }
        }

        if( g_bToggleLightShaftsFirst )
        {
            DrawLightShafts( pd3dDevice, fTime, fElapsedTime, pUserContext );
        }
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Drawing Scene" );
        if( g_DrawSequence == Standard || g_DrawSequence == BadXForm )
        {
            for( int obj = 0; obj < NUM_OBJ; ++obj )
            {
                D3DXMATRIXA16 mWV;
                if( g_DrawSequence == BadXForm )
                {
                    mWorldViewProjection = g_mScaler * g_amInitObjWorldBadXForm[obj] * mWorld * mView * mProj;
                    mWV = g_mScaler * g_amInitObjWorldBadXForm[obj] * mWorld * mView;


                }
                else
                {
                    mWorldViewProjection = g_mScaler * g_amInitObjWorld[obj] * mWorld * mView * mProj;
                    mWV = g_mScaler * g_amInitObjWorld[obj] * mWorld * mView;

                }

                V( g_pEffect9->SetMatrix( "WorldViewProj", &mWorldViewProjection ) );


                V( g_pEffect9->SetMatrix( "WorldView", &mWV ) );
                D3DXMATRIXA16 matWorldViewIT;
                D3DXMatrixInverse( &matWorldViewIT, NULL, &mWV );
                V( g_pEffect9->SetMatrix( "WorldViewIT", &matWorldViewIT ) );


                // Apply the technique contained in the effect
                LPD3DXMESH pMesh = g_MeshObj[obj].GetMesh();
                g_pEffect9->SetTexture( "MeshTexture", g_pMeshTex[obj] );
                UINT iPass, cPasses;
                V( g_pEffect9->Begin( &cPasses, 0 ) );

                for( iPass = 0; iPass < cPasses; iPass++ )
                {
                    V( g_pEffect9->BeginPass( iPass ) );

                    // The effect interface queues up the changes and performs them 
                    // with the CommitChanges call. You do not need to call CommitChanges if 
                    // you are not setting any parameters between the BeginPass and EndPass.
                    // V( g_pEffect->CommitChanges() );
                    for( DWORD i = 0; i < g_MeshObj[obj].m_dwNumMaterials; ++i )
                    {
                        if( obj == 0 && !g_bToggleZFight )
                        {
                            i++;
                        }
                        if( obj == 0 )
                        {
                            // Render the mesh with the applied technique
                            V( pMesh->DrawSubset( i ) );
                        }
                        else
                        {
                            for( int clone = 0; clone < g_iDrawCount; clone++ )
                            {
                                V( pMesh->DrawSubset( i ) );
                            }
                        }

                    }

                    V( g_pEffect9->EndPass() );
                }


                V( g_pEffect9->End() );

            }


        } // end Standard & BadXForm
        if( g_DrawSequence == FrontToBack )
        {
            D3DXMATRIXA16 mWV;

            int obj = 1;
            float z = -1.5f;

            while( z <= 1.6f ) // rounding error compensated
            {
                float x = 1.5f;
                while( x >= -1.6f ) // rounding error compensated
                {
                    D3DXMATRIXA16 xform = D3DXMATRIXA16( 10.0f, 0.0f, 0.0f, 0.0f,
                                                         0.0f, 10.0f, 0.0f, 0.0f,
                                                         0.0f, 0.0f, 10.0f, 0.0f,
                                                         x, 0.1f, z, 1.0f );
                    mWorldViewProjection = xform * mWorld * mView * mProj;
                    mWV = xform * mWorld * mView;

                    V( g_pEffect9->SetMatrix( "WorldViewProj", &mWorldViewProjection ) );

                    V( g_pEffect9->SetMatrix( "WorldView", &mWV ) );
                    D3DXMATRIXA16 matWorldViewIT;
                    D3DXMatrixInverse( &matWorldViewIT, NULL, &mWV );
                    V( g_pEffect9->SetMatrix( "WorldViewIT", &matWorldViewIT ) );

                    LPD3DXMESH pMesh = g_MeshObj[obj].GetMesh();
                    g_pEffect9->SetTexture( "MeshTexture", g_pMeshTex[obj] );
                    // Apply the technique contained in the effect
                    UINT iPass, cPasses;
                    V( g_pEffect9->Begin( &cPasses, 0 ) );

                    for( iPass = 0; iPass < cPasses; iPass++ )
                    {
                        V( g_pEffect9->BeginPass( iPass ) );

                        for( DWORD i = 0; i < g_MeshObj[obj].m_dwNumMaterials; ++i )
                        {
                            for( int clone = 0; clone < g_iDrawCount; clone++ )
                            {
                                V( pMesh->DrawSubset( i ) );
                            }
                        }

                        V( g_pEffect9->EndPass() );
                    }


                    V( g_pEffect9->End() );

                    x -= 0.6f;
                }
                obj++;
                z += 0.6f;
            }

            // draw chessboard after all pieces
            {
                mWorldViewProjection = g_mScaler * g_amInitObjWorld[0] * mWorld * mView * mProj;
                mWV = g_mScaler * g_amInitObjWorld[0] * mWorld * mView;

                V( g_pEffect9->SetMatrix( "WorldViewProj", &mWorldViewProjection ) );
                V( g_pEffect9->SetMatrix( "WorldView", &mWV ) );
                D3DXMATRIXA16 matWorldViewIT;
                D3DXMatrixInverse( &matWorldViewIT, NULL, &mWV );
                V( g_pEffect9->SetMatrix( "WorldViewIT", &matWorldViewIT ) );
                LPD3DXMESH pMesh = g_MeshObj[0].GetMesh();
                g_pEffect9->SetTexture( "MeshTexture", g_pMeshTex[0] );
                UINT iPass, cPasses;
                V( g_pEffect9->Begin( &cPasses, 0 ) );
                for( iPass = 0; iPass < cPasses; iPass++ )
                {
                    V( g_pEffect9->BeginPass( iPass ) );
                    for( DWORD i = 0; i < g_MeshObj[0].m_dwNumMaterials; ++i )
                    {
                        if( !g_bToggleZFight )
                        {
                            i++; // skip the first subset if z-fighting is off
                        }
                        V( pMesh->DrawSubset( i ) );

                    }
                    V( g_pEffect9->EndPass() );
                }
                V( g_pEffect9->End() );
            }
        }

        if( g_DrawSequence == BackToFront )
        {
            D3DXMATRIXA16 mWV;

            // draw chessboard before all pieces
            {
                mWorldViewProjection = g_mScaler * g_amInitObjWorld[0] * mWorld * mView * mProj;
                mWV = g_mScaler * g_amInitObjWorld[0] * mWorld * mView;
                V( g_pEffect9->SetMatrix( "WorldViewProj", &mWorldViewProjection ) );
                V( g_pEffect9->SetMatrix( "WorldView", &mWV ) );
                D3DXMATRIXA16 matWorldViewIT;
                D3DXMatrixInverse( &matWorldViewIT, NULL, &mWV );
                V( g_pEffect9->SetMatrix( "WorldViewIT", &matWorldViewIT ) );
                LPD3DXMESH pMesh = g_MeshObj[0].GetMesh();
                g_pEffect9->SetTexture( "MeshTexture", g_pMeshTex[0] );
                UINT iPass, cPasses;
                V( g_pEffect9->Begin( &cPasses, 0 ) );
                for( iPass = 0; iPass < cPasses; iPass++ )
                {
                    V( g_pEffect9->BeginPass( iPass ) );
                    for( DWORD i = 0; i < g_MeshObj[0].m_dwNumMaterials; ++i )
                    {
                        if( !g_bToggleZFight )
                        {
                            i++; // skip the first subset if z-fighting is off
                        }
                        V( pMesh->DrawSubset( i ) );

                    }
                    V( g_pEffect9->EndPass() );
                }
                V( g_pEffect9->End() );
            }

            int obj = 6;
            float z = 1.5f;

            while( z >= -1.6f ) // rounding error compensated
            {
                float x = 1.5f;
                while( x >= -1.6f ) // rounding error compensated
                {
                    D3DXMATRIXA16 xform = D3DXMATRIXA16( 10.0f, 0.0f, 0.0f, 0.0f,
                                                         0.0f, 10.0f, 0.0f, 0.0f,
                                                         0.0f, 0.0f, 10.0f, 0.0f,
                                                         x, 0.1f, z, 1.0f );
                    mWorldViewProjection = xform * mWorld * mView * mProj;
                    mWV = xform * mWorld * mView;

                    V( g_pEffect9->SetMatrix( "WorldViewProj", &mWorldViewProjection ) );

                    V( g_pEffect9->SetMatrix( "WorldView", &mWV ) );
                    D3DXMATRIXA16 matWorldViewIT;
                    D3DXMatrixInverse( &matWorldViewIT, NULL, &mWV );
                    V( g_pEffect9->SetMatrix( "WorldViewIT", &matWorldViewIT ) );

                    LPD3DXMESH pMesh = g_MeshObj[obj].GetMesh();
                    g_pEffect9->SetTexture( "MeshTexture", g_pMeshTex[obj] );

                    // Apply the technique contained in the effect
                    UINT iPass, cPasses;
                    V( g_pEffect9->Begin( &cPasses, 0 ) );

                    for( iPass = 0; iPass < cPasses; iPass++ )
                    {
                        V( g_pEffect9->BeginPass( iPass ) );
                        for( DWORD i = 0; i < g_MeshObj[obj].m_dwNumMaterials; ++i )
                        {
                            for( int clone = 0; clone < g_iDrawCount; clone++ )
                            {
                                V( pMesh->DrawSubset( i ) );
                            }
                        }

                        V( g_pEffect9->EndPass() );
                    }


                    V( g_pEffect9->End() );

                    x -= 0.6f;
                }
                obj--;
                z -= 0.6f;
            }


        }
        DXUT_EndPerfEvent(); // end drawing scene

        if( !g_bToggleLightShaftsFirst )
        {
            DrawLightShafts( pd3dDevice, fTime, fElapsedTime, pUserContext );
        }

        if( g_bToggleHud )
        {
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing

            if( g_iSelectScissorsState )
            {
                pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE ); // don't crop the hud
            }

            RenderText();
            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_SampleUI.OnRender( fElapsedTime ) );
            DXUT_EndPerfEvent();
        }


        V( pd3dDevice->EndScene() );
    }
    DXUT_EndPerfEvent();
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
    if( g_bToggleHud || uMsg == 256 ) // turn off gui hotspots when hud is inactive
    {
        *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
    }

    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

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
        case IDC_TOGGLEHUD:
            g_bToggleHud = !g_bToggleHud; break;
        case IDC_TOGGLEZFIGHT:
            g_bToggleZFight = !g_bToggleZFight; break;
        case IDC_TOGGLEBADALPHABLEND:
            g_bToggleBadAlphaBlend = !g_bToggleBadAlphaBlend; break;
        case IDC_TOGGLECULLING:
            g_bToggleCulling = !g_bToggleCulling; break;
        case IDC_TOGGLECLEAR:
            g_bToggleClear = !g_bToggleClear; break;
        case IDC_TOGGLEBLACKCLEAR:
            g_bToggleBlackClear = !g_bToggleBlackClear; break;
        case IDC_TOGGLEBLOCKYLIGHT:
            g_bToggleBlockyLight = !g_bToggleBlockyLight; break;
        case IDC_TOGGLETIMERENDER:
            g_bToggleTimeRender = !g_bToggleTimeRender; break;
        case IDC_TOGGLEREFCOUNTBUG:
            g_bToggleRefCountBug = !g_bToggleRefCountBug; break;
        case IDC_TOGGLELIGHTSHAFTSFIRST:
            g_bToggleLightShaftsFirst = !g_bToggleLightShaftsFirst; break;
        case IDC_SELECTDRAWSEQUENCE:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_DrawSequence = ( DrawSequenceType )
                    ( ( int )( size_t )g_HUD.GetComboBox( IDC_SELECTDRAWSEQUENCE )->GetSelectedData() );
            }
        }
            break;
        case IDC_SELECTCAUSTICTEXTURE:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_iSelectCausticTexture = ( DrawSequenceType )
                    ( ( int )( size_t )g_HUD.GetComboBox( IDC_SELECTCAUSTICTEXTURE )->GetSelectedData() );
            }
        }
            break;

        case IDC_SELECTLIGHTSHAFTS:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_iLightShaftCount = g_HUD.GetComboBox( IDC_SELECTLIGHTSHAFTS )->GetSelectedIndex();
            }
        }
            break;
        case IDC_SELECTDRAWCOUNT:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_iDrawCount = g_HUD.GetComboBox( IDC_SELECTDRAWCOUNT )->GetSelectedIndex();
            }
        }
            break;
        case IDC_SELECTSCISSORSSTATE:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_iSelectScissorsState = g_HUD.GetComboBox( IDC_SELECTSCISSORSSTATE )->GetSelectedIndex();
            }
        }
            break;
        case IDC_SELECTMIPOVERRIDE:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_bToggleAltTexturesUpdate = true;
                g_uiMipLevel = ( UINT )g_HUD.GetComboBox( IDC_SELECTMIPOVERRIDE )->GetSelectedIndex();
            }
        }
            break;
        case IDC_SELECTFOG:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_iSelectFogState = g_HUD.GetComboBox( IDC_SELECTFOG )->GetSelectedIndex();
            }
        }
            break;
        case IDC_SELECTTEXTURE:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_iSelectMeshTexture = g_HUD.GetComboBox( IDC_SELECTTEXTURE )->GetSelectedIndex();
                g_bToggleAltTexturesUpdate = true;
            }
        }
            break;
        case IDC_SELECTGAMECRASH:
        {
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                if( g_HUD.GetComboBox( IDC_SELECTGAMECRASH )->GetSelectedIndex() == 1 )
                {
                    g_bTriggerCrash = true;
                }
            }
        }
            break;
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: OnD3D9LostDevice" );
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont9 ) g_pFont9->OnLostDevice();
    if( g_pEffect9 ) g_pEffect9->OnLostDevice();
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_pTxtHelper );
    DestroyTextures();
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR3, L"Method: OnD3D9DestroyDevice" );
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();

    SAFE_RELEASE( g_pEffect9 );
    SAFE_RELEASE( g_pFont9 );
    SAFE_RELEASE( g_pLightVertDecl );
    SAFE_RELEASE( g_pVertDecl );

    if( !g_bToggleRefCountBug )
    {
        SAFE_RELEASE( g_pVB );
        SAFE_DELETE( g_pVB );
    }

    SAFE_DELETE( g_pEffect9 );
    SAFE_DELETE( g_pFont9 );
    SAFE_DELETE( g_pLightVertDecl );
    SAFE_DELETE( g_pVertDecl );

    for( int i = 0; i < NUM_OBJ; ++i )
    {
        g_MeshObj[i].Destroy();
    }

    DXUT_EndPerfEvent();
}



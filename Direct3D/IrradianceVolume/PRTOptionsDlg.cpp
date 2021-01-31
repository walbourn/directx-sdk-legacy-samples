//--------------------------------------------------------------------------------------
// File: PRTOptionsDlg.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include <msxml.h>
#include <oleauto.h>
#include <commdlg.h>
#include "prtmesh.h"
#include "prtsimulator.h"
#include "prtoptionsdlg.h"
#include "resource.h"
#include <shlobj.h>

#define MAX_PCA_VECTORS 24
COptionsFile            g_OptionsFile;
CPRTOptionsDlg*         g_pDlg = NULL;
CPRTLoadDlg*            g_pLoadDlg = NULL;
CPRTAdaptiveOptionsDlg* g_pAdaptiveDlg = NULL;
extern WCHAR g_szAppFolder[];

SIMULATOR_OPTIONS& GetGlobalOptions()
{
    return g_OptionsFile.m_Options;
}

COptionsFile& GetGlobalOptionsFile()
{
    return g_OptionsFile;
}

//--------------------------------------------------------------------------------------
// Struct to store material params
//--------------------------------------------------------------------------------------
struct PREDEFINED_MATERIALS
{
    const TCHAR* strName;
    D3DCOLORVALUE ReducedScattering;
    D3DCOLORVALUE Absorption;
    D3DCOLORVALUE Diffuse;
    float fRelativeIndexOfRefraction;
};


//--------------------------------------------------------------------------------------
// Subsurface scattering parameters from:
// "A Practical Model for Subsurface Light Transport", 
// Henrik Wann Jensen, Steve R. Marschner, Marc Levoy, Pat Hanrahan.
// SIGGRAPH 2001
//--------------------------------------------------------------------------------------
const PREDEFINED_MATERIALS g_aPredefinedMaterials[] =
{
    // name             scattering (R/G/B/A)            absorption (R/G/B/A)                    reflectance (R/G/B/A)           index of refraction
    TEXT( "Default" ), {2.00f, 2.00f, 2.00f, 1.0f}, {0.0030f, 0.0030f, 0.0460f, 1.0f}, {1.00f, 1.00f, 1.00f, 1.0f},
    1.3f,
    TEXT( "Apple" ), {2.29f, 2.39f, 1.97f, 1.0f}, {0.0030f, 0.0030f, 0.0460f, 1.0f}, {0.85f, 0.84f, 0.53f, 1.0f},
    1.3f,
    TEXT( "Chicken1" ), {0.15f, 0.21f, 0.38f, 1.0f}, {0.0150f, 0.0770f, 0.1900f, 1.0f}, {0.31f, 0.15f, 0.10f,
        1.0f},    1.3f,
    TEXT( "Chicken2" ), {0.19f, 0.25f, 0.32f, 1.0f}, {0.0180f, 0.0880f, 0.2000f, 1.0f}, {0.32f, 0.16f, 0.10f,
        1.0f},    1.3f,
    TEXT( "Cream" ), {7.38f, 5.47f, 3.15f, 1.0f}, {0.0002f, 0.0028f, 0.0163f, 1.0f}, {0.98f, 0.90f, 0.73f, 1.0f},
    1.3f,
    TEXT( "Ketchup" ), {0.18f, 0.07f, 0.03f, 1.0f}, {0.0610f, 0.9700f, 1.4500f, 1.0f}, {0.16f, 0.01f, 0.00f, 1.0f},
    1.3f,
    TEXT( "Marble" ), {2.19f, 2.62f, 3.00f, 1.0f}, {0.0021f, 0.0041f, 0.0071f, 1.0f}, {0.83f, 0.79f, 0.75f, 1.0f},
    1.5f,
    TEXT( "Potato" ), {0.68f, 0.70f, 0.55f, 1.0f}, {0.0024f, 0.0090f, 0.1200f, 1.0f}, {0.77f, 0.62f, 0.21f, 1.0f},
    1.3f,
    TEXT( "Skimmilk" ), {0.70f, 1.22f, 1.90f, 1.0f}, {0.0014f, 0.0025f, 0.0142f, 1.0f}, {0.81f, 0.81f, 0.69f,
        1.0f},    1.3f,
    TEXT( "Skin1" ), {0.74f, 0.88f, 1.01f, 1.0f}, {0.0320f, 0.1700f, 0.4800f, 1.0f}, {0.44f, 0.22f, 0.13f, 1.0f},
    1.3f,
    TEXT( "Skin2" ), {1.09f, 1.59f, 1.79f, 1.0f}, {0.0130f, 0.0700f, 0.1450f, 1.0f}, {0.63f, 0.44f, 0.34f, 1.0f},
    1.3f,
    TEXT( "Spectralon" ), {11.60f,20.40f,14.90f, 1.0f}, {0.0000f, 0.0000f, 0.0000f, 1.0f}, {1.00f, 1.00f, 1.00f,
        1.0f},    1.3f,
    TEXT( "Wholemilk" ), {2.55f, 3.21f, 3.77f, 1.0f}, {0.0011f, 0.0024f, 0.0140f, 1.0f}, {0.91f, 0.88f, 0.76f,
        1.0f},    1.3f,
    TEXT( "Custom" ), {0.00f, 0.00f, 0.00f, 1.0f}, {0.0000f, 0.0000f, 0.0000f, 1.0f}, {0.00f, 0.00f, 0.00f, 1.0f},
    0.0f,
};
const int               g_aPredefinedMaterialsSize = sizeof( g_aPredefinedMaterials ) / sizeof
    ( g_aPredefinedMaterials[0] );


//--------------------------------------------------------------------------------------
CPRTOptionsDlg::CPRTOptionsDlg( void )
{
    m_hDlg = NULL;
    g_pDlg = this;
    m_hToolTip = NULL;
    m_hMsgProcHook = NULL;
    m_hDlg = NULL;
    m_bShowTooltips = true;
    m_bComboBoxSelChange = false;
}


//--------------------------------------------------------------------------------------
CPRTOptionsDlg::~CPRTOptionsDlg( void )
{
    g_pDlg = NULL;
}


//--------------------------------------------------------------------------------------
HRESULT CPRTOptionsDlg::SaveOptions( WCHAR* strFile )
{
    return g_OptionsFile.SaveOptions( strFile );
}


//--------------------------------------------------------------------------------------
COptionsFile::COptionsFile()
{
    ResetSettings();

    WCHAR strPath[MAX_PATH] = {0};
    SHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strPath );
    wcscat_s( strPath, MAX_PATH, g_szAppFolder );

    // Create the app directory if it's not already there
    if( 0xFFFFFFFF == GetFileAttributes( strPath ) )
    {
        // create the directory
        if( !CreateDirectory( strPath, NULL ) )
        {
            assert( true );
        }
    }

    wcscpy_s( m_strFile, MAX_PATH, strPath );
    wcscat_s( m_strFile, MAX_PATH, L"options.xml" );

    LoadOptions( m_strFile );
}


//--------------------------------------------------------------------------------------
COptionsFile::~COptionsFile()
{
}


//--------------------------------------------------------------------------------------
HRESULT COptionsFile::SaveOptions( WCHAR* strFile )
{
    if( strFile == NULL )
        strFile = m_strFile;

    HRESULT hr = S_OK;
    IXMLDOMDocument* pDoc = NULL;

    CoInitialize( NULL );

    // Create an empty XML document
    V_RETURN( CoCreateInstance( CLSID_DOMDocument, NULL,
                                CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument,
                                ( void** )&pDoc ) );

    IXMLDOMNode* pRootNode = NULL;
    IXMLDOMNode* pTopNode = NULL;
    pDoc->QueryInterface( IID_IXMLDOMNode, ( VOID** )&pRootNode );
    CXMLHelper::CreateChildNode( pDoc, pRootNode, L"PRTOptions", NODE_ELEMENT, &pTopNode );

    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"strInitialDir", m_Options.strInitialDir );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"strInputMesh", m_Options.strInputMesh );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"strResultsFile", m_Options.strResultsFile );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwOrder", m_Options.dwOrder );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwNumRays", m_Options.dwNumRays );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwNumBounces", m_Options.dwNumBounces );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"bShowTooltips", ( DWORD )m_Options.bShowTooltips );

    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwNumClusters", m_Options.dwNumClusters );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"Quality", ( DWORD )m_Options.Quality );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwNumPCA", m_Options.dwNumPCA );

    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"bSubsurfaceScattering", ( DWORD )m_Options.bSubsurfaceScattering );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"fLengthScale", m_Options.fLengthScale );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwNumChannels", ( DWORD )m_Options.dwNumChannels );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwPredefinedMatIndex", m_Options.dwPredefinedMatIndex );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"Diffuse.r", m_Options.Diffuse.r );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"Diffuse.g", m_Options.Diffuse.g );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"Diffuse.b", m_Options.Diffuse.b );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"Absorption.r", m_Options.Absorption.r );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"Absorption.g", m_Options.Absorption.g );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"Absorption.b", m_Options.Absorption.b );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"ReducedScattering.r", m_Options.ReducedScattering.r );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"ReducedScattering.g", m_Options.ReducedScattering.g );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"ReducedScattering.b", m_Options.ReducedScattering.b );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"fRelativeIndexOfRefraction", m_Options.fRelativeIndexOfRefraction );

    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"bAdaptive", ( DWORD )m_Options.bAdaptive );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"bRobustMeshRefine", ( DWORD )m_Options.bRobustMeshRefine );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"fRobustMeshRefineMinEdgeLength",
                                m_Options.fRobustMeshRefineMinEdgeLength );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwRobustMeshRefineMaxSubdiv",
                                m_Options.dwRobustMeshRefineMaxSubdiv );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"bAdaptiveDL", ( DWORD )m_Options.bAdaptiveDL );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"fAdaptiveDLMinEdgeLength", m_Options.fAdaptiveDLMinEdgeLength );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"fAdaptiveDLThreshold", m_Options.fAdaptiveDLThreshold );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwAdaptiveDLMaxSubdiv", m_Options.dwAdaptiveDLMaxSubdiv );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"bAdaptiveBounce", ( DWORD )m_Options.bAdaptiveBounce );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"fAdaptiveBounceMinEdgeLength",
                                m_Options.fAdaptiveBounceMinEdgeLength );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"fAdaptiveBounceThreshold", m_Options.fAdaptiveBounceThreshold );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"dwAdaptiveBounceMaxSubdiv", m_Options.dwAdaptiveBounceMaxSubdiv );
    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"strOutputMesh", m_Options.strOutputMesh );

    CXMLHelper::CreateNewValue( pDoc, pTopNode, L"bBinaryOutputXFile", ( DWORD )m_Options.bBinaryOutputXFile );

    SAFE_RELEASE( pTopNode );
    SAFE_RELEASE( pRootNode );

    // Save the doc
    VARIANT vName;
    vName.vt = VT_BSTR;
    vName.bstrVal = SysAllocString( strFile );
    hr = pDoc->save( vName );
    VariantClear( &vName );

    SAFE_RELEASE( pDoc );

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CPRTOptionsDlg::LoadOptions( WCHAR* strFile )
{
    return g_OptionsFile.LoadOptions( strFile );
}

//--------------------------------------------------------------------------------------
HRESULT COptionsFile::LoadOptions( WCHAR* strFile )
{
    if( strFile == NULL )
        strFile = m_strFile;

    HRESULT hr = S_OK;
    VARIANT v;
    VARIANT_BOOL vb;
    IXMLDOMDocument* pDoc = NULL;
    IXMLDOMNode* pRootNode = NULL;
    IXMLDOMNode* pTopNode = NULL;
    IXMLDOMNode* pNode = NULL;

    CoInitialize( NULL );
    V_RETURN( CoCreateInstance( CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER,
                                IID_IXMLDOMDocument, ( void** )&pDoc ) );

    VariantInit( &v );
    v.vt = VT_BSTR;
    V_BSTR (&v ) = SysAllocString( strFile );
    hr = pDoc->load( v, &vb );
    if( FAILED( hr ) || hr == S_FALSE )
        return E_FAIL;
    VariantClear( &v );

    pDoc->QueryInterface( IID_IXMLDOMNode, ( void** )&pRootNode );
    SAFE_RELEASE( pDoc );
    pRootNode->get_firstChild( &pTopNode );
    SAFE_RELEASE( pRootNode );
    pTopNode->get_firstChild( &pNode );
    SAFE_RELEASE( pTopNode );

    ZeroMemory( &m_Options, sizeof( SIMULATOR_OPTIONS ) );
    CXMLHelper::GetValue( pNode, L"strInitialDir", m_Options.strInitialDir, MAX_PATH );
    CXMLHelper::GetValue( pNode, L"strInputMesh", m_Options.strInputMesh, MAX_PATH );
    CXMLHelper::GetValue( pNode, L"strResultsFile", m_Options.strResultsFile, MAX_PATH );
    CXMLHelper::GetValue( pNode, L"dwOrder", &m_Options.dwOrder );
    CXMLHelper::GetValue( pNode, L"dwNumRays", &m_Options.dwNumRays );
    CXMLHelper::GetValue( pNode, L"dwNumBounces", &m_Options.dwNumBounces );
    CXMLHelper::GetValue( pNode, L"bShowTooltips", &m_Options.bShowTooltips );

    CXMLHelper::GetValue( pNode, L"dwNumClusters", &m_Options.dwNumClusters );
    DWORD dwQuality;
    CXMLHelper::GetValue( pNode, L"Quality", &dwQuality );
    m_Options.Quality = static_cast<D3DXSHCOMPRESSQUALITYTYPE>( dwQuality );
    CXMLHelper::GetValue( pNode, L"dwNumPCA", &m_Options.dwNumPCA );

    CXMLHelper::GetValue( pNode, L"bSubsurfaceScattering", &m_Options.bSubsurfaceScattering );
    CXMLHelper::GetValue( pNode, L"fLengthScale", &m_Options.fLengthScale );
    CXMLHelper::GetValue( pNode, L"dwNumChannels", &m_Options.dwNumChannels );
    CXMLHelper::GetValue( pNode, L"dwPredefinedMatIndex", &m_Options.dwPredefinedMatIndex );
    CXMLHelper::GetValue( pNode, L"Diffuse.r", &m_Options.Diffuse.r );
    CXMLHelper::GetValue( pNode, L"Diffuse.g", &m_Options.Diffuse.g );
    CXMLHelper::GetValue( pNode, L"Diffuse.b", &m_Options.Diffuse.b );
    CXMLHelper::GetValue( pNode, L"Absorption.r", &m_Options.Absorption.r );
    CXMLHelper::GetValue( pNode, L"Absorption.g", &m_Options.Absorption.g );
    CXMLHelper::GetValue( pNode, L"Absorption.b", &m_Options.Absorption.b );
    CXMLHelper::GetValue( pNode, L"ReducedScattering.r", &m_Options.ReducedScattering.r );
    CXMLHelper::GetValue( pNode, L"ReducedScattering.g", &m_Options.ReducedScattering.g );
    CXMLHelper::GetValue( pNode, L"ReducedScattering.b", &m_Options.ReducedScattering.b );
    CXMLHelper::GetValue( pNode, L"fRelativeIndexOfRefraction", &m_Options.fRelativeIndexOfRefraction );

    CXMLHelper::GetValue( pNode, L"bAdaptive", &m_Options.bAdaptive );
    CXMLHelper::GetValue( pNode, L"bRobustMeshRefine", &m_Options.bRobustMeshRefine );
    CXMLHelper::GetValue( pNode, L"fRobustMeshRefineMinEdgeLength", &m_Options.fRobustMeshRefineMinEdgeLength );
    CXMLHelper::GetValue( pNode, L"dwRobustMeshRefineMaxSubdiv", &m_Options.dwRobustMeshRefineMaxSubdiv );
    CXMLHelper::GetValue( pNode, L"bAdaptiveDL", &m_Options.bAdaptiveDL );
    CXMLHelper::GetValue( pNode, L"fAdaptiveDLMinEdgeLength", &m_Options.fAdaptiveDLMinEdgeLength );
    CXMLHelper::GetValue( pNode, L"fAdaptiveDLThreshold", &m_Options.fAdaptiveDLThreshold );
    CXMLHelper::GetValue( pNode, L"dwAdaptiveDLMaxSubdiv", &m_Options.dwAdaptiveDLMaxSubdiv );
    CXMLHelper::GetValue( pNode, L"bAdaptiveBounce", &m_Options.bAdaptiveBounce );
    CXMLHelper::GetValue( pNode, L"fAdaptiveBounceMinEdgeLength", &m_Options.fAdaptiveBounceMinEdgeLength );
    CXMLHelper::GetValue( pNode, L"fAdaptiveBounceThreshold", &m_Options.fAdaptiveBounceThreshold );
    CXMLHelper::GetValue( pNode, L"dwAdaptiveBounceMaxSubdiv", &m_Options.dwAdaptiveBounceMaxSubdiv );
    CXMLHelper::GetValue( pNode, L"strOutputMesh", m_Options.strOutputMesh, MAX_PATH );
    CXMLHelper::GetValue( pNode, L"bBinaryOutputXFile", &m_Options.bBinaryOutputXFile );

    SAFE_RELEASE( pNode );

    return S_OK;
}


//--------------------------------------------------------------------------------------
bool CPRTOptionsDlg::Show()
{
    // Ask the user about param settings for the PRT Simulation
    int nResult = ( int )DialogBox( NULL, MAKEINTRESOURCE( IDD_SIMULATION_OPTIONS ),
                                    DXUTGetHWND(), StaticDlgProc );

    UnhookWindowsHookEx( m_hMsgProcHook );
    DestroyWindow( m_hToolTip );

    if( nResult == IDOK )
        return true;
    else
        return false;
}


//--------------------------------------------------------------------------------------
INT_PTR CALLBACK CPRTOptionsDlg::StaticDlgProc( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    return g_pDlg->DlgProc( hDlg, uMsg, wParam, lParam );
}


//--------------------------------------------------------------------------------------
// Handles messages for the Simulation options dialog
//--------------------------------------------------------------------------------------
INT_PTR CALLBACK CPRTOptionsDlg::DlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_INITDIALOG:
        {
            m_hDlg = hDlg;

            UpdateControlsWithSettings( hDlg );

            m_hToolTip = CreateWindowEx( 0, TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP,
                                         CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                         hDlg, NULL, ( HINSTANCE )GetModuleHandle( NULL ), NULL );
            SendMessage( m_hToolTip, TTM_SETMAXTIPWIDTH, 0, 300 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_AUTOPOP, 32000 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_INITIAL, 0 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_RESHOW, 0 );
            EnumChildWindows( hDlg, ( WNDENUMPROC )EnumChildProc, 0 );
            m_hMsgProcHook = SetWindowsHookEx( WH_GETMESSAGE, GetMsgProc, ( HINSTANCE )GetModuleHandle( NULL ),
                                               GetCurrentThreadId() );

            return TRUE;
        }

        case WM_NOTIFY:
        {
            NMHDR* pNMHDR = ( LPNMHDR )lParam;
            if( pNMHDR == NULL )
                break;

            if( pNMHDR->code == TTN_NEEDTEXT )
            {
                NMTTDISPINFO* pNMTDI = ( LPNMTTDISPINFO )lParam;
                int nDlgId = GetDlgCtrlID( ( HWND )pNMHDR->idFrom );
                if( m_bShowTooltips )
                {
                    GetToolTipText( nDlgId, pNMTDI );
                    return TRUE;
                }
                else
                {
                    return FALSE;
                }
            }
            break;
        }

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDC_SETTINGS_RESET:
                    ResetSettings();
                    UpdateControlsWithSettings( hDlg );
                    break;

                case IDC_ADAPTIVE_CHECK:
                case IDC_SPECTRAL_CHECK:
                case IDC_SUBSURF_CHECK:
                    {
                        BOOL bSubSurface = ( IsDlgButtonChecked( hDlg, IDC_SUBSURF_CHECK ) == BST_CHECKED );
                        BOOL bSpectral = ( IsDlgButtonChecked( hDlg, IDC_SPECTRAL_CHECK ) == BST_CHECKED );
                        BOOL bAdaptive = ( IsDlgButtonChecked( hDlg, IDC_ADAPTIVE_CHECK ) == BST_CHECKED );

                        EnableWindow( GetDlgItem( hDlg, IDC_OUTPUT_MESH_TEXT ), bAdaptive );
                        EnableWindow( GetDlgItem( hDlg, IDC_OUTPUT_MESH_EDIT ), bAdaptive );
                        EnableWindow( GetDlgItem( hDlg, IDC_OUTPUT_MESH_BROWSE_BUTTON ), bAdaptive );
                        EnableWindow( GetDlgItem( hDlg, IDC_ADAPTIVE_SETTINGS ), bAdaptive );
                        EnableWindow( GetDlgItem( hDlg, IDC_MESH_SAVE_BINARY_RADIO ), bAdaptive );
                        EnableWindow( GetDlgItem( hDlg, IDC_MESH_SAVE_TEXT_RADIO ), bAdaptive );

                        EnableWindow( GetDlgItem( hDlg, IDC_REFRACTION_TEXT ), bSubSurface );
                        EnableWindow( GetDlgItem( hDlg, IDC_ABSORPTION_TEXT ), bSubSurface );
                        EnableWindow( GetDlgItem( hDlg, IDC_SCATTERING_TEXT ), bSubSurface );
                        EnableWindow( GetDlgItem( hDlg, IDC_REFLECTANCE_R_EDIT ), true );
                        EnableWindow( GetDlgItem( hDlg, IDC_REFLECTANCE_G_EDIT ), bSpectral );
                        EnableWindow( GetDlgItem( hDlg, IDC_REFLECTANCE_B_EDIT ), bSpectral );
                        EnableWindow( GetDlgItem( hDlg, IDC_ABSORPTION_R_EDIT ), bSubSurface );
                        EnableWindow( GetDlgItem( hDlg, IDC_ABSORPTION_G_EDIT ), bSubSurface && bSpectral );
                        EnableWindow( GetDlgItem( hDlg, IDC_ABSORPTION_B_EDIT ), bSubSurface && bSpectral );
                        EnableWindow( GetDlgItem( hDlg, IDC_SCATTERING_R_EDIT ), bSubSurface );
                        EnableWindow( GetDlgItem( hDlg, IDC_SCATTERING_G_EDIT ), bSubSurface && bSpectral );
                        EnableWindow( GetDlgItem( hDlg, IDC_SCATTERING_B_EDIT ), bSubSurface && bSpectral );
                        EnableWindow( GetDlgItem( hDlg, IDC_REFRACTION_EDIT ), bSubSurface );
                        EnableWindow( GetDlgItem( hDlg, IDC_LENGTH_SCALE_TEXT ), bSubSurface );
                        EnableWindow( GetDlgItem( hDlg, IDC_LENGTH_SCALE_EDIT ), bSubSurface );
                        break;
                    }

                case IDC_PREDEF_COMBO:
                    if( HIWORD( wParam ) == CBN_SELCHANGE )
                    {
                        HWND hPreDefCombo = GetDlgItem( hDlg, IDC_PREDEF_COMBO );
                        int nIndex = ( int )SendMessage( hPreDefCombo, CB_GETCURSEL, 0, 0 );
                        LRESULT lResult = SendMessage( hPreDefCombo, CB_GETITEMDATA, nIndex, 0 );
                        if( lResult != CB_ERR )
                        {
                            // If this is the "Custom" material, don't change the numbers
                            if( nIndex == g_aPredefinedMaterialsSize - 1 )
                                break;

                            PREDEFINED_MATERIALS* pMat = ( PREDEFINED_MATERIALS* )lResult;
                            TCHAR sz[256];
                            ZeroMemory( sz, 256 );

                            m_bComboBoxSelChange = true;
                            swprintf_s( sz, 256, TEXT( "%0.4f" ), pMat->Absorption.r );
                            SetDlgItemText( hDlg, IDC_ABSORPTION_R_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.4f" ), pMat->Absorption.g );
                            SetDlgItemText( hDlg, IDC_ABSORPTION_G_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.4f" ), pMat->Absorption.b );
                            SetDlgItemText( hDlg, IDC_ABSORPTION_B_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.2f" ), pMat->Diffuse.r );
                            SetDlgItemText( hDlg, IDC_REFLECTANCE_R_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.2f" ), pMat->Diffuse.g );
                            SetDlgItemText( hDlg, IDC_REFLECTANCE_G_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.2f" ), pMat->Diffuse.b );
                            SetDlgItemText( hDlg, IDC_REFLECTANCE_B_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.2f" ), pMat->ReducedScattering.r );
                            SetDlgItemText( hDlg, IDC_SCATTERING_R_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.2f" ), pMat->ReducedScattering.g );
                            SetDlgItemText( hDlg, IDC_SCATTERING_G_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.2f" ), pMat->ReducedScattering.b );
                            SetDlgItemText( hDlg, IDC_SCATTERING_B_EDIT, sz );
                            swprintf_s( sz, 256, TEXT( "%0.2f" ), pMat->fRelativeIndexOfRefraction );
                            SetDlgItemText( hDlg, IDC_REFRACTION_EDIT, sz );
                            m_bComboBoxSelChange = false;
                        }
                    }
                    break;

                case IDC_REFLECTANCE_R_EDIT:
                case IDC_REFLECTANCE_G_EDIT:
                case IDC_REFLECTANCE_B_EDIT:
                case IDC_SCATTERING_R_EDIT:
                case IDC_SCATTERING_G_EDIT:
                case IDC_SCATTERING_B_EDIT:
                case IDC_ABSORPTION_R_EDIT:
                case IDC_ABSORPTION_G_EDIT:
                case IDC_ABSORPTION_B_EDIT:
                    {
                        if( HIWORD( wParam ) == EN_CHANGE && !m_bComboBoxSelChange )
                        {
                            HWND hPreDefCombo = GetDlgItem( hDlg, IDC_PREDEF_COMBO );
                            SendMessage( hPreDefCombo, CB_SETCURSEL, g_aPredefinedMaterialsSize - 1, 0 );
                        }
                        break;
                    }

                case IDC_INPUT_BROWSE_BUTTON:
                {
                    // Display the OpenFileName dialog
                    OPENFILENAME ofn =
                    {
                        sizeof( OPENFILENAME ), hDlg, NULL,
                        L".X Files (.x)\0*.x\0All Files\0*.*\0\0",
                        NULL, 0, 1, GetGlobalOptions().strInputMesh, MAX_PATH, NULL, 0,
                        GetGlobalOptions().strInitialDir, L"Open Mesh File",
                        OFN_FILEMUSTEXIST, 0, 1, NULL, 0, NULL, NULL
                    };
                    if( TRUE == GetOpenFileName( &ofn ) )
                    {
                        wcscpy_s( GetGlobalOptions().strInitialDir, MAX_PATH, GetGlobalOptions().strInputMesh );
                        WCHAR* pLastSlash = wcsrchr( GetGlobalOptions().strInitialDir, L'\\' );
                        if( pLastSlash )
                        {
                            *pLastSlash = 0;
                            pLastSlash++;
                            WCHAR strResults[MAX_PATH];
                            wcscpy_s( strResults, MAX_PATH, pLastSlash );
                            WCHAR* pLastDot = wcsrchr( strResults, L'.' );
                            if( pLastDot )
                                *pLastDot = 0;

                            WCHAR strFolder[MAX_PATH] = {0};
                            WCHAR strPath[MAX_PATH] = {0};
                            SHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strFolder );
                            wcscat_s( strFolder, MAX_PATH, g_szAppFolder );

                            wcscpy_s( strPath, MAX_PATH, strFolder );
                            wcscat_s( strPath, MAX_PATH, strResults );
                            wcscat_s( strPath, MAX_PATH, L"_prtresults.prt" );
                            wcscpy_s( GetGlobalOptions().strResultsFile, MAX_PATH, strPath );
                            SetDlgItemText( hDlg, IDC_OUTPUT_EDIT, GetGlobalOptions().strResultsFile );

                            wcscpy_s( strResults, MAX_PATH, pLastSlash );
                            pLastDot = wcsrchr( strResults, L'.' );
                            if( pLastDot )
                                *pLastDot = 0;
                            wcscat_s( strResults, MAX_PATH, L"_adaptive.x" );
                            wcscpy_s( strPath, MAX_PATH, strFolder );
                            wcscat_s( strPath, MAX_PATH, strResults );

                            wcscpy_s( GetGlobalOptions().strOutputMesh, MAX_PATH, strPath );
                            SetDlgItemText( hDlg, IDC_OUTPUT_MESH_EDIT, GetGlobalOptions().strOutputMesh );

                        }
                        SetCurrentDirectory( GetGlobalOptions().strInitialDir );
                        SetDlgItemText( hDlg, IDC_MESH_NAME, GetGlobalOptions().strInputMesh );
                        SendMessage( GetDlgItem( hDlg, IDC_MESH_NAME ), EM_SETSEL,
                                     wcslen( GetGlobalOptions().strInputMesh ),
                                     wcslen( GetGlobalOptions().strInputMesh ) );
                        SendMessage( GetDlgItem( hDlg, IDC_MESH_NAME ), EM_SCROLLCARET, 0, 0 );
                    }
                    break;
                }

                case IDC_OUTPUT_BROWSE_BUTTON:
                {
                    // Display the OpenFileName dialog
                    BOOL bResult;
                    WCHAR strResults[MAX_PATH];
                    GetDlgItemText( hDlg, IDC_OUTPUT_EDIT, strResults, MAX_PATH );

                    OPENFILENAME ofn =
                    {
                        sizeof( OPENFILENAME ), hDlg, NULL,
                        L"Uncompressed PRT buffer files (.prt)\0*.prt\0All Files\0*.*\0\0",
                        NULL, 0, 1, strResults, MAX_PATH, NULL, 0,
                        GetGlobalOptions().strInitialDir, L"Save Results File",
                        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOREADONLYRETURN,
                        0, 1, L".prt", 0, NULL, NULL
                    };
                    bResult = GetSaveFileName( &ofn );

                    if( bResult )
                    {
                        wcscpy_s( GetGlobalOptions().strInitialDir, MAX_PATH, strResults );
                        WCHAR* pLastSlash = wcsrchr( GetGlobalOptions().strInitialDir, L'\\' );
                        if( pLastSlash )
                            *pLastSlash = 0;
                        SetCurrentDirectory( GetGlobalOptions().strInitialDir );
                        SetDlgItemText( hDlg, IDC_OUTPUT_EDIT, strResults );
                        SendMessage( GetDlgItem( hDlg, IDC_OUTPUT_EDIT ), EM_SETSEL, wcslen( strResults ),
                                     wcslen( strResults ) );
                        SendMessage( GetDlgItem( hDlg, IDC_OUTPUT_EDIT ), EM_SCROLLCARET, 0, 0 );
                    }
                    break;
                }

                case IDC_OUTPUT_MESH_BROWSE_BUTTON:
                {
                    // Display the OpenFileName dialog
                    OPENFILENAME ofn =
                    {
                        sizeof( OPENFILENAME ), hDlg, NULL,
                        L"X Files (.x)\0*.x\0All Files\0*.*\0\0",
                        NULL, 0, 1, GetGlobalOptions().strOutputMesh, MAX_PATH, NULL, 0,
                        GetGlobalOptions().strInitialDir, L"Save Output Mesh",
                        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOREADONLYRETURN,
                        0, 1, L".x", 0, NULL, NULL
                    };
                    if( TRUE == GetSaveFileName( &ofn ) )
                    {
                        wcscpy_s( GetGlobalOptions().strInitialDir, MAX_PATH, GetGlobalOptions().strResultsFile );
                        WCHAR* pLastSlash = wcsrchr( GetGlobalOptions().strInitialDir, L'\\' );
                        if( pLastSlash )
                            *pLastSlash = 0;
                        SetCurrentDirectory( GetGlobalOptions().strInitialDir );
                        SetDlgItemText( hDlg, IDC_OUTPUT_MESH_EDIT, GetGlobalOptions().strOutputMesh );
                        SendMessage( GetDlgItem( hDlg, IDC_OUTPUT_MESH_EDIT ), EM_SETSEL,
                                     wcslen( GetGlobalOptions().strOutputMesh ),
                                     wcslen( GetGlobalOptions().strOutputMesh ) );
                        SendMessage( GetDlgItem( hDlg, IDC_OUTPUT_MESH_EDIT ), EM_SCROLLCARET, 0, 0 );
                    }
                    break;
                }

                case IDC_TOOLTIPS:
                {
                    m_bShowTooltips = ( IsDlgButtonChecked( hDlg, IDC_TOOLTIPS ) == BST_CHECKED );
                    break;
                }

                case IDC_ADAPTIVE_SETTINGS:
                {
                    CPRTAdaptiveOptionsDlg dlg;
                    dlg.Show( hDlg );
                    break;
                }

                case IDOK:
                {
                    GetDlgItemText( hDlg, IDC_MESH_NAME, GetGlobalOptions().strInputMesh, MAX_PATH );
                    GetDlgItemText( hDlg, IDC_OUTPUT_MESH_EDIT, GetGlobalOptions().strOutputMesh, MAX_PATH );

                    /*
                      WCHAR szMesh[MAX_PATH];
                      HRESULT hr = DXUTFindDXSDKMediaFileCch( szMesh, MAX_PATH, m_Options.strInputMesh );
                      if( FAILED(hr) )
                      {
                      MessageBox( hDlg, TEXT("Couldn't find the mesh file.  Be sure the mesh file exists."), TEXT("Error"), MB_OK );
                      break;
                      }
                     */
                    GetGlobalOptions().dwOrder = ( DWORD )SendMessage( GetDlgItem( hDlg, IDC_ORDER_SLIDER ),
                                                                       TBM_GETPOS, 0, 0 );
                    GetGlobalOptions().dwNumBounces = ( DWORD )LOWORD( SendMessage( GetDlgItem( hDlg,
                                                                                                IDC_NUM_BOUNCES_SPIN ),
                                                                                    UDM_GETPOS, 0, 0 ) );
                    GetGlobalOptions().dwNumRays = ( DWORD )LOWORD( SendMessage( GetDlgItem( hDlg, IDC_NUM_RAYS_SPIN ),
                                                                                 UDM_GETPOS, 0, 0 ) );
                    GetGlobalOptions().bSubsurfaceScattering = ( IsDlgButtonChecked( hDlg, IDC_SUBSURF_CHECK ) ==
                                                                 BST_CHECKED );
                    GetGlobalOptions().bAdaptive = ( IsDlgButtonChecked( hDlg, IDC_ADAPTIVE_CHECK ) == BST_CHECKED );
                    GetGlobalOptions().dwNumChannels = ( IsDlgButtonChecked( hDlg,
                                                                             IDC_SPECTRAL_CHECK ) == BST_CHECKED ) ?
                        3 : 1;
                    GetGlobalOptions().dwPredefinedMatIndex = ( DWORD )SendMessage( GetDlgItem( hDlg,
                                                                                                IDC_PREDEF_COMBO ),
                                                                                    CB_GETCURSEL, 0, 0 );
                    GetGlobalOptions().bBinaryOutputXFile = ( IsDlgButtonChecked( hDlg,
                                                                                  IDC_MESH_SAVE_BINARY_RADIO ) ==
                                                              BST_CHECKED );

                    if( GetGlobalOptions().bAdaptive )
                    {
                        if( !GetGlobalOptions().bRobustMeshRefine &&
                            !GetGlobalOptions().bAdaptiveDL &&
                            !GetGlobalOptions().bAdaptiveBounce )
                        {
                            GetGlobalOptions().bAdaptive = false;
                        }
                    }

                    TCHAR sz[256];
#ifdef _CRT_INSECURE_DEPRECATE
                    GetDlgItemText( hDlg, IDC_REFRACTION_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().fRelativeIndexOfRefraction ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_ABSORPTION_R_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().Absorption.r ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_ABSORPTION_G_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().Absorption.g ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_ABSORPTION_B_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().Absorption.b ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_REFLECTANCE_R_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().Diffuse.r ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_REFLECTANCE_G_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().Diffuse.g ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_REFLECTANCE_B_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().Diffuse.b ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_SCATTERING_R_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().ReducedScattering.r ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_SCATTERING_G_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().ReducedScattering.g ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_SCATTERING_B_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().ReducedScattering.b ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_LENGTH_SCALE_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().fLengthScale ) == 0 ) return false;
#else
                    GetDlgItemText( hDlg, IDC_REFRACTION_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().fRelativeIndexOfRefraction ) == 0 ) return
                            false;
                    GetDlgItemText( hDlg, IDC_ABSORPTION_R_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().Absorption.r ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_ABSORPTION_G_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().Absorption.g ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_ABSORPTION_B_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().Absorption.b ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_REFLECTANCE_R_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().Diffuse.r ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_REFLECTANCE_G_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().Diffuse.g ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_REFLECTANCE_B_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().Diffuse.b ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_SCATTERING_R_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().ReducedScattering.r ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_SCATTERING_G_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().ReducedScattering.g ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_SCATTERING_B_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().ReducedScattering.b ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_LENGTH_SCALE_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().fLengthScale ) == 0 ) return false;
#endif
                    GetDlgItemText( hDlg, IDC_OUTPUT_EDIT, GetGlobalOptions().strResultsFile, MAX_PATH );

                    g_OptionsFile.SaveOptions();

                    EndDialog( hDlg, IDOK );
                    break;
                }

                case IDCANCEL:
                    EndDialog( hDlg, IDCANCEL );
                    break;
            }
            break;

        case WM_CLOSE:
            break;
    }

    return FALSE;
}


//-----------------------------------------------------------------------------
// Name: EnumChildProc
// Desc: Helper function to add tooltips to all the children (buttons/etc) 
//       of the dialog box
//-----------------------------------------------------------------------------
BOOL CALLBACK CPRTOptionsDlg::EnumChildProc( HWND hwndChild, LPARAM lParam )
{
    TOOLINFO ti;
    ti.cbSize = sizeof( TOOLINFO );
    ti.uFlags = TTF_IDISHWND;
    ti.hwnd = g_pDlg->m_hDlg;
    ti.uId = ( UINT_PTR )hwndChild;
    ti.hinst = 0;
    ti.lpszText = LPSTR_TEXTCALLBACK;
    SendMessage( g_pDlg->m_hToolTip, TTM_ADDTOOL, 0, ( LPARAM )( LPTOOLINFO )&ti );

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: GetMsgProc
// Desc: msg proc hook needed to display tooltips in a dialog box
//-----------------------------------------------------------------------------
LRESULT CALLBACK CPRTOptionsDlg::GetMsgProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    MSG* pMSG = ( MSG* )lParam;
    if( nCode < 0 || !( IsChild( g_pDlg->m_hDlg, pMSG->hwnd ) ) )
        return CallNextHookEx( g_pDlg->m_hMsgProcHook, nCode, wParam, lParam );

    switch( pMSG->message )
    {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            {
                if( g_pDlg->m_hToolTip )
                {
                    MSG newMsg;
                    newMsg.lParam = pMSG->lParam;
                    newMsg.wParam = pMSG->wParam;
                    newMsg.message = pMSG->message;
                    newMsg.hwnd = pMSG->hwnd;
                    SendMessage( g_pDlg->m_hToolTip, TTM_RELAYEVENT, 0, ( LPARAM )&newMsg );
                }
                break;
            }
    }

    return CallNextHookEx( g_pDlg->m_hMsgProcHook, nCode, wParam, lParam );
}


//-----------------------------------------------------------------------------
// Returns the tooltip text for the control 
//-----------------------------------------------------------------------------
void CPRTOptionsDlg::GetToolTipText( int nDlgId, NMTTDISPINFO* pNMTDI )
{
    TCHAR* str = NULL;
    switch( nDlgId )
    {
        case IDC_INPUT_MESH_TEXT:
        case IDC_MESH_NAME:
            str =
                TEXT(
                "This is the file that is loaded as a ID3DXMesh* and passed into the ID3DXPRTEngine* so that it can compute and return spherical harmonic transfer coefficients for each vertex in the mesh. It returns these coefficients in a ID3DXBuffer*. This process takes some time and should be precomputed, however the results can be used in real time. For more detailed information, see \"Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments\" by Peter-Pike Sloan, Jan Kautz, and John Snyder, SIGGRAPH 2002." ); break;

        case IDC_ORDER_TEXT:
        case IDC_ORDER_SLIDER:
            str =
                TEXT(
                "This controls the number of spherical harmonic basis functions used. The simulator generates order^2 coefficients per channel. Higher order allows for higher frequency lighting environments which allow for sharper shadows with the tradeoff of more coefficients per vertex that need to be processed by the vertex shader. For convex objects (no shadows), 3rd order has very little approximation error.  For more detailed information, see \"Spherical Harmonic Lighting: The Gritty Details\" by Robin Green, GDC 2003 and \"An Efficient Representation of Irradiance Environment Maps\" by Ravi Ramamoorthi, and Pat Hanrahan, SIGGRAPH 2001." ); break;

        case IDC_NUM_BOUNCES_TEXT:
        case IDC_NUM_BOUNCES_EDIT:
        case IDC_NUM_BOUNCES_SPIN:
            str =
                TEXT(
                "This controls the number of bounces simulated. If this is non-zero then inter-reflections are calculated. Inter-reflections are, for example, when a light shines on a red wall and bounces on a white wall. The white wall even though it contains no red in the material will reflect some red do to the bouncing of the light off the red wall." ); break;

        case IDC_NUM_RAYS_TEXT:
        case IDC_NUM_RAYS_EDIT:
        case IDC_NUM_RAYS_SPIN:
            str =
                TEXT(
                "This controls the number of rays to shoot at each sample. The more rays the more accurate the final result will be, but it will increase time it takes to precompute the transfer coefficients." ); break;

        case IDC_SUBSURF_CHECK:
            str =
                TEXT(
                "If checked then subsurface scattering will be done in the simulator. Subsurface scattering is when light penetrates a translucent surface and comes out the other side. For example, a jade sculpture or a flashlight shining through skin exhibits subsurface scattering. The simulator assumes the mesh is made of a homogenous material. If subsurface scattering is not used, then the length scale, the relative index of refraction, the reduced scattering coefficients, and the absorption coefficients are not used." ); break;

        case IDC_SPECTRAL_CHECK:
            str =
                TEXT(
                "If checked then the simulator will process 3 channels: red, green, and blue and return order^2 spherical harmonic transfer coefficients for each of these channels in a single ID3DXBuffer* buffer. Otherwise it use values of only one channel (the red channel) and return the transfer coefficients for just that single channel. A single channel is useful for lighting environments that don't need to have the whole spectrum of light such as shadows" ); break;

        case IDC_RED_TEXT:
            str =
                TEXT(
                "The values below are the red coefficients. If spectral is turned off, then this is the channel that will be used." ); break;
        case IDC_GREEN_TEXT:
            str = TEXT( "The values below are the green coefficients" ); break;
        case IDC_BLUE_TEXT:
            str = TEXT( "The values below are the blue coefficients" ); break;

        case IDC_PREDEF_COMBO:
        case IDC_PREDEF_TEXT:
            str =
                TEXT(
                "These are some example materials. Choosing one of these materials with change the all the material values below. The parameters for these materials are from \"A Practical Model for Subsurface Light Transport\" by Henrik Wann Jensen, Steve R. Marschner, Marc Levoy, Pat Hanrahan, SIGGRAPH 2001. The relative index of refraction is with respect the material immersed in air." ); break;

        case IDC_REFRACTION_TEXT:
        case IDC_REFRACTION_EDIT:
            str =
                TEXT(
                "Relative index of refraction is the ratio between two absolute indexes of refraction. An index of refraction is ratio of the sine of the angle of incidence to the sine of the angle of refraction." ); break;

        case IDC_LENGTH_SCALE_TEXT:
        case IDC_LENGTH_SCALE_EDIT:
            str =
                TEXT(
                "When subsurface scattering is used the object is mapped to a cube of length scale mm per side. For example, if length scale is 10, then the object is mapped to a 10mm x 10mm x 10mm cube.  The smaller the cube the more light penetrates the object." ); break;

        case IDC_SCATTERING_TEXT:
        case IDC_SCATTERING_G_EDIT:
        case IDC_SCATTERING_B_EDIT:
        case IDC_SCATTERING_R_EDIT:
            str =
                TEXT(
                "The reduced scattering coefficient is a parameter to the volume rendering equation used to model light propagation in a participating medium. For more detail, see \"A Practical Model for Subsurface Light Transport\" by Henrik Wann Jensen, Steve R. Marschner, Marc Levoy, Pat Hanrahan, SIGGRAPH 2001" ); break;

        case IDC_ABSORPTION_TEXT:
        case IDC_ABSORPTION_G_EDIT:
        case IDC_ABSORPTION_B_EDIT:
        case IDC_ABSORPTION_R_EDIT:
            str =
                TEXT(
                "The absorption coefficient is a parameter to the volume rendering equation used to model light propagation in a participating medium. For more detail, see \"A Practical Model for Subsurface Light Transport\" by Henrik Wann Jensen, Steve R. Marschner, Marc Levoy, Pat Hanrahan, SIGGRAPH 2001" ); break;

        case IDC_REFLECTANCE_TEXT:
        case IDC_REFLECTANCE_R_EDIT:
        case IDC_REFLECTANCE_B_EDIT:
        case IDC_REFLECTANCE_G_EDIT:
            str =
                TEXT(
                "The diffuse reflectance coefficient is the fraction of diffuse light reflected back. This value is typically between 0 and 1." ); break;

        case IDC_OUTPUT_TEXT:
        case IDC_OUTPUT_EDIT:
            str =
                TEXT(
                "This sample will save a binary buffer of spherical harmonic transfer coefficients to a file which the sample can read in later.  This is the file name of the where the resulting binary buffer will be saved" ); break;

        case IDC_OUTPUT_MESH_TEXT:
        case IDC_OUTPUT_MESH_EDIT:
            str =
                TEXT(
                "If adaptive tessellation is on, then this sample will save the resulting tessellated mesh to this file." ); break;

        case IDC_OUTPUT_BROWSE_BUTTON:
            str = TEXT( "Select the output buffer file name" ); break;
        case IDC_OUTPUT_MESH_BROWSE_BUTTON:
            str = TEXT( "Select the output mesh file name" ); break;

        case IDOK:
            str =
                TEXT(
                "This will start the simulator based on the options selected above. This process takes some time and should be precomputed, however the results can be used in real time. When the simulator is done calculating the spherical harmonic transfer coefficients for each vertex, the sample will use D3DXSHPRTCompress() to compress the data based on the number of PCA vectors chosen. This sample will then save the binary data of coefficients to a file. This sample will also allow you to view the results by passing these coefficients to a vertex shader for real time rendering (if the Direct3D device has hardware accelerated programmable vertex shader support). This sample also stores the settings of this dialog to the registry so it remembers them for next time." ); break;

        case IDCANCEL:
            str = TEXT( "This cancels the dialog, does not save the settings, and does not run the simulator." );
            break;
    }

    pNMTDI->lpszText = str;
}


//-----------------------------------------------------------------------------
void CPRTOptionsDlg::ResetSettings()
{
    g_OptionsFile.ResetSettings();
}


//-----------------------------------------------------------------------------
void COptionsFile::ResetSettings()
{
    ZeroMemory( &m_Options, sizeof( SIMULATOR_OPTIONS ) );

    wcscpy_s( m_Options.strInputMesh, MAX_PATH, L"PRT Demo\\LandShark.x" );
    DXUTFindDXSDKMediaFileCch( m_Options.strInitialDir, MAX_PATH, m_Options.strInputMesh );
    WCHAR* pLastSlash = wcsrchr( m_Options.strInitialDir, L'\\' );
    if( pLastSlash )
        *pLastSlash = 0;
    wcscpy_s( m_Options.strResultsFile, MAX_PATH, L"LandShark_prtresults.prt" );
    m_Options.dwOrder = 6;
    m_Options.dwNumRays = 256;
    m_Options.dwNumBounces = 1;
    m_Options.bSubsurfaceScattering = false;
    m_Options.fLengthScale = 25.0f;
    m_Options.dwNumChannels = 3;

    m_Options.dwPredefinedMatIndex = 0;
    m_Options.fRelativeIndexOfRefraction = g_aPredefinedMaterials[0].fRelativeIndexOfRefraction;
    m_Options.Diffuse = g_aPredefinedMaterials[0].Diffuse;
    m_Options.Absorption = g_aPredefinedMaterials[0].Absorption;
    m_Options.ReducedScattering = g_aPredefinedMaterials[0].ReducedScattering;

    m_Options.bAdaptive = false;
    m_Options.bRobustMeshRefine = true;
    m_Options.fRobustMeshRefineMinEdgeLength = 0.0f;
    m_Options.dwRobustMeshRefineMaxSubdiv = 2;
    m_Options.bAdaptiveDL = true;
    m_Options.fAdaptiveDLMinEdgeLength = 0.03f;
    m_Options.fAdaptiveDLThreshold = 8e-5f;
    m_Options.dwAdaptiveDLMaxSubdiv = 3;
    m_Options.bAdaptiveBounce = false;
    m_Options.fAdaptiveBounceMinEdgeLength = 0.03f;
    m_Options.fAdaptiveBounceThreshold = 8e-5f;
    m_Options.dwAdaptiveBounceMaxSubdiv = 3;
    wcscpy_s( m_Options.strOutputMesh, MAX_PATH, L"LandShark_adaptive.x" );
    m_Options.bBinaryOutputXFile = true;

    //    m_Options.Quality = D3DXSHCQUAL_FASTLOWQUALITY;
    m_Options.Quality = D3DXSHCQUAL_SLOWHIGHQUALITY;
    m_Options.dwNumPCA = 24;
    m_Options.dwNumClusters = 1;

}


//-----------------------------------------------------------------------------
void CPRTOptionsDlg::UpdateControlsWithSettings( HWND hDlg )
{
    SetCurrentDirectory( GetGlobalOptions().strInitialDir );

    SetDlgItemText( hDlg, IDC_MESH_NAME, GetGlobalOptions().strInputMesh );
    SetDlgItemText( hDlg, IDC_OUTPUT_MESH_EDIT, GetGlobalOptions().strOutputMesh );
    SendMessage( GetDlgItem( hDlg, IDC_MESH_NAME ), EM_SETSEL, wcslen( GetGlobalOptions().strInputMesh ),
                 wcslen( GetGlobalOptions().strInputMesh ) );
    SendMessage( GetDlgItem( hDlg, IDC_MESH_NAME ), EM_SCROLLCARET, 0, 0 );

    HWND hOrderSlider = GetDlgItem( hDlg, IDC_ORDER_SLIDER );
    SendMessage( hOrderSlider, TBM_SETRANGE, 0, MAKELONG( 2, 6 ) );
    SendMessage( hOrderSlider, TBM_SETTICFREQ, 1, 0 );
    SendMessage( hOrderSlider, TBM_SETPOS, 1, GetGlobalOptions().dwOrder );

    HWND hNumBouncesSpin = GetDlgItem( hDlg, IDC_NUM_BOUNCES_SPIN );
    SendMessage( hNumBouncesSpin, UDM_SETRANGE, 0, ( LPARAM )MAKELONG( 10, 1 ) );
    SendMessage( hNumBouncesSpin, UDM_SETPOS, 0, ( LPARAM )MAKELONG( GetGlobalOptions().dwNumBounces, 0 ) );

    HWND hNumRaysSpin = GetDlgItem( hDlg, IDC_NUM_RAYS_SPIN );
    UDACCEL udAccel[3];
    udAccel[0].nSec = 0; udAccel[0].nInc = 1;
    udAccel[1].nSec = 1; udAccel[1].nInc = 100;
    udAccel[2].nSec = 2; udAccel[2].nInc = 1000;
    SendMessage( hNumRaysSpin, UDM_SETACCEL, 3, ( LPARAM )udAccel );
    SendMessage( hNumRaysSpin, UDM_SETRANGE, 0, ( LPARAM )MAKELONG( 30000, 8 ) );
    SendMessage( hNumRaysSpin, UDM_SETPOS, 0, ( LPARAM )MAKELONG( GetGlobalOptions().dwNumRays, 0 ) );

    CheckDlgButton( hDlg, IDC_SUBSURF_CHECK, GetGlobalOptions().bSubsurfaceScattering ? BST_CHECKED : BST_UNCHECKED );
    CheckDlgButton( hDlg, IDC_ADAPTIVE_CHECK, GetGlobalOptions().bAdaptive ? BST_CHECKED : BST_UNCHECKED );
    CheckDlgButton( hDlg, IDC_SPECTRAL_CHECK,
                    ( GetGlobalOptions().dwNumChannels == 3 ) ? BST_CHECKED : BST_UNCHECKED );
    SendMessage( hDlg, WM_COMMAND, IDC_SUBSURF_CHECK, 0 );

    HWND hPreDefCombo = GetDlgItem( hDlg, IDC_PREDEF_COMBO );
    SendMessage( hPreDefCombo, CB_RESETCONTENT, 0, 0 );
    DWORD i;
    for( i = 0; i < ( DWORD )g_aPredefinedMaterialsSize; i++ )
    {
        int nIndex = ( int )SendMessage( hPreDefCombo, CB_ADDSTRING, 0, ( LPARAM )g_aPredefinedMaterials[i].strName );
        if( nIndex >= 0 )
            SendMessage( hPreDefCombo, CB_SETITEMDATA, nIndex, ( LPARAM )&g_aPredefinedMaterials[i] );
    }
    SendMessage( hPreDefCombo, CB_SETCURSEL, GetGlobalOptions().dwPredefinedMatIndex, 0 );

    TCHAR sz[256];
    m_bComboBoxSelChange = true;
    swprintf_s( sz, 256, TEXT( "%0.2f" ), GetGlobalOptions().fRelativeIndexOfRefraction ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_REFRACTION_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.4f" ), GetGlobalOptions().Absorption.r ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_ABSORPTION_R_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.4f" ), GetGlobalOptions().Absorption.g ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_ABSORPTION_G_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.4f" ), GetGlobalOptions().Absorption.b ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_ABSORPTION_B_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.2f" ), GetGlobalOptions().Diffuse.r ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_REFLECTANCE_R_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.2f" ), GetGlobalOptions().Diffuse.g ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_REFLECTANCE_G_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.2f" ), GetGlobalOptions().Diffuse.b ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_REFLECTANCE_B_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.2f" ), GetGlobalOptions().ReducedScattering.r ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_SCATTERING_R_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.2f" ), GetGlobalOptions().ReducedScattering.g ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_SCATTERING_G_EDIT, sz );
    swprintf_s( sz, 256, TEXT( "%0.2f" ), GetGlobalOptions().ReducedScattering.b ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_SCATTERING_B_EDIT, sz );
    m_bComboBoxSelChange = false;

    swprintf_s( sz, 256, TEXT( "%0.1f" ), GetGlobalOptions().fLengthScale ); sz[255] = 0;
    SetDlgItemText( hDlg, IDC_LENGTH_SCALE_EDIT, sz );

    SetDlgItemText( hDlg, IDC_OUTPUT_EDIT, GetGlobalOptions().strResultsFile );
    SendMessage( GetDlgItem( hDlg, IDC_OUTPUT_EDIT ), EM_SETSEL, wcslen( GetGlobalOptions().strResultsFile ),
                 wcslen( GetGlobalOptions().strResultsFile ) );
    SendMessage( GetDlgItem( hDlg, IDC_OUTPUT_EDIT ), EM_SCROLLCARET, 0, 0 );

    CheckRadioButton( hDlg, IDC_MESH_SAVE_BINARY_RADIO, IDC_MESH_SAVE_TEXT_RADIO,
                      GetGlobalOptions().bBinaryOutputXFile ? IDC_MESH_SAVE_BINARY_RADIO : IDC_MESH_SAVE_TEXT_RADIO );

    CheckDlgButton( hDlg, IDC_TOOLTIPS, m_bShowTooltips ? BST_CHECKED : BST_UNCHECKED );
}


//--------------------------------------------------------------------------------------
CPRTLoadDlg::CPRTLoadDlg()
{
    g_pLoadDlg = this;
}


//--------------------------------------------------------------------------------------
CPRTLoadDlg::~CPRTLoadDlg()
{
    g_pLoadDlg = NULL;
}


//--------------------------------------------------------------------------------------
bool CPRTLoadDlg::Show()
{
    // Ask the user about param settings for the PRT Simulation
    int nResult = ( int )DialogBox( NULL, MAKEINTRESOURCE( IDD_LOAD_PRTBUFFER ),
                                    DXUTGetHWND(), StaticDlgProc );

    UnhookWindowsHookEx( m_hMsgProcHook );
    DestroyWindow( m_hToolTip );

    if( nResult == IDOK )
        return true;
    else
        return false;
}


//--------------------------------------------------------------------------------------
INT_PTR CALLBACK CPRTLoadDlg::StaticDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    return g_pLoadDlg->DlgProc( hDlg, msg, wParam, lParam );
}


//--------------------------------------------------------------------------------------
INT_PTR CALLBACK CPRTLoadDlg::DlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_INITDIALOG:
        {
            m_hDlg = hDlg;

            WCHAR* strMesh;
            if( !GetGlobalOptions().bAdaptive )
                strMesh = GetGlobalOptions().strInputMesh;
            else
                strMesh = GetGlobalOptions().strOutputMesh;
            SetDlgItemText( hDlg, IDC_INPUT_MESH_EDIT, strMesh );
            SendMessage( GetDlgItem( hDlg, IDC_INPUT_MESH_EDIT ), EM_SETSEL, wcslen( strMesh ), wcslen( strMesh ) );
            SendMessage( GetDlgItem( hDlg, IDC_INPUT_MESH_EDIT ), EM_SCROLLCARET, 0, 0 );

            SetDlgItemText( hDlg, IDC_SIM_RESULTS_EDIT, GetGlobalOptions().strResultsFile );
            SendMessage( GetDlgItem( hDlg, IDC_SIM_RESULTS_EDIT ), EM_SETSEL,
                         wcslen( GetGlobalOptions().strResultsFile ), wcslen( GetGlobalOptions().strResultsFile ) );
            SendMessage( GetDlgItem( hDlg, IDC_SIM_RESULTS_EDIT ), EM_SCROLLCARET, 0, 0 );

            SetCurrentDirectory( GetGlobalOptions().strInitialDir );

            m_hToolTip = CreateWindowEx( 0, TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP,
                                         CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                         hDlg, NULL, ( HINSTANCE )GetModuleHandle( NULL ), NULL );
            SendMessage( m_hToolTip, TTM_SETMAXTIPWIDTH, 0, 300 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_AUTOPOP, 32000 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_INITIAL, 0 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_RESHOW, 0 );
            EnumChildWindows( hDlg, ( WNDENUMPROC )EnumChildProc, 0 );
            m_hMsgProcHook = SetWindowsHookEx( WH_GETMESSAGE, GetMsgProc, ( HINSTANCE )GetModuleHandle( NULL ),
                                               GetCurrentThreadId() );

            return TRUE;
        }

        case WM_NOTIFY:
        {
            NMHDR* pNMHDR = ( LPNMHDR )lParam;
            if( pNMHDR == NULL )
                break;

            if( pNMHDR->code == TTN_NEEDTEXT )
            {
                NMTTDISPINFO* pNMTDI = ( LPNMTTDISPINFO )lParam;
                int nDlgId = GetDlgCtrlID( ( HWND )pNMHDR->idFrom );
                GetToolTipText( nDlgId, pNMTDI );
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDC_INPUT_MESH_BROWSE_BUTTON:
                {
                    WCHAR* strMesh;
                    if( !GetGlobalOptions().bAdaptive )
                        strMesh = GetGlobalOptions().strInputMesh;
                    else
                        strMesh = GetGlobalOptions().strOutputMesh;

                    // Display the OpenFileName dialog
                    OPENFILENAME ofn =
                    {
                        sizeof( OPENFILENAME ), hDlg, NULL,
                        L".X Files (.x)\0*.x\0All Files\0*.*\0\0",
                        NULL, 0, 1, strMesh, MAX_PATH, NULL, 0,
                        GetGlobalOptions().strInitialDir, L"Open Mesh File",
                        OFN_FILEMUSTEXIST, 0, 1, NULL, 0, NULL, NULL
                    };
                    if( TRUE == GetOpenFileName( &ofn ) )
                    {
                        wcscpy_s( GetGlobalOptions().strInitialDir, MAX_PATH, strMesh );
                        WCHAR* pLastSlash = wcsrchr( GetGlobalOptions().strInitialDir, L'\\' );
                        if( pLastSlash )
                        {
                            *pLastSlash = 0;
                            pLastSlash++;
                            WCHAR strResults[MAX_PATH];
                            wcscpy_s( strResults, MAX_PATH, pLastSlash );
                            WCHAR* pLastDot = wcsrchr( strResults, L'.' );
                            if( pLastDot )
                                *pLastDot = 0;
                            pLastDot = wcsrchr( strResults, L'_' );
                            if( pLastDot )
                                *pLastDot = 0;
                            wcscat_s( strResults, MAX_PATH, L"_prtresults.prt" );
                            wcscpy_s( GetGlobalOptions().strResultsFile, MAX_PATH, strResults );
                            SetDlgItemText( hDlg, IDC_SIM_RESULTS_EDIT, GetGlobalOptions().strResultsFile );
                        }
                        SetCurrentDirectory( GetGlobalOptions().strInitialDir );
                        SetDlgItemText( hDlg, IDC_INPUT_MESH_EDIT, strMesh );
                        SendMessage( GetDlgItem( hDlg, IDC_INPUT_MESH_EDIT ), EM_SETSEL, wcslen( strMesh ),
                                     wcslen( strMesh ) );
                        SendMessage( GetDlgItem( hDlg, IDC_INPUT_MESH_EDIT ), EM_SCROLLCARET, 0, 0 );
                    }
                    break;
                }

                case IDC_SIM_RESULTS_BROWSE_BUTTON:
                {
                    // Display the OpenFileName dialog
                    BOOL bResult;
                    OPENFILENAME ofn =
                    {
                        sizeof( OPENFILENAME ), hDlg, NULL,
                        L"Uncompressed PRT buffer files (.prt)\0*.prt\0All Files\0*.*\0\0",
                        NULL, 0, 1, GetGlobalOptions().strResultsFile, MAX_PATH, NULL, 0,
                        GetGlobalOptions().strInitialDir, L"Load Results File",
                        OFN_FILEMUSTEXIST, 0, 1, L"", 0, NULL, NULL
                    };
                    bResult = GetOpenFileName( &ofn );
                    if( bResult )
                    {
                        wcscpy_s( GetGlobalOptions().strInitialDir, MAX_PATH, GetGlobalOptions().strResultsFile );
                        WCHAR* pLastSlash = wcsrchr( GetGlobalOptions().strInitialDir, L'\\' );
                        if( pLastSlash )
                            *pLastSlash = 0;
                        SetCurrentDirectory( GetGlobalOptions().strInitialDir );
                        SetDlgItemText( hDlg, IDC_SIM_RESULTS_EDIT, GetGlobalOptions().strResultsFile );
                        SendMessage( GetDlgItem( hDlg, IDC_SIM_RESULTS_EDIT ), EM_SETSEL,
                                     wcslen( GetGlobalOptions().strResultsFile ),
                                     wcslen( GetGlobalOptions().strResultsFile ) );
                        SendMessage( GetDlgItem( hDlg, IDC_SIM_RESULTS_EDIT ), EM_SCROLLCARET, 0, 0 );
                    }
                    break;
                }

                case IDOK:
                {
                    GetDlgItemText( hDlg, IDC_INPUT_MESH_EDIT, GetGlobalOptions().strInputMesh, MAX_PATH );
                    GetDlgItemText( hDlg, IDC_SIM_RESULTS_EDIT, GetGlobalOptions().strResultsFile, MAX_PATH );

                    WCHAR strResults[MAX_PATH];
                    WCHAR strMesh[MAX_PATH];
                    HRESULT hr;
                    SetCurrentDirectory( GetGlobalOptions().strInitialDir );
                    if( FAILED( hr = DXUTFindDXSDKMediaFileCch( strResults, MAX_PATH,
                                                                GetGlobalOptions().strResultsFile ) ) )
                    {
                        MessageBox( hDlg,
                                    TEXT(
                                    "Couldn't find the simulator results file.  Run the simulator first to precompute the transfer vectors for the mesh." ), TEXT( "Error" ), MB_OK );
                        break;
                    }

                    if( FAILED( hr = DXUTFindDXSDKMediaFileCch( strMesh, MAX_PATH,
                                                                GetGlobalOptions().strInputMesh ) ) )
                    {
                        MessageBox( hDlg, TEXT( "Couldn't find the mesh file.  Be sure the mesh file exists." ),
                                    TEXT( "Error" ), MB_OK );
                        break;
                    }

                    g_OptionsFile.SaveOptions();
                    EndDialog( hDlg, IDOK );
                    break;
                }

                case IDCANCEL:
                {
                    EndDialog( hDlg, IDCANCEL );
                    break;
                }
            }
            break;

        case WM_CLOSE:
            break;
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------
void CPRTLoadDlg::GetToolTipText( int nDlgId, NMTTDISPINFO* pNMTDI )
{
}


//--------------------------------------------------------------------------------------
LRESULT CALLBACK CPRTLoadDlg::GetMsgProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    return 0;
}


//--------------------------------------------------------------------------------------
BOOL CALLBACK CPRTLoadDlg::EnumChildProc( HWND hwnd, LPARAM lParam )
{
    return false;
}

//--------------------------------------------------------------------------------------
CPRTAdaptiveOptionsDlg::CPRTAdaptiveOptionsDlg()
{
    g_pAdaptiveDlg = this;
}


//--------------------------------------------------------------------------------------
CPRTAdaptiveOptionsDlg::~CPRTAdaptiveOptionsDlg()
{
    g_pAdaptiveDlg = NULL;
}


//--------------------------------------------------------------------------------------
bool CPRTAdaptiveOptionsDlg::Show( HWND hDlg )
{
    // Ask the user about param settings for the PRT Simulation
    int nResult = ( int )DialogBox( NULL, MAKEINTRESOURCE( IDD_ADAPTIVE_OPTIONS ),
                                    hDlg, StaticDlgProc );

    UnhookWindowsHookEx( m_hMsgProcHook );
    DestroyWindow( m_hToolTip );

    if( nResult == IDOK )
        return true;
    else
        return false;
}


//--------------------------------------------------------------------------------------
INT_PTR CALLBACK CPRTAdaptiveOptionsDlg::StaticDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    return g_pAdaptiveDlg->DlgProc( hDlg, msg, wParam, lParam );
}


//--------------------------------------------------------------------------------------
INT_PTR CALLBACK CPRTAdaptiveOptionsDlg::DlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_INITDIALOG:
        {
            m_hDlg = hDlg;

            SIMULATOR_OPTIONS& options = GetGlobalOptions();

            CheckDlgButton( hDlg, IDC_ENABLE_ROBUST_MESH_REFINE,
                            options.bRobustMeshRefine ? BST_CHECKED : BST_UNCHECKED );
            CheckDlgButton( hDlg, IDC_ENABLE_ADAPTIVE_DIRECT_LIGHTING,
                            options.bAdaptiveDL ? BST_CHECKED : BST_UNCHECKED );
            CheckDlgButton( hDlg, IDC_ENABLE_ADAPTIVE_BOUNCE, options.bAdaptiveBounce ? BST_CHECKED : BST_UNCHECKED );

            UpdateUI( hDlg );

            HWND hSpin;
            hSpin = GetDlgItem( hDlg, IDC_RMR_MAX_SUBD_SPIN );
            SendMessage( hSpin, UDM_SETRANGE, 0, ( LPARAM )MAKELONG( 10, 1 ) );
            SendMessage( hSpin, UDM_SETPOS, 0, ( LPARAM )MAKELONG( GetGlobalOptions().dwRobustMeshRefineMaxSubdiv,
                                                                   0 ) );

            hSpin = GetDlgItem( hDlg, IDC_DL_MAX_SUBD_SPIN );
            SendMessage( hSpin, UDM_SETRANGE, 0, ( LPARAM )MAKELONG( 10, 1 ) );
            SendMessage( hSpin, UDM_SETPOS, 0, ( LPARAM )MAKELONG( GetGlobalOptions().dwAdaptiveDLMaxSubdiv, 0 ) );

            hSpin = GetDlgItem( hDlg, IDC_AB_MAX_SUBD_SPIN );
            SendMessage( hSpin, UDM_SETRANGE, 0, ( LPARAM )MAKELONG( 10, 1 ) );
            SendMessage( hSpin, UDM_SETPOS, 0, ( LPARAM )MAKELONG( GetGlobalOptions().dwAdaptiveBounceMaxSubdiv, 0 ) );

            WCHAR sz[256];
            swprintf_s( sz, 256, L"%0.6f", GetGlobalOptions().fRobustMeshRefineMinEdgeLength ); sz[255] = 0;
            SetDlgItemText( hDlg, IDC_RMR_MIN_EDGE_EDIT, sz );
            swprintf_s( sz, 256, L"%0.6f", GetGlobalOptions().fAdaptiveDLMinEdgeLength ); sz[255] = 0;
            SetDlgItemText( hDlg, IDC_DL_MIN_EDGE_EDIT, sz );
            swprintf_s( sz, 256, L"%0.6f", GetGlobalOptions().fAdaptiveBounceMinEdgeLength ); sz[255] = 0;
            SetDlgItemText( hDlg, IDC_AB_MIN_EDGE_EDIT, sz );
            swprintf_s( sz, 256, L"%0.6f", GetGlobalOptions().fAdaptiveDLThreshold ); sz[255] = 0;
            SetDlgItemText( hDlg, IDC_DL_SUBD_THRESHOLD_EDIT, sz );
            swprintf_s( sz, 256, L"%0.6f", GetGlobalOptions().fAdaptiveBounceThreshold ); sz[255] = 0;
            SetDlgItemText( hDlg, IDC_AB_SUBD_THRESHOLD_EDIT, sz );

            m_hToolTip = CreateWindowEx( 0, TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP,
                                         CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                         hDlg, NULL, ( HINSTANCE )GetModuleHandle( NULL ), NULL );
            SendMessage( m_hToolTip, TTM_SETMAXTIPWIDTH, 0, 300 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_AUTOPOP, 32000 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_INITIAL, 0 );
            SendMessage( m_hToolTip, TTM_SETDELAYTIME, ( WPARAM )( DWORD )TTDT_RESHOW, 0 );
            EnumChildWindows( hDlg, ( WNDENUMPROC )EnumChildProc, 0 );
            m_hMsgProcHook = SetWindowsHookEx( WH_GETMESSAGE, GetMsgProc, ( HINSTANCE )GetModuleHandle( NULL ),
                                               GetCurrentThreadId() );

            return TRUE;
        }

        case WM_NOTIFY:
        {
            NMHDR* pNMHDR = ( LPNMHDR )lParam;
            if( pNMHDR == NULL )
                break;

            if( pNMHDR->code == TTN_NEEDTEXT )
            {
                NMTTDISPINFO* pNMTDI = ( LPNMTTDISPINFO )lParam;
                int nDlgId = GetDlgCtrlID( ( HWND )pNMHDR->idFrom );
                GetToolTipText( nDlgId, pNMTDI );
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDC_ENABLE_ROBUST_MESH_REFINE:
                case IDC_ENABLE_ADAPTIVE_DIRECT_LIGHTING:
                case IDC_ENABLE_ADAPTIVE_BOUNCE:
                    {
                        GetGlobalOptions().bRobustMeshRefine = ( IsDlgButtonChecked( hDlg,
                                                                                     IDC_ENABLE_ROBUST_MESH_REFINE ) ==
                                                                 BST_CHECKED );
                        GetGlobalOptions().bAdaptiveDL = ( IsDlgButtonChecked( hDlg,
                                                                               IDC_ENABLE_ADAPTIVE_DIRECT_LIGHTING ) ==
                                                           BST_CHECKED );
                        GetGlobalOptions().bAdaptiveBounce = ( IsDlgButtonChecked( hDlg,
                                                                                   IDC_ENABLE_ADAPTIVE_BOUNCE ) ==
                                                               BST_CHECKED );
                        UpdateUI( hDlg );
                        break;
                    }

                case IDOK:
                {
                    GetGlobalOptions().bRobustMeshRefine = ( IsDlgButtonChecked( hDlg,
                                                                                 IDC_ENABLE_ROBUST_MESH_REFINE ) ==
                                                             BST_CHECKED );
                    GetGlobalOptions().bAdaptiveDL = ( IsDlgButtonChecked( hDlg,
                                                                           IDC_ENABLE_ADAPTIVE_DIRECT_LIGHTING ) ==
                                                       BST_CHECKED );
                    GetGlobalOptions().bAdaptiveBounce = ( IsDlgButtonChecked( hDlg,
                                                                               IDC_ENABLE_ADAPTIVE_BOUNCE ) ==
                                                           BST_CHECKED );

                    GetGlobalOptions().dwRobustMeshRefineMaxSubdiv = ( DWORD )LOWORD( SendMessage( GetDlgItem( hDlg,
                                                                                                               IDC_RMR_MAX_SUBD_SPIN
                                                                                                               
                                                                                                               ),
                                                                                                   UDM_GETPOS, 0,
                                                                                                   0 ) );
                    GetGlobalOptions().dwAdaptiveDLMaxSubdiv = ( DWORD )LOWORD( SendMessage( GetDlgItem( hDlg,
                                                                                                         IDC_DL_MAX_SUBD_SPIN
                                                                                                         
                                                                                                         ),
                                                                                             UDM_GETPOS, 0, 0 ) );
                    GetGlobalOptions().dwAdaptiveBounceMaxSubdiv = ( DWORD )LOWORD( SendMessage( GetDlgItem( hDlg,
                                                                                                             IDC_AB_MAX_SUBD_SPIN
                                                                                                             
                                                                                                             ),
                                                                                                 UDM_GETPOS, 0, 0 ) );

                    WCHAR sz[256];
#ifdef _CRT_INSECURE_DEPRECATE
                    GetDlgItemText( hDlg, IDC_RMR_MIN_EDGE_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().fRobustMeshRefineMinEdgeLength ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_DL_MIN_EDGE_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().fAdaptiveDLMinEdgeLength ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_AB_MIN_EDGE_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().fAdaptiveBounceMinEdgeLength ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_DL_SUBD_THRESHOLD_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().fAdaptiveDLThreshold ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_AB_SUBD_THRESHOLD_EDIT, sz, 256 ); if( swscanf_s( sz, TEXT("%f"), &GetGlobalOptions().fAdaptiveBounceThreshold ) == 0 ) return false;
#else
                    GetDlgItemText( hDlg, IDC_RMR_MIN_EDGE_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().fRobustMeshRefineMinEdgeLength ) == 0 ) return
                            false;
                    GetDlgItemText( hDlg, IDC_DL_MIN_EDGE_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().fAdaptiveDLMinEdgeLength ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_AB_MIN_EDGE_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().fAdaptiveBounceMinEdgeLength ) == 0 ) return
                            false;
                    GetDlgItemText( hDlg, IDC_DL_SUBD_THRESHOLD_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().fAdaptiveDLThreshold ) == 0 ) return false;
                    GetDlgItemText( hDlg, IDC_AB_SUBD_THRESHOLD_EDIT, sz, 256 );
                    if( swscanf( sz, TEXT( "%f" ), &GetGlobalOptions().fAdaptiveBounceThreshold ) == 0 ) return false;
#endif
                    EndDialog( hDlg, IDOK );
                    break;
                }

                case IDCANCEL:
                {
                    EndDialog( hDlg, IDCANCEL );
                    break;
                }
            }
            break;

        case WM_CLOSE:
            break;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
void CPRTAdaptiveOptionsDlg::UpdateUI( HWND hDlg )
{
    SIMULATOR_OPTIONS& options = GetGlobalOptions();
    EnableWindow( GetDlgItem( hDlg, IDC_RMR_MIN_EDGE_EDIT ), options.bRobustMeshRefine );
    EnableWindow( GetDlgItem( hDlg, IDC_RMR_MIN_EDGE_TEXT ), options.bRobustMeshRefine );
    EnableWindow( GetDlgItem( hDlg, IDC_RMR_MAX_SUBD_EDIT ), options.bRobustMeshRefine );
    EnableWindow( GetDlgItem( hDlg, IDC_RMR_MAX_SUBD_SPIN ), options.bRobustMeshRefine );
    EnableWindow( GetDlgItem( hDlg, IDC_RMR_MAX_SUBD_TEXT ), options.bRobustMeshRefine );

    EnableWindow( GetDlgItem( hDlg, IDC_DL_MAX_SUBD_EDIT ), options.bAdaptiveDL );
    EnableWindow( GetDlgItem( hDlg, IDC_DL_MAX_SUBD_SPIN ), options.bAdaptiveDL );
    EnableWindow( GetDlgItem( hDlg, IDC_DL_MAX_SUBD_TEXT ), options.bAdaptiveDL );
    EnableWindow( GetDlgItem( hDlg, IDC_DL_MIN_EDGE_EDIT ), options.bAdaptiveDL );
    EnableWindow( GetDlgItem( hDlg, IDC_DL_MIN_EDGE_TEXT ), options.bAdaptiveDL );
    EnableWindow( GetDlgItem( hDlg, IDC_DL_SUBD_THRESHOLD_EDIT ), options.bAdaptiveDL );
    EnableWindow( GetDlgItem( hDlg, IDC_DL_SUBD_THRESHOLD_TEXT ), options.bAdaptiveDL );

    EnableWindow( GetDlgItem( hDlg, IDC_AB_MAX_SUBD_EDIT ), options.bAdaptiveBounce );
    EnableWindow( GetDlgItem( hDlg, IDC_AB_MAX_SUBD_SPIN ), options.bAdaptiveBounce );
    EnableWindow( GetDlgItem( hDlg, IDC_AB_MAX_SUBD_TEXT ), options.bAdaptiveBounce );
    EnableWindow( GetDlgItem( hDlg, IDC_AB_MIN_EDGE_EDIT ), options.bAdaptiveBounce );
    EnableWindow( GetDlgItem( hDlg, IDC_AB_MIN_EDGE_TEXT ), options.bAdaptiveBounce );
    EnableWindow( GetDlgItem( hDlg, IDC_AB_SUBD_THRESHOLD_EDIT ), options.bAdaptiveBounce );
    EnableWindow( GetDlgItem( hDlg, IDC_AB_SUBD_THRESHOLD_TEXT ), options.bAdaptiveBounce );
}


//--------------------------------------------------------------------------------------
void CPRTAdaptiveOptionsDlg::GetToolTipText( int nDlgId, NMTTDISPINFO* pNMTDI )
{
}


//--------------------------------------------------------------------------------------
LRESULT CALLBACK CPRTAdaptiveOptionsDlg::GetMsgProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    return 0;
}


//--------------------------------------------------------------------------------------
BOOL CALLBACK CPRTAdaptiveOptionsDlg::EnumChildProc( HWND hwnd, LPARAM lParam )
{
    return false;
}


//--------------------------------------------------------------------------------------
void CXMLHelper::CreateChildNode( IXMLDOMDocument* pDoc, IXMLDOMNode* pParentNode,
                                  WCHAR* strName, int nType, IXMLDOMNode** ppNewNode )
{
    IXMLDOMNode* pNewNode;
    VARIANT vtype;
    vtype.vt = VT_I4;
    V_I4 (&vtype ) = ( int )nType;
    BSTR bstrName = SysAllocString( strName );
    pDoc->createNode( vtype, bstrName, NULL, &pNewNode );
    SysFreeString( bstrName );
    pParentNode->appendChild( pNewNode, ppNewNode );
    SAFE_RELEASE( pNewNode );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::CreateNewValue( IXMLDOMDocument* pDoc, IXMLDOMNode* pNode, WCHAR* strName, WCHAR* strValue )
{
    IXMLDOMNode* pNewNode = NULL;
    IXMLDOMNode* pNewTextNode = NULL;
    CreateChildNode( pDoc, pNode, strName, NODE_ELEMENT, &pNewNode );
    CreateChildNode( pDoc, pNewNode, strName, NODE_TEXT, &pNewTextNode );
    VARIANT var;
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString( strValue );
    pNewTextNode->put_nodeTypedValue( var );
    VariantClear( &var );
    SAFE_RELEASE( pNewTextNode );
    SAFE_RELEASE( pNewNode );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::CreateNewValue( IXMLDOMDocument* pDoc, IXMLDOMNode* pNode, WCHAR* strName, DWORD nValue )
{
    WCHAR strValue[MAX_PATH];
    swprintf_s( strValue, MAX_PATH, L"%d", nValue );
    CreateNewValue( pDoc, pNode, strName, strValue );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::CreateNewValue( IXMLDOMDocument* pDoc, IXMLDOMNode* pNode, WCHAR* strName, float fValue )
{
    WCHAR strValue[MAX_PATH];
    swprintf_s( strValue, MAX_PATH, L"%f", fValue );
    CreateNewValue( pDoc, pNode, strName, strValue );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, WCHAR* strValue, int cchValue )
{
    if( !pNode )
        return;

    BSTR nodeName;
    pNode->get_nodeName( &nodeName );
    if( wcscmp( nodeName, strName ) == 0 )
    {
        VARIANT v;
        IXMLDOMNode* pChild = NULL;
        pNode->get_firstChild( &pChild );
        if( pChild )
        {
            HRESULT hr = pChild->get_nodeTypedValue( &v );
            if( SUCCEEDED( hr ) && v.vt == VT_BSTR )
                wcscpy_s( strValue, cchValue, v.bstrVal );
            VariantClear( &v );
            SAFE_RELEASE( pChild );
        }
    }
    SysFreeString( nodeName );

    IXMLDOMNode* pNextNode = NULL;
    if( SUCCEEDED( pNode->get_nextSibling( &pNextNode ) ) )
    {
        SAFE_RELEASE( pNode );
        pNode = pNextNode;
    }
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, int* pnValue )
{
    if( NULL == pNode )
        return;
    WCHAR strValue[256];
    GetValue( pNode, strName, strValue, 256 );
    *pnValue = _wtoi( strValue );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, float* pfValue )
{
    WCHAR strValue[256];
    GetValue( pNode, strName, strValue, 256 );
#ifdef _CRT_INSECURE_DEPRECATE
    if( swscanf_s( strValue, L"%f", pfValue ) == 0 )
        *pfValue = 0;
#else
    if( swscanf( strValue, L"%f", pfValue ) == 0 )
        *pfValue = 0;
#endif
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, bool* pbValue )
{
    int n = 0;
    GetValue( pNode, strName, &n );
    *pbValue = ( n == 1 );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, D3DXCOLOR* pclrValue )
{
    int n = 0;
    GetValue( pNode, strName, &n );
    *pclrValue = ( D3DXCOLOR )n;
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, DWORD* pdwValue )
{
    WCHAR strValue[256];
    CXMLHelper::GetValue( pNode, strName, strValue, 256 );
#ifdef _CRT_INSECURE_DEPRECATE
    if( swscanf_s( strValue, L"%d", pdwValue ) == 0 )
        *pdwValue = 0;
#else
    if( swscanf( strValue, L"%d", pdwValue ) == 0 )
        *pdwValue = 0;
#endif
}



SIMULATOR_OPTIONS* CPRTOptionsDlg::GetOptions()
{
    return &g_OptionsFile.m_Options;
}

SIMULATOR_OPTIONS* CPRTLoadDlg::GetOptions()
{
    return &g_OptionsFile.m_Options;
}



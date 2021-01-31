//----------------------------------------------------------------------------
// File: Config.cpp
//
// Desc: 
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include <msxml.h>
#include <oleauto.h>
#include <stdio.h>
#include <conio.h>
#include "config.h"


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
const int g_aPredefinedMaterialsSize = sizeof( g_aPredefinedMaterials ) / sizeof( g_aPredefinedMaterials[0] );


//--------------------------------------------------------------------------------------
COptionsFile::COptionsFile()
{
}


//--------------------------------------------------------------------------------------
COptionsFile::~COptionsFile()
{
}


//--------------------------------------------------------------------------------------
HRESULT COptionsFile::SaveOptions( WCHAR* strFile, SIMULATOR_OPTIONS* pOptions )
{
    HRESULT hr = S_OK;
    IXMLDOMDocument* pDoc = NULL;

    CoInitialize( NULL );

    // Create an empty XML document
    V_RETURN( CoCreateInstance( CLSID_DOMDocument, NULL,
                                CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument,
                                ( void** )&pDoc ) );

    IXMLDOMNode* pRootNode = NULL, *pTopNode = NULL;
    IXMLDOMNode* pMTNode = NULL, *pGlobalsNode = NULL, *pMeshNode = NULL;
    IXMLDOMNode* pCompressionNode = NULL, *pMeshArrayNode = NULL, *pOutputNode = NULL, *pSHMaterialNode = NULL;
    pDoc->QueryInterface( IID_IXMLDOMNode, ( VOID** )&pRootNode );
    CXMLHelper::CreateChildNode( pDoc, pRootNode, L"Options", NODE_ELEMENT, &pTopNode );

    CXMLHelper::CreateChildNode( pDoc, pTopNode, L"Input", NODE_ELEMENT, &pMeshArrayNode );
    for( DWORD iMesh = 0; iMesh < pOptions->dwNumMeshes; iMesh++ )
    {
        INPUT_MESH* pInputMesh = &pOptions->pInputMeshes[iMesh];

        CXMLHelper::CreateChildNode( pDoc, pMeshArrayNode, L"Mesh", NODE_ELEMENT, &pMeshNode );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"MeshFile", pInputMesh->strMeshFile );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"IsBlockerMesh", ( DWORD )pInputMesh->bIsBlockerMesh );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Translate.x", pInputMesh->vTranslate.x );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Translate.y", pInputMesh->vTranslate.y );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Translate.z", pInputMesh->vTranslate.z );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Scale.x", pInputMesh->vScale.x );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Scale.y", pInputMesh->vScale.y );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Scale.z", pInputMesh->vScale.z );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Yaw", pInputMesh->fYaw );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Pitch", pInputMesh->fPitch );
        CXMLHelper::CreateNewValue( pDoc, pMeshNode, L"Roll", pInputMesh->fRoll );

        for( DWORD iSH = 0; iSH < pInputMesh->dwNumSHMaterials; iSH++ )
        {
            CXMLHelper::CreateChildNode( pDoc, pMeshNode, L"SHMaterial", NODE_ELEMENT, &pSHMaterialNode );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"Diffuse.r", pInputMesh->pSHMaterials[iSH].Diffuse.r );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"Diffuse.g", pInputMesh->pSHMaterials[iSH].Diffuse.g );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"Diffuse.b", pInputMesh->pSHMaterials[iSH].Diffuse.b );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"Absorption.r",
                                        pInputMesh->pSHMaterials[iSH].Absorption.r );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"Absorption.g",
                                        pInputMesh->pSHMaterials[iSH].Absorption.g );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"Absorption.b",
                                        pInputMesh->pSHMaterials[iSH].Absorption.b );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"EnableSubsurfaceScattering",
                                        ( DWORD )pInputMesh->pSHMaterials[iSH].bSubSurf );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"RelativeIndexOfRefraction",
                                        pInputMesh->pSHMaterials[iSH].RelativeIndexOfRefraction );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"ReducedScattering.r",
                                        pInputMesh->pSHMaterials[iSH].ReducedScattering.r );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"ReducedScattering.g",
                                        pInputMesh->pSHMaterials[iSH].ReducedScattering.g );
            CXMLHelper::CreateNewValue( pDoc, pSHMaterialNode, L"ReducedScattering.b",
                                        pInputMesh->pSHMaterials[iSH].ReducedScattering.b );
            SAFE_RELEASE( pSHMaterialNode );
        }

        SAFE_RELEASE( pMeshNode );
    }

    CXMLHelper::CreateChildNode( pDoc, pTopNode, L"Settings", NODE_ELEMENT, &pGlobalsNode );
    CXMLHelper::CreateNewValue( pDoc, pGlobalsNode, L"Order", pOptions->dwOrder );
    CXMLHelper::CreateNewValue( pDoc, pGlobalsNode, L"NumRays", pOptions->dwNumRays );
    CXMLHelper::CreateNewValue( pDoc, pGlobalsNode, L"NumBounces", pOptions->dwNumBounces );
    CXMLHelper::CreateNewValue( pDoc, pGlobalsNode, L"LengthScale", pOptions->fLengthScale );
    CXMLHelper::CreateNewValue( pDoc, pGlobalsNode, L"NumChannels", ( DWORD )pOptions->dwNumChannels );

    CXMLHelper::CreateChildNode( pDoc, pGlobalsNode, L"Compression", NODE_ELEMENT, &pCompressionNode );
    CXMLHelper::CreateNewValue( pDoc, pCompressionNode, L"EnableCompression", ( DWORD )pOptions->bEnableCompression );
    CXMLHelper::CreateNewValue( pDoc, pCompressionNode, L"NumClusters", pOptions->dwNumClusters );
    CXMLHelper::CreateNewValue( pDoc, pCompressionNode, L"Quality", ( DWORD )pOptions->Quality );
    CXMLHelper::CreateNewValue( pDoc, pCompressionNode, L"NumPCA", pOptions->dwNumPCA );
    SAFE_RELEASE( pCompressionNode );

    CXMLHelper::CreateChildNode( pDoc, pGlobalsNode, L"MeshTessellation", NODE_ELEMENT, &pMTNode );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"EnableTessellation", ( DWORD )pOptions->bEnableTessellation );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"RobustMeshRefine", ( DWORD )pOptions->bRobustMeshRefine );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"RobustMeshRefineMinEdgeLength",
                                pOptions->fRobustMeshRefineMinEdgeLength );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"RobustMeshRefineMaxSubdiv", pOptions->dwRobustMeshRefineMaxSubdiv );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveDL", ( DWORD )pOptions->bAdaptiveDL );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveDLMinEdgeLength", pOptions->fAdaptiveDLMinEdgeLength );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveDLThreshold", pOptions->fAdaptiveDLThreshold );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveDLMaxSubdiv", pOptions->dwAdaptiveDLMaxSubdiv );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveBounce", ( DWORD )pOptions->bAdaptiveBounce );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveBounceMinEdgeLength",
                                pOptions->fAdaptiveBounceMinEdgeLength );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveBounceThreshold", pOptions->fAdaptiveBounceThreshold );
    CXMLHelper::CreateNewValue( pDoc, pMTNode, L"AdaptiveBounceMaxSubdiv", pOptions->dwAdaptiveBounceMaxSubdiv );
    SAFE_RELEASE( pMTNode );
    SAFE_RELEASE( pGlobalsNode );

    CXMLHelper::CreateChildNode( pDoc, pTopNode, L"Output", NODE_ELEMENT, &pOutputNode );
    CXMLHelper::CreateNewValue( pDoc, pOutputNode, L"OutputConcatPRTMesh", pOptions->strOutputConcatPRTMesh );
    CXMLHelper::CreateNewValue( pDoc, pOutputNode, L"OutputConcatBlockerMesh", pOptions->strOutputConcatBlockerMesh );
    CXMLHelper::CreateNewValue( pDoc, pOutputNode, L"OutputTessellatedMesh", pOptions->strOutputTessellatedMesh );
    CXMLHelper::CreateNewValue( pDoc, pOutputNode, L"BinaryXFile", ( DWORD )pOptions->bBinaryXFile );
    CXMLHelper::CreateNewValue( pDoc, pOutputNode, L"OutputPRTBuffer", pOptions->strOutputPRTBuffer );
    CXMLHelper::CreateNewValue( pDoc, pOutputNode, L"OutputCompPRTBuffer", pOptions->strOutputCompPRTBuffer );
    SAFE_RELEASE( pOutputNode );

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
HRESULT COptionsFile::LoadOptions( WCHAR* strFile, SIMULATOR_OPTIONS* pOptions )
{
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

    V( pDoc->QueryInterface( IID_IXMLDOMNode, ( void** )&pRootNode ) );

    CXMLHelper::SkipCommentNodes( pRootNode );
    V( pRootNode->get_firstChild( &pTopNode ) );
    SAFE_RELEASE( pRootNode );
    CXMLHelper::SkipCommentNodes( pTopNode );
    V( pTopNode->get_firstChild( &pNode ) );
    SAFE_RELEASE( pTopNode );

    ZeroMemory( pOptions, sizeof( SIMULATOR_OPTIONS ) );

    long nNumMeshes = 1;
    IXMLDOMNodeList* pMeshList = NULL;
    V( pDoc->getElementsByTagName( L"Mesh", &pMeshList ) );
    V( pMeshList->get_length( &nNumMeshes ) );
    pOptions->dwNumMeshes = nNumMeshes;
    SAFE_RELEASE( pDoc );

    CXMLHelper::SkipCommentNodes( pNode );
    if( FAILED( CXMLHelper::GetChild( pNode, L"Input" ) ) )
        return E_FAIL;

    pOptions->pInputMeshes = new INPUT_MESH[pOptions->dwNumMeshes];
    if( pOptions->pInputMeshes == NULL )
    {
        SAFE_RELEASE( pNode );
        return E_OUTOFMEMORY;
    }

    ZeroMemory( pOptions->pInputMeshes, sizeof( INPUT_MESH ) );
    for( int iMesh = 0; iMesh < nNumMeshes; iMesh++ )
    {
        INPUT_MESH* pInputMesh = &pOptions->pInputMeshes[iMesh];

        pInputMesh->dwNumSHMaterials = CXMLHelper::GetNumberOfChildren( pNode, L"SHMaterial" );
        pInputMesh->pSHMaterials = new D3DXSHMATERIAL[pInputMesh->dwNumSHMaterials];
        if( pInputMesh->pSHMaterials == NULL )
        {
            SAFE_RELEASE( pNode );
            return E_OUTOFMEMORY;
        }

        CXMLHelper::GetChild( pNode, L"Mesh" );
        CXMLHelper::GetValue( pNode, L"MeshFile", pInputMesh->strMeshFile, MAX_PATH );
        CXMLHelper::GetValue( pNode, L"IsBlockerMesh", &pInputMesh->bIsBlockerMesh );
        CXMLHelper::GetValue( pNode, L"Translate.x", &pInputMesh->vTranslate.x );
        CXMLHelper::GetValue( pNode, L"Translate.y", &pInputMesh->vTranslate.y );
        CXMLHelper::GetValue( pNode, L"Translate.z", &pInputMesh->vTranslate.z );
        CXMLHelper::GetValue( pNode, L"Scale.x", &pInputMesh->vScale.x );
        CXMLHelper::GetValue( pNode, L"Scale.y", &pInputMesh->vScale.y );
        CXMLHelper::GetValue( pNode, L"Scale.z", &pInputMesh->vScale.z );
        CXMLHelper::GetValue( pNode, L"Yaw", &pInputMesh->fYaw );
        CXMLHelper::GetValue( pNode, L"Pitch", &pInputMesh->fPitch );
        CXMLHelper::GetValue( pNode, L"Roll", &pInputMesh->fRoll );

        for( DWORD iSH = 0; iSH < pInputMesh->dwNumSHMaterials; iSH++ )
        {
            CXMLHelper::GetChild( pNode, L"SHMaterial" );
            CXMLHelper::GetValue( pNode, L"Diffuse.r", &pInputMesh->pSHMaterials[iSH].Diffuse.r );
            CXMLHelper::GetValue( pNode, L"Diffuse.g", &pInputMesh->pSHMaterials[iSH].Diffuse.g );
            CXMLHelper::GetValue( pNode, L"Diffuse.b", &pInputMesh->pSHMaterials[iSH].Diffuse.b );
            CXMLHelper::GetValue( pNode, L"Absorption.r", &pInputMesh->pSHMaterials[iSH].Absorption.r );
            CXMLHelper::GetValue( pNode, L"Absorption.g", &pInputMesh->pSHMaterials[iSH].Absorption.g );
            CXMLHelper::GetValue( pNode, L"Absorption.b", &pInputMesh->pSHMaterials[iSH].Absorption.b );
            CXMLHelper::GetValue( pNode, L"EnableSubsurfaceScattering", &pInputMesh->pSHMaterials[iSH].bSubSurf );
            CXMLHelper::GetValue( pNode,
                                  L"RelativeIndexOfRefraction",
                                  &pInputMesh->pSHMaterials[iSH].RelativeIndexOfRefraction );
            CXMLHelper::GetValue( pNode, L"ReducedScattering.r", &pInputMesh->pSHMaterials[iSH].ReducedScattering.r );
            CXMLHelper::GetValue( pNode, L"ReducedScattering.g", &pInputMesh->pSHMaterials[iSH].ReducedScattering.g );
            CXMLHelper::GetValue( pNode, L"ReducedScattering.b", &pInputMesh->pSHMaterials[iSH].ReducedScattering.b );
            pInputMesh->pSHMaterials[iSH].bMirror = false;
            CXMLHelper::GetParentSibling( pNode );
        }

        CXMLHelper::GetParentSibling( pNode );
    }
    CXMLHelper::GetParentSibling( pNode );

    CXMLHelper::GetChild( pNode, L"Settings" );
    CXMLHelper::GetValue( pNode, L"Order", &pOptions->dwOrder );
    CXMLHelper::GetValue( pNode, L"NumRays", &pOptions->dwNumRays );
    CXMLHelper::GetValue( pNode, L"NumBounces", &pOptions->dwNumBounces );
    CXMLHelper::GetValue( pNode, L"LengthScale", &pOptions->fLengthScale );
    CXMLHelper::GetValue( pNode, L"NumChannels", &pOptions->dwNumChannels );

    CXMLHelper::GetChild( pNode, L"Compression" );
    CXMLHelper::GetValue( pNode, L"EnableCompression", &pOptions->bEnableCompression );
    CXMLHelper::GetValue( pNode, L"NumClusters", &pOptions->dwNumClusters );
    DWORD dwQuality;
    CXMLHelper::GetValue( pNode, L"Quality", &dwQuality );
    pOptions->Quality = static_cast<D3DXSHCOMPRESSQUALITYTYPE>( dwQuality );
    CXMLHelper::GetValue( pNode, L"NumPCA", &pOptions->dwNumPCA );
    CXMLHelper::GetParentSibling( pNode );

    CXMLHelper::GetChild( pNode, L"MeshTessellation" );
    CXMLHelper::GetValue( pNode, L"EnableTessellation", &pOptions->bEnableTessellation );
    CXMLHelper::GetValue( pNode, L"RobustMeshRefine", &pOptions->bRobustMeshRefine );
    CXMLHelper::GetValue( pNode, L"RobustMeshRefineMinEdgeLength", &pOptions->fRobustMeshRefineMinEdgeLength );
    CXMLHelper::GetValue( pNode, L"RobustMeshRefineMaxSubdiv", &pOptions->dwRobustMeshRefineMaxSubdiv );
    CXMLHelper::GetValue( pNode, L"AdaptiveDL", &pOptions->bAdaptiveDL );
    CXMLHelper::GetValue( pNode, L"AdaptiveDLMinEdgeLength", &pOptions->fAdaptiveDLMinEdgeLength );
    CXMLHelper::GetValue( pNode, L"AdaptiveDLThreshold", &pOptions->fAdaptiveDLThreshold );
    CXMLHelper::GetValue( pNode, L"AdaptiveDLMaxSubdiv", &pOptions->dwAdaptiveDLMaxSubdiv );
    CXMLHelper::GetValue( pNode, L"AdaptiveBounce", &pOptions->bAdaptiveBounce );
    CXMLHelper::GetValue( pNode, L"AdaptiveBounceMinEdgeLength", &pOptions->fAdaptiveBounceMinEdgeLength );
    CXMLHelper::GetValue( pNode, L"AdaptiveBounceThreshold", &pOptions->fAdaptiveBounceThreshold );
    CXMLHelper::GetValue( pNode, L"AdaptiveBounceMaxSubdiv", &pOptions->dwAdaptiveBounceMaxSubdiv );
    CXMLHelper::GetParent( pNode );
    CXMLHelper::GetParentSibling( pNode );

    if( SUCCEEDED( CXMLHelper::GetChild( pNode, L"Output" ) ) )
    {
        CXMLHelper::GetValue( pNode, L"OutputConcatPRTMesh", pOptions->strOutputConcatPRTMesh, MAX_PATH );
        CXMLHelper::GetValue( pNode, L"OutputConcatBlockerMesh", pOptions->strOutputConcatBlockerMesh, MAX_PATH );
        CXMLHelper::GetValue( pNode, L"OutputTessellatedMesh", pOptions->strOutputTessellatedMesh, MAX_PATH );
        CXMLHelper::GetValue( pNode, L"BinaryXFile", &pOptions->bBinaryXFile );
        CXMLHelper::GetValue( pNode, L"OutputPRTBuffer", pOptions->strOutputPRTBuffer, MAX_PATH );
        CXMLHelper::GetValue( pNode, L"OutputCompPRTBuffer", pOptions->strOutputCompPRTBuffer, MAX_PATH );
        CXMLHelper::GetParentSibling( pNode );
    }
    else
    {
        WCHAR szBaseName[MAX_PATH];
        wcscpy_s( szBaseName, MAX_PATH, strFile );
        WCHAR* strLastDot = wcsrchr( szBaseName, L'.' );
        if( strLastDot )
            *strLastDot = 0;

        wcscpy_s( pOptions->strOutputConcatPRTMesh, MAX_PATH, szBaseName );
        wcscat_s( pOptions->strOutputConcatPRTMesh, MAX_PATH, L".x" );

        wcscpy_s( pOptions->strOutputConcatBlockerMesh, MAX_PATH, szBaseName );
        wcscat_s( pOptions->strOutputConcatBlockerMesh, MAX_PATH, L"_blocker.x" );

        wcscpy_s( pOptions->strOutputTessellatedMesh, MAX_PATH, szBaseName );
        wcscat_s( pOptions->strOutputTessellatedMesh, MAX_PATH, L"_tessellated.x" );

        pOptions->bBinaryXFile = false;

        wcscpy_s( pOptions->strOutputPRTBuffer, MAX_PATH, szBaseName );
        wcscat_s( pOptions->strOutputPRTBuffer, MAX_PATH, L"_prtresults.prt" );

        wcscpy_s( pOptions->strOutputCompPRTBuffer, MAX_PATH, szBaseName );
        wcscat_s( pOptions->strOutputCompPRTBuffer, MAX_PATH, L"_compprtresults.pca" );
    }

    SAFE_RELEASE( pNode );

    return S_OK;
}


//-----------------------------------------------------------------------------
HRESULT COptionsFile::ResetOptions( SIMULATOR_OPTIONS* pOptions )
{
    ZeroMemory( pOptions, sizeof( SIMULATOR_OPTIONS ) );

    pOptions->dwOrder = 6;
    pOptions->dwNumRays = 128;
    pOptions->dwNumBounces = 1;
    pOptions->fLengthScale = 25.0f;
    pOptions->dwNumChannels = 3;

    pOptions->bEnableCompression = true;
    pOptions->Quality = D3DXSHCQUAL_SLOWHIGHQUALITY;
    pOptions->dwNumClusters = 1;
    pOptions->dwNumPCA = 24;

    pOptions->bEnableTessellation = false;
    pOptions->bRobustMeshRefine = true;
    pOptions->fRobustMeshRefineMinEdgeLength = 0.0f;
    pOptions->dwRobustMeshRefineMaxSubdiv = 2;
    pOptions->bAdaptiveDL = true;
    pOptions->fAdaptiveDLMinEdgeLength = 0.03f;
    pOptions->fAdaptiveDLThreshold = 8e-5f;
    pOptions->dwAdaptiveDLMaxSubdiv = 3;
    pOptions->bAdaptiveBounce = false;
    pOptions->fAdaptiveBounceMinEdgeLength = 0.03f;
    pOptions->fAdaptiveBounceThreshold = 8e-5f;
    pOptions->dwAdaptiveBounceMaxSubdiv = 3;

    wcscpy_s( pOptions->strOutputConcatPRTMesh, MAX_PATH, L"meshConcat.x" );
    wcscpy_s( pOptions->strOutputConcatBlockerMesh, MAX_PATH, L"blockerConcat.x" );
    wcscpy_s( pOptions->strOutputTessellatedMesh, MAX_PATH, L"tesslatedMesh.x" );
    wcscpy_s( pOptions->strOutputPRTBuffer, MAX_PATH, L"prtbuffer.prt" );
    wcscpy_s( pOptions->strOutputCompPRTBuffer, MAX_PATH, L"prtCompBuffer.pca" );
    pOptions->bBinaryXFile = false;

    pOptions->dwNumMeshes = 2;
    pOptions->pInputMeshes = new INPUT_MESH[pOptions->dwNumMeshes];
    if( pOptions->pInputMeshes == NULL )
        return E_OUTOFMEMORY;

    for( DWORD iMesh = 0; iMesh < pOptions->dwNumMeshes; iMesh++ )
    {
        wcscpy_s( pOptions->pInputMeshes[iMesh].strMeshFile, MAX_PATH, L"PRT Demo\\LandShark.x" );
        pOptions->pInputMeshes[iMesh].bIsBlockerMesh = 0;
        pOptions->pInputMeshes[iMesh].vTranslate = D3DXVECTOR3( 0, 0, 0 );
        pOptions->pInputMeshes[iMesh].vScale = D3DXVECTOR3( 1, 1, 1 );
        pOptions->pInputMeshes[iMesh].fYaw = 0.0f;
        pOptions->pInputMeshes[iMesh].fPitch = 0.0f;
        pOptions->pInputMeshes[iMesh].fRoll = 0.0f;

        pOptions->pInputMeshes[iMesh].dwNumSHMaterials = 1;
        pOptions->pInputMeshes[iMesh].pSHMaterials = new
            D3DXSHMATERIAL[pOptions->pInputMeshes[iMesh].dwNumSHMaterials];
        for( DWORD iSH = 0; iSH < pOptions->pInputMeshes[iMesh].dwNumSHMaterials; iSH++ )
        {
            pOptions->pInputMeshes[iMesh].pSHMaterials[iSH].Diffuse = g_aPredefinedMaterials[0].Diffuse;
            pOptions->pInputMeshes[iMesh].pSHMaterials[iSH].Absorption = g_aPredefinedMaterials[0].Absorption;
            pOptions->pInputMeshes[iMesh].pSHMaterials[iSH].bSubSurf = false;
            pOptions->pInputMeshes[iMesh].pSHMaterials[iSH].RelativeIndexOfRefraction =
                g_aPredefinedMaterials[0].fRelativeIndexOfRefraction;
            pOptions->pInputMeshes[iMesh].pSHMaterials[iSH].ReducedScattering =
                g_aPredefinedMaterials[0].ReducedScattering;
        }
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
void COptionsFile::FreeOptions( SIMULATOR_OPTIONS* pOptions )
{
    for( DWORD iMesh = 0; iMesh < pOptions->dwNumMeshes; iMesh++ )
    {
        SAFE_DELETE_ARRAY( pOptions->pInputMeshes[iMesh].pSHMaterials );
    }
    SAFE_DELETE_ARRAY( pOptions->pInputMeshes );
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
    HRESULT hr;
    IXMLDOMNode* pNewNode = NULL;
    IXMLDOMNode* pNewTextNode = NULL;
    CreateChildNode( pDoc, pNode, strName, NODE_ELEMENT, &pNewNode );
    CreateChildNode( pDoc, pNewNode, strName, NODE_TEXT, &pNewTextNode );
    VARIANT var;
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString( strValue );
    V( pNewTextNode->put_nodeTypedValue( var ) );
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
void CXMLHelper::SkipCommentNodes( IXMLDOMNode*& pNode )
{
    IXMLDOMNode* pTemp = NULL;
    BSTR nodeName = NULL;
    while( pNode )
    {
        if( SUCCEEDED( pNode->get_nodeName( &nodeName ) ) && wcscmp( nodeName, L"#comment" ) == 0 )
        {
            if( SUCCEEDED( pNode->get_nextSibling( &pTemp ) ) && pTemp )
            {
                SAFE_RELEASE( pNode );
                pNode = pTemp;
            }
            else
            {
                if( SUCCEEDED( pNode->get_parentNode( &pTemp ) ) && pTemp )
                {
                    SAFE_RELEASE( pNode );
                    pNode = pTemp;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            break;
        }

        if( nodeName )
        {
            SysFreeString( nodeName );
            nodeName = NULL;
        }
    }

    SysFreeString( nodeName );
}

//--------------------------------------------------------------------------------------
HRESULT CXMLHelper::GetChild( IXMLDOMNode*& pNode, WCHAR* strName )
{
    if( !pNode )
        return E_FAIL;

    BSTR nodeName = NULL;
    IXMLDOMNode* pChild = NULL;

    if( SUCCEEDED( pNode->get_nodeName( &nodeName ) ) && wcscmp( nodeName, strName ) == 0 &&
        SUCCEEDED( pNode->get_firstChild( &pChild ) ) && pChild )
    {
        SAFE_RELEASE( pNode );
        pNode = pChild;
        SkipCommentNodes( pNode );
        SysFreeString( nodeName );
        return S_OK;
    }
    else
    {
        SysFreeString( nodeName );
        return E_FAIL;
    }
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetParent( IXMLDOMNode*& pNode )
{
    if( !pNode )
        return;

    IXMLDOMNode* pParent = NULL;
    if( SUCCEEDED( pNode->get_parentNode( &pParent ) ) && pParent )
    {
        SAFE_RELEASE( pNode );
        pNode = pParent;
        SkipCommentNodes( pNode );
    }
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetSibling( IXMLDOMNode*& pNode )
{
    if( !pNode )
        return;

    IXMLDOMNode* pNextNode = NULL;
    if( SUCCEEDED( pNode->get_nextSibling( &pNextNode ) ) && pNextNode )
    {
        SAFE_RELEASE( pNode );
        pNode = pNextNode;
        SkipCommentNodes( pNode );
    }
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetParentSibling( IXMLDOMNode*& pNode )
{
    GetParent( pNode );
    GetSibling( pNode );
}


//--------------------------------------------------------------------------------------
DWORD CXMLHelper::GetNumberOfChildren( IXMLDOMNode* pNode, WCHAR* strName )
{
    if( !pNode )
        return 0;

    int nMatches = 0;
    int nChildren = 0;
    IXMLDOMNode* pChild = NULL;
    pNode->get_firstChild( &pChild );
    while( pChild )
    {
        nChildren++;

        BSTR nodeName;
        pChild->get_nodeName( &nodeName );
        if( wcscmp( nodeName, strName ) == 0 )
        {
            nMatches++;
        }
        SysFreeString( nodeName );

        IXMLDOMNode* pNextNode = NULL;
        pChild->get_nextSibling( &pNextNode );
        SAFE_RELEASE( pChild );
        pChild = pNextNode;
        SkipCommentNodes( pChild );
    }

    return nMatches;
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
    else
    {
        assert( false );
    }
    SysFreeString( nodeName );

    GetSibling( pNode );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, int* pnValue )
{
    if( NULL == pNode )
        return;
    WCHAR strValue[256] = {0};
    GetValue( pNode, strName, strValue, 256 );
    *pnValue = _wtoi( strValue );
}


//--------------------------------------------------------------------------------------
void CXMLHelper::GetValue( IXMLDOMNode*& pNode, WCHAR* strName, float* pfValue )
{
    WCHAR strValue[256] = {0};
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
    WCHAR strValue[256] = {0};
    CXMLHelper::GetValue( pNode, strName, strValue, 256 );
#ifdef _CRT_INSECURE_DEPRECATE
    if( swscanf_s( strValue, L"%d", pdwValue ) == 0 )
        *pdwValue = 0;
#else
    if( swscanf( strValue, L"%d", pdwValue ) == 0 )
        *pdwValue = 0;
#endif

}


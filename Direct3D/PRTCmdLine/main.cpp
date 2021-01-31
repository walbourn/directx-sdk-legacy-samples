//----------------------------------------------------------------------------
// File: main.cpp
//
// Desc: 
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include <stdio.h>
#include <conio.h>
#include "config.h"
#include "PRTSim.h"


//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
bool ParseCommandLine( SETTINGS* pSettings );
bool GetCmdParam( WCHAR*& strsettings, WCHAR* strFlag, int nFlagLen );
IDirect3DDevice9* CreateNULLRefDevice();
HRESULT LoadMeshes( IDirect3DDevice9* pd3dDevice, SIMULATOR_OPTIONS* pOptions, CONCAT_MESH* pPRTMesh,
                    CONCAT_MESH* pBlockerMesh, SETTINGS* pSettings );
void DisplayUsage();
void NormalizeNormals( ID3DXMesh* pMesh );
bool IsNextArg( WCHAR*& strsettings, WCHAR* strArg );
bool GetNextArg( WCHAR*& strsettings, WCHAR* strArg, int cchArg );
void SearchDirForFile( IDirect3DDevice9* pd3dDevice, WCHAR* strDir, WCHAR* strFile, SETTINGS* pSettings );
void SeachSubdirsForFile( IDirect3DDevice9* pd3dDevice, WCHAR* strDir, WCHAR* strFile, SETTINGS* pSettings );
HRESULT ProcessOptionsFile( IDirect3DDevice9* pd3dDevice, WCHAR* strOptionsFileName, SETTINGS* pSettings );


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point for the application.  We use just the console window
//-----------------------------------------------------------------------------
int wmain( int argc, wchar_t* argv[] )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    int nRet = 0;
    IDirect3DDevice9* pd3dDevice = NULL;
    SETTINGS settings;
    settings.bUserAbort = false;
    settings.bSubDirs = false;
    settings.bVerbose = false;

    if( argc < 2 )
    {
        DisplayUsage();
        return 1;
    }

    if( !ParseCommandLine( &settings ) )
    {
        nRet = 0;
        goto LCleanup;
    }

    if( settings.aFiles.GetSize() == 0 )
    {
        WCHAR* strNewArg = new WCHAR[256];
        if( strNewArg )
        {
            wcscpy_s( strNewArg, 256, L"options.xml" );
            settings.aFiles.Add( strNewArg );
        }
    }

    // Create NULLREF device 
    pd3dDevice = CreateNULLRefDevice();
    if( pd3dDevice == NULL )
    {
        wprintf( L"Error: Can not create NULLREF Direct3D device\n" );
        nRet = 1;
        goto LCleanup;
    }

    for( int i = 0; i < settings.aFiles.GetSize(); i++ )
    {
        WCHAR* strOptionsFileName = settings.aFiles[i];
        if( settings.bVerbose )
            wprintf( L"Processing command line arg filename '%s'\n", strOptionsFileName );

        // For this cmd line arg, extract the full dir & filename
        WCHAR strDir[MAX_PATH] = {0};
        WCHAR strFile[MAX_PATH] = {0};
        WCHAR* pFilePart;
        DWORD dwWrote = GetFullPathName( strOptionsFileName, MAX_PATH, strDir, &pFilePart );
        if( dwWrote > 1 && pFilePart )
        {
            wcscpy_s( strFile, MAX_PATH, pFilePart );
            *pFilePart = NULL;
        }
        if( settings.bVerbose )
        {
            wprintf( L"Base dir: %s\n", strDir );
            wprintf( L"Base file: %s\n", strFile );
        }

        if( settings.bSubDirs )
            SeachSubdirsForFile( pd3dDevice, strDir, strFile, &settings );
        else
            SearchDirForFile( pd3dDevice, strDir, strFile, &settings );

        if( settings.bUserAbort )
            break;
    }

LCleanup:
    wprintf( L"\n" );

    // Cleanup
    for( int i = 0; i < settings.aFiles.GetSize(); i++ )
        SAFE_DELETE_ARRAY( settings.aFiles[i] );
    SAFE_RELEASE( pd3dDevice );

    return nRet;
}


//--------------------------------------------------------------------------------------
// Parses the command line for parameters.  See DXUTInit() for list 
//--------------------------------------------------------------------------------------
bool ParseCommandLine( SETTINGS* pSettings )
{
    bool bDisplayHelp = false;
    WCHAR strArg[256];
    WCHAR* strsettings = GetCommandLine();

    // Skip past program name (first token in command line).
    if( *strsettings == L'"' )  // Check for and handle quoted program name
    {
        strsettings++;

        // Skip over until another double-quote or a null 
        while( *strsettings && ( *strsettings != L'"' ) )
            strsettings++;

        // Skip over double-quote
        if( *strsettings == L'"' )
            strsettings++;
    }
    else
    {
        // First token wasn't a quote
        while( *strsettings && !iswspace( *strsettings ) )
            strsettings++;
    }

    for(; ; )
    {
        // Skip past any white space preceding the next token
        while( *strsettings && iswspace( *strsettings ) )
            strsettings++;
        if( *strsettings == 0 )
            break;

        // Handle flag args
        if( *strsettings == L'/' || *strsettings == L'-' )
        {
            strsettings++;

            if( IsNextArg( strsettings, L"s" ) )
            {
                pSettings->bSubDirs = true;
                continue;
            }

            if( IsNextArg( strsettings, L"v" ) )
            {
                pSettings->bVerbose = true;
                continue;
            }

            if( IsNextArg( strsettings, L"?" ) )
            {
                DisplayUsage();
                return false;
            }

            // Unrecognized flag
            GetNextArg( strsettings, strArg, 256 );
            wprintf( L"Unrecognized or incorrect flag usage: %s\n", strArg );
            bDisplayHelp = true;
        }
        else if( GetNextArg( strsettings, strArg, 256 ) )
        {
            // Handle non-flag args
            int nArgLen = ( int )wcslen( strArg );
            WCHAR* strNewArg = new WCHAR[nArgLen + 1];
            wcscpy_s( strNewArg, nArgLen + 1, strArg );
            pSettings->aFiles.Add( strNewArg );
            continue;
        }
    }

    if( bDisplayHelp )
    {
        wprintf( L"Type \"PRTsettings.exe /?\" for a complete list of options\n" );
        return false;
    }

    return true;
}


//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strsettings, WCHAR* strArg )
{
    int nArgLen = ( int )wcslen( strArg );
    if( _wcsnicmp( strsettings, strArg, nArgLen ) == 0 &&
        ( strsettings[nArgLen] == 0 || iswspace( strsettings[nArgLen] ) ) )
    {
        strsettings += nArgLen;
        return true;
    }

    return false;
}


//--------------------------------------------------------------------------------------
bool GetNextArg( WCHAR*& strsettings, WCHAR* strArg, int cchArg )
{
    HRESULT hr;

    // Place NULL terminator in strFlag after current token
    V( wcscpy_s( strArg, 256, strsettings ) );
    WCHAR* strSpace = strArg;
    while( *strSpace && !iswspace( *strSpace ) )
        strSpace++;
    *strSpace = 0;

    // Update strsettings
    int nArgLen = ( int )wcslen( strArg );
    strsettings += nArgLen;
    if( nArgLen > 0 )
        return true;
    else
        return false;
}


//--------------------------------------------------------------------------------------
void SeachSubdirsForFile( IDirect3DDevice9* pd3dDevice, WCHAR* strDir, WCHAR* strFile, SETTINGS* pSettings )
{
    // First search this dir for the file
    SearchDirForFile( pd3dDevice, strDir, strFile, pSettings );
    if( pSettings->bUserAbort )
        return;

    // Then search this dir for other dirs and recurse
    WCHAR strFullPath[MAX_PATH] = {0};
    WCHAR strSearchDir[MAX_PATH];
    wcscpy_s( strSearchDir, MAX_PATH, strDir );
    wcscat_s( strSearchDir, MAX_PATH, L"*" );

    WIN32_FIND_DATA fileData;
    ZeroMemory( &fileData, sizeof( WIN32_FIND_DATA ) );
    HANDLE hFindFile = FindFirstFile( strSearchDir, &fileData );
    if( hFindFile != INVALID_HANDLE_VALUE )
    {
        BOOL bSuccess = TRUE;
        while( bSuccess )
        {
            if( ( fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
            {
                // Don't process '.' and '..' dirs
                if( fileData.cFileName[0] != L'.' )
                {
                    wcscpy_s( strFullPath, MAX_PATH, strDir );
                    wcscat_s( strFullPath, MAX_PATH, fileData.cFileName );
                    wcscat_s( strFullPath, MAX_PATH, L"\\" );
                    SeachSubdirsForFile( pd3dDevice, strFullPath, strFile, pSettings );
                }
            }
            bSuccess = FindNextFile( hFindFile, &fileData );

            if( pSettings->bUserAbort )
                break;
        }
        FindClose( hFindFile );
    }
}


//--------------------------------------------------------------------------------------
void SearchDirForFile( IDirect3DDevice9* pd3dDevice, WCHAR* strDir, WCHAR* strFile, SETTINGS* pSettings )
{
    WCHAR strFullPath[MAX_PATH] = {0};
    WCHAR strSearchDir[MAX_PATH];
    wcscpy_s( strSearchDir, MAX_PATH, strDir );
    wcscat_s( strSearchDir, MAX_PATH, strFile );

    if( pSettings->bVerbose )
        wprintf( L"Searching dir %s for %s\n", strDir, strFile );

    WIN32_FIND_DATA fileData;
    ZeroMemory( &fileData, sizeof( WIN32_FIND_DATA ) );
    HANDLE hFindFile = FindFirstFile( strSearchDir, &fileData );
    if( hFindFile != INVALID_HANDLE_VALUE )
    {
        BOOL bSuccess = TRUE;
        while( bSuccess )
        {
            if( ( fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
            {
                wcscpy_s( strFullPath, MAX_PATH, strDir );
                wcscat_s( strFullPath, MAX_PATH, fileData.cFileName );
                ProcessOptionsFile( pd3dDevice, strFullPath, pSettings );
            }
            bSuccess = FindNextFile( hFindFile, &fileData );
            if( pSettings->bUserAbort )
                break;
        }
        FindClose( hFindFile );
    }
}


//--------------------------------------------------------------------------------------
HRESULT ProcessOptionsFile( IDirect3DDevice9* pd3dDevice, WCHAR* strOptionsFileName, SETTINGS* pSettings )
{
    HRESULT hr;

    WCHAR sz[256] = {0};
    SIMULATOR_OPTIONS options;
    COptionsFile optFile;
    CONCAT_MESH prtMesh;
    CONCAT_MESH blockerMesh;

    // Init structs
    ZeroMemory( &options, sizeof( SIMULATOR_OPTIONS ) );
    prtMesh.pMesh = NULL;
    prtMesh.dwNumMaterials = 0;
    blockerMesh.pMesh = NULL;
    blockerMesh.dwNumMaterials = 0;

    // Load options xml file
    swprintf_s( sz, 256, L"Reading options file: %s\n", strOptionsFileName );
    wprintf( sz );
    if( FAILED( hr = optFile.LoadOptions( strOptionsFileName, &options ) ) )
    {
        wprintf( L"Error: Failure reading options file.  Ensure schema matchs example options.xml file\n" );
        goto LCleanup;
    }

    // Load and concat meshes to a single mesh
    if( FAILED( hr = LoadMeshes( pd3dDevice, &options, &prtMesh, &blockerMesh, pSettings ) ) )
    {
        wprintf( L"Error: Can not load meshes\n" );
        goto LCleanup;
    }

    // Save mesh
    DWORD dwFlags = ( options.bBinaryXFile ) ? D3DXF_FILEFORMAT_BINARY : D3DXF_FILEFORMAT_TEXT;
    if( prtMesh.pMesh )
    {
        swprintf_s( sz, 256, L"Saving concatenated PRT meshes: %s\n", options.strOutputConcatPRTMesh );
        wprintf( sz );
        if( FAILED( hr = D3DXSaveMeshToX( options.strOutputConcatPRTMesh, prtMesh.pMesh, NULL,
                                          prtMesh.materialArray.GetData(), NULL, prtMesh.dwNumMaterials, dwFlags ) ) )
        {
            wprintf( L"Error: Failed saving mesh\n" );
            goto LCleanup;
        }
    }
    if( blockerMesh.pMesh )
    {
        swprintf_s( sz, 256, L"Saving concatenated blocker meshes: %s\n", options.strOutputConcatBlockerMesh );
        wprintf( sz );
        if( FAILED( hr = D3DXSaveMeshToX( options.strOutputConcatBlockerMesh, blockerMesh.pMesh, NULL,
                                          blockerMesh.materialArray.GetData(), NULL, blockerMesh.dwNumMaterials,
                                          dwFlags ) ) )
        {
            wprintf( L"Error: Failed saving mesh\n" );
            goto LCleanup;
        }
    }

    if( prtMesh.pMesh == NULL )
    {
        wprintf( L"Error: Need at least 1 non-blocker mesh for PRT simulator\n" );
        hr = E_FAIL;
        goto LCleanup;
    }

    // Run PRT simulator
    if( FAILED( hr = RunPRTSimulator( pd3dDevice, &options, &prtMesh, &blockerMesh, pSettings ) ) )
    {
        goto LCleanup;
    }

LCleanup:

    // Cleanup
    optFile.FreeOptions( &options );
    for( int i = 0; i < prtMesh.materialArray.GetSize(); i++ )
        SAFE_DELETE_ARRAY( prtMesh.materialArray[i].pTextureFilename );
    SAFE_RELEASE( prtMesh.pMesh );
    for( int i = 0; i < blockerMesh.materialArray.GetSize(); i++ )
        SAFE_DELETE_ARRAY( blockerMesh.materialArray[i].pTextureFilename );
    SAFE_RELEASE( blockerMesh.pMesh );

    return hr;
}


//--------------------------------------------------------------------------------------
IDirect3DDevice9* CreateNULLRefDevice()
{
    HRESULT hr;
    IDirect3D9* pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( NULL == pD3D )
        return NULL;

    D3DDISPLAYMODE Mode;
    pD3D->GetAdapterDisplayMode( 0, &Mode );

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory( &pp, sizeof( D3DPRESENT_PARAMETERS ) );
    pp.BackBufferWidth = 1;
    pp.BackBufferHeight = 1;
    pp.BackBufferFormat = Mode.Format;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Windowed = TRUE;

    IDirect3DDevice9* pd3dDevice;
    hr = pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, GetConsoleWindow(),
                             D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &pd3dDevice );
    SAFE_RELEASE( pD3D );
    if( FAILED( hr ) || pd3dDevice == NULL )
        return NULL;

    return pd3dDevice;
}


//--------------------------------------------------------------------------------------
HRESULT LoadMeshes( IDirect3DDevice9* pd3dDevice, SIMULATOR_OPTIONS* pOptions,
                    CONCAT_MESH* pPRTMesh, CONCAT_MESH* pBlockerMesh, SETTINGS* pSettings )
{
    HRESULT hr;
    ID3DXBuffer* pMaterialBuffer = NULL;
    DWORD dwNumMaterials = 0;
    D3DXMATERIAL* pMaterials = NULL;

    CGrowableArray <ID3DXMesh*> blockerMeshArray;
    CGrowableArray <D3DXMATRIX> blockerMatrixArray;

    CGrowableArray <ID3DXMesh*> prtMeshArray;
    CGrowableArray <D3DXMATRIX> prtMatrixArray;

    pPRTMesh->pMesh = NULL;
    pPRTMesh->dwNumMaterials = 0;
    pBlockerMesh->pMesh = NULL;
    pBlockerMesh->dwNumMaterials = 0;

    DWORD iMesh;
    for( iMesh = 0; iMesh < pOptions->dwNumMeshes; iMesh++ )
    {
        if( pSettings->bVerbose )
            wprintf( L"Looking for mesh: %s\n", pOptions->pInputMeshes[iMesh].strMeshFile );

        ID3DXMesh* pMesh = NULL;
        WCHAR str[MAX_PATH];
        WCHAR strMediaDir[MAX_PATH];
        if( FAILED( hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, pOptions->pInputMeshes[iMesh].strMeshFile ) ) )
        {
            wprintf( L"Can not find mesh %s\n", str );
            return hr;
        }

        wprintf( L"Reading mesh: %s\n", str );

        // Store the directory where the mesh was found
        wcscpy_s( strMediaDir, MAX_PATH, str );
        WCHAR* pch = wcsrchr( strMediaDir, L'\\' );
        if( pch ) *pch = 0;
        DXUTSetMediaSearchPath( strMediaDir );

        // Load mesh
        if( FAILED( hr = D3DXLoadMeshFromX( str, D3DXMESH_SYSTEMMEM, pd3dDevice, NULL,
                                            &pMaterialBuffer, NULL, &dwNumMaterials, &pMesh ) ) )
            return hr;

        // Compact & attribute sort mesh
        DWORD* rgdwAdjacency = NULL;
        rgdwAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
        if( rgdwAdjacency == NULL )
            return E_OUTOFMEMORY;
        V_RETURN( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
        V_RETURN( pMesh->OptimizeInplace( D3DXMESHOPT_COMPACT | D3DXMESHOPT_ATTRSORT,
                                          rgdwAdjacency, NULL, NULL, NULL ) );
        delete []rgdwAdjacency;

        DWORD dwNumAttribs = 0;
        V( pMesh->GetAttributeTable( NULL, &dwNumAttribs ) );
        assert( dwNumAttribs == dwNumMaterials );

        // Loop over D3D materials in mesh and store them
        pMaterials = ( D3DXMATERIAL* )pMaterialBuffer->GetBufferPointer();
        UINT iMat;
        for( iMat = 0; iMat < dwNumMaterials; iMat++ )
        {
            D3DXMATERIAL mat;
            mat.MatD3D = pMaterials[iMat].MatD3D;
            if( pMaterials[iMat].pTextureFilename )
            {
                mat.pTextureFilename = new CHAR[MAX_PATH];
                if( mat.pTextureFilename )
                {
                    WCHAR strTextureTemp[MAX_PATH];
                    WCHAR strFullPath[MAX_PATH];
                    CHAR strFullPathA[MAX_PATH];
                    MultiByteToWideChar( CP_ACP, 0, pMaterials[iMat].pTextureFilename, -1, strTextureTemp, MAX_PATH );
                    strTextureTemp[MAX_PATH - 1] = 0;
                    DXUTFindDXSDKMediaFileCch( strFullPath, MAX_PATH, strTextureTemp );
                    WideCharToMultiByte( CP_ACP, 0, strFullPath, -1, strFullPathA, MAX_PATH, NULL, NULL );
                    strcpy_s( mat.pTextureFilename, MAX_PATH, strFullPathA );
                }
            }
            else
            {
                mat.pTextureFilename = NULL;
            }

            // handle if there's not enough SH materials specified for this mesh 
            D3DXSHMATERIAL shMat;
            if( pOptions->pInputMeshes[iMesh].dwNumSHMaterials == 0 )
            {
                shMat.Diffuse = D3DXCOLOR( 2.00f, 2.00f, 2.00f, 1.0f );
                shMat.Absorption = D3DXCOLOR( 0.0030f, 0.0030f, 0.0460f, 1.0f );
                shMat.bSubSurf = FALSE;
                shMat.RelativeIndexOfRefraction = 1.3f;
                shMat.ReducedScattering = D3DXCOLOR( 1.00f, 1.00f, 1.00f, 1.0f );
            }
            else
            {
                DWORD iSH = iMat;
                if( iSH >= pOptions->pInputMeshes[iMesh].dwNumSHMaterials )
                    iSH = pOptions->pInputMeshes[iMesh].dwNumSHMaterials - 1;
                shMat = pOptions->pInputMeshes[iMesh].pSHMaterials[iSH];
            }

            if( pOptions->pInputMeshes[iMesh].bIsBlockerMesh )
            {
                pBlockerMesh->materialArray.Add( mat );
                pBlockerMesh->shMaterialArray.Add( shMat );
            }
            else
            {
                pPRTMesh->materialArray.Add( mat );
                pPRTMesh->shMaterialArray.Add( shMat );
            }
        }

        SAFE_RELEASE( pMaterialBuffer );

        // Build world matrix from settings in options file
        D3DXMATRIX mScale, mTranslate, mRotate, mWorld;
        D3DXMatrixScaling( &mScale, pOptions->pInputMeshes[iMesh].vScale.x, pOptions->pInputMeshes[iMesh].vScale.y,
                           pOptions->pInputMeshes[iMesh].vScale.z );
        D3DXMatrixRotationYawPitchRoll( &mRotate, pOptions->pInputMeshes[iMesh].fYaw,
                                        pOptions->pInputMeshes[iMesh].fPitch, pOptions->pInputMeshes[iMesh].fRoll );
        D3DXMatrixTranslation( &mTranslate, pOptions->pInputMeshes[iMesh].vTranslate.x,
                               pOptions->pInputMeshes[iMesh].vTranslate.y,
                               pOptions->pInputMeshes[iMesh].vTranslate.z );
        mWorld = mScale * mRotate * mTranslate;

        // Record data in arrays
        if( pOptions->pInputMeshes[iMesh].bIsBlockerMesh )
        {
            pBlockerMesh->dwNumMaterials += dwNumMaterials;
            pBlockerMesh->numMaterialsArray.Add( dwNumMaterials );
            blockerMeshArray.Add( pMesh );
            blockerMatrixArray.Add( mWorld );
        }
        else
        {
            pPRTMesh->dwNumMaterials += dwNumMaterials;
            pPRTMesh->numMaterialsArray.Add( dwNumMaterials );
            prtMeshArray.Add( pMesh );
            prtMatrixArray.Add( mWorld );
        }
    }

    ID3DXMesh** ppMeshes;
    DWORD dwNumMeshes;
    D3DXMATRIX* pGeomXForms;

    // Concat blocker meshes from arrays
    dwNumMeshes = blockerMeshArray.GetSize();
    if( dwNumMeshes > 0 )
    {
        LONGLONG dwNumVerts = 0;
        LONGLONG dwNumFaces = 0;
        for( DWORD iMesh = 0; iMesh < dwNumMeshes; iMesh++ )
        {
            dwNumVerts += blockerMeshArray[iMesh]->GetNumVertices();
            dwNumFaces += blockerMeshArray[iMesh]->GetNumFaces();
        }

        DWORD dwFlags = D3DXMESH_SYSTEMMEM;
        if( dwNumVerts > 0xFFFF || dwNumFaces > 0xFFFF )
            dwFlags |= D3DXMESH_32BIT;

        ppMeshes = blockerMeshArray.GetData();
        pGeomXForms = blockerMatrixArray.GetData();
        V_RETURN( D3DXConcatenateMeshes( ppMeshes, dwNumMeshes, dwFlags, pGeomXForms, NULL, NULL,
                                         pd3dDevice, &pBlockerMesh->pMesh ) );
        for( int i = 0; i < blockerMeshArray.GetSize(); i++ )
            SAFE_RELEASE( blockerMeshArray[i] );

        NormalizeNormals( pBlockerMesh->pMesh );
    }

    // Concat prt meshes from arrays
    dwNumMeshes = prtMeshArray.GetSize();
    if( dwNumMeshes > 0 )
    {
        LONGLONG dwNumVerts = 0;
        LONGLONG dwNumFaces = 0;
        for( DWORD iMesh = 0; iMesh < dwNumMeshes; iMesh++ )
        {
            dwNumVerts += prtMeshArray[iMesh]->GetNumVertices();
            dwNumFaces += prtMeshArray[iMesh]->GetNumFaces();
        }

        DWORD dwFlags = D3DXMESH_SYSTEMMEM;
        if( dwNumVerts > 0xFFFF || dwNumFaces > 0xFFFF )
            dwFlags |= D3DXMESH_32BIT;

        CGrowableArray <D3DXMATRIX> uvMatrixArray;
        for( int i = 0; i < prtMatrixArray.GetSize(); i++ )
        {
            D3DXMATRIX mat;
            D3DXMatrixIdentity( &mat );
            uvMatrixArray.Add( mat );
        }
        D3DXMATRIX* pUVXForms = uvMatrixArray.GetData();

        ppMeshes = prtMeshArray.GetData();
        pGeomXForms = prtMatrixArray.GetData();
        V_RETURN( D3DXConcatenateMeshes( ppMeshes, dwNumMeshes, dwFlags, pGeomXForms, pUVXForms, NULL,
                                         pd3dDevice, &pPRTMesh->pMesh ) );
        for( int i = 0; i < prtMeshArray.GetSize(); i++ )
            SAFE_RELEASE( prtMeshArray[i] );

        DWORD dwNumAttribs = 0;
        V( pPRTMesh->pMesh->GetAttributeTable( NULL, &dwNumAttribs ) );
        assert( dwNumAttribs == ( DWORD )pPRTMesh->materialArray.GetSize() );
        assert( dwNumAttribs > 0 );

        NormalizeNormals( pPRTMesh->pMesh );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
void NormalizeNormals( ID3DXMesh* pMesh )
{
    HRESULT hr;
    BYTE* pV = NULL;

    D3DVERTEXELEMENT9 decl[MAXD3DDECLLENGTH + 1];
    pMesh->GetDeclaration( decl );
    int nNormalOffset = -1;
    for( int di = 0; di < MAX_FVF_DECL_SIZE; di++ )
    {
        if( decl[di].Usage == D3DDECLUSAGE_NORMAL )
        {
            nNormalOffset = decl[di].Offset;
            break;
        }
        if( decl[di].Stream == 255 )
            break;
    }
    if( nNormalOffset < 0 )
        return;

    V( pMesh->LockVertexBuffer( 0, ( void** )&pV ) );
    UINT uStride = pMesh->GetNumBytesPerVertex();
    BYTE* pNormals = pV + nNormalOffset;
    for( UINT uVert = 0; uVert < pMesh->GetNumVertices(); uVert++ )
    {
        D3DXVECTOR3* pCurNormal = ( D3DXVECTOR3* )pNormals;
        D3DXVec3Normalize( pCurNormal, pCurNormal );
        pNormals += uStride;
    }
    pMesh->UnlockVertexBuffer();
}


//--------------------------------------------------------------------------------------
void DisplayUsage()
{
    wprintf( L"\n" );
    wprintf( L"PRTCmdLine - a command line PRT simulator tool\n" );
    wprintf( L"\n" );
    wprintf( L"Usage: PRTCmdLine.exe [/s] [filename1] [filename2] ...\n" );
    wprintf( L"\n" );
    wprintf( L"where:\n" );
    wprintf( L"\n" );
    wprintf( L"  [/v]\t\tVerbose output.  Useful for debugging\n" );
    wprintf( L"  [/s]\t\tSearches in the specified directory and all subdirectoies of\n" );
    wprintf( L"  \t\teach filename\n" );
    wprintf( L"  [filename*]\tSpecifies the directory and XML files to read.  Wildcards are\n" );
    wprintf( L"  \t\tsupported.\n" );
    wprintf( L"  \t\tSee options.xml for an example options XML file\n" );
}

//----------------------------------------------------------------------------
// File: main.cpp
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "crackdecl.h"
#include <stdio.h>
#include <conio.h>


#define COLOR_COUNT 8

FLOAT ColorList[COLOR_COUNT][3] =
{
    {1.0f, 0.5f, 0.5f},
    {0.5f, 1.0f, 0.5f},
    {1.0f, 1.0f, 0.5f},
    {0.5f, 1.0f, 1.0f},
    {1.0f, 0.5f,0.75f},
    {0.0f, 0.5f, 0.75f},
    {0.5f, 0.5f, 0.75f},
    {0.5f, 0.5f, 1.0f},
};

const FLOAT         g_cfEpsilon = 1e-5f;

//-----------------------------------------------------------------------------
struct SETTINGS
{
    int maxcharts, width, height;
    FLOAT maxstretch, gutter;
    BYTE nOutputTextureIndex;
    bool bTopologicalAdjacency;
    bool bGeometricAdjacency;
    bool bFalseEdges;
    bool bFileAdjacency;
    WCHAR   szAdjacencyFilename[MAX_PATH];
    WCHAR   szFalseEdgesFilename[MAX_PATH];

    bool bUserAbort, bSubDirs, bOverwrite, bOutputTexture, bColorMesh;
    bool bVerbose;
    bool bOutputFilenameGiven;
    WCHAR   szOutputFilename[MAX_PATH];

    bool bIMT;
    bool bTextureSignal;
    bool bPRTSignal;
    bool bVertexSignal;
    DWORD QualityFlag;
    D3DDECLUSAGE VertexSignalUsage;
    BYTE VertexSignalIndex;
    BYTE nIMTInputTextureIndex;
    WCHAR   szIMTInputFilename[MAX_PATH];

    bool bResampleTexture;
    D3DDECLUSAGE ResampleTextureUsage;
    UINT ResampleTextureUsageIndex;
    WCHAR   szResampleTextureFile[MAX_PATH];

    CGrowableArray <WCHAR*> aFiles;
};


//-----------------------------------------------------------------------------
class CProcessedFileList
{
public:
            CProcessedFileList()
            {
            }

            ~CProcessedFileList()
            {
                for( int i = 0; i < m_fileList.GetSize(); i++ )
                    SAFE_DELETE_ARRAY( m_fileList[i] );
                m_fileList.RemoveAll();
            }

    bool    IsInList( WCHAR* strFullPath )
    {
        for( int i = 0; i < m_fileList.GetSize(); i++ )
        {
            if( wcscmp( m_fileList[i], strFullPath ) == 0 )
                return true;
        }

        return false;
    }

    void    Add( WCHAR* strFullPath )
    {
        WCHAR* strTemp = new WCHAR[MAX_PATH];
        wcscpy_s( strTemp, MAX_PATH, strFullPath );
        m_fileList.Add( strTemp );
    }

protected:
    CGrowableArray <WCHAR*> m_fileList;
};

CProcessedFileList  g_ProcessedFileList;


//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
bool ParseCommandLine( SETTINGS* pSettings );
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
IDirect3DDevice9*   CreateNULLRefDevice();
void SearchDirForFile( IDirect3DDevice9* pd3dDevice, WCHAR* strDir, WCHAR* strFile, SETTINGS* pSettings );
void SearchSubdirsForFile( IDirect3DDevice9* pd3dDevice, WCHAR* strDir, WCHAR* strFile, SETTINGS* pSettings );
HRESULT ProcessFile( IDirect3DDevice9* pd3dDevice, WCHAR* strFile, SETTINGS* pSettings );
void DisplayUsage();
WCHAR*              TraceD3DDECLUSAGEtoString( BYTE u );
HRESULT LoadFile( WCHAR* szFile, ID3DXBuffer** ppBuffer );


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
    memset( &settings, 0, sizeof( SETTINGS ) );
    settings.bOverwrite = false;
    settings.bOutputFilenameGiven = false;
    settings.szOutputFilename[0] = 0;
    settings.bUserAbort = false;
    settings.bSubDirs = false;
    settings.bTopologicalAdjacency = false;
    settings.bGeometricAdjacency = false;
    settings.bFileAdjacency = false;
    settings.szAdjacencyFilename[0] = 0;
    settings.QualityFlag = D3DXUVATLAS_DEFAULT;

    settings.bFalseEdges = false;
    settings.szFalseEdgesFilename[0] = 0;

    settings.maxcharts = 0;
    settings.maxstretch = 1 / 6.0f;
    settings.gutter = 2.0f;
    settings.width = 512;
    settings.height = 512;
    settings.bOutputTexture = false;
    settings.bColorMesh = false;
    settings.nOutputTextureIndex = 0;

    settings.nIMTInputTextureIndex = 0;
    settings.bIMT = false;
    settings.bTextureSignal = false;
    settings.bPRTSignal = false;
    settings.bVertexSignal = false;
    settings.VertexSignalUsage = D3DDECLUSAGE_COLOR;
    settings.VertexSignalIndex = 0;
    settings.szIMTInputFilename[0] = 0;

    settings.bResampleTexture = false;
    settings.ResampleTextureUsage = D3DDECLUSAGE_TEXCOORD;
    settings.ResampleTextureUsageIndex = 0;
    settings.szResampleTextureFile[0] = 0;

    if( !ParseCommandLine( &settings ) )
    {
        nRet = 0;
        goto LCleanup;
    }

    DXUTGetGlobalTimer()->Start();

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
        WCHAR* strParamFilename = settings.aFiles[i];

        // For this cmd line arg, extract the full dir & filename
        WCHAR strDir[MAX_PATH] = {0};
        WCHAR strFile[MAX_PATH] = {0};
        WCHAR* pFilePart;
        DWORD dwWrote = GetFullPathName( strParamFilename, MAX_PATH, strDir, &pFilePart );
        if( dwWrote > 1 && pFilePart )
        {
            wcscpy_s( strFile, MAX_PATH, pFilePart );
            *pFilePart = NULL;
        }

        if( settings.bSubDirs )
            SearchSubdirsForFile( pd3dDevice, strDir, strFile, &settings );
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
    WCHAR* strCmdLine;
    WCHAR* strArg;

    int nNumArgs;
    LPWSTR* pstrArgList = CommandLineToArgvW( GetCommandLine(), &nNumArgs );
    for( int iArg = 1; iArg < nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( IsNextArg( strCmdLine, L"ta" ) )
            {
                if( pSettings->bGeometricAdjacency || pSettings->bFileAdjacency )
                {
                    wprintf( L"Incorrect flag usage: /ta, /ga, and /fa are exclusive\n" );
                    bDisplayHelp = true;
                    continue;
                }

                pSettings->bTopologicalAdjacency = true;
                pSettings->bGeometricAdjacency = false;
                pSettings->bFileAdjacency = false;
                continue;
            }

            if( IsNextArg( strCmdLine, L"ga" ) )
            {
                if( pSettings->bTopologicalAdjacency || pSettings->bFileAdjacency )
                {
                    wprintf( L"Incorrect flag usage: /ta, /ga, and /fa are exclusive\n" );
                    bDisplayHelp = true;
                    continue;
                }

                pSettings->bTopologicalAdjacency = false;
                pSettings->bGeometricAdjacency = true;
                pSettings->bFileAdjacency = false;
                continue;
            }

            if( IsNextArg( strCmdLine, L"fa" ) )
            {
                if( pSettings->bTopologicalAdjacency || pSettings->bGeometricAdjacency )
                {
                    wprintf( L"Incorrect flag usage: /ta, /ga, and /fa are exclusive\n" );
                    bDisplayHelp = true;
                }

                if( iArg + 1 < nNumArgs )
                {
                    pSettings->bTopologicalAdjacency = false;
                    pSettings->bGeometricAdjacency = false;
                    pSettings->bFileAdjacency = true;
                    strArg = pstrArgList[++iArg];
                    wcscpy_s( pSettings->szAdjacencyFilename, 256, strArg );
                    continue;
                }
                wprintf( L"Incorrect flag usage: /fa\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"fe" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->bFalseEdges = true;
                    wcscpy_s( pSettings->szFalseEdgesFilename, 256, strArg );
                    continue;
                }
                wprintf( L"Incorrect flag usage: /fe\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"s" ) )
            {
                pSettings->bSubDirs = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"c" ) )
            {
                pSettings->bColorMesh = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"o" ) )
            {
                if( pSettings->bOverwrite )
                {
                    wprintf( L"Incorrect flag usage: /f and /o\n" );
                    bDisplayHelp = true;
                    continue;
                }

                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->bOutputFilenameGiven = true;
                    wcscpy_s( pSettings->szOutputFilename, 256, strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /o\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"it" ) )
            {
                if( pSettings->bPRTSignal || pSettings->bVertexSignal )
                {
                    wprintf( L"Incorrect flag usage: /it, /ip, and /iv are exclusive\n" );
                    bDisplayHelp = true;
                    continue;
                }

                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    wcscpy_s( pSettings->szIMTInputFilename, 256, strArg );
                    pSettings->bIMT = true;
                    pSettings->bTextureSignal = true;
                    continue;
                }

                wprintf( L"Incorrect flag usage: /it\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"ip" ) )
            {
                if( pSettings->bTextureSignal || pSettings->bVertexSignal )
                {
                    wprintf( L"Incorrect flag usage: /it, /ip, and /iv are exclusive\n" );
                    bDisplayHelp = true;
                    continue;
                }

                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    wcscpy_s( pSettings->szIMTInputFilename, 256, strArg );
                    pSettings->bIMT = true;
                    pSettings->bPRTSignal = true;
                    continue;
                }

                wprintf( L"Incorrect flag usage: /ip\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"q" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    if( !_wcsicmp( strArg, L"DEFAULT" ) )
                    {
                        pSettings->QualityFlag = D3DXUVATLAS_DEFAULT;
                    }
                    else if( !_wcsicmp( strArg, L"FAST" ) )
                    {
                        pSettings->QualityFlag = D3DXUVATLAS_GEODESIC_FAST;
                    }
                    else if( !_wcsicmp( strArg, L"QUALITY" ) )
                    {
                        pSettings->QualityFlag = D3DXUVATLAS_GEODESIC_QUALITY;
                    }
                    else
                    {
                        wprintf( L"Incorrect /q flag usage: unknown usage parameter \"%s\"\n", strArg );
                        bDisplayHelp = true;
                        continue;
                    }
                    continue;
                }

                wprintf( L"Incorrect flag usage: /ip\n" );
                bDisplayHelp = true;
                continue;
            }


            if( IsNextArg( strCmdLine, L"iv" ) )
            {
                if( pSettings->bTextureSignal || pSettings->bPRTSignal )
                {
                    wprintf( L"Incorrect flag usage: /it, /ip, and /iv are exclusive\n" );
                    bDisplayHelp = true;
                    continue;
                }

                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    if( !_wcsicmp( strArg, L"COLOR" ) )
                    {
                        pSettings->VertexSignalUsage = D3DDECLUSAGE_COLOR;
                    }
                    else if( !_wcsicmp( strArg, L"NORMAL" ) )
                    {
                        pSettings->VertexSignalUsage = D3DDECLUSAGE_NORMAL;
                    }
                    else if( !_wcsicmp( strArg, L"TEXCOORD" ) )
                    {
                        pSettings->VertexSignalUsage = D3DDECLUSAGE_TEXCOORD;
                    }
                    else if( !_wcsicmp( strArg, L"TANGENT" ) )
                    {
                        pSettings->VertexSignalUsage = D3DDECLUSAGE_TANGENT;
                    }
                    else if( !_wcsicmp( strArg, L"BINORMAL" ) )
                    {
                        pSettings->VertexSignalUsage = D3DDECLUSAGE_BINORMAL;
                    }
                    else
                    {
                        wprintf( L"Incorrect /iv flag usage: unknown usage parameter \"%s\"\n", strArg );
                        bDisplayHelp = true;
                        continue;
                    }
                    pSettings->bIMT = true;
                    pSettings->bVertexSignal = true;
                    continue;
                }

                wprintf( L"Incorrect flag usage: /ip\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"f" ) )
            {
                pSettings->bOverwrite = true;
                if( pSettings->bOutputFilenameGiven )
                {
                    wprintf( L"Incorrect flag usage: /f and /o\n" );
                    bDisplayHelp = true;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"t" ) )
            {
                pSettings->bOutputTexture = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"v" ) )
            {
                pSettings->bVerbose = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"n" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->maxcharts = _wtoi( strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /n\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"w" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->width = _wtoi( strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /w\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"h" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->height = _wtoi( strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /h\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"st" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->maxstretch = ( FLOAT )_wtof( strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /st\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"rt" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    pSettings->bResampleTexture = true;
                    strArg = pstrArgList[++iArg];
                    wcscpy_s( pSettings->szResampleTextureFile, 256, strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /rt\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"uvi" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->nOutputTextureIndex = ( BYTE )_wtoi( strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /uvi\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"rtu" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    if( !_wcsicmp( strArg, L"NORMAL" ) )
                    {
                        pSettings->ResampleTextureUsage = D3DDECLUSAGE_NORMAL;
                    }
                    else if( !_wcsicmp( strArg, L"POSITION" ) )
                    {
                        pSettings->ResampleTextureUsage = D3DDECLUSAGE_POSITION;
                    }
                    else if( !_wcsicmp( strArg, L"TEXCOORD" ) )
                    {
                        pSettings->ResampleTextureUsage = D3DDECLUSAGE_TEXCOORD;
                    }
                    else if( !_wcsicmp( strArg, L"TANGENT" ) )
                    {
                        pSettings->ResampleTextureUsage = D3DDECLUSAGE_TANGENT;
                    }
                    else if( !_wcsicmp( strArg, L"BINORMAL" ) )
                    {
                        pSettings->ResampleTextureUsage = D3DDECLUSAGE_BINORMAL;
                    }
                    else
                    {
                        wprintf( L"Incorrect /rtu flag usage: unknown usage parameter \"%s\"\n", strArg );
                        bDisplayHelp = true;
                        continue;
                    }
                    continue;
                }

                wprintf( L"Incorrect flag usage: /rtu\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"rti" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->ResampleTextureUsageIndex = ( UINT )_wtoi( strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /rti\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"g" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    pSettings->gutter = ( FLOAT )_wtof( strArg );
                    continue;
                }

                wprintf( L"Incorrect flag usage: /g\n" );
                bDisplayHelp = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"?" ) )
            {
                DisplayUsage();
                return false;
            }

            // Unrecognized flag
            wprintf( L"Unrecognized or incorrect flag usage: %s\n", strCmdLine );
            bDisplayHelp = true;
        }
        else
        {
            // Handle non-flag args as seperate input files
            int nArgLen = ( int )wcslen( strCmdLine );
            WCHAR* strNewArg = new WCHAR[nArgLen + 1];
            if( strNewArg )
            {
                wcscpy_s( strNewArg, nArgLen + 1, strCmdLine );
                pSettings->aFiles.Add( strNewArg );
            }
            continue;
        }
    }

    if( pSettings->aFiles.GetSize() == 0 )
    {
        DisplayUsage();
        return false;
    }

    if( bDisplayHelp )
    {
        wprintf( L"Type \"UVAtlas.exe /?\" for a complete list of options\n" );
        return false;
    }

    LocalFree( pstrArgList );

    return true;
}


//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
    int nArgLen = ( int )wcslen( strArg );
    if( _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 && strCmdLine[nArgLen] == 0 )
        return true;

    return false;
}


//--------------------------------------------------------------------------------------
void SearchSubdirsForFile( IDirect3DDevice9* pd3dDevice, WCHAR* strDir, WCHAR* strFile, SETTINGS* pSettings )
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
                    SearchSubdirsForFile( pd3dDevice, strFullPath, strFile, pSettings );
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
                if( !g_ProcessedFileList.IsInList( strFullPath ) )
                    ProcessFile( pd3dDevice, strFullPath, pSettings );

                if( pSettings->bOutputFilenameGiven ) // only process 1 file if this option is on
                    break;
            }
            bSuccess = FindNextFile( hFindFile, &fileData );
            if( pSettings->bUserAbort )
                break;
        }
        FindClose( hFindFile );
    }
    else
    {
        wprintf( L"File(s) not found.\n" );
    }
}


//--------------------------------------------------------------------------------------
BOOL CheckMeshValidation( LPD3DXMESH pMesh, LPD3DXMESH* pMeshOut, DWORD** ppAdjacency,
                          BOOL bTopologicalAdjacency, BOOL bGeometricAdjacency, LPD3DXBUFFER pOrigAdj )
{
    HRESULT hr = S_OK;
    BOOL bResult = TRUE;
    DWORD* pAdjacencyIn = NULL, *pAdjacencyAlloc = NULL;
    LPD3DXBUFFER pErrorsAndWarnings = NULL;

    if( !( pMesh && ppAdjacency ) )
    {
        bResult = FALSE;
        goto FAIL;
    }

    if( bTopologicalAdjacency || bGeometricAdjacency )
    {
        pAdjacencyAlloc = new DWORD[pMesh->GetNumFaces() * sizeof( DWORD ) * 3];
        if( !pAdjacencyAlloc )
        {
            wprintf( L"Out of memory\n" );
            bResult = FALSE;
            goto FAIL;
        }
        pAdjacencyIn = pAdjacencyAlloc;
    }

    if( bTopologicalAdjacency )
    {
        hr = pMesh->ConvertPointRepsToAdjacency( NULL, pAdjacencyIn );
        if( FAILED( hr ) )
        {
            wprintf( L"ConvertPointRepsToAdjacency() failed: %s\n", DXGetErrorString( hr ) );
            bResult = FALSE;
            goto FAIL;
        }
    }
    else if( bGeometricAdjacency )
    {
        hr = pMesh->GenerateAdjacency( g_cfEpsilon, pAdjacencyIn );
        if( FAILED( hr ) )
        {
            wprintf( L"GenerateAdjacency() failed: %s\n", DXGetErrorString( hr ) );
            bResult = FALSE;
            goto FAIL;
        }
    }
    else if( pOrigAdj )
    {
        pAdjacencyIn = ( DWORD* )pOrigAdj->GetBufferPointer();
    }

    hr = D3DXValidMesh( pMesh, pAdjacencyIn, &pErrorsAndWarnings );
    if( NULL != pErrorsAndWarnings )
    {
        char* s = ( char* )pErrorsAndWarnings->GetBufferPointer();
        wprintf( L"%S", s );
        SAFE_RELEASE( pErrorsAndWarnings );
    }

    if( FAILED( hr ) )
    {
        wprintf( L"D3DXValidMesh() failed: %s.  Attempting D3DXCleanMesh()\n", DXGetErrorString( hr ) );

        hr = D3DXCleanMesh( D3DXCLEAN_SIMPLIFICATION, pMesh, pAdjacencyIn, pMeshOut,
                            pAdjacencyIn, &pErrorsAndWarnings );

        if( NULL != pErrorsAndWarnings )
        {
            char* s = ( char* )pErrorsAndWarnings->GetBufferPointer();
            wprintf( L"%S", s );
        }

        if( FAILED( hr ) )
        {
            wprintf( L"D3DXCleanMesh() failed: %s\n", DXGetErrorString( hr ) );
            bResult = FALSE;
            goto FAIL;
        }
        else
        {
            wprintf( L"D3DXCleanMesh() succeeded: %s\n", DXGetErrorString( hr ) );
        }
    }
    else
    {
        *pMeshOut = pMesh;
        ( *pMeshOut )->AddRef();
    }
FAIL:
    SAFE_RELEASE( pErrorsAndWarnings );
    if( bResult == FALSE )
    {
        SAFE_DELETE( pAdjacencyAlloc );
        SAFE_DELETE( *pMeshOut );
    }
    *ppAdjacency = pAdjacencyIn;
    return bResult;
}


//--------------------------------------------------------------------------------------
HRESULT CALLBACK UVAtlasCallback( FLOAT fPercentDone, LPVOID lpUserContext )
{
    static double s_fLastTime = 0.0;
    double fTime = DXUTGetGlobalTimer()->GetTime();

    if( fTime - s_fLastTime > 0.1 )
    {
        wprintf( L"%.2f%%   \r", fPercentDone * 100 );
        s_fLastTime = fTime;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT PerVertexPRTIMT( ID3DXMesh* pMesh, ID3DXPRTBuffer* pPRTBuffer, const UINT cuNumCoeffs,
                         ID3DXBuffer** ppIMTData )
{
    HRESULT hr;
    FLOAT* pPRTSignal = NULL;

    if( !pMesh || !ppIMTData || !pPRTBuffer )
    {
        wprintf( L"Error: pMesh, ppIMTData and pPRTBuffer must not be NULL\n" );
        hr = D3DERR_INVALIDCALL;
        goto FAIL;
    }

    if( pPRTBuffer->IsTexture() )
    {
        wprintf( L"Error: pPRTBuffer must be per-vertex\n" );
        hr = D3DERR_INVALIDCALL;
        goto FAIL;
    }

    const UINT uStride = pPRTBuffer->GetNumChannels() * pPRTBuffer->GetNumCoeffs() * sizeof( FLOAT );
    if( FAILED( hr = pPRTBuffer->LockBuffer( 0, pPRTBuffer->GetNumSamples(), &pPRTSignal ) ) )
    {
        wprintf( L"ID3DXPRTBuffer::LockBuffer() failed: %s\n", DXGetErrorString( hr ) );
        goto FAIL;
    }

    if( FAILED( hr = D3DXComputeIMTFromPerVertexSignal( pMesh, pPRTSignal, cuNumCoeffs, uStride, 0L, NULL, NULL,
                                                        ppIMTData ) ) )
    {
        wprintf( L"D3DXComputeIMTFromVertexSignal() failed: %s\n", DXGetErrorString( hr ) );
        goto FAIL;
    }

FAIL:
    if( pPRTSignal )
    {
        HRESULT hr2;
        if( FAILED( hr2 = pPRTBuffer->UnlockBuffer() ) )
        {
            wprintf( L"ID3DXPRTBuffer::UnlockVertexBuffer() failed: %s!\n", DXGetErrorString( hr2 ) );
        }
        pPRTSignal = NULL;
    }
    SAFE_RELEASE( pPRTBuffer );
    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT PerVertexIMT( ID3DXMesh *pMesh, CONST D3DVERTEXELEMENT9 *pDecl, D3DDECLUSAGE usage, DWORD usageIndex, CD3DXCrackDecl *Crack,
                     ID3DXBuffer **ppIMTData ) {
    HRESULT hr;
    FLOAT *pfVertexData = NULL;
    const D3DVERTEXELEMENT9 *elmt;
    UINT uSignalDimension;
    CD3DXCrackDecl cd;

    if( !pMesh || !ppIMTData )
 {
        wprintf( L"Error: some of pMesh and ppIMTData are == NULL\n" );
        hr = D3DERR_INVALIDCALL;
        goto FAIL;
    }

    if( NULL == ( elmt = GetDeclElement( pDecl, ( BYTE )usage, ( BYTE )usageIndex ) ) )
 {
        wprintf( L"Error: Requested vertex data not found in mesh\n" );
        hr = E_FAIL;
        goto FAIL;
    }

    uSignalDimension = Crack->GetFields( elmt );

    if( FAILED( hr = pMesh->LockVertexBuffer( D3DLOCK_NOSYSLOCK, ( VOID** )&pfVertexData ) ) )
 {
        wprintf( L"ID3DXMesh::LockVertexBuffer() failed: %s\n", DXGetErrorString( hr ) );
        goto FAIL;
    }

    if( FAILED( hr = D3DXComputeIMTFromPerVertexSignal( pMesh, pfVertexData+elmt->Offset, uSignalDimension, pMesh->GetNumBytesPerVertex(), 0L, NULL, NULL, ppIMTData ) ) )
 {
        wprintf( L"D3DXComputeIMTFromPerVertexSignal() failed: %s\n", DXGetErrorString( hr ) );
        goto FAIL;
    }

  FAIL:
    if( pfVertexData ) {
        HRESULT hr2;
        if( FAILED( hr2 = pMesh->UnlockVertexBuffer() ) )
 {
            wprintf( L"ID3DXMesh::UnlockVertexBuffer() failed: %s!\n", DXGetErrorString( hr2 ) );
        }
        pfVertexData = NULL;
    }
    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT TextureSignalIMT( IDirect3DDevice9* pDevice, ID3DXMesh* pMesh, DWORD dwTextureIndex, const WCHAR* szFilename,
                          ID3DXBuffer** ppIMTData )
{
    HRESULT hr;
    IDirect3DTexture9* pTexture = NULL;

    if( !pMesh || !szFilename || !ppIMTData )
    {
        wprintf( L"Error: some of pMesh, szFilename and ppIMTData are == NULL\n" );
        hr = D3DERR_INVALIDCALL;
        goto FAIL;
    }

    hr = D3DXCreateTextureFromFileEx( pDevice, szFilename, D3DX_FROM_FILE, D3DX_FROM_FILE, 1, 0,
                                      D3DFMT_A32B32G32R32F, D3DPOOL_SCRATCH,
                                      D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &pTexture );
    if( FAILED( hr ) )
    {
        wprintf( L"Error: failed to load %s: %s\n", szFilename, DXGetErrorString( hr ) );
        goto FAIL;
    }

    if( FAILED( hr = D3DXComputeIMTFromTexture( pMesh, pTexture, dwTextureIndex, 0L, NULL, NULL, ppIMTData ) ) )
    {
        wprintf( L"D3DXComputeIMTFromTexture() failed: %s\n", DXGetErrorString( hr ) );
        goto FAIL;
    }

FAIL:
    SAFE_RELEASE( pTexture );
    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT PRTSignalIMT( ID3DXMesh* pMesh, DWORD dwTextureIndex, SETTINGS* pSettings, const UINT cuNumCoeffs,
                      ID3DXBuffer** ppIMTData )
{
    HRESULT hr;
    FLOAT* pPRTSignal = NULL;
    ID3DXPRTBuffer* pPRTBuffer = NULL;
    WCHAR* szFilename;
    if( !pMesh || !pSettings || !ppIMTData )
    {
        wprintf( L"Error: pMesh, pSettings and ppIMTData must not be NULL\n" );
        hr = D3DERR_INVALIDCALL;
        goto FAIL;
    }

    szFilename = pSettings->szIMTInputFilename;
    if( FAILED( hr = D3DXLoadPRTBufferFromFile( szFilename, &pPRTBuffer ) ) )
    {
        wprintf( L"Error: failed to load %s: %s\n", szFilename, DXGetErrorString( hr ) );
        goto FAIL;
    }

    if( !pPRTBuffer->IsTexture() )
    {
        hr = PerVertexPRTIMT( pMesh, pPRTBuffer, cuNumCoeffs, ppIMTData );
        goto DONE;
    }

    const UINT cuDimension = pPRTBuffer->GetNumChannels() * pPRTBuffer->GetNumCoeffs();
    if( FAILED( hr = pPRTBuffer->LockBuffer( 0, pPRTBuffer->GetNumSamples(), &pPRTSignal ) ) )
    {
        wprintf( L"ID3DXPRTBuffer::LockBuffer() failed: %s\n", DXGetErrorString( hr ) );
        goto FAIL;
    }

    if( FAILED( hr = D3DXComputeIMTFromPerTexelSignal( pMesh, dwTextureIndex, pPRTSignal, pPRTBuffer->GetWidth(),
                                                       pPRTBuffer->GetHeight(), cuNumCoeffs, cuDimension, 0L, NULL,
                                                       NULL, ppIMTData ) ) )
    {
        wprintf( L"D3DXComputeIMTFromPerTexelSignal() failed: %s\n", DXGetErrorString( hr ) );
        goto FAIL;
    }

FAIL:
DONE:
    if( pPRTSignal )
    {
        HRESULT hr2;
        if( FAILED( hr2 = pPRTBuffer->UnlockBuffer() ) )
        {
            wprintf( L"ID3DXPRTBuffer::UnlockVertexBuffer() failed: %s!\n", DXGetErrorString( hr2 ) );
        }
        pPRTSignal = NULL;
    }
    SAFE_RELEASE( pPRTBuffer );
    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CopyUVs( ID3DXMesh* pMeshFrom, D3DDECLUSAGE FromUsage, UINT FromIndex,
                 ID3DXMesh* pMeshTo, D3DDECLUSAGE ToUsage, UINT ToIndex,
                 LPD3DXBUFFER pVertexRemapArray )
{
    HRESULT hr;
    D3DVERTEXELEMENT9 declFrom[MAX_FVF_DECL_SIZE];
    D3DVERTEXELEMENT9 declTo[MAX_FVF_DECL_SIZE];
    CD3DXCrackDecl declCrackFrom;
    CD3DXCrackDecl declCrackTo;
    IDirect3DVertexBuffer9* pVBFrom = NULL;
    char* pVBDataFrom = NULL;
    IDirect3DVertexBuffer9* pVBTo = NULL;
    char* pVBDataTo = NULL;
    DWORD* pdwVertexMapping = NULL;
    if( pVertexRemapArray )
        pdwVertexMapping = ( DWORD* )pVertexRemapArray->GetBufferPointer();

    V_RETURN( pMeshFrom->GetDeclaration( declFrom ) );
    V_RETURN( declCrackFrom.SetDeclaration( declFrom ) );

    V_RETURN( pMeshTo->GetDeclaration( declTo ) );
    V_RETURN( declCrackTo.SetDeclaration( declTo ) );

    V_RETURN( pMeshFrom->GetVertexBuffer( &pVBFrom ) );
    V_RETURN( pVBFrom->Lock( 0, 0, ( void** )&pVBDataFrom, D3DLOCK_READONLY ) );
    const DWORD NBPVFrom = pMeshFrom->GetNumBytesPerVertex();
    declCrackFrom.SetStreamSource( 0, ( void* )pVBDataFrom, NBPVFrom );

    V_RETURN( pMeshTo->GetVertexBuffer( &pVBTo ) );
    V_RETURN( pVBTo->Lock( 0, 0, ( void** )&pVBDataTo, 0 ) );
    const unsigned int uNumVertsTo = pMeshTo->GetNumVertices();
    const DWORD NBPVTo = pMeshTo->GetNumBytesPerVertex();
    declCrackTo.SetStreamSource( 0, ( void* )pVBDataTo, NBPVTo );

    UINT viFrom;
    D3DXVECTOR2 uvFrom;
    for( UINT viTo = 0; viTo < uNumVertsTo; viTo++ )
    {
        // Use the vertex remap if supplied
        if( pdwVertexMapping )
            viFrom = pdwVertexMapping[viTo];
        else
            viFrom = viTo;

        declCrackFrom.DecodeSemantic( FromUsage, FromIndex, viFrom, ( float* )&uvFrom, 2 );
        declCrackTo.EncodeSemantic( ToUsage, ToIndex, viTo, ( float* )&uvFrom, 2 );
    }

    pVBFrom->Unlock();
    SAFE_RELEASE( pVBFrom );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT ProcessFile( IDirect3DDevice9* pd3dDevice, WCHAR* strFile, SETTINGS* pSettings )
{
    HRESULT hr = S_OK;
    ID3DXMesh* pOrigMesh = NULL, *pMesh = NULL, *pMeshValid = NULL, *pMeshResult = NULL, *pTexResampleMesh = NULL;
    LPD3DXBUFFER pMaterials = NULL, pEffectInstances = NULL, pFacePartitioning = NULL, pVertexRemapArray = NULL;
    LPD3DXBUFFER pIMTBuffer = NULL, pOrigAdj = NULL, pFalseEdges = NULL;
    LPD3DXTEXTUREGUTTERHELPER pGutterHelper = NULL;
    LPDIRECT3DTEXTURE9 pOriginalTex = NULL, pResampledTex = NULL;
    FLOAT* pIMTArray = NULL;
    DWORD* pAdjacency = NULL, *pdwAttributeOut = NULL;
    D3DVERTEXELEMENT9 declResamp[MAX_FVF_DECL_SIZE];
    D3DVERTEXELEMENT9 decl[MAX_FVF_DECL_SIZE];
    UINT uLen;
    DWORD dwNumMaterials, dwNumVerts, dwNumFaces;
    FLOAT stretchOut;
    UINT numchartsOut = 0;
    WCHAR* strResult = NULL, *strResultTexture = NULL, *szResampledTextureFile = NULL;
    LPSTR szResampleTextureFileA = NULL, szResampledTextureFileA = NULL;
    CD3DXCrackDecl declCrack;
    D3DVERTEXELEMENT9 declElem;
    VOID* pVertexData = NULL;
    D3DXMATERIAL pGeneratedMaterials[COLOR_COUNT];

    // add _result before the last period in the file name
    size_t ustrLen, uPeriodPos;

    wprintf( L"Processing file %s\n", strFile, strResult );

    ustrLen = lstrlen(strFile);

    ustrLen /= sizeof( WCHAR );
    strResult = new WCHAR[ustrLen + 8];
    strResultTexture = new WCHAR[ustrLen + 9];
    uPeriodPos = ustrLen;
    for( UINT i = 0; i < ustrLen; i++ )
        if( strFile[i] == L'.' )
            uPeriodPos = i;

    if( FAILED( hr = wcsncpy_s( strResult, ( ustrLen + 8 ) * sizeof( WCHAR ), strFile,
                                    uPeriodPos * sizeof( WCHAR ) ) ) )
    {
        wprintf( L"Unable to cat original string.\n" );
        goto FAIL;
    }

    if( FAILED( hr = wcsncat_s( strResult + uPeriodPos, ( ustrLen + 8 - uPeriodPos ) * sizeof( WCHAR ), L"_result",
                                   7 * sizeof( WCHAR ) ) ) )
    {
        wprintf( L"Unable to append _result to original string.\n" );
        goto FAIL;
    }

    if( FAILED( hr = wcsncat_s( strResult + uPeriodPos + 7, ( ustrLen + 8 - uPeriodPos - 7 ) * sizeof( WCHAR ),
                                   strFile + uPeriodPos, ( ustrLen - uPeriodPos ) * sizeof( WCHAR ) ) ) )
    {
        wprintf( L"Unable to append rest of original string.\n" );
        goto FAIL;
    }

    if( FAILED( hr = wcsncpy_s( strResultTexture, ( ustrLen + 9 ) * sizeof( WCHAR ), strFile,
                                    uPeriodPos * sizeof( WCHAR ) ) ) )
    {
        wprintf( L"Unable to cat original string.\n" );
        goto FAIL;
    }

    if( FAILED( hr = wcsncat_s( strResultTexture + uPeriodPos, ( ustrLen + 9 - uPeriodPos ) * sizeof( WCHAR ),
                                   L"_texture", 8 * sizeof( WCHAR ) ) ) )
    {
        wprintf( L"Unable to append _texture to original string.\n" );
        goto FAIL;
    }

    if( FAILED( hr = wcsncat_s( strResultTexture + uPeriodPos + 8,
                                   ( ustrLen + 9 - uPeriodPos - 8 ) * sizeof( WCHAR ), strFile + uPeriodPos,
                                   ( ustrLen - uPeriodPos ) * sizeof( WCHAR ) ) ) )
    {
        wprintf( L"Unable to append rest of original string.\n" );
        goto FAIL;
    }

    if( FAILED( hr = D3DXLoadMeshFromXW( strFile,
                                         D3DXMESH_32BIT | D3DXMESH_SYSTEMMEM,
                                         pd3dDevice,
                                         &pOrigAdj,
                                         &pMaterials,
                                         &pEffectInstances,
                                         &dwNumMaterials,
                                         &pOrigMesh ) ) )
    {
        wprintf( L"Unable to open mesh\n" );
        goto FAIL;
    }

    if( FAILED( hr = pOrigMesh->GetDeclaration( decl ) ) )
        goto FAIL;
    uLen = D3DXGetDeclLength( decl );
    const D3DVERTEXELEMENT9* pDeclElement = GetDeclElement( decl, D3DDECLUSAGE_TEXCOORD,
                                                            pSettings->nOutputTextureIndex );

    if( pDeclElement && declCrack.GetFields( pDeclElement ) < 2 )
    {
        wprintf( L"D3DDECLUSAGE_TEXCOORD[%d] must have at least 2 components. Use /uvi to change the index\n",
                 pSettings->nOutputTextureIndex );
        hr = E_FAIL;
        goto FAIL;
    }

    if( NULL == pDeclElement )
    {
        wprintf( L"Adding texture coordinate slot to vertex decl.\n" );
        if( uLen == MAX_FVF_DECL_SIZE )
        {
            wprintf( L"Not enough room to store texture coordinates in mesh\n" );
            hr = E_FAIL;
            goto FAIL;
        }

        declElem.Stream = 0;
        declElem.Type = D3DDECLTYPE_FLOAT2;
        declElem.Method = D3DDECLMETHOD_DEFAULT;
        declElem.Usage = D3DDECLUSAGE_TEXCOORD;
        declElem.UsageIndex = pSettings->nOutputTextureIndex;

        AppendDeclElement( &declElem, decl );
    }
    if( FAILED( hr = declCrack.SetDeclaration( decl ) ) )
        goto FAIL;

    if( FAILED( hr = pOrigMesh->CloneMesh( D3DXMESH_32BIT | D3DXMESH_SYSTEMMEM,
                                           decl,
                                           pd3dDevice,
                                           &pMesh ) ) )
    {
        wprintf( L"Unable to clone mesh.\n" );
        goto FAIL;
    }

    if( pSettings->bFileAdjacency )
    {
        wprintf( L"Loading adjacency from file %s\n", pSettings->szAdjacencyFilename );
        SAFE_RELEASE( pOrigAdj );

        if( FAILED( hr = LoadFile( pSettings->szAdjacencyFilename, &pOrigAdj ) ) )
        {
            wprintf( L"Unable to load adjacency from file: %s!\n", pSettings->szAdjacencyFilename );
            goto FAIL;
        }

        const DWORD cdwAdjacencySize = 3 * pMesh->GetNumFaces() * sizeof( DWORD );
        if( cdwAdjacencySize != pOrigAdj->GetBufferSize() )
        {
            wprintf( L"Adjacency from file: %s is incorrect size: %d. Expected: %d!\n",
                     pSettings->szAdjacencyFilename,
                     pOrigAdj->GetBufferSize(), cdwAdjacencySize );
            goto FAIL;
        }
    }

    if( FALSE == CheckMeshValidation( pMesh, &pMeshValid, &pAdjacency, pSettings->bTopologicalAdjacency,
                                      pSettings->bGeometricAdjacency, pOrigAdj ) )
    {
        wprintf( L"Unable to clean mesh\n" );
        goto FAIL;
    }

    wprintf( L"Face count: %d\n", pMesh->GetNumFaces() );
    wprintf( L"Vertex count: %d\n", pMesh->GetNumVertices() );
    if( pSettings->maxcharts != 0 )
        wprintf( L"Max charts: %d\n", pSettings->maxcharts );
    else
        wprintf( L"Max charts: Atlas will be parameterized based solely on stretch\n" );
    wprintf( L"Max stretch: %f\n", pSettings->maxstretch );
    wprintf( L"Texture size: %d x %d\n", pSettings->width, pSettings->height );
    if( pSettings->QualityFlag == D3DXUVATLAS_DEFAULT )
        wprintf( L"Quality: D3DXUVATLAS_DEFAULT\n" );
    else if( pSettings->QualityFlag == D3DXUVATLAS_GEODESIC_FAST )
        wprintf( L"Quality: D3DXUVATLAS_GEODESIC_FAST\n" );
    else if( pSettings->QualityFlag == D3DXUVATLAS_GEODESIC_QUALITY )
        wprintf( L"Quality: D3DXUVATLAS_GEODESIC_QUALITY\n" );
    wprintf( L"Gutter size: %f texels\n", pSettings->gutter );
    wprintf( L"Updating UVs in mesh's D3DDECLUSAGE_TEXCOORD[%d]\n", pSettings->nOutputTextureIndex );

    if( pSettings->bIMT )
    {
        if( pSettings->bTextureSignal )
        {
            wprintf( L"Computing IMT from file %s\n", pSettings->szIMTInputFilename );
            hr = TextureSignalIMT( pd3dDevice, pMesh, 0, pSettings->szIMTInputFilename, &pIMTBuffer );
        }
        else if( pSettings->bPRTSignal )
        {
            wprintf( L"Computing IMT from file %s\n", pSettings->szIMTInputFilename );
            hr = PRTSignalIMT( pMesh, pSettings->nIMTInputTextureIndex, pSettings, 3, &pIMTBuffer );
        }
        else if( pSettings->bVertexSignal )
        {
            wprintf( L"Computing IMT from %s, Index %d\n", declCrack.DeclUsageToString( pSettings->VertexSignalUsage ),
                     pSettings->VertexSignalIndex );
            hr = PerVertexIMT( pMesh, decl, pSettings->VertexSignalUsage,
                               pSettings->VertexSignalIndex, &declCrack, &pIMTBuffer );
        }
        else
        {
            hr = E_FAIL;
            assert( false );
        }
        if( FAILED( hr ) )
        {
            wprintf( L"warn: IMT computation failed: %s\n", DXGetErrorString( hr ) );
            wprintf( L"warn: proceeding w/out IMT...\n" );
        }
        else
        {
            pIMTArray = ( FLOAT* )pIMTBuffer->GetBufferPointer();
        }
    }

    if( pSettings->bFalseEdges )
    {
        wprintf( L"Loading false edges from file %s\n", pSettings->szFalseEdgesFilename );

        if( FAILED( hr = LoadFile( pSettings->szFalseEdgesFilename, &pFalseEdges ) ) )
        {
            wprintf( L"Unable to load false edges from file: %s!\n", pSettings->szFalseEdgesFilename );
            goto FAIL;
        }
        const DWORD cdwFalseEdgesSize = 3 * pMeshValid->GetNumFaces() * sizeof( DWORD );
        if( cdwFalseEdgesSize != pFalseEdges->GetBufferSize() )
        {
            wprintf( L"False edges from file: %s is incorrect size: %d. Expected: %d!\n",
                     pSettings->szFalseEdgesFilename,
                     pFalseEdges->GetBufferSize(), cdwFalseEdgesSize );
            goto FAIL;
        }
    }

    wprintf( L"Executing D3DXUVAtlasCreate() on mesh...\n" );

    if( FAILED( hr = D3DXUVAtlasCreate( pMeshValid,
                                        pSettings->maxcharts,
                                        pSettings->maxstretch,
                                        pSettings->width,
                                        pSettings->height,
                                        pSettings->gutter,
                                        pSettings->nOutputTextureIndex,
                                        pAdjacency,
                                        pFalseEdges ? ( DWORD* )pFalseEdges->GetBufferPointer() : NULL,
                                        pIMTArray,
                                        UVAtlasCallback,
                                        0.0001f,
                                        NULL,
                                        pSettings->QualityFlag,
                                        &pMeshResult,
                                        &pFacePartitioning,
                                        &pVertexRemapArray,
                                        &stretchOut,
                                        &numchartsOut ) ) )
    {
        wprintf( L"UV Atlas creation failed: " );
        switch( hr )
        {
            case D3DXERR_INVALIDMESH:
                wprintf( L"Non-manifold mesh\n" ); break;
            default:
                if( numchartsOut != 0 && pSettings->maxcharts < ( int )numchartsOut )
                    wprintf( L"Minimum number of charts is %d\n", numchartsOut );
                wprintf( L"Error code %s, check debug output for more detail\n", DXGetErrorString( hr ) );
                wprintf( L"Try increasing the max number of charts or max stretch\n" );
                break;
        }
        goto FAIL;
    }

    wprintf( L"D3DXUVAtlasCreate() succeeded\n" );
    wprintf( L"Output # of charts: %d\n", numchartsOut );
    wprintf( L"Output stretch: %f\n", stretchOut );

    if( pSettings->bResampleTexture )
    {
        D3DXIMAGE_INFO TextureInfo;
        wprintf( L"Resampling texture %s using data from %s[%d]\n", pSettings->szResampleTextureFile,
                 TraceD3DDECLUSAGEtoString( ( BYTE )pSettings->ResampleTextureUsage ), pSettings->ResampleTextureUsageIndex );

        // Read the original texture from the file
        if( FAILED( hr = D3DXCreateTextureFromFileEx( pd3dDevice, pSettings->szResampleTextureFile,
                                                      D3DX_FROM_FILE, D3DX_FROM_FILE, 1, 0, D3DFMT_FROM_FILE,
                                                      D3DPOOL_SCRATCH,
                                                      D3DX_DEFAULT, D3DX_DEFAULT, 0, &TextureInfo,
                                                      NULL, &pOriginalTex ) ) )
        {
            wprintf( L"Texture creation and loading failed: " );
            switch( hr )
            {
                case D3DERR_NOTAVAILABLE:
                    wprintf( L"This device does not support the queried technique. \n" ); break;
                case D3DERR_OUTOFVIDEOMEMORY:
                    wprintf( L"Microsoft Direct3D does not have enough display memory to perform the operation.\n" );
                    break;
                case D3DERR_INVALIDCALL:
                    wprintf(
                        L"The method call is invalid. For example, a method's parameter may have an invalid value.\n"
                        ); break;
                case D3DXERR_INVALIDDATA:
                    wprintf( L"The data is invalid.\n" ); break;
                case E_OUTOFMEMORY:
                    wprintf( L"Out of memory\n" ); break;
                default:
                    wprintf( L"Error code %s, check debug output for more detail\n", DXGetErrorString( hr ) );
                    break;
            }
            goto FAIL;
        }

        // Create a new blank texture that is the same size
        if( FAILED( hr = D3DXCreateTexture( pd3dDevice, TextureInfo.Width, TextureInfo.Height, 1, 0,
                                            TextureInfo.Format, D3DPOOL_SCRATCH, &pResampledTex ) ) )
        {
            wprintf( L"Texture creation failed: " );
            switch( hr )
            {
                case D3DERR_INVALIDCALL:
                    wprintf(
                        L"The method call is invalid. For example, a method's parameter may have an invalid value.\n"
                        ); break;
                case D3DERR_NOTAVAILABLE:
                    wprintf( L"This device does not support the queried technique. \n" ); break;
                case D3DERR_OUTOFVIDEOMEMORY:
                    wprintf( L"Microsoft Direct3D does not have enough display memory to perform the operation.\n" );
                    break;
                case E_OUTOFMEMORY:
                    wprintf( L"Out of memory\n" ); break;
                default:
                    wprintf( L"Error code %s, check debug output for more detail\n", DXGetErrorString( hr ) );
                    break;
            }
            goto FAIL;
        }

        // Get the decl of the original mesh 
        if( FAILED( hr = pMeshResult->GetDeclaration( declResamp ) ) )
            goto FAIL;
        uLen = D3DXGetDeclLength( declResamp );

        // Ensure the decl has a D3DDECLUSAGE_TEXCOORD:0
        if( NULL == GetDeclElement( declResamp, D3DDECLUSAGE_TEXCOORD, 0 ) )
        {
            if( uLen == MAX_FVF_DECL_SIZE )
            {
                wprintf( L"Not enough room to store texture coordinates in mesh\n" );
                hr = E_FAIL;
                goto FAIL;
            }

            declElem.Stream = 0;
            declElem.Type = D3DDECLTYPE_FLOAT2;
            declElem.Method = D3DDECLMETHOD_DEFAULT;
            declElem.Usage = D3DDECLUSAGE_TEXCOORD;
            declElem.UsageIndex = 0;

            AppendDeclElement( &declElem, declResamp );
        }

        // Ensure the decl has a D3DDECLUSAGE_TEXCOORD:1
        if( NULL == GetDeclElement( declResamp, D3DDECLUSAGE_TEXCOORD, 1 ) )
        {
            if( uLen == MAX_FVF_DECL_SIZE )
            {
                wprintf( L"Not enough room to store texture coordinates in mesh\n" );
                hr = E_FAIL;
                goto FAIL;
            }

            declElem.Stream = 0;
            declElem.Type = D3DDECLTYPE_FLOAT2;
            declElem.Method = D3DDECLMETHOD_DEFAULT;
            declElem.Usage = D3DDECLUSAGE_TEXCOORD;
            declElem.UsageIndex = 1;

            AppendDeclElement( &declElem, declResamp );
        }

        // Clone the original mesh to ensure it has 2 D3DDECLUSAGE_TEXCOORD slots
        if( FAILED( hr = pMeshResult->CloneMesh( D3DXMESH_32BIT | D3DXMESH_SYSTEMMEM,
                                                 declResamp,
                                                 pd3dDevice,
                                                 &pTexResampleMesh ) ) )
        {
            wprintf( L"Unable to clone mesh.\n" );
            goto FAIL;
        }

        // Put new UVAtlas parameterization in D3DDECLUSAGE_TEXCOORD:0
        CopyUVs( pMeshResult, D3DDECLUSAGE_TEXCOORD, pSettings->nOutputTextureIndex,  // from
                 pTexResampleMesh, D3DDECLUSAGE_TEXCOORD, 0, NULL );  // to

        // Put original texture parameterization to D3DDECLUSAGE_TEXCOORD:1
        CopyUVs( pOrigMesh, pSettings->ResampleTextureUsage, pSettings->ResampleTextureUsageIndex, // from
                 pTexResampleMesh, D3DDECLUSAGE_TEXCOORD, 1, pVertexRemapArray ); // to

        // Create a gutter helper
        if( FAILED( hr = D3DXCreateTextureGutterHelper( TextureInfo.Width, TextureInfo.Height, pTexResampleMesh,
                                                        pSettings->gutter, &pGutterHelper ) ) )
        {
            wprintf( L"Gutter Helper creation failed: " );
            switch( hr )
            {
                case D3DERR_INVALIDCALL:
                    wprintf(
                        L"The method call is invalid. For example, a method's parameter may have an invalid value.\n"
                        ); break;
                case E_OUTOFMEMORY:
                    wprintf( L"Out of memory\n" ); break;
                default:
                    wprintf( L"Error code %s, check debug output for more detail\n", DXGetErrorString( hr ) );
                    break;
            }
            goto FAIL;
        }

        // Call ResampleTex() to convert the texture from the original parameterization in D3DDECLUSAGE_TEXCOORD:1
        // to the new UVAtlas parameterization in D3DDECLUSAGE_TEXCOORD:0
        if( FAILED( hr = pGutterHelper->ResampleTex( pOriginalTex, pTexResampleMesh, D3DDECLUSAGE_TEXCOORD, 1,
                                                     pResampledTex ) ) )
        {
            wprintf( L"Gutter Helper texture resampling failed: " );
            switch( hr )
            {
                case D3DERR_INVALIDCALL:
                    wprintf(
                        L"The method call is invalid. For example, a method's parameter may have an invalid value.\n"
                        ); break;
                case E_OUTOFMEMORY:
                    wprintf( L"Out of memory\n" ); break;
                default:
                    wprintf( L"Error code %s, check debug output for more detail\n", DXGetErrorString( hr ) );
                    break;
            }
            goto FAIL;
        }

        // Create the filepath string
        ustrLen =  lstrlen (pSettings->szResampleTextureFile);

        ustrLen /= sizeof( WCHAR );
        szResampledTextureFile = new WCHAR[ustrLen + 11];
        uPeriodPos = ustrLen;
        for( UINT i = 0; i < ustrLen; i++ )
            if( ( pSettings->szResampleTextureFile )[i] == L'.' )
                uPeriodPos = i;
        if( FAILED( hr = wcsncpy_s( szResampledTextureFile, ( ustrLen + 11 ) * sizeof( WCHAR ),
                                        pSettings->szResampleTextureFile, uPeriodPos * sizeof( WCHAR ) ) ) )
        {
            wprintf( L"Unable to copy original string.\n" );
            goto FAIL;
        }
        if( FAILED( hr = wcsncat_s( szResampledTextureFile + uPeriodPos,
                                       ( ustrLen + 11 - uPeriodPos ) * sizeof( WCHAR ), L"_resampled",
                                       10 * sizeof( WCHAR ) ) ) )
        {
            wprintf( L"Unable to append _resampled to original string.\n" );
            goto FAIL;
        }
        if( FAILED( hr = wcsncat_s( szResampledTextureFile + uPeriodPos + 10,
                                       ( ustrLen + 11 - uPeriodPos - 10 ) * sizeof( WCHAR ),
                                       pSettings->szResampleTextureFile + uPeriodPos,
                                       ( ustrLen - uPeriodPos ) * sizeof( WCHAR ) ) ) )
        {
            wprintf( L"Unable to append rest of original string.\n" );
            goto FAIL;
        }

        wprintf( L"Writing resampled texture to %s\n", szResampledTextureFile );

        // Save the new resampled texture
        if( FAILED( hr = D3DXSaveTextureToFile( szResampledTextureFile, TextureInfo.ImageFileFormat, pResampledTex,
                                                NULL ) ) )
        {
            wprintf( L"Saving texture to file failed: " );
            switch( hr )
            {
                case D3DERR_INVALIDCALL:
                    wprintf(
                        L"The method call is invalid. For example, a method's parameter may have an invalid value.\n"
                        ); break;
                default:
                    wprintf( L"Error code %s, check debug output for more detail\n", DXGetErrorString( hr ) );
                    break;
            }
            goto FAIL;
        }
    }

    if( pSettings->bColorMesh )
    {
        for( UINT i = 0; i < COLOR_COUNT; i++ )
        {
            pGeneratedMaterials[i].MatD3D.Ambient.a = 0;
            pGeneratedMaterials[i].MatD3D.Ambient.r = ColorList[i][0];
            pGeneratedMaterials[i].MatD3D.Ambient.g = ColorList[i][1];
            pGeneratedMaterials[i].MatD3D.Ambient.b = ColorList[i][2];

            pGeneratedMaterials[i].MatD3D.Diffuse = pGeneratedMaterials[i].MatD3D.Ambient;

            pGeneratedMaterials[i].MatD3D.Power = 0;

            pGeneratedMaterials[i].MatD3D.Emissive.a = 0;
            pGeneratedMaterials[i].MatD3D.Emissive.r = 0;
            pGeneratedMaterials[i].MatD3D.Emissive.g = 0;
            pGeneratedMaterials[i].MatD3D.Emissive.b = 0;

            pGeneratedMaterials[i].MatD3D.Specular.a = 0;
            pGeneratedMaterials[i].MatD3D.Specular.r = 0.5;
            pGeneratedMaterials[i].MatD3D.Specular.g = 0.5;
            pGeneratedMaterials[i].MatD3D.Specular.b = 0.5;

            pGeneratedMaterials[i].pTextureFilename = NULL;

        }

        if( FAILED( hr = pMeshResult->LockAttributeBuffer( D3DLOCK_NOSYSLOCK, &pdwAttributeOut ) ) )
        {
            wprintf( L"Unable to lock result attribute buffer.\n" );
            goto FAIL;
        }

        DWORD* pdwChartMapping = ( DWORD* )pFacePartitioning->GetBufferPointer();

        dwNumFaces = pMeshResult->GetNumFaces();

        for( DWORD i = 0; i < dwNumFaces; i++ )
        {
            pdwAttributeOut[i] = pdwChartMapping[i] % COLOR_COUNT;
        }

        pdwAttributeOut = NULL;
        if( FAILED( hr = pMeshResult->UnlockAttributeBuffer() ) )
            goto FAIL;
    }

    WCHAR strOutputFilename[MAX_PATH];
    if( pSettings->bOverwrite )
    {
        wcscpy_s( strOutputFilename, MAX_PATH, strFile );
    }
    else if( pSettings->bOutputFilenameGiven )
    {
        wcscpy_s( strOutputFilename, MAX_PATH, pSettings->szOutputFilename );
    }
    else
    {
        wcscpy_s( strOutputFilename, MAX_PATH, strResult );
    }

    if( pSettings->bResampleTexture && !pSettings->bColorMesh && szResampledTextureFile )
    {
        szResampleTextureFileA = new CHAR[ustrLen + 1];
        szResampledTextureFileA = new CHAR[ustrLen + 12];

        WideCharToMultiByte( CP_ACP, 0, pSettings->szResampleTextureFile, -1,
                             szResampleTextureFileA, ( int )ustrLen * sizeof( CHAR ), NULL, NULL );
        szResampleTextureFileA[ustrLen] = 0;

        WideCharToMultiByte( CP_ACP, 0, szResampledTextureFile, -1, szResampledTextureFileA, ( int )
                             ( ustrLen + 11 ) * sizeof( CHAR ), NULL, NULL );
        szResampledTextureFileA[ustrLen + 11] = 0;

        for( DWORD index = 0; index < dwNumMaterials; index++ )
        {
            if( 0 == strcmp( ( ( D3DXMATERIAL* )( pMaterials->GetBufferPointer() ) + index )->pTextureFilename,
                             szResampleTextureFileA ) )
            {
                ( ( D3DXMATERIAL* )( pMaterials->GetBufferPointer() ) + index )->pTextureFilename =
                    szResampledTextureFileA;
            }
        }
    }

    g_ProcessedFileList.Add( strOutputFilename );
    if( FAILED( hr = D3DXSaveMeshToX( strOutputFilename,
                                      pMeshResult,
                                      NULL,
                                      pSettings->bColorMesh?pGeneratedMaterials:( D3DXMATERIAL* )
                                      pMaterials->GetBufferPointer(),
                                      NULL,
                                      pSettings->bColorMesh?COLOR_COUNT:dwNumMaterials,
                                      D3DXF_FILEFORMAT_TEXT ) ) )
    {
        wprintf( L"Unable to save result mesh.\n" );
        goto FAIL;
    }

    if( pSettings->bOutputTexture )
    {
        if( FAILED( hr = pMeshResult->LockVertexBuffer( D3DLOCK_NOSYSLOCK, &pVertexData ) ) )
        {
            wprintf( L"Unable to lock result vertex buffer.\n" );
            goto FAIL;
        }

        if( FAILED( hr = declCrack.SetStreamSource( 0, pVertexData, 0 ) ) )
            goto FAIL;

        dwNumVerts = pMeshResult->GetNumVertices();

        const D3DVERTEXELEMENT9* pNormal = declCrack.GetSemanticElement( D3DDECLUSAGE_NORMAL, 0 );

        FLOAT normal[3];
        normal[0] = 0;
        normal[1] = 0;
        normal[2] = 1;

        for( DWORD i = 0; i < dwNumVerts; i++ )
        {
            FLOAT texcoords[2];
            declCrack.DecodeSemantic( D3DDECLUSAGE_TEXCOORD, pSettings->nOutputTextureIndex, i, texcoords, 2 );
            declCrack.EncodeSemantic( D3DDECLUSAGE_POSITION, pSettings->nOutputTextureIndex, i, texcoords, 2 );
            if( pNormal )
                declCrack.Encode( pNormal, i, normal, 3 );
        }

        pVertexData = NULL;

        if( FAILED( hr = pMeshResult->UnlockVertexBuffer() ) )
            goto FAIL;

        if( FAILED( hr = D3DXSaveMeshToX( strResultTexture,
                                          pMeshResult,
                                          NULL,
                                          pSettings->bColorMesh?pGeneratedMaterials:( D3DXMATERIAL* )
                                          pMaterials->GetBufferPointer(),
                                          NULL,
                                          pSettings->bColorMesh?COLOR_COUNT:dwNumMaterials,
                                          D3DXF_FILEFORMAT_TEXT ) ) )
        {
            wprintf( L"Unable to save result mesh.\n" );
            goto FAIL;
        }
    }

    wprintf( L"Output mesh with new UV atlas: %s\n", strOutputFilename );
    if( pSettings->bOutputTexture )
        wprintf( L"Output UV space mesh: %s\n", strResultTexture );

FAIL:
    if( FAILED( hr ) )
    {
        wprintf( L"Failure code: %s\n", DXGetErrorString( hr ) );
    }

    if( pVertexData )
    {
        pMeshResult->UnlockVertexBuffer();
        pVertexData = NULL;
    }

    if( pdwAttributeOut )
    {
        pMeshResult->UnlockAttributeBuffer();
        pdwAttributeOut = NULL;
    }

    SAFE_RELEASE( pOrigMesh );
    SAFE_RELEASE( pMesh );
    SAFE_RELEASE( pMeshValid );
    SAFE_RELEASE( pMeshResult );
    SAFE_RELEASE( pTexResampleMesh );
    SAFE_RELEASE( pGutterHelper );
    SAFE_RELEASE( pOriginalTex );
    SAFE_RELEASE( pResampledTex );
    SAFE_RELEASE( pMaterials );
    SAFE_RELEASE( pEffectInstances );
    SAFE_RELEASE( pOrigAdj );
    SAFE_RELEASE( pFalseEdges );
    SAFE_RELEASE( pFacePartitioning );
    SAFE_RELEASE( pVertexRemapArray );
    SAFE_DELETE( strResult );
    SAFE_DELETE( strResultTexture );
    SAFE_DELETE( szResampledTextureFile );
    SAFE_DELETE( szResampleTextureFileA );
    SAFE_DELETE( szResampledTextureFileA );

    if( pSettings->bGeometricAdjacency || pSettings->bTopologicalAdjacency )
        SAFE_DELETE( pAdjacency );
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
void DisplayUsage()
{
    wprintf( L"\n" );
    wprintf( L"UVAtlas - a command line tool for generating UV Atlases\n" );
    wprintf( L"\n" );
    wprintf( L"Usage: UVAtlas.exe [options] [filename1] [filename2] ...\n" );
    wprintf( L"\n" );
    wprintf( L"where:\n" );
    wprintf( L"\n" );
    wprintf( L"  [/n #]\tSpecifies the maximum number of charts to generate\n" );
    wprintf( L"  \t\tDefault is 0 meaning the atlas will be parameterized based\n" );
    wprintf( L"  \t\tsolely on stretch\n" );
    wprintf( L"  [/st #.##]\tSpecifies the maximum amount of stretch, valid range is [0-1]\n" );
    wprintf( L"  \t\tDefault is 0.16667. 0.0 means do not stretch; 1.0 means any\n" );
    wprintf( L"  \t\tamount of stretching is allowed.\n" );
    wprintf( L"  [/g #.##]\tSpecifies the gutter width (default 2).\n" );
    wprintf( L"  [/w #]\tSpecifies the texture width (default 512).\n" );
    wprintf( L"  [/h #]\tSpecifies the texture height (default 512).\n" );
    wprintf( L"  [/uvi #]\tSpecifies the output D3DDECLUSAGE_TEXCOORD index for the\n" );
    wprintf( L"  \t\tUVAtlas data (default 0).\n" );
    wprintf( L"  [/ta]\t\tGenerate topological adjacency, where triangles are marked\n" );
    wprintf( L"  \t\tadjacent if they share edge vertices. Mutually exclusive with\n" );
    wprintf( L"  \t\t/ga & /fa.\n" );
    wprintf( L"  [/ga]\t\tGenerate geometric adjacency, where triangles are marked\n" );
    wprintf( L"  \t\tadjacent if edge vertices are positioned within 1e-5 of each\n" );
    wprintf( L"  \t\tother. Mutually exclusive with /ta & /fa.\n" );
    wprintf( L"  [/fa file]\tLoad adjacency array entries directly into memory from\n" );
    wprintf( L"  \t\ta binary file. Mutually exclusive with /ta & /ga.\n" );
    wprintf( L"  [/fe file]\tLoad \"False Edge\" adjacency array entries directly into\n" );
    wprintf( L"  \t\tmemory from a binary file. A non-false edge is indicated by -1,\n" );
    wprintf( L"  \t\twhile a false edge is indicated by any other value, e.g. 0 or\n" );
    wprintf( L"  \t\tthe original adjacency value. This enables the parameterization\n" );
    wprintf( L"  \t\tof meshes containing quads and higher order n-gons, and the\n" );
    wprintf( L"  \t\tinternal edges of each n-gon will not be cut during the\n" );
    wprintf( L"  \t\tparameterization process.\n" );
    wprintf( L"  [/ip file]\tCalculate the Integrated Metric Tensor (IMT) array for the mesh\n" );
    wprintf( L"  \t\tusing a PRT buffer in file.\n" );
    wprintf( L"  [/it file]\tCalculate the IMT for the mesh using a texture map in file.\n" );
    wprintf( L"  [/iv usage]\tCalculate the IMT for the mesh using a per-vertex data from the\n" );
    wprintf( L"  \t\tmesh. The usage parameter lets you select which part of the\n" );
    wprintf( L"  \t\tmesh to use (default COLOR). It must be one of NORMAL, COLOR,\n" );
    wprintf( L"  \t\tTEXCOORD, TANGENT, or BINORMAL.\n" );
    wprintf( L"  [/t]\t\tCreate a separate mesh in u-v space (appending _texture).\n" );
    wprintf( L"  [/c]\t\tModify the materials of the mesh to graphically show\n" );
    wprintf( L"  \t\twhich chart each triangle is in.\n" );
    wprintf( L"  [/rt file]\tResamples a texture using the new UVAtlas parameterization.\n" );
    wprintf( L"  \t\tThe resampled texture is saved to a filename with \"_resampled\"\n" );
    wprintf( L"  \t\tappended. Defaults to reading old texture parameterization from\n" );
    wprintf( L"  \t\tD3DDECLUSAGE_TEXCOORD[0] in original mesh Use /rtu and /rti to\n" );
    wprintf( L"  \t\toverride this.\n" );
    wprintf( L"  [/rtu usage]\tSpecifies the vertex data usage for texture resampling (default\n" );
    wprintf( L"  \t\tTEXCOORD). It must be one of NORMAL, POSITION, COLOR, TEXCOORD,\n" );
    wprintf( L"  \t\tTANGENT, or BINORMAL.\n" );
    wprintf( L"  [/rti #]\tSpecifies the usage index for texture resampling (default 0).\n" );
    wprintf( L"  [/o file]\tOutput mesh filename.  Defaults to a filename with \"_result\"\n" );
    wprintf( L"  \t\tappended Using this option disables batch processing.\n" );
    wprintf( L"  [/f]\t\tOverwrite original file with output (default off).\n" );
    wprintf( L"  \t\tMutually exclusive with /o.\n" );
    wprintf( L"  [/q usage]\tQuality flag for D3DXUVAtlasCreate. It must be\n" );
    wprintf( L"  \t\teither DEFAULT, FAST, or QUALITY.\n" );
    wprintf( L"  [/s]\t\tSearch sub-directories for files (default off).\n" );
    wprintf( L"  [filename*]\tSpecifies the files to generate atlases for.\n" );
    wprintf( L"  \t\tWildcards and quotes are supported.\n" );
}


//--------------------------------------------------------------------------------------
WCHAR* TraceD3DDECLUSAGEtoString( BYTE u )
{
    switch( u )
    {
        case D3DDECLUSAGE_POSITION:
            return L"D3DDECLUSAGE_POSITION";
        case D3DDECLUSAGE_BLENDWEIGHT:
            return L"D3DDECLUSAGE_BLENDWEIGHT";
        case D3DDECLUSAGE_BLENDINDICES:
            return L"D3DDECLUSAGE_BLENDINDICES";
        case D3DDECLUSAGE_NORMAL:
            return L"D3DDECLUSAGE_NORMAL";
        case D3DDECLUSAGE_PSIZE:
            return L"D3DDECLUSAGE_PSIZE";
        case D3DDECLUSAGE_TEXCOORD:
            return L"D3DDECLUSAGE_TEXCOORD";
        case D3DDECLUSAGE_TANGENT:
            return L"D3DDECLUSAGE_TANGENT";
        case D3DDECLUSAGE_BINORMAL:
            return L"D3DDECLUSAGE_BINORMAL";
        case D3DDECLUSAGE_TESSFACTOR:
            return L"D3DDECLUSAGE_TESSFACTOR";
        case D3DDECLUSAGE_POSITIONT:
            return L"D3DDECLUSAGE_POSITIONT";
        case D3DDECLUSAGE_COLOR:
            return L"D3DDECLUSAGE_COLOR";
        case D3DDECLUSAGE_FOG:
            return L"D3DDECLUSAGE_FOG";
        case D3DDECLUSAGE_DEPTH:
            return L"D3DDECLUSAGE_DEPTH";
        case D3DDECLUSAGE_SAMPLE:
            return L"D3DDECLUSAGE_SAMPLE";
        default:
            return L"D3DDECLUSAGE Unknown";
    }
}


//--------------------------------------------------------------------------------------
HRESULT LoadFile( WCHAR* szFile, ID3DXBuffer** ppBuffer )
{
    HRESULT hr = D3DERR_INVALIDCALL;
    HANDLE hFile = NULL;

    if( !ppBuffer || !szFile )
        goto LCleanup;

    *ppBuffer = NULL;
    hr = E_FAIL;

    if( GetFileAttributes( szFile ) == INVALID_FILE_ATTRIBUTES )
        goto LCleanup;

    hFile = CreateFile( szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( !hFile )
        goto LCleanup;

    const DWORD cdwFileSize = GetFileSize( hFile, NULL );
    if( INVALID_FILE_SIZE == cdwFileSize )
        goto LCleanup;

    if( FAILED( hr = D3DXCreateBuffer( cdwFileSize, ppBuffer ) ) )
        goto LCleanup;

    DWORD dwRead;
    ReadFile( hFile, ( BYTE* )( *ppBuffer )->GetBufferPointer(), cdwFileSize, &dwRead, NULL );

    if( cdwFileSize != dwRead )
    {
        SAFE_RELEASE( *ppBuffer );
        hr = E_FAIL;
        goto LCleanup;
    }

    hr = S_OK;

LCleanup:
    if( hFile ) CloseHandle( hFile );

    return hr;
}

//--------------------------------------------------------------------------------------
// File: PackedFile.cpp
//
// Illustrates streaming content using Direct3D 9/10
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "PackedFile.h"
#include "SDKMisc.h"
#include "Terrain.h"

//--------------------------------------------------------------------------------------
CPackedFile::CPackedFile() : m_pChunks( NULL ),
                             m_pMappedChunks( NULL ),
                             m_pFileIndices( NULL ),
                             m_hFileMapping( 0 ),
                             m_hFile( 0 ),
                             m_MaxChunksMapped( 78 )
{
    ZeroMemory( &m_FileHeader, sizeof( PACKED_FILE_HEADER ) );
}

//--------------------------------------------------------------------------------------
CPackedFile::~CPackedFile()
{
    UnloadPackedFile();
}

//--------------------------------------------------------------------------------------
UINT64 GetSize( char* szFile )
{
    UINT64 Size = 0;

    HANDLE hFile = CreateFileA( szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                NULL );
    if( INVALID_HANDLE_VALUE != hFile )
    {
        LARGE_INTEGER FileSize;
        GetFileSizeEx( hFile, &FileSize );
        Size = FileSize.QuadPart;
        CloseHandle( hFile );
    }

    return Size;
}

//--------------------------------------------------------------------------------------
UINT64 GetSize( WCHAR* szFile )
{
    UINT64 Size = 0;

    HANDLE hFile = CreateFile( szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                               NULL );
    if( INVALID_HANDLE_VALUE != hFile )
    {
        LARGE_INTEGER FileSize;
        GetFileSizeEx( hFile, &FileSize );
        Size = FileSize.QuadPart;
        CloseHandle( hFile );
    }

    return Size;
}

//--------------------------------------------------------------------------------------
// Align the input offset to the specified granularity (see CreatePackedFile below)
//--------------------------------------------------------------------------------------
UINT64 AlignToGranularity( UINT64 Offset, UINT64 Granularity )
{
    UINT64 floor = Offset / Granularity;
    return ( floor + 1 ) * Granularity;
}

//--------------------------------------------------------------------------------------
// Write bytes into the file until the granularity is reached (see CreatePackedFile below)
//--------------------------------------------------------------------------------------
UINT64 FillToGranularity( HANDLE hFile, UINT64 CurrentOffset, UINT64 Granularity )
{
    UINT64 NewOffset = AlignToGranularity( CurrentOffset, Granularity );
    UINT64 NumBytes = NewOffset - CurrentOffset;

    DWORD dwWritten;
    BYTE Zero = 0;
    for( UINT64 i = 0; i < NumBytes; i++ )
    {
        if( !WriteFile( hFile, &Zero, sizeof( BYTE ), &dwWritten, NULL ) )
            return 0;
    }

    return NewOffset;
}

//--------------------------------------------------------------------------------------
// Creates a packed file.  The file is a flat uncompressed file containing all resources
// needed for the sample.  The file consists of chunks of data.  Each chunk represents
// a mappable window that can be accessed by MapViewOfFile.  Since MapViewOfFile can
// only map a view onto a file in 64k granularities, each chunk must start on a 64k
// boundary.  The packed file also creates an index.  This index is loaded into memory
// at startup and is not memory mapped.  The index is used to find the locations of 
// resource files within the packed file.
//--------------------------------------------------------------------------------------
struct STRING
{
    WCHAR str[MAX_PATH];
};
bool CPackedFile::CreatePackedFile( ID3D10Device* pDev10, IDirect3DDevice9* pDev9, WCHAR* szFileName,
                                    UINT SqrtNumTiles, UINT SidesPerTile, float fWorldScale, float fHeightScale )
{
    bool bRet = false;
    HANDLE hFile;

    m_FileHeader.NumFiles = 4 * SqrtNumTiles * SqrtNumTiles;

    CGrowableArray <FILE_INDEX*> TempFileIndices;
    CGrowableArray <CHUNK_HEADER*> TempHeaderList;
    CGrowableArray <STRING> FullFilePath;

    STRING strDiffuseTexture;
    STRING strNormalTexture;
    if( FAILED( DXUTFindDXSDKMediaFileCch( strDiffuseTexture.str, MAX_PATH,
                                           L"contentstreaming\\2kPanels_Diff.dds" ) ) )
        return false;
    if( FAILED( DXUTFindDXSDKMediaFileCch( strNormalTexture.str, MAX_PATH, L"contentstreaming\\2kPanels_Norm.dds" ) ) )
        return false;

    UINT64 SizeDiffuse = GetSize( strDiffuseTexture.str );
    UINT64 SizeNormal = GetSize( strNormalTexture.str );

    STRING strTerrainHeight;
    if( FAILED( DXUTFindDXSDKMediaFileCch( strTerrainHeight.str, MAX_PATH, L"contentstreaming\\terrain1.bmp" ) ) )
        return false;
    CTerrain Terrain;
    HRESULT hr = Terrain.LoadTerrain( strTerrainHeight.str, SqrtNumTiles, SidesPerTile, fWorldScale, fHeightScale,
                                      true );
    if( FAILED( hr ) )
        return false;

    TERRAIN_TILE* pTile = Terrain.GetTile( 0 );
    UINT64 SizeTerrainVB = pTile->NumVertices * sizeof( TERRAIN_VERTEX );
    UINT64 SizeTerrainIB = Terrain.GetNumIndices() * sizeof( SHORT );

    float fTileWidth = pTile->BBox.max.x - pTile->BBox.min.x;
    float fChunkSpan = sqrtf( ( float )m_MaxChunksMapped ) - 1;
    UINT64 TotalTerrainTileSize = SizeTerrainVB + SizeTerrainIB + SizeDiffuse + SizeNormal;
    UINT64 LastChunkMegs = 0;
    UINT64 ChunkMegs = TotalTerrainTileSize;
    UINT iSqrt = 1;
    UINT64 SizeTry = 512;
    UINT64 VideoMemoryUsage = 0;
    UINT64 PrevVideoMemoryUsage = 0;
    UINT64 VidMemLimit = 512 * 1024 * 1024;
    UINT PrevMaxLoadedTiles = 0;
    UINT MaxLoadedTiles = 0;
    float fLoadingRadius = 0;
    float fPrevLoadingRadius = 0;
    while( VideoMemoryUsage < VidMemLimit )
    {
        LastChunkMegs = ChunkMegs;
        ChunkMegs = TotalTerrainTileSize;

        fPrevLoadingRadius = fLoadingRadius;
        fLoadingRadius = iSqrt * fTileWidth * ( fChunkSpan / 2.0f );
        float fLoadingArea = D3DX_PI * fLoadingRadius * fLoadingRadius;
        float fTileArea = fTileWidth * fTileWidth;
        PrevMaxLoadedTiles = MaxLoadedTiles;
        MaxLoadedTiles = ( UINT )floorf( fLoadingArea / fTileArea );
        PrevVideoMemoryUsage = VideoMemoryUsage;
        VideoMemoryUsage = ( UINT64 )( MaxLoadedTiles * TotalTerrainTileSize );

        iSqrt ++;
        SizeTry += 32;
    }
    iSqrt --;
    m_MaxChunksMapped = PrevMaxLoadedTiles + 20;
    ChunkMegs = LastChunkMegs;
    VideoMemoryUsage = PrevVideoMemoryUsage;
    fLoadingRadius = fPrevLoadingRadius;

    // Create Chunks
    UINT ChunkSide = SqrtNumTiles;
    int NumChunks = ChunkSide * ChunkSide;

    for( int i = 0; i < NumChunks; i++ )
    {	
        CHUNK_HEADER* pChunkHeader = new CHUNK_HEADER;
        TempHeaderList.Add( pChunkHeader );
    }

    // Create indices
    UINT iTile = 0;
    for( UINT y = 0; y < SqrtNumTiles; y++ )
    {
        for( UINT x = 0; x < SqrtNumTiles; x++ )
        {
            UINT ChunkX = x;
            UINT ChunkY = y;
            UINT ChunkIndex = ChunkY * ChunkSide + ChunkX;

            // Tile
            TERRAIN_TILE* pTile = Terrain.GetTile( iTile );
            D3DXVECTOR3 vCenter = ( pTile->BBox.min + pTile->BBox.max ) / 2.0f;

            // TerrainVB
            FILE_INDEX* pFileIndex = new FILE_INDEX;
            swprintf_s( pFileIndex->szFileName, MAX_PATH, L"terrainVB%d_%d", x, y );
            pFileIndex->FileSize = SizeTerrainVB;
            pFileIndex->ChunkIndex = ChunkIndex;
            pFileIndex->OffsetIntoChunk = 0; // unknown
            pFileIndex->vCenter = vCenter;
            TempFileIndices.Add( pFileIndex );

            STRING strTemp;
            wcscpy_s( strTemp.str, MAX_PATH, L"VB" );
            FullFilePath.Add( strTemp );

            // TerrainIB
            pFileIndex = new FILE_INDEX;
            swprintf_s( pFileIndex->szFileName, MAX_PATH, L"terrainIB%d_%d", x, y );
            pFileIndex->FileSize = SizeTerrainIB;
            pFileIndex->ChunkIndex = ChunkIndex;
            pFileIndex->OffsetIntoChunk = 0; // unknown
            pFileIndex->vCenter = vCenter;
            TempFileIndices.Add( pFileIndex );

            wcscpy_s( strTemp.str, MAX_PATH, L"IB" );
            FullFilePath.Add( strTemp );

            // TerrainDiffuse
            pFileIndex = new FILE_INDEX;
            swprintf_s( pFileIndex->szFileName, MAX_PATH, L"terrainDiff%d_%d", x, y );
            pFileIndex->FileSize = SizeDiffuse;
            pFileIndex->ChunkIndex = ChunkIndex;
            pFileIndex->OffsetIntoChunk = 0; // unknown
            pFileIndex->vCenter = vCenter;
            TempFileIndices.Add( pFileIndex );

            FullFilePath.Add( strDiffuseTexture );

            // TerrainDiffuse
            pFileIndex = new FILE_INDEX;
            swprintf_s( pFileIndex->szFileName, MAX_PATH, L"terrainNorm%d_%d", x, y );
            pFileIndex->FileSize = SizeNormal;
            pFileIndex->ChunkIndex = ChunkIndex;
            pFileIndex->OffsetIntoChunk = 0; // unknown
            pFileIndex->vCenter = vCenter;
            TempFileIndices.Add( pFileIndex );

            FullFilePath.Add( strNormalTexture );

            iTile++;
        }
    }

    // Get granularity
    SYSTEM_INFO SystemInfo;
    GetSystemInfo( &SystemInfo );
    UINT64 Granularity = SystemInfo.dwAllocationGranularity; // Allocation granularity (always 64k)

    // Calculate offsets into chunks
    for( int c = 0; c < NumChunks; c++ )
    {
        CHUNK_HEADER* pChunkHeader = TempHeaderList.GetAt( c );
        pChunkHeader->ChunkSize = 0;

        for( int i = 0; i < TempFileIndices.GetSize(); i++ )
        {
            FILE_INDEX* pIndex = TempFileIndices.GetAt( i );

            if( pIndex->ChunkIndex == c )
            {
                pIndex->OffsetIntoChunk = pChunkHeader->ChunkSize;
                pChunkHeader->ChunkSize += pIndex->FileSize;
            }
        }
    }

    UINT64 IndexSize = sizeof( PACKED_FILE_HEADER ) + sizeof( CHUNK_HEADER ) * TempHeaderList.GetSize() + sizeof
        ( FILE_INDEX ) * TempFileIndices.GetSize();
    UINT64 ChunkOffset = AlignToGranularity( IndexSize, Granularity );

    // Align chunks to the proper granularities
    for( int c = 0; c < NumChunks; c++ )
    {
        CHUNK_HEADER* pChunkHeader = TempHeaderList.GetAt( c );
        pChunkHeader->ChunkOffset = ChunkOffset;

        ChunkOffset += AlignToGranularity( pChunkHeader->ChunkSize, Granularity );
    }

    // Fill in the header data
    m_FileHeader.FileSize = ChunkOffset;
    m_FileHeader.NumChunks = TempHeaderList.GetSize();
    m_FileHeader.NumFiles = TempFileIndices.GetSize();
    m_FileHeader.Granularity = Granularity;
    m_FileHeader.MaxChunksInVA = m_MaxChunksMapped;

    m_FileHeader.TileBytesSize = TotalTerrainTileSize;
    m_FileHeader.TileSideSize = pTile->BBox.max.x - pTile->BBox.min.x;
    m_FileHeader.LoadingRadius = fLoadingRadius;
    m_FileHeader.VideoMemoryUsageAtFullMips = VideoMemoryUsage;

    // Open the file
    hFile = CreateFile( szFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN,
                        NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return bRet;

    // write the header
    DWORD dwWritten;
    DWORD dwRead;
    if( !WriteFile( hFile, &m_FileHeader, sizeof( PACKED_FILE_HEADER ), &dwWritten, NULL ) )
        goto Error;

    // write out chunk headers
    for( int i = 0; i < TempHeaderList.GetSize(); i++ )
    {
        CHUNK_HEADER* pChunkHeader = TempHeaderList.GetAt( i );
        if( !WriteFile( hFile, pChunkHeader, sizeof( CHUNK_HEADER ), &dwWritten, NULL ) )
            goto Error;
    }

    // write the index
    for( int i = 0; i < TempFileIndices.GetSize(); i++ )
    {
        FILE_INDEX* pIndex = TempFileIndices.GetAt( i );
        if( !WriteFile( hFile, pIndex, sizeof( FILE_INDEX ), &dwWritten, NULL ) )
            goto Error;
    }

    // Fill in up to the granularity
    UINT64 CurrentFileSize = IndexSize;
    CurrentFileSize = FillToGranularity( hFile, CurrentFileSize, Granularity );
    if( 0 == CurrentFileSize )
        goto Error;

    // Write out the files
    for( int c = 0; c < TempHeaderList.GetSize(); c++ )
    {
        for( int i = 0; i < TempFileIndices.GetSize(); i++ )
        {
            FILE_INDEX* pIndex = TempFileIndices.GetAt( i );

            if( pIndex->ChunkIndex == c )
            {
                // Write out the indexed file
                bool bDel = false;
                BYTE* pTempData = NULL;

                if( 0 == wcscmp( FullFilePath.GetAt( i ).str, L"VB" ) )
                {
                    pTempData = ( BYTE* )Terrain.GetTile( i / 4 )->pRawVertices;
                }
                else if( 0 == wcscmp( FullFilePath.GetAt( i ).str, L"IB" ) )
                {
                    pTempData = ( BYTE* )Terrain.GetIndices();
                }
                else
                {
                    HANDLE hIndexFile = CreateFile( FullFilePath.GetAt( i ).str, FILE_READ_DATA, FILE_SHARE_READ, NULL,
                                                    OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
                    if( INVALID_HANDLE_VALUE == hIndexFile )
                        goto Error;

                    pTempData = new BYTE[ ( SIZE_T )pIndex->FileSize ];
                    if( !pTempData )
                    {
                        CloseHandle( hIndexFile );
                        goto Error;
                    }

                    bDel = true;

                    if( !ReadFile( hIndexFile, pTempData, ( DWORD )pIndex->FileSize, &dwRead, NULL ) )
                    {
                        CloseHandle( hIndexFile );
                        SAFE_DELETE_ARRAY( pTempData );
                        goto Error;
                    }

                    CloseHandle( hIndexFile );
                }

                if( !WriteFile( hFile, pTempData, ( DWORD )pIndex->FileSize, &dwWritten, NULL ) )
                {
                    if( bDel )
                        SAFE_DELETE_ARRAY( pTempData );
                    goto Error;
                }

                if( bDel )
                    SAFE_DELETE_ARRAY( pTempData );

                CurrentFileSize += pIndex->FileSize;
            }
        }

        // Fill in up to the granularity
        CurrentFileSize = FillToGranularity( hFile, CurrentFileSize, Granularity );
        if( 0 == CurrentFileSize )
            goto Error;
    }

    bRet = true;
Error:

    for( int i = 0; i < TempFileIndices.GetSize(); i++ )
    {
        FILE_INDEX* pIndex = TempFileIndices.GetAt( i );
        SAFE_DELETE( pIndex );
    }

    for( int i = 0; i < TempHeaderList.GetSize(); i++ )
    {
        CHUNK_HEADER* pChunkHeader = TempHeaderList.GetAt( i );
        SAFE_DELETE( pChunkHeader );
    }

    FlushFileBuffers( hFile );
    CloseHandle( hFile );
    return bRet;
}

//--------------------------------------------------------------------------------------
// Loads the index of a packed file and optionally creates mapped pointers using
// MapViewOfFile for each of the different chunks in the file.  The chunks must be
// aligned to the proper granularity (64k) or MapViewOfFile will fail.
//--------------------------------------------------------------------------------------
bool CPackedFile::LoadPackedFile( WCHAR* szFileName, bool b64Bit, CGrowableArray <LEVEL_ITEM*>* pLevelItemArray )
{
    bool bRet = false;

    // Open the file
    m_hFile = CreateFile( szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                          NULL );
    if( INVALID_HANDLE_VALUE == m_hFile )
        return bRet;

    // read the header
    DWORD dwRead;
    if( !ReadFile( m_hFile, &m_FileHeader, sizeof( PACKED_FILE_HEADER ), &dwRead, NULL ) )
        goto Error;

    // Make sure the granularity is the same
    SYSTEM_INFO SystemInfo;
    GetSystemInfo( &SystemInfo );
    if( m_FileHeader.Granularity != SystemInfo.dwAllocationGranularity )
        goto Error;

    m_ChunksMapped = 0;

    // Create the chunk and index data
    m_pChunks = new CHUNK_HEADER[ ( SIZE_T )m_FileHeader.NumChunks ];
    if( !m_pChunks )
        goto Error;
    m_pFileIndices = new FILE_INDEX[ ( SIZE_T )m_FileHeader.NumFiles ];
    if( !m_pFileIndices )
        goto Error;

    // Load the chunk and index data
    if( !ReadFile( m_hFile, m_pChunks, sizeof( CHUNK_HEADER ) * ( DWORD )m_FileHeader.NumChunks, &dwRead, NULL ) )
        goto Error;
    if( !ReadFile( m_hFile, m_pFileIndices, sizeof( FILE_INDEX ) * ( ( DWORD )m_FileHeader.NumFiles ), &dwRead,
                   NULL ) )
        goto Error;

    // Load the level item array
    for( UINT i = 0; i < m_FileHeader.NumFiles; i += 4 )
    {
        LEVEL_ITEM* pLevelItem = new LEVEL_ITEM;
        ZeroMemory( pLevelItem, sizeof( LEVEL_ITEM ) );
        pLevelItem->vCenter = m_pFileIndices[i].vCenter;
        wcscpy_s( pLevelItem->szVBName, MAX_PATH, m_pFileIndices[i].szFileName );
        wcscpy_s( pLevelItem->szIBName, MAX_PATH, m_pFileIndices[i + 1].szFileName );
        wcscpy_s( pLevelItem->szDiffuseName, MAX_PATH, m_pFileIndices[i + 2].szFileName );
        wcscpy_s( pLevelItem->szNormalName, MAX_PATH, m_pFileIndices[i + 3].szFileName );
        pLevelItem->bLoaded = false;
        pLevelItem->bLoading = false;
        pLevelItem->bInLoadRadius = false;
        pLevelItem->bInFrustum = false;
        pLevelItem->CurrentCountdownDiff = 0;
        pLevelItem->CurrentCountdownNorm = 0;
        pLevelItem->bHasBeenRenderedDiffuse = false;
        pLevelItem->bHasBeenRenderedNormal = false;

        pLevelItemArray->Add( pLevelItem );
    }

    if( !b64Bit )
    {
        m_pMappedChunks = new MAPPED_CHUNK[ ( SIZE_T )m_FileHeader.NumChunks ];
        if( !m_pMappedChunks )
            goto Error;

        // Memory map the rest
        m_hFileMapping = CreateFileMapping( m_hFile, NULL, PAGE_READONLY, 0, 0, NULL );
        if( INVALID_HANDLE_VALUE == m_hFile )
            goto Error;

        for( UINT64 i = 0; i < m_FileHeader.NumChunks; i++ )
        {
            m_pMappedChunks[i].bInUse = FALSE;
            m_pMappedChunks[i].pMappingPointer = NULL;
            m_pMappedChunks[i].UseCounter = 0;
        }
    }
    else
    {
        // TODO: Map the entire file on 64bits
    }

    bRet = true;
Error:
    return bRet;
}

//--------------------------------------------------------------------------------------
void CPackedFile::UnloadPackedFile()
{
    if( m_pMappedChunks )
    {
        for( UINT i = 0; i < m_FileHeader.NumChunks; i++ )
        {
            if( m_pMappedChunks[i].bInUse )
            {
                UnmapViewOfFile( m_pMappedChunks[i].pMappingPointer );
            }
        }
    }

    SAFE_DELETE_ARRAY( m_pMappedChunks );


    if( m_hFileMapping )
        CloseHandle( m_hFileMapping );
    m_hFileMapping = 0;

    if( m_hFile )
        CloseHandle( m_hFile );
    m_hFile = 0;

    SAFE_DELETE_ARRAY( m_pChunks );
    SAFE_DELETE_ARRAY( m_pFileIndices );
}


//--------------------------------------------------------------------------------------
void CPackedFile::EnsureChunkMapped( UINT64 iChunk )
{
    if( !m_pMappedChunks[iChunk].bInUse )
    {
        if( m_ChunksMapped == m_MaxChunksMapped )
        {
            // We need to free a chunk
            UINT lruValue = m_CurrentUseCounter;
            UINT64 lruChunk = ( UINT )-1;
            for( UINT64 i = 0; i < m_FileHeader.NumChunks; i++ )
            {
                if( m_pMappedChunks[i].bInUse )
                {
                    if( lruChunk == ( UINT )-1 || m_pMappedChunks[i].UseCounter < lruValue )
                    {
                        lruValue = m_pMappedChunks[i].UseCounter;
                        lruChunk = i;
                    }
                }
            }

            UnmapViewOfFile( m_pMappedChunks[lruChunk].pMappingPointer );
            m_pMappedChunks[lruChunk].pMappingPointer = NULL;
            m_pMappedChunks[lruChunk].bInUse = FALSE;
            m_ChunksMapped --;

            OutputDebugString( L"Unmapped File Chunk\n" );
        }

        // Map this chunk
        DWORD dwOffsetHigh = ( DWORD )( ( m_pChunks[iChunk].ChunkOffset & 0xFFFFFFFF00000000 ) >> 32 );
        DWORD dwOffsetLow = ( DWORD )( ( m_pChunks[iChunk].ChunkOffset & 0x00000000FFFFFFFF ) );
        m_pMappedChunks[iChunk].bInUse = TRUE;
        m_pMappedChunks[iChunk].pMappingPointer = MapViewOfFile( m_hFileMapping, FILE_MAP_READ, dwOffsetHigh,
                                                                 dwOffsetLow, ( DWORD )m_pChunks[iChunk].ChunkSize );
        if( !m_pMappedChunks[iChunk].pMappingPointer )
        {
            OutputDebugString( L"File Chunk not Mapped!\n" );
        }
        m_ChunksMapped ++;
    }

    // Set our use counter for the LRU check
    m_pMappedChunks[iChunk].UseCounter = m_CurrentUseCounter;
    m_CurrentUseCounter ++;
}

//--------------------------------------------------------------------------------------
bool CPackedFile::GetPackedFileInfo( char* szFile, UINT* pDataBytes )
{
    WCHAR str[MAX_PATH];
    MultiByteToWideChar( CP_ACP, 0, szFile, -1, str, MAX_PATH );

    return GetPackedFileInfo( str, pDataBytes );
}

//--------------------------------------------------------------------------------------
// Finds information about a resource using the index
//--------------------------------------------------------------------------------------
bool CPackedFile::GetPackedFileInfo( WCHAR* szFile, UINT* pDataBytes )
{
    // Look the file up in the index
    int iFoundIndex = -1;
    for( UINT i = 0; i < m_FileHeader.NumFiles; i++ )
    {
        if( 0 == wcscmp( szFile, m_pFileIndices[i].szFileName ) )
        {
            iFoundIndex = i;
            break;
        }
    }

    if( -1 == iFoundIndex )
        return false;

    *pDataBytes = ( UINT )m_pFileIndices[iFoundIndex].FileSize;

    return true;
}

//--------------------------------------------------------------------------------------
bool CPackedFile::GetPackedFile( char* szFile, BYTE** ppData, UINT* pDataBytes )
{
    WCHAR str[MAX_PATH];
    MultiByteToWideChar( CP_ACP, 0, szFile, -1, str, MAX_PATH );

    return GetPackedFile( str, ppData, pDataBytes );
}

//--------------------------------------------------------------------------------------
// Finds the location of a resource in a packed file and returns its contents in 
// *ppData.
//--------------------------------------------------------------------------------------
bool CPackedFile::GetPackedFile( WCHAR* szFile, BYTE** ppData, UINT* pDataBytes )
{
    // Look the file up in the index
    int iFoundIndex = -1;
    for( UINT i = 0; i < m_FileHeader.NumFiles; i++ )
    {
        if( 0 == wcscmp( szFile, m_pFileIndices[i].szFileName ) )
        {
            iFoundIndex = i;
            break;
        }
    }

    if( -1 == iFoundIndex )
        return false;

    *pDataBytes = ( UINT )m_pFileIndices[iFoundIndex].FileSize;

    // Memory mapped io
    EnsureChunkMapped( m_pFileIndices[iFoundIndex].ChunkIndex );
    *ppData = ( BYTE* )m_pMappedChunks[ m_pFileIndices[iFoundIndex].ChunkIndex ].pMappingPointer +
        m_pFileIndices[iFoundIndex].OffsetIntoChunk;

    return true;
}

//--------------------------------------------------------------------------------------
bool CPackedFile::UsingMemoryMappedIO()
{
    return ( NULL != m_pMappedChunks );
}

//--------------------------------------------------------------------------------------
void CPackedFile::SetMaxChunksMapped( UINT maxmapped )
{
    m_MaxChunksMapped = maxmapped;
}

//--------------------------------------------------------------------------------------
UINT64 CPackedFile::GetTileBytesSize()
{
    return m_FileHeader.TileBytesSize;
}

//--------------------------------------------------------------------------------------
float CPackedFile::GetTileSideSize()
{
    return m_FileHeader.TileSideSize;
}

//--------------------------------------------------------------------------------------
float CPackedFile::GetLoadingRadius()
{
    return m_FileHeader.LoadingRadius;
}

//--------------------------------------------------------------------------------------
UINT CPackedFile::GetMaxChunksMapped()
{
    return m_MaxChunksMapped;
}

//--------------------------------------------------------------------------------------
UINT CPackedFile::GetMaxChunksInVA()
{
    return m_FileHeader.MaxChunksInVA;
}

//--------------------------------------------------------------------------------------
UINT64 CPackedFile::GetNumChunks()
{
    return m_FileHeader.NumChunks;
}

//--------------------------------------------------------------------------------------
UINT64 CPackedFile::GetVideoMemoryUsageAtFullMips()
{
    return m_FileHeader.VideoMemoryUsageAtFullMips;
}

//--------------------------------------------------------------------------------------
// Terrain.cpp
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Terrain.h"


//--------------------------------------------------------------------------------------
CTerrain::CTerrain() : m_SqrtNumTiles( 0 ),
                       m_NumTiles( 0 ),
                       m_NumSidesPerTile( 0 ),
                       m_pTiles( NULL ),
                       m_fWorldScale( 0.0f ),
                       m_fHeightScale( 0.0f ),
                       m_HeightMapX( 0 ),
                       m_HeightMapY( 0 ),
                       m_pHeightBits( NULL ),
                       m_NumIndices( 0 ),
                       m_pTerrainRawIndices( NULL )
{
}


//--------------------------------------------------------------------------------------
CTerrain::~CTerrain()
{
    if( m_pTiles )
    {
        for( UINT i = 0; i < m_NumTiles; i++ )
        {
            SAFE_DELETE_ARRAY( m_pTiles[i].pRawVertices );
        }
    }

    SAFE_DELETE_ARRAY( m_pTiles );
    SAFE_DELETE_ARRAY( m_pHeightBits );
    SAFE_DELETE_ARRAY( m_pTerrainRawIndices );
}

//--------------------------------------------------------------------------------------
HRESULT CTerrain::LoadTerrain( WCHAR* strHeightMap, UINT SqrtNumTiles, UINT NumSidesPerTile, float fWorldScale,
                               float fHeightScale, bool bCreateTiles )
{
    HRESULT hr = S_OK;

    // Store variables
    m_SqrtNumTiles = SqrtNumTiles;
    m_fWorldScale = fWorldScale;
    m_fHeightScale = fHeightScale;
    m_NumSidesPerTile = NumSidesPerTile;
    m_NumTiles = SqrtNumTiles * SqrtNumTiles;
    m_NumIndices = ( m_NumSidesPerTile + 2 ) * 2 * ( m_NumSidesPerTile )- 2;

    // Load the image
    V_RETURN( LoadBMPImage( strHeightMap ) );

    // Create tiles
    if( bCreateTiles )
    {
        m_pTiles = new TERRAIN_TILE[ m_NumTiles ];
        if( !m_pTiles )
            return E_OUTOFMEMORY;

        UINT iTile = 0;
        float zStart = -m_fWorldScale / 2.0f;
        float zDelta = m_fWorldScale / ( float )m_SqrtNumTiles;
        float xDelta = m_fWorldScale / ( float )m_SqrtNumTiles;
        for( UINT z = 0; z < m_SqrtNumTiles; z++ )
        {
            float xStart = -m_fWorldScale / 2.0f;
            for( UINT x = 0; x < m_SqrtNumTiles; x++ )
            {
                BOUNDING_BOX BBox;
                BBox.min = D3DXVECTOR3( xStart, 0, zStart );
                BBox.max = D3DXVECTOR3( xStart + xDelta, 0, zStart + zDelta );

                V_RETURN( GenerateTile( &m_pTiles[iTile], &BBox ) );

                iTile ++;
                xStart += xDelta;
            }
            zStart += zDelta;
        }

        // Create the indices for the tile strips
        m_pTerrainRawIndices = new SHORT[ m_NumIndices ];
        if( !m_pTerrainRawIndices )
            return E_OUTOFMEMORY;

        SHORT vIndex = 0;
        UINT iIndex = 0;
        for( UINT z = 0; z < m_NumSidesPerTile; z++ )
        {
            for( UINT x = 0; x < m_NumSidesPerTile + 1; x++ )
            {
                m_pTerrainRawIndices[iIndex] = vIndex;
                iIndex++;
                m_pTerrainRawIndices[iIndex] = vIndex + ( SHORT )m_NumSidesPerTile + 1;
                iIndex++;
                vIndex++;
            }
            if( z != m_NumSidesPerTile - 1 )
            {
                // add a degenerate tri
                m_pTerrainRawIndices[iIndex] = vIndex + ( SHORT )m_NumSidesPerTile;
                iIndex++;
                m_pTerrainRawIndices[iIndex] = vIndex;
                iIndex++;
            }
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
float CTerrain::GetHeightForTile( UINT iTile, D3DXVECTOR3* pPos )
{
    // TODO: impl
    return 0.0f;
}


//--------------------------------------------------------------------------------------
#define HEIGHT_INDEX( a, b ) ( (b)*m_HeightMapX + (a) )
#define LINEAR_INTERPOLATE(a,b,x) (a*(1.0f-x) + b*x)
float CTerrain::GetHeightOnMap( D3DXVECTOR3* pPos )
{
    // move x and z into [0..1] range
    D3DXVECTOR2 uv = GetUVForPosition( pPos );
    float x = uv.x;
    float z = uv.y;

    // scale into heightmap space
    x *= m_HeightMapX;
    z *= m_HeightMapY;
    x += 0.5f;
    z += 0.5f;
    if( x >= m_HeightMapX - 1 )
        x = ( float )m_HeightMapX - 2;
    if( z >= m_HeightMapY - 1 )
        z = ( float )m_HeightMapY - 2;
    z = max( 0, z );
    x = max( 0, x );

    // bilinearly interpolate
    unsigned long integer_X = unsigned long( x );
    float fractional_X = x - integer_X;

    unsigned long integer_Z = unsigned long( z );
    float fractional_Z = z - integer_Z;

    float v1 = m_pHeightBits[ HEIGHT_INDEX( integer_X,    integer_Z ) ];
    float v2 = m_pHeightBits[ HEIGHT_INDEX( integer_X + 1,integer_Z ) ];
    float v3 = m_pHeightBits[ HEIGHT_INDEX( integer_X,    integer_Z + 1 ) ];
    float v4 = m_pHeightBits[ HEIGHT_INDEX( integer_X + 1,integer_Z + 1 ) ];

    float i1 = LINEAR_INTERPOLATE( v1 , v2 , fractional_X );
    float i2 = LINEAR_INTERPOLATE( v3 , v4 , fractional_X );

    float result = LINEAR_INTERPOLATE( i1 , i2 , fractional_Z );

    return result;
}


//--------------------------------------------------------------------------------------
D3DXVECTOR3 CTerrain::GetNormalOnMap( D3DXVECTOR3* pPos )
{
    // Calculate the normal
    float xDelta = ( m_fWorldScale / ( float )m_SqrtNumTiles ) / ( float )m_NumSidesPerTile;
    float zDelta = ( m_fWorldScale / ( float )m_SqrtNumTiles ) / ( float )m_NumSidesPerTile;

    D3DXVECTOR3 vLeft = *pPos - D3DXVECTOR3( xDelta, 0, 0 );
    D3DXVECTOR3 vRight = *pPos + D3DXVECTOR3( xDelta, 0, 0 );
    D3DXVECTOR3 vUp = *pPos + D3DXVECTOR3( 0, 0, zDelta );
    D3DXVECTOR3 vDown = *pPos - D3DXVECTOR3( 0, 0, zDelta );

    vLeft.y = GetHeightOnMap( &vLeft );
    vRight.y = GetHeightOnMap( &vRight );
    vUp.y = GetHeightOnMap( &vUp );
    vDown.y = GetHeightOnMap( &vDown );

    D3DXVECTOR3 e0 = vRight - vLeft;
    D3DXVECTOR3 e1 = vUp - vDown;
    D3DXVECTOR3 ortho;
    D3DXVECTOR3 norm;
    D3DXVec3Cross( &ortho, &e1, &e0 );
    D3DXVec3Normalize( &norm, &ortho );

    return norm;
}


//--------------------------------------------------------------------------------------
/*void CTerrain::RenderTile( TERRAIN_TILE* pTile )
   {
   if( m_pDev10 )
   {
   UINT stride = sizeof(TERRAIN_VERTEX);
   UINT offset = 0;
   m_pDev10->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
   m_pDev10->IASetVertexBuffers( 0, 1, &pTile->pVB10, &stride, &offset );
   m_pDev10->DrawIndexed( m_NumIndices, 0, 0 );
   }
   else
   {
   m_pDev->SetStreamSource( 0, pTile->pVB, 0, sizeof(TERRAIN_VERTEX) );
   m_pDev->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, pTile->NumVertices, 0, m_NumIndices-2 );
   }
   }*/


//--------------------------------------------------------------------------------------
D3DXVECTOR2 CTerrain::GetUVForPosition( D3DXVECTOR3* pPos )
{
    D3DXVECTOR2 uv;
    uv.x = ( pPos->x / m_fWorldScale ) + 0.5f;
    uv.y = ( pPos->z / m_fWorldScale ) + 0.5f;
    return uv;
}


//--------------------------------------------------------------------------------------
HRESULT CTerrain::LoadBMPImage( WCHAR* strHeightMap )
{
    HANDLE hFile = CreateFile( strHeightMap, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_INVALIDARG;

    DWORD dwBytesRead = 0;

    // read the bfh
    BITMAPFILEHEADER bfh;
    ReadFile( hFile, &bfh, sizeof( BITMAPFILEHEADER ), &dwBytesRead, NULL );
    unsigned long toBitmapData = bfh.bfOffBits;
    unsigned long bitmapSize = bfh.bfSize;

    // read the header
    BITMAPINFOHEADER bih;
    ReadFile( hFile, &bih, sizeof( BITMAPINFOHEADER ), &dwBytesRead, NULL );
    if( bih.biCompression != BI_RGB )
        goto Error;

    // alloc memory
    unsigned long U = m_HeightMapX = bih.biWidth;
    unsigned long V = m_HeightMapY = abs( bih.biHeight );
    m_pHeightBits = new float[ U * V ];
    if( !m_pHeightBits )
        return E_OUTOFMEMORY;

    // find the step size
    unsigned long iStep = 4;
    if( 24 == bih.biBitCount )
        iStep = 3;

    // final check for size
    unsigned long UNew = ( ( U * iStep * 8 + 31 ) & ~31 ) / ( 8 * iStep );
    if( bitmapSize < UNew * V * iStep * sizeof( BYTE ) )
        goto Error;

    // seek
    LARGE_INTEGER liMove;
    liMove.QuadPart = toBitmapData;
    if( !SetFilePointerEx( hFile, liMove, NULL, FILE_BEGIN ) )
        goto Error;

    // read in the bits
    BYTE* pBits = new BYTE[ bitmapSize ];
    if( !pBits )
        return E_OUTOFMEMORY;
    ReadFile( hFile, pBits, bitmapSize * sizeof( BYTE ), &dwBytesRead, NULL );

    // Load the Height Information
    unsigned long iHeight = 0;
    unsigned long iBitmap = 0;
    for( unsigned long y = 0; y < V; y++ )
    {
        iBitmap = y * UNew * iStep;
        for( unsigned long x = 0; x < U * iStep; x += iStep )
        {
            m_pHeightBits[iHeight] = 0;
            for( unsigned long c = 0; c < iStep; c++ )
            {
                m_pHeightBits[iHeight] += pBits[ iBitmap + c ];
            }
            m_pHeightBits[iHeight] /= ( FLOAT )( iStep * 255.0 );
            m_pHeightBits[iHeight] *= m_fHeightScale;

            iHeight ++;
            iBitmap += iStep;
        }
    }

    SAFE_DELETE_ARRAY( pBits );

    return S_OK;

Error:
    CloseHandle( hFile );
    return E_FAIL;
}


//--------------------------------------------------------------------------------------
HRESULT CTerrain::GenerateTile( TERRAIN_TILE* pTile, BOUNDING_BOX* pBBox )
{
    HRESULT hr = S_OK;

    // Alloc memory for the vertices
    pTile->NumVertices = ( m_NumSidesPerTile + 1 ) * ( m_NumSidesPerTile + 1 );
    pTile->pRawVertices = new TERRAIN_VERTEX[ pTile->NumVertices ];
    if( !pTile->pRawVertices )
        return E_OUTOFMEMORY;

    pTile->BBox = *pBBox;
    pTile->Color.x = 0.60f + RPercent() * 0.40f;
    pTile->Color.y = 0.60f + RPercent() * 0.40f;
    pTile->Color.z = 0.60f + RPercent() * 0.40f;
    pTile->Color.w = 1.0f;

    UINT iVertex = 0;
    float zStart = pBBox->min.z;
    float xDelta = ( pBBox->max.x - pBBox->min.x ) / ( float )m_NumSidesPerTile;
    float zDelta = ( pBBox->max.z - pBBox->min.z ) / ( float )m_NumSidesPerTile;

    // Loop through terrain vertices and get height from the heightmap
    for( UINT z = 0; z < m_NumSidesPerTile + 1; z++ )
    {
        float xStart = pBBox->min.x;
        for( UINT x = 0; x < m_NumSidesPerTile + 1; x++ )
        {
            D3DXVECTOR3 pos( xStart,0,zStart );
            D3DXVECTOR3 norm;
            pos.y = GetHeightOnMap( &pos );
            pTile->pRawVertices[iVertex].pos = pos;
            //pTile->pRawVertices[iVertex].uv = GetUVForPosition( &pos );
            //pTile->pRawVertices[iVertex].uv.y = 1.0f - pTile->pRawVertices[iVertex].uv.y;
            pTile->pRawVertices[iVertex].uv.x = ( float )x / ( ( float )m_NumSidesPerTile );
            pTile->pRawVertices[iVertex].uv.y = 1.0f - ( float )z / ( ( float )m_NumSidesPerTile );
            pTile->pRawVertices[iVertex].norm = GetNormalOnMap( &pos );

            iVertex ++;
            xStart += xDelta;
        }
        zStart += zDelta;
    }

    return hr;
}

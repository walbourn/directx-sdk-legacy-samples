//--------------------------------------------------------------------------------------
// Terrain.cpp
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Terrain.h"


static D3DXVECTOR3 s_vDirections[8] =
{
    D3DXVECTOR3( -1, 0, 0 ),			//left
    D3DXVECTOR3( -0.707f, 0, 0.707f ),	//up-left
    D3DXVECTOR3( 0, 0, 1 ),				//up
    D3DXVECTOR3( 0.707f, 0, 0.707f ),	//up-right
    D3DXVECTOR3( 1, 0, 0 ),				//right
    D3DXVECTOR3( 0.707f, 0, -0.707f ),	//down-right
    D3DXVECTOR3( 0, 0, -1 ),			//down
    D3DXVECTOR3( -0.707f, 0, -0.707f )  //down-left
};

//--------------------------------------------------------------------------------------
CTerrain::CTerrain() : m_pDev( NULL ),
                       m_pDev10( NULL ),
                       m_SqrtNumTiles( 0 ),
                       m_NumTiles( 0 ),
                       m_NumSidesPerTile( 0 ),
                       m_pTiles( NULL ),
                       m_fWorldScale( 0.0f ),
                       m_fHeightScale( 0.0f ),
                       m_HeightMapX( 0 ),
                       m_HeightMapY( 0 ),
                       m_pHeightBits( NULL ),
                       m_NumIndices( 0 ),
                       m_pTerrainIB( NULL ),
                       m_pTerrainIB10( NULL ),
                       m_pTerrainRawIndices( NULL ),
                       m_pGrassVB( NULL ),
                       m_ppGrassIB( NULL ),
                       m_pGrassVB10( NULL ),
                       m_ppGrassIB10( NULL ),
                       m_NumDirections( 0 ),
                       m_pDirections( NULL )
{
}


//--------------------------------------------------------------------------------------
CTerrain::~CTerrain()
{
    for( UINT i = 0; i < m_NumTiles; i++ )
    {
        SAFE_DELETE_ARRAY( m_pTiles[i].pRawVertices );
    }

    SAFE_DELETE_ARRAY( m_pTiles );
    SAFE_DELETE_ARRAY( m_pHeightBits );
    SAFE_DELETE_ARRAY( m_pTerrainRawIndices );
}


//--------------------------------------------------------------------------------------
void CTerrain::OnLostDevice()
{
    for( UINT i = 0; i < m_NumTiles; i++ )
    {
        SAFE_RELEASE( m_pTiles[i].pVB );
    }
    SAFE_RELEASE( m_pTerrainIB );
    SAFE_RELEASE( m_pGrassVB );
    for( UINT i = 0; i < m_NumDirections; i++ )
        SAFE_RELEASE( m_ppGrassIB[i] );

    SAFE_DELETE_ARRAY( m_ppGrassIB );
    SAFE_DELETE_ARRAY( m_pDirections );
}


//--------------------------------------------------------------------------------------
void CTerrain::OnDestroyDevice()
{
    for( UINT i = 0; i < m_NumTiles; i++ )
    {
        SAFE_RELEASE( m_pTiles[i].pVB10 );
    }
    SAFE_RELEASE( m_pTerrainIB10 );
    SAFE_RELEASE( m_pGrassVB10 );
    for( UINT i = 0; i < m_NumDirections; i++ )
        SAFE_RELEASE( m_ppGrassIB10[i] );

    SAFE_DELETE_ARRAY( m_ppGrassIB10 );
    SAFE_DELETE_ARRAY( m_pDirections );
}


//--------------------------------------------------------------------------------------
HRESULT CTerrain::OnResetDevice( LPDIRECT3DDEVICE9 pDev )
{
    HRESULT hr = S_OK;
    m_pDev = pDev;
    m_pDev10 = NULL;

    if( 0 == m_NumTiles )
        return S_FALSE;

    // Create the terrain tile vertex buffers
    for( UINT i = 0; i < m_NumTiles; i++ )
    {
        V_RETURN( CreateTileResources( &m_pTiles[i] ) );
    }

    // Create the index buffer
    V_RETURN( m_pDev->CreateIndexBuffer( m_NumIndices * sizeof( SHORT ),
                                         D3DUSAGE_WRITEONLY,
                                         D3DFMT_INDEX16,
                                         D3DPOOL_DEFAULT,
                                         &m_pTerrainIB,
                                         NULL ) );
    SHORT* pIndices = NULL;
    V_RETURN( m_pTerrainIB->Lock( 0, 0, ( void** )&pIndices, 0 ) );
    CopyMemory( pIndices, m_pTerrainRawIndices, m_NumIndices * sizeof( SHORT ) );
    m_pTerrainIB->Unlock();

    V_RETURN( CreateGrass() );
    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CTerrain::OnCreateDevice( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;
    m_pDev10 = pd3dDevice;
    m_pDev = NULL;

    if( 0 == m_NumTiles )
        return S_FALSE;

    // Create the terrain tile vertex buffers
    for( UINT i = 0; i < m_NumTiles; i++ )
    {
        V_RETURN( CreateTileResources( &m_pTiles[i] ) );
    }

    // Create the index buffer
    D3D10_BUFFER_DESC BufferDesc;
    BufferDesc.ByteWidth = m_NumIndices * sizeof( SHORT );
    BufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
    BufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    BufferDesc.CPUAccessFlags = 0;
    BufferDesc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = m_pTerrainRawIndices;

    V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, &InitData, &m_pTerrainIB10 ) );

    V_RETURN( CreateGrass() );
    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CTerrain::LoadTerrain( WCHAR* strHeightMap, UINT SqrtNumTiles, UINT NumSidesPerTile, float fWorldScale,
                               float fHeightScale,
                               UINT NumGrassBlades, float fGrassWidth, float fGrassHeight )
{
    HRESULT hr = S_OK;

    // Store variables
    m_SqrtNumTiles = SqrtNumTiles;
    m_fWorldScale = fWorldScale;
    m_fHeightScale = fHeightScale;
    m_NumSidesPerTile = NumSidesPerTile;
    m_NumTiles = SqrtNumTiles * SqrtNumTiles;
    m_NumGrassBlades = NumGrassBlades;
    m_fGrassWidth = fGrassWidth;
    m_fGrassHeight = fGrassHeight;

    // Load the image
    V_RETURN( LoadBMPImage( strHeightMap ) );

    // Create tiles
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
    m_NumIndices = ( m_NumSidesPerTile + 2 ) * 2 * ( m_NumSidesPerTile )- 2;
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
void CTerrain::RenderTile( TERRAIN_TILE* pTile )
{
    if( m_pDev10 )
    {
        UINT stride = sizeof( TERRAIN_VERTEX );
        UINT offset = 0;
        m_pDev10->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
        m_pDev10->IASetVertexBuffers( 0, 1, &pTile->pVB10, &stride, &offset );
        m_pDev10->DrawIndexed( m_NumIndices, 0, 0 );
    }
    else
    {
        m_pDev->SetStreamSource( 0, pTile->pVB, 0, sizeof( TERRAIN_VERTEX ) );
        m_pDev->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, pTile->NumVertices, 0, m_NumIndices - 2 );
    }
}


//--------------------------------------------------------------------------------------
void CTerrain::RenderGrass( D3DXVECTOR3* pViewDir, UINT NumInstances )
{
    // Pick the IB
    float fMaxDot = -1.0f;
    UINT maxDir = 0;
    for( UINT i = 0; i < m_NumDirections; i++ )
    {
        float fDot = D3DXVec3Dot( pViewDir, &m_pDirections[i] );
        if( fDot > fMaxDot )
        {
            fMaxDot = fDot;
            maxDir = i;
        }
    }

    if( m_pDev10 )
    {
        m_pDev10->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        m_pDev10->IASetIndexBuffer( m_ppGrassIB10[maxDir], DXGI_FORMAT_R16_UINT, 0 );
        UINT stride = sizeof( GRASS_VERTEX );
        UINT offset = 0;
        m_pDev10->IASetVertexBuffers( 0, 1, &m_pGrassVB10, &stride, &offset );

        m_pDev10->DrawIndexedInstanced( m_NumGrassBlades * 6, NumInstances, 0, 0, 0 );
    }
    else
    {
        m_pDev->SetIndices( m_ppGrassIB[maxDir] );
        m_pDev->SetStreamSource( 0, m_pGrassVB, 0, sizeof( GRASS_VERTEX ) );
        m_pDev->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, m_NumGrassBlades * 4, 0, m_NumGrassBlades * 2 );
    }
}


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
    FILE* fp = NULL;
    _wfopen_s( &fp, strHeightMap, L"rb" );
    if( !fp )
        return E_INVALIDARG;

    // read the bfh
    BITMAPFILEHEADER bfh;
    fread( &bfh, sizeof( BITMAPFILEHEADER ), 1, fp );
    unsigned long toBitmapData = bfh.bfOffBits;
    unsigned long bitmapSize = bfh.bfSize;

    // read the header
    BITMAPINFOHEADER bih;
    fread( &bih, sizeof( BITMAPINFOHEADER ), 1, fp );
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
    fseek( fp, toBitmapData, SEEK_SET );

    // read in the bits
    BYTE* pBits = new BYTE[ bitmapSize ];
    if( !pBits )
        return E_OUTOFMEMORY;
    fread( pBits, bitmapSize, 1, fp );
	
    // close the file
    fclose( fp );

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
    fclose( fp );
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
            pTile->pRawVertices[iVertex].uv = GetUVForPosition( &pos );
            pTile->pRawVertices[iVertex].uv.y = 1.0f - pTile->pRawVertices[iVertex].uv.y;
            pTile->pRawVertices[iVertex].norm = GetNormalOnMap( &pos );

            iVertex ++;
            xStart += xDelta;
        }
        zStart += zDelta;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CTerrain::CreateTileResources( TERRAIN_TILE* pTile )
{
    HRESULT hr = S_OK;

    if( m_pDev10 )
    {
        D3D10_BUFFER_DESC BufferDesc;
        BufferDesc.ByteWidth = m_pTiles->NumVertices * sizeof( TERRAIN_VERTEX );
        BufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
        BufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = 0;
        BufferDesc.MiscFlags = 0;

        D3D10_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pTile->pRawVertices;
        V_RETURN( m_pDev10->CreateBuffer( &BufferDesc, &InitData, &pTile->pVB10 ) );
    }
    else
    {
        V_RETURN( m_pDev->CreateVertexBuffer( m_pTiles->NumVertices * sizeof( TERRAIN_VERTEX ),
                                              D3DUSAGE_WRITEONLY,
                                              0,
                                              D3DPOOL_DEFAULT,
                                              &pTile->pVB,
                                              NULL ) );

        TERRAIN_VERTEX* pVertices = NULL;
        V_RETURN( pTile->pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) );
        CopyMemory( pVertices, pTile->pRawVertices, pTile->NumVertices * sizeof( TERRAIN_VERTEX ) );
        pTile->pVB->Unlock();
    }

    return hr;
}


//--------------------------------------------------------------------------------------
float RPercent()
{
    float ret = ( float )( ( rand() % 20000 ) - 10000 );
    return ret / 10000.0f;
}


//--------------------------------------------------------------------------------------
HRESULT CTerrain::CreateGrass()
{
    HRESULT hr = S_OK;

    float fTileSize = m_fWorldScale / ( float )m_SqrtNumTiles;
    fTileSize /= 2.0f;
    float fGrassWidth = m_fGrassWidth / 2.0f;
    float fGrassHeight = m_fGrassHeight;

    D3DXVECTOR3* pGrassCenters = new D3DXVECTOR3[ m_NumGrassBlades ];
    if( !pGrassCenters )
        return E_OUTOFMEMORY;

    for( UINT i = 0; i < m_NumGrassBlades; i++ )
    {
        pGrassCenters[i].x = RPercent() * fTileSize;
        pGrassCenters[i].y = 0.0f;
        pGrassCenters[i].z = RPercent() * fTileSize;
    }

    GRASS_VERTEX* pVertices = NULL;
    D3D10_BUFFER_DESC BufferDesc;
    if( m_pDev10 )
    {
        BufferDesc.ByteWidth = m_NumGrassBlades * 4 * sizeof( GRASS_VERTEX );
        BufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
        BufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = 0;
        BufferDesc.MiscFlags = 0;

        pVertices = new GRASS_VERTEX[ m_NumGrassBlades * 4 ];
        if( !pVertices )
            return E_OUTOFMEMORY;
    }
    else
    {
        V_RETURN( m_pDev->CreateVertexBuffer( m_NumGrassBlades * 4 * sizeof( GRASS_VERTEX ),
                                              D3DUSAGE_WRITEONLY,
                                              0,
                                              D3DPOOL_DEFAULT,
                                              &m_pGrassVB,
                                              NULL ) );
        V_RETURN( m_pGrassVB->Lock( 0, 0, ( void** )&pVertices, 0 ) );
    }

    UINT vIndex = 0;
    for( UINT i = 0; i < m_NumGrassBlades; i++ )
    {
        D3DXVECTOR3 vRandRight( RPercent(), 0, RPercent() );
        D3DXVec3Normalize( &vRandRight, &vRandRight );

        pVertices[vIndex  ].pos = pGrassCenters[i] - vRandRight * fGrassWidth;
        pVertices[vIndex  ].uv = D3DXVECTOR2( 0, 1 );
        pVertices[vIndex + 1].pos = pGrassCenters[i] + vRandRight * fGrassWidth;
        pVertices[vIndex + 1].uv = D3DXVECTOR2( 1, 1 );
        pVertices[vIndex + 2].pos = pVertices[vIndex + 1].pos + D3DXVECTOR3( 0, fGrassHeight, 0 );
        pVertices[vIndex + 2].uv = D3DXVECTOR2( 1, 0 );
        pVertices[vIndex + 3].pos = pVertices[vIndex  ].pos + D3DXVECTOR3( 0, fGrassHeight, 0 );
        pVertices[vIndex + 3].uv = D3DXVECTOR2( 0, 0 );
        vIndex += 4;
    }
    if( m_pDev10 )
    {
        D3D10_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pVertices;
        V_RETURN( m_pDev10->CreateBuffer( &BufferDesc, &InitData, &m_pGrassVB10 ) );
        SAFE_DELETE_ARRAY( pVertices );
    }
    else
    {
        m_pGrassVB->Unlock();
    }

    // Alloc indices and distances
    SHORT* pGrassIndices = new SHORT[ m_NumGrassBlades ];
    if( !pGrassIndices )
        return E_OUTOFMEMORY;
    float* pGrassDistances = new float[ m_NumGrassBlades ];
    if( !pGrassDistances )
        return E_OUTOFMEMORY;

    m_NumDirections = 16;

    if( m_pDev10 )
    {
        m_ppGrassIB10 = new ID3D10Buffer*[ m_NumDirections ];
        if( !m_ppGrassIB10 )
            return E_OUTOFMEMORY;
    }
    else
    {
        m_ppGrassIB = new LPDIRECT3DINDEXBUFFER9[ m_NumDirections ];
        if( !m_ppGrassIB )
            return E_OUTOFMEMORY;
    }
    m_pDirections = new D3DXVECTOR3[ m_NumDirections ];
    if( !m_pDirections )
        return E_OUTOFMEMORY;

    D3DXVECTOR3 vStartDir( -1,0,0 );
    float fAngleDelta = ( D3DX_PI * 2.0f ) / ( float )( m_NumDirections );
    for( UINT i = 0; i < m_NumDirections; i++ )
    {
        D3DXMATRIX mRot;
        D3DXMatrixRotationY( &mRot, i * fAngleDelta );
        D3DXVec3TransformNormal( &m_pDirections[i], &vStartDir, &mRot );

        // init indices and distances
        for( UINT g = 0; g < m_NumGrassBlades; g++ )
        {
            pGrassIndices[g] = ( SHORT )g;
            pGrassDistances[g] = -D3DXVec3Dot( &m_pDirections[i], &pGrassCenters[g] );
        }

        // sort indices
        QuickDepthSort( pGrassIndices, pGrassDistances, 0, m_NumGrassBlades - 1 );

        SHORT* pIndices = NULL;
        if( m_pDev10 )
        {
            BufferDesc.ByteWidth = m_NumGrassBlades * 6 * sizeof( SHORT );
            BufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
            BufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
            BufferDesc.CPUAccessFlags = 0;
            BufferDesc.MiscFlags = 0;

            pIndices = new SHORT[ m_NumGrassBlades * 6 ];
            if( !pIndices )
                return E_OUTOFMEMORY;
        }
        else
        {
            V_RETURN( m_pDev->CreateIndexBuffer( m_NumGrassBlades * 6 * sizeof( SHORT ),
                                                 D3DUSAGE_WRITEONLY,
                                                 D3DFMT_INDEX16,
                                                 D3DPOOL_DEFAULT,
                                                 &m_ppGrassIB[i],
                                                 NULL ) );
            V_RETURN( m_ppGrassIB[i]->Lock( 0, 0, ( void** )&pIndices, 0 ) );
        }

        UINT iIndex = 0;
        for( UINT g = 0; g < m_NumGrassBlades; g++ )
        {
            //Starting vert
            SHORT GrassIndex = pGrassIndices[g] * 4;
            //Tri1
            pIndices[iIndex  ] = GrassIndex;
            pIndices[iIndex + 1] = GrassIndex + 3;
            pIndices[iIndex + 2] = GrassIndex + 1;
            //Tri2
            pIndices[iIndex + 3] = GrassIndex + 1;
            pIndices[iIndex + 4] = GrassIndex + 3;
            pIndices[iIndex + 5] = GrassIndex + 2;

            iIndex += 6;
        }
        if( m_pDev10 )
        {
            D3D10_SUBRESOURCE_DATA InitData;
            InitData.pSysMem = pIndices;
            V_RETURN( m_pDev10->CreateBuffer( &BufferDesc, &InitData, &m_ppGrassIB10[i] ) );
            SAFE_DELETE_ARRAY( pIndices );
        }
        else
        {
            m_ppGrassIB[i]->Unlock();
        }
    }									

    SAFE_DELETE_ARRAY( pGrassIndices );
    SAFE_DELETE_ARRAY( pGrassDistances );
    SAFE_DELETE_ARRAY( pGrassCenters );

    return hr;
}

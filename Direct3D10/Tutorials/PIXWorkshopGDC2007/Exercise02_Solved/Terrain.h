//--------------------------------------------------------------------------------------
// Terrain.h
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

struct TERRAIN_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 norm;
    D3DXVECTOR2 uv;
};

struct GRASS_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR2 uv;
};

struct BOUNDING_BOX
{
    D3DXVECTOR3 min;
    D3DXVECTOR3 max;
};

struct TERRAIN_TILE
{
    LPDIRECT3DVERTEXBUFFER9 pVB;
    ID3D10Buffer* pVB10;
    UINT NumVertices;
    TERRAIN_VERTEX* pRawVertices;
    D3DXVECTOR4 Color;
    BOUNDING_BOX BBox;
};

class CTerrain
{
private:
    LPDIRECT3DDEVICE9 m_pDev;
    ID3D10Device* m_pDev10;
    UINT m_SqrtNumTiles;
    UINT m_NumTiles;
    UINT m_NumSidesPerTile;
    TERRAIN_TILE* m_pTiles;
    float m_fWorldScale;
    float m_fHeightScale;
    UINT m_HeightMapX;
    UINT m_HeightMapY;
    float* m_pHeightBits;

    UINT m_NumIndices;
    LPDIRECT3DINDEXBUFFER9 m_pTerrainIB;
    ID3D10Buffer* m_pTerrainIB10;
    SHORT* m_pTerrainRawIndices;

    UINT m_NumGrassBlades;
    float m_fGrassWidth;
    float m_fGrassHeight;
    LPDIRECT3DVERTEXBUFFER9 m_pGrassVB;
    LPDIRECT3DINDEXBUFFER9* m_ppGrassIB;
    ID3D10Buffer* m_pGrassVB10;
    ID3D10Buffer** m_ppGrassIB10;

    UINT m_NumDirections;
    D3DXVECTOR3* m_pDirections;

public:
                            CTerrain();
                            ~CTerrain();

    void                    OnLostDevice();
    void                    OnDestroyDevice();
    HRESULT                 OnResetDevice( LPDIRECT3DDEVICE9 pDev );
    HRESULT                 OnCreateDevice( ID3D10Device* pDev );

    HRESULT                 LoadTerrain( WCHAR* strHeightMap, UINT SqrtNumTiles, UINT NumSidesPerTile,
                                         float fWorldScale, float fHeightScale,
                                         UINT NumGrassBlades, float fGrassWidth, float fGrassHeight );
    float                   GetHeightForTile( UINT iTile, D3DXVECTOR3* pPos );
    float                   GetHeightOnMap( D3DXVECTOR3* pPos );
    D3DXVECTOR3             GetNormalOnMap( D3DXVECTOR3* pPos );
    void                    RenderTile( TERRAIN_TILE* pTile );
    void                    RenderGrass( D3DXVECTOR3* pViewDir, UINT NumInstances );

    float                   GetWorldScale()
    {
        return m_fWorldScale;
    }
    LPDIRECT3DINDEXBUFFER9  GetTerrainIB()
    {
        return m_pTerrainIB;
    }
    ID3D10Buffer* GetTerrainIB10()
    {
        return m_pTerrainIB10;
    }
    UINT                    GetNumTiles()
    {
        return m_NumTiles;
    }
    TERRAIN_TILE* GetTile( UINT iTile )
    {
        return &m_pTiles[iTile];
    }

protected:
    D3DXVECTOR2             GetUVForPosition( D3DXVECTOR3* pPos );
    HRESULT                 LoadBMPImage( WCHAR* strHeightMap );
    HRESULT                 GenerateTile( TERRAIN_TILE* pTile, BOUNDING_BOX* pBBox );
    HRESULT                 CreateTileResources( TERRAIN_TILE* pTile );
    HRESULT                 CreateGrass();
};

float RPercent();

//--------------------------------------------------------------------------------------
template <class T> void QuickDepthSort( T* indices, float* depths, int lo, int hi )
{
    //  lo is the lower index, hi is the upper index
    //  of the region of array a that is to be sorted
    int i = lo, j = hi;
    float h;
    T index;
    float x = depths[( lo + hi ) / 2];

    //  partition
    do
    {
        while( depths[i] < x ) i++;
        while( depths[j] > x ) j--;
        if( i <= j )
        {
            h = depths[i]; depths[i] = depths[j]; depths[j] = h;
            index = indices[i]; indices[i] = indices[j]; indices[j] = index;
            i++; j--;
        }
    } while( i <= j );

    //  recursion
    if( lo < j ) QuickDepthSort( indices, depths, lo, j );
    if( i < hi ) QuickDepthSort( indices, depths, i, hi );
}

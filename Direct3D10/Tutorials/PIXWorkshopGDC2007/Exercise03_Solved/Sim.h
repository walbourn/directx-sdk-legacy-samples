//--------------------------------------------------------------------------------------
// Sim.h
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "Terrain.h"

class CSim;
struct THREAD_DATA
{
    CSim* pSim;
    UINT iPlayerStart;
    UINT iPlayerCount;

    double fTime;
    float fElapsedTime;
    HANDLE hThreadStart;
    HANDLE hThreadDone;
};

struct GRID_CELL
{
    BOUNDING_BOX BBox;
    CGrowableArray <UINT> BallArray;
};

class CPlayer;
class CSim
{
private:
    CPlayer* m_pPlayers;
    UINT m_NumPlayers;
    UINT m_NumSimThreads;
    HANDLE* m_pThreadHandles;
    HANDLE* m_pThreadStartHandles;
    HANDLE* m_pThreadDoneHandles;

    THREAD_DATA* m_pThreadData;
    bool m_bDone;

    UINT m_NumCells;
    UINT m_CellsX;
    UINT m_CellsZ;
    float m_fCellXSize;
    float m_fCellZSize;
    float m_fWorldScale;
    GRID_CELL* m_pGridCells;

    UINT m_MaxBallsInACell;

public:
            CSim();
            ~CSim();

    HRESULT CreateSimThreads( UINT iNumThreads, CPlayer* pPlayers, UINT NumPlayers );
    void    DestroySimThreads();

    void    StartSim( double fTime, float fElapsedTime );
    void    WaitForSim();

    void    SimLoop( THREAD_DATA* pData );

    HRESULT CreateCollisionGrid( UINT CellsX, UINT CellsZ, float fWorldScale );
    void    DestroyCollisionGrid();
    void    InsertPlayersIntoGrid();

    GRID_CELL* GetGridCell( UINT iGrid );
    CPlayer* GetPlayer( UINT iPlayer );
    UINT    GetMaxBallsInACell()
    {
        return m_MaxBallsInACell;
    }

protected:
    UINT    GetGridIndex( D3DXVECTOR3* pPos );
};

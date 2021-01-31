//--------------------------------------------------------------------------------------
// Sim.cpp
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Sim.h"
#include "Player.h"
#include <process.h>

unsigned int __stdcall SimulationThread( void* pArg )
{
    THREAD_DATA* pData = ( THREAD_DATA* )pArg;
    pData->pSim->SimLoop( pData );

    return 0;
}

CSim::CSim() : m_pPlayers( NULL ),
               m_NumPlayers( 0 ),
               m_NumSimThreads( 0 ),
               m_pThreadHandles( NULL ),
               m_pThreadStartHandles( NULL ),
               m_pThreadDoneHandles( NULL ),
               m_pThreadData( NULL ),
               m_bDone( false ),
               m_pGridCells( NULL ),
               m_MaxBallsInACell( 0 )
{
}

CSim::~CSim()
{
    DestroySimThreads();
    DestroyCollisionGrid();
}

HRESULT CSim::CreateSimThreads( UINT iNumThreads, CPlayer* pPlayers, UINT NumPlayers )
{
    m_pPlayers = pPlayers;
    m_NumPlayers = NumPlayers;
    m_NumSimThreads = iNumThreads;

    // Create Events
    m_pThreadHandles = new HANDLE[ m_NumSimThreads ];
    if( !m_pThreadHandles )
        return E_OUTOFMEMORY;

    m_pThreadStartHandles = new HANDLE[ m_NumSimThreads ];
    if( !m_pThreadStartHandles )
        return E_OUTOFMEMORY;

    m_pThreadDoneHandles = new HANDLE[ m_NumSimThreads ];
    if( !m_pThreadDoneHandles )
        return E_OUTOFMEMORY;

    // Create Events
    for( UINT i = 0; i < m_NumSimThreads; i++ )
    {
        m_pThreadStartHandles[i] = CreateEvent( NULL, FALSE, FALSE, NULL );
        m_pThreadDoneHandles[i] = CreateEvent( NULL, FALSE, FALSE, NULL );
    }

    // Create Per-Thread Data
    m_pThreadData = new THREAD_DATA[ m_NumSimThreads ];
    if( !m_pThreadData )
        return E_OUTOFMEMORY;
    ZeroMemory( m_pThreadData, m_NumSimThreads * sizeof( THREAD_DATA ) );
    UINT iDelta = m_NumPlayers / m_NumSimThreads;
    UINT iStart = 0;
    for( UINT i = 0; i < m_NumSimThreads; i++ )
    {
        m_pThreadData[i].iPlayerStart = iStart;
        m_pThreadData[i].iPlayerCount = iDelta;
        m_pThreadData[i].pSim = this;
        m_pThreadData[i].hThreadStart = m_pThreadStartHandles[i];
        m_pThreadData[i].hThreadDone = m_pThreadDoneHandles[i];

        if( i == m_NumSimThreads - 1 )
            m_pThreadData[i].iPlayerCount = m_NumPlayers - m_pThreadData[i].iPlayerStart;

        iStart += iDelta;
    }

    // Start all of the threads
    for( UINT i = 0; i < m_NumSimThreads; i++ )
    {
        unsigned int threadAddr;
        m_pThreadHandles[i] = ( HANDLE )_beginthreadex( NULL, 0, SimulationThread, &m_pThreadData[i],
                                                        CREATE_SUSPENDED, &threadAddr );
        //SetThreadAffinityMask( m_pThreadHandles[i], 1<<i );
        ResumeThread( m_pThreadHandles[i] );
    }

    return S_OK;
}

void CSim::DestroySimThreads()
{
    // Tell everyone we're done
    m_bDone = true;

    // Set the signal to run one last time
    if( m_pThreadStartHandles )
    {
        for( UINT i = 0; i < m_NumSimThreads; i++ )
        {
            SetEvent( m_pThreadStartHandles[i] );
        }
    }

    // Wait for all threads to complete
    if( m_pThreadHandles )
        WaitForMultipleObjects( m_NumSimThreads, m_pThreadHandles, TRUE, 10000 );

    // Cleanup
    for( UINT i = 0; i < m_NumSimThreads; i++ )
    {
        CloseHandle( m_pThreadHandles[i] );
        CloseHandle( m_pThreadStartHandles[i] );
        CloseHandle( m_pThreadDoneHandles[i] );
    }

    m_NumSimThreads = 0;
    SAFE_DELETE_ARRAY( m_pThreadHandles );
    SAFE_DELETE_ARRAY( m_pThreadStartHandles );
    SAFE_DELETE_ARRAY( m_pThreadDoneHandles );
    SAFE_DELETE_ARRAY( m_pThreadData );
}

void CSim::StartSim( double fTime, float fElapsedTime )
{
    // Make sure our data is up to date
    for( UINT i = 0; i < m_NumSimThreads; i++ )
    {
        m_pThreadData[i].fTime = fTime;
        m_pThreadData[i].fElapsedTime = fElapsedTime;
    }

    // Set the signal to run
    for( UINT i = 0; i < m_NumSimThreads; i++ )
    {
        SetEvent( m_pThreadStartHandles[i] );
    }
}

void CSim::WaitForSim()
{
    WaitForMultipleObjects( m_NumSimThreads, m_pThreadDoneHandles, TRUE, 5000 );
}

void CSim::SimLoop( THREAD_DATA* pData )
{
    while( !m_bDone )
    {
        // Wait for the start signal
        WaitForSingleObject( pData->hThreadStart, INFINITE );
        if( m_bDone )
            return;

        UINT iStart = pData->iPlayerStart;
        UINT iEnd = iStart + pData->iPlayerCount;
        for( UINT i = iStart; i < iEnd; i++ )
        {
            m_pPlayers[i].OnFrameMove( pData->fTime, pData->fElapsedTime, this );
        }

        SetEvent( pData->hThreadDone );
    }
}

HRESULT CSim::CreateCollisionGrid( UINT CellsX, UINT CellsZ, float fWorldScale )
{
    m_NumCells = CellsX * CellsZ;
    m_CellsX = CellsX;
    m_CellsZ = CellsZ;
    m_fWorldScale = fWorldScale;

    m_pGridCells = new GRID_CELL[ m_NumCells ];
    if( !m_pGridCells )
        return E_OUTOFMEMORY;

    UINT iGrid = 0;
    float zStart = -m_fWorldScale / 2.0f;
    float zDelta = m_fCellZSize = m_fWorldScale / ( float )m_CellsZ;
    float xDelta = m_fCellXSize = m_fWorldScale / ( float )m_CellsX;
    for( UINT z = 0; z < m_CellsZ; z++ )
    {
        float xStart = -m_fWorldScale / 2.0f;
        for( UINT x = 0; x < m_CellsX; x++ )
        {
            BOUNDING_BOX BBox;
            BBox.min = D3DXVECTOR3( xStart, 0, zStart );
            BBox.max = D3DXVECTOR3( xStart + xDelta, 0, zStart + zDelta );

            m_pGridCells[iGrid].BBox = BBox;

            iGrid ++;
            xStart += xDelta;
        }
        zStart += zDelta;
    }

    return S_OK;
}

void CSim::DestroyCollisionGrid()
{
    SAFE_DELETE_ARRAY( m_pGridCells );
}

void CSim::InsertPlayersIntoGrid()
{
    for( UINT i = 0; i < m_NumCells; i++ )
    {
        m_pGridCells[i].BallArray.Reset();
    }

    for( UINT i = 0; i < m_NumPlayers; i++ )
    {
        m_pPlayers[i].ResetGridArray();

        float fRadius = m_pPlayers[i].GetRadius();
        D3DXVECTOR3 vPos0 = m_pPlayers[i].GetPosition();
        D3DXVECTOR3 vPosL = vPos0 + D3DXVECTOR3( -fRadius, 0, 0 );
        D3DXVECTOR3 vPosR = vPos0 + D3DXVECTOR3( fRadius, 0, 0 );
        D3DXVECTOR3 vPosU = vPos0 + D3DXVECTOR3( 0, 0, fRadius );
        D3DXVECTOR3 vPosD = vPos0 + D3DXVECTOR3( 0, 0, -fRadius );

        D3DXVECTOR3 vPosUL = vPos0 + D3DXVECTOR3( -fRadius, 0, fRadius );
        D3DXVECTOR3 vPosUR = vPos0 + D3DXVECTOR3( fRadius, 0, fRadius );
        D3DXVECTOR3 vPosDL = vPos0 + D3DXVECTOR3( -fRadius, 0, -fRadius );
        D3DXVECTOR3 vPosDR = vPos0 + D3DXVECTOR3( fRadius, 0, -fRadius );
		
        UINT index0 = GetGridIndex( &vPos0 );
        UINT indexL = GetGridIndex( &vPosL );
        UINT indexR = GetGridIndex( &vPosR );
        UINT indexU = GetGridIndex( &vPosU );
        UINT indexD = GetGridIndex( &vPosD );

        UINT indexUL = GetGridIndex( &vPosUL );
        UINT indexUR = GetGridIndex( &vPosUR );
        UINT indexDL = GetGridIndex( &vPosDL );
        UINT indexDR = GetGridIndex( &vPosDR );
	
        m_pGridCells[index0].BallArray.Add( i );
        m_pPlayers[i].AddGrid( index0 );
        if( indexL != index0 )
        {
            m_pGridCells[indexL].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexL );
        }
        if( indexR != index0 )
        {
            m_pGridCells[indexR].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexR );
        }
        if( indexU != index0 )
        {
            m_pGridCells[indexU].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexU );
        }
        if( indexD != index0 )
        {
            m_pGridCells[indexD].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexD );
        }

        if( indexUL != indexU && indexUL != indexL )
        {
            m_pGridCells[indexUL].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexUL );
        }
        if( indexUR != indexU && indexUR != indexR )
        {
            m_pGridCells[indexUR].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexUR );
        }
        if( indexDL != indexD && indexDL != indexL )
        {
            m_pGridCells[indexDL].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexDL );
        }
        if( indexDR != indexD && indexDR != indexR )
        {
            m_pGridCells[indexDR].BallArray.Add( i );
            m_pPlayers[i].AddGrid( indexDR );
        }
    }

    m_MaxBallsInACell = 0;
    for( UINT i = 0; i < m_NumCells; i++ )
    {
        UINT NumBalls = ( UINT )m_pGridCells[i].BallArray.GetSize();
        if( NumBalls > m_MaxBallsInACell )
            m_MaxBallsInACell = NumBalls;
    }
	
}

GRID_CELL* CSim::GetGridCell( UINT iGrid )
{
    return &m_pGridCells[iGrid];
}

CPlayer* CSim::GetPlayer( UINT iPlayer )
{
    return &m_pPlayers[iPlayer];
}

UINT CSim::GetGridIndex( D3DXVECTOR3* pPos )
{
    UINT GridX = ( UINT )( ( pPos->x + ( m_fWorldScale / 2.0f ) ) / m_fCellXSize );
    UINT GridZ = ( UINT )( ( pPos->z + ( m_fWorldScale / 2.0f ) ) / m_fCellZSize );
    if( GridX >= m_CellsX )
        GridX = m_CellsX - 1;
    if( GridZ >= m_CellsZ )
        GridZ = m_CellsZ - 1;

    return GridZ * m_CellsX + GridX;
}

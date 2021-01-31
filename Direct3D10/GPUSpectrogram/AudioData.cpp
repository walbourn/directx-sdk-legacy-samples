//--------------------------------------------------------------------------------------
// File: AudioData.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "AudioData.h"
#include <mmsystem.h>
#include <mmreg.h>
#define _KS_NO_ANONYMOUS_STRUCTURES_
#pragma warning( disable : 4201 ) // disable nonstandard extension used : nameless struct/union
#include <ks.h>
#include <ksmedia.h>
#pragma warning( default : 4201 )
#include <math.h>


//--------------------------------------------------------------------------------------
unsigned long NumSetBitsInDWORD( unsigned long ulValue )
{
    unsigned long ulCount = 0;
    unsigned long ulBitMask = 1;

    for( int i = 0; i < 32; i++ )
    {
        if( ulValue & ulBitMask )
            ulCount ++;

        ulBitMask = ulBitMask << 1;
    }

    return ulCount;
}


//--------------------------------------------------------------------------------------
void CAudioData::Cleanup()
{
    if ( m_ppChannel != NULL )
    {
        for( unsigned long i = 0; i < m_ulNumChannels; i++ )
        {
            SAFE_DELETE_ARRAY( m_ppChannel[i] );
        }
    }
    SAFE_DELETE_ARRAY( m_ppChannel );

    m_ulNumSamples = 0;
    m_ulNumChannels = 0;
}


//--------------------------------------------------------------------------------------
void CAudioData::SplitDataIntoChannels_8Bit( unsigned char* pData )
{
    // 8 bit unsigned format
    unsigned char* pDataCast = pData;
    unsigned long ulChannelIndex = 0;
    for( unsigned long i = 0; i < m_ulNumSamples * m_ulNumChannels; i += m_ulNumChannels )
    {
        for( unsigned long c = 0; c < m_ulNumChannels; c++ )
        {
            m_ppChannel[c][ulChannelIndex] = ( float )( pDataCast[i + c] );
        }
        ulChannelIndex ++;
    }
}


//--------------------------------------------------------------------------------------
void CAudioData::SplitDataIntoChannels_16Bit( unsigned char* pData )
{
    // 16 bit signed format
    short* pDataCast = ( short* )pData;
    unsigned long ulChannelIndex = 0;
    for( unsigned long i = 0; i < m_ulNumSamples * m_ulNumChannels; i += m_ulNumChannels )
    {
        for( unsigned long c = 0; c < m_ulNumChannels; c++ )
        {
            m_ppChannel[c][ulChannelIndex] = ( float )( pDataCast[i + c] );
        }
        ulChannelIndex ++;
    }
}


//--------------------------------------------------------------------------------------
void CAudioData::SplitDataIntoChannels_32Bit( unsigned char* pData )
{
    // 32 bit signed format
    unsigned long* pDataCast = ( unsigned long* )pData;
    unsigned long ulChannelIndex = 0;
    for( unsigned long i = 0; i < m_ulNumSamples * m_ulNumChannels; i += m_ulNumChannels )
    {
        for( unsigned long c = 0; c < m_ulNumChannels; c++ )
        {
            m_ppChannel[c][ulChannelIndex] = ( float )( pDataCast[i + c] );
        }
        ulChannelIndex ++;
    }
}


//--------------------------------------------------------------------------------------
void CAudioData::SplitDataIntoChannels_IEEEFloat( unsigned char* pData )
{
    // 32 bit float format
    float* pDataCast = ( float* )pData;
    unsigned long ulChannelIndex = 0;
    for( unsigned long i = 0; i < m_ulNumSamples * m_ulNumChannels; i += m_ulNumChannels )
    {
        for( unsigned long c = 0; c < m_ulNumChannels; c++ )
        {
            m_ppChannel[c][ulChannelIndex] = ( float )( pDataCast[i + c] );
        }
        ulChannelIndex ++;
    }
}


//--------------------------------------------------------------------------------------
CAudioData::CAudioData()
{
    m_ulNumSamples = 0;
    m_ulNumChannels = 0;
    m_ppChannel = NULL;
}


//--------------------------------------------------------------------------------------
CAudioData::~CAudioData()
{
    Cleanup();
}


//--------------------------------------------------------------------------------------
bool CAudioData::LoadWaveFile( TCHAR* szWave )
{
    Cleanup();  // Cleanup just in case we already have data

    CWaveFile waveFile; // Create a wave file object

    // Open the wave file
    if( FAILED( waveFile.Open( szWave, NULL, WAVEFILE_READ ) ) )
    {
        waveFile.Close();
        return false;
    }

    // Get some information about the sound data
    WAVEFORMATEX* pwfx = waveFile.m_pwfx;
    WAVEFORMATEXTENSIBLE* pwfex = NULL;
    WORD wBitsPerSample = pwfx->wBitsPerSample;
    if( 0 == wBitsPerSample )
    {
        // We don't support compressed formats
        return false;
    }

    // Get the number of channels
    if( WAVE_FORMAT_PCM == pwfx->wFormatTag )
    {
        m_ulNumChannels = pwfx->nChannels;
    }
    else if( WAVE_FORMAT_EXTENSIBLE == pwfx->wFormatTag )
    {
        pwfex = ( WAVEFORMATEXTENSIBLE* )pwfx;
        if( KSDATAFORMAT_SUBTYPE_PCM == pwfex->SubFormat ||
            KSDATAFORMAT_SUBTYPE_IEEE_FLOAT == pwfex->SubFormat )
        {
            m_ulNumChannels = NumSetBitsInDWORD( pwfex->dwChannelMask );
        }
        else
        {
            // We don't support compressed formats
            return false;
        }
    }

    if ( m_ulNumChannels == 0 )
        return false;

    // Get the size of the PCM sound data
    DWORD dwDataSize = waveFile.GetSize();

    // Calculate the number of samples
    m_ulNumSamples = dwDataSize / ( m_ulNumChannels * ( wBitsPerSample / 8 ) );

    // Allocate a buffer for each channel
    m_ppChannel = new float*[ m_ulNumChannels ];
    if( !m_ppChannel )
    {
        waveFile.Close();
        return false;
    }
    ZeroMemory( m_ppChannel, sizeof( float* ) * m_ulNumChannels );
    for( unsigned long i = 0; i < m_ulNumChannels; i++ )
    {
        m_ppChannel[i] = new float[ m_ulNumSamples ];
        if( !m_ppChannel[i] )
        {
            waveFile.Close();
            Cleanup();
            return false;
        }
    }

    // Allocate a buffer big enough to hold the wave data
    unsigned char* pBits = new unsigned char[ dwDataSize ];
    if( !pBits )
    {
        waveFile.Close();
        Cleanup();
        return false;
    }

    // Read in the wave data
    DWORD dwBytesRead = 0;
    if( FAILED( waveFile.Read( pBits, dwDataSize, &dwBytesRead ) ) )
    {
        waveFile.Close();
        SAFE_DELETE_ARRAY( pBits );
        Cleanup();
        return false;
    }

    // Split the big data into samples
    switch( wBitsPerSample )
    {
        case 8:
            SplitDataIntoChannels_8Bit( pBits );
            break;
        case 16:
            SplitDataIntoChannels_16Bit( pBits );
            break;
        case 32:
            if( pwfex )
            {
                if( KSDATAFORMAT_SUBTYPE_PCM == pwfex->SubFormat )
                    SplitDataIntoChannels_32Bit( pBits );
                else if( KSDATAFORMAT_SUBTYPE_IEEE_FLOAT == pwfex->SubFormat )
                    SplitDataIntoChannels_IEEEFloat( pBits );
            }
            break;
    };

    // Delete the big buffer
    SAFE_DELETE_ARRAY( pBits );

    // Close the wave file
    waveFile.Close();
    return true;
}


//--------------------------------------------------------------------------------------
bool CAudioData::CreateDataSpace( unsigned long ulNumChannels, unsigned long ulNumSamples )
{
    m_ulNumChannels = ulNumChannels;

    // Allocate a buffer for each channel
    m_ppChannel = new float*[ m_ulNumChannels ];
    if( !m_ppChannel )
    {
        return false;
    }

    // Allocate samples for each channel
    for( unsigned long i = 0; i < ulNumChannels; i++ )
    {
        m_ppChannel[i] = new float[ulNumSamples];
        if( !m_ppChannel[i] )
            return false;
    }

    return true;

}


//--------------------------------------------------------------------------------------
float* CAudioData::GetChannelPtr( unsigned long ulChannel )
{
    if( ulChannel >= m_ulNumChannels )
        return NULL;

    return m_ppChannel[ ulChannel ];
}

//--------------------------------------------------------------------------------------
// Normalize the data
// This is an irreversible operation!
//--------------------------------------------------------------------------------------
bool CAudioData::NormalizeData()
{
    for( unsigned long ulChannel = 0; ulChannel < m_ulNumChannels; ulChannel++ )
    {
        // Find the maximum
        float fSampleMax = 0.0f;
        for( unsigned long i = 0; i < m_ulNumSamples; i++ )
        {
            if( fabs( m_ppChannel[ ulChannel ][i] ) > fSampleMax )
                fSampleMax = fabs( m_ppChannel[ ulChannel ][i] );
        }

        // Normalize
        if( fSampleMax != 0.0f )
        {
            for( unsigned long i = 0; i < m_ulNumSamples; i++ )
            {
                m_ppChannel[ ulChannel ][i] /= fSampleMax;
            }
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
unsigned long CAudioData::FindStartingPoint()
{
    unsigned long ulClosestPoint = m_ulNumSamples + 1;
    for( unsigned long ulChannel = 0; ulChannel < m_ulNumChannels; ulChannel++ )
    {
        for( unsigned long i = 0; i < m_ulNumSamples; i++ )
        {
            if( m_ppChannel[ ulChannel ][i] != 0 )
            {
                if( i < ulClosestPoint )
                    ulClosestPoint = i;
                i = m_ulNumSamples; // Exit the loop
            }
        }
    }

    return ulClosestPoint;
}



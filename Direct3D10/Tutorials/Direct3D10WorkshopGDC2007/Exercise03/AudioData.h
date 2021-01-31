//--------------------------------------------------------------------------------------
// File: AudioData.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "WaveFile.h"

class CAudioData
{
private:
    unsigned long m_ulNumSamples;
    unsigned long m_ulNumChannels;
    float** m_ppChannel;
    unsigned char* m_pBits;
    unsigned long m_ulBitsSize;
    WAVEFORMATEX m_wfx;

public:
    inline unsigned long    GetNumSamples()
    {
        return m_ulNumSamples;
    }
    inline unsigned long    GetNumChannels()
    {
        return m_ulNumChannels;
    }

private:
    void                    SplitDataIntoChannels_8Bit( unsigned char* pData );
    void                    SplitDataIntoChannels_16Bit( unsigned char* pData );
    void                    SplitDataIntoChannels_32Bit( unsigned char* pData );
    void                    SplitDataIntoChannels_IEEEFloat( unsigned char* pData );

public:
                            CAudioData();
                            ~CAudioData();

    void                    Cleanup();
    bool                    LoadWaveFile( TCHAR* szWave );
    bool                    CreateDataSpace( unsigned long ulNumChannels, unsigned long ulNumSamples );
    float* GetChannelPtr( unsigned long ulChannel );
    bool                    GetRawAudioData( unsigned long ulByteOffset, unsigned long ulSize,
                                             unsigned char** ppBytes1, unsigned long* pulSize1,
                                             unsigned char** ppBytes2, unsigned long* pulSize2 );
    bool                    GetChannelData( unsigned long ulChannel,
                                            unsigned long ulSampleOffset, unsigned long ulSampleSize,
                                            float** ppSamples1, unsigned long* pulSampleSize1,
                                            float** ppSamples2, unsigned long* pulSampleSize2 );

    bool                    NormalizeData();
    unsigned long           FindStartingPoint();
    unsigned char* GetRawBits()
    {
        return m_pBits;
    }
    unsigned long           GetBitsSize()
    {
        return m_ulBitsSize;
    }
    WAVEFORMATEX            GetFormat()
    {
        return m_wfx;
    };
};

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
    void                    Cleanup();
    void                    SplitDataIntoChannels_8Bit( unsigned char* pData );
    void                    SplitDataIntoChannels_16Bit( unsigned char* pData );
    void                    SplitDataIntoChannels_32Bit( unsigned char* pData );
    void                    SplitDataIntoChannels_IEEEFloat( unsigned char* pData );

public:
                            CAudioData();
                            ~CAudioData();

    bool                    LoadWaveFile( TCHAR* szWave );
    bool                    CreateDataSpace( unsigned long ulNumChannels, unsigned long ulNumSamples );
    float* GetChannelPtr( unsigned long ulChannel );
    bool                    NormalizeData();
    unsigned long           FindStartingPoint();
};

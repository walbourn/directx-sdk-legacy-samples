//--------------------------------------------------------------------------------------
// MemoryMapping.h
// PIX Pluging for PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef _MEMORY_MAPPING_H_
#define _MEMORY_MAPPING_H_

struct SHARED_MEMORY_DATA
{
    float Scaling;
    float EffectLimit;
    float EffectLife;
    float Normalization;
    float UpdateFrequency;
    int StreamOut;
    int ScreenEffects;
    int Fireworks;
};

bool                g_bSharedMemoryInit = false;
HANDLE              g_hMapFile = NULL;
SHARED_MEMORY_DATA* g_pSharedData = NULL;
WCHAR g_szSharedName[] = L"Local\\D3DWorkshopGDC2007";

#endif

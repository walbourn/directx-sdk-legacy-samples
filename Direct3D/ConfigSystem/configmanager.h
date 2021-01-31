//--------------------------------------------------------------------------------------
// File: ConfigManager.h
//
// Header file containing configuration-related declarations.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef __CONFIGMANAGER_H__
#define __CONFIGMANAGER_H__


HRESULT GetDXVersion( DWORD* pdwDirectXVersionMajor, DWORD* pdwDirectXVersionMinor, WCHAR* pcDirectXVersionLetter );


//--------------------------------------------------------------------------------------
// Class CConfigManager encapsulates configuration flags and methods for loading
// these flags.
//--------------------------------------------------------------------------------------
class CConfigManager
{
public:
    // Configuration Flags
    // These flags are initialized from device detection result and the configuration
    // database file.

    DWORD cf_ForceShader;
    DWORD cf_MaximumResolution;
    DWORD cf_LinearTextureAddressing;
    DWORD cf_UseFixedFunction;
    DWORD cf_DisableSpecular;
    DWORD cf_PrototypeCard;
    DWORD cf_UnsupportedCard;
    DWORD cf_OldDriver;
    DWORD cf_OldSoundDriver;
    DWORD cf_InvalidDriver;
    DWORD cf_InvalidSoundDriver;
    DWORD cf_SafeMode;
    DWORD cf_UseAnisotropicFilter;
    DWORD cf_DisableDriverManagement;
    DWORD cf_DisableRenderTargets;
    DWORD cf_DisableAlphaRenderTargets;
    DWORD cf_DoNotUseMinMaxBlendOp;
    DWORD cf_EnableStopStart;
    char* VendorName;
    char* DeviceName;
    DWORD DeviceID;
    DWORD VendorID;
    DWORD DriverVersionLowPart;
    DWORD DriverVersionHighPart;
    DWORD SysMemory;
    DWORD CpuSpeed;
    DWORD VideoMemory;
    char* AppendText;
    SOUND_DEVICE Sound_device;
    DWORD req_CPUSpeed;
    DWORD req_Memory;
    DWORD req_VideoMemory;
    DWORD req_DirectXMajor;
    DWORD req_DirectXMinor;
    int req_DirectXLetter;
    DWORD req_DiskSpace;
    DWORD req_OSMajor;
    DWORD req_OSMinor;

public:
                    CConfigManager();
    void            InitConfigProperties();
    void            InitConfigInformation( D3DCAPS9& d3dCaps );
    HRESULT         VerifyRequirements();
    HRESULT         Initialize( WCHAR* FileName, D3DADAPTER_IDENTIFIER9& id, D3DCAPS9& Caps,
                                REFGUID DSoundDeviceGuid );
    bool            InitMaximumResolution( const char* value );
    bool            InitForceShader( const char* value );
    bool            InitDisableBuffering( const char* value );
    bool            InitLinearTextureAddressing( const char* value );
    bool            InitUseAnisotropicFilter( const char* value );
    bool            InitDisableSpecular( const char* value );
    bool            InitUseFixedFunction( const char* value );
    bool            InitDisableRenderTargets( const char* value );
    bool            InitDisableAlphaRenderTargets( const char* value );
    bool            InitPrototypeCard( const char* value );
    bool            InitUnsupportedCard( const char* value );
    bool            InitOldDriver( const char* value );
    bool            InitEnableStopStart( const char* value );
    bool            InitOldSoundDriver( const char* value );
    bool            InitInvalidDriver( const char* value );
    bool            InitInvalidSoundDriver( const char* value );
    bool            InitSafeMode( const char* value );
    bool            InitDisableDriverManagement( const char* value );
    bool            InitUMA( const char* value );
    bool            InitDoNotUseMinMaxBlendOp( const char* value );

    typedef struct
    {
        GUID guid;
        char DriverName[32];
        DWORD MemorySize;
    } VIDEODEVICES;

    VIDEODEVICES    m_AdapterArray[10];  // Maximum 10 devices supported
    int m_nAdapterCount;
};


#endif

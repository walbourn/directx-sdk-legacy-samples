## Security

THESE SAMPLES ARE FROM THE LEGACY DIRECTX SDK AND ARE PROVIDED ON AN "AS IS" BASIS FOR REFERENCE AND DEVELOPER EDUCATION ONLY.

### Legacy DirectX SDK Components

The legacy DirectX SDK is out of its support lifecycle, and no longer receives any security updates. This includes D3DX9, D3DX10, D3DX11, D3DCompiler 43 and prior, XInput 1.1 - 1.3, XAudio 2.7 and earlier, X3DAudio 1.0 - 1.7, and all versions of XACT. All these components are deployed by the end-of-life *DirectX End-User Runtime* (a.k.a. DXSETUP), and the payload DLLs are only available SHA-1 signed. **SHA-1 signing is deprecated due to known security weaknesses**.

> DirectDraw, Direct3D, DirectSound, DirectInput, and other DirectX technologies are part of the Windows OS and subject to the OS lifecycle and security policy.

#### Mitigations

* The recommended solution is to avoid using the legacy DirectX SDK for development. The DirectX technologies are supported through the Windows SDK, and serviced as part of the OS. Modern versions of utility code can be obtained as open source from GitHub.

* For older projects that use the D3DX9, D3DX10, D3DX11, and/or D3DCompiler 43 versions, you can make use of the [Microsoft.DXSDK.D3DX](https://www.nuget.org/packages/Microsoft.DXSDK.D3DX) NuGet Package. These payload files are identical to the original June 2010 DirectX SDK, but have been resigned using SHA-256. They can also be redistributed application local. *The binaries themselves are unchanged and are not receiving security updates.*

* For Win32 desktop apps released through the Windows Store, a *Directx Framework* package is available for [x86](https://aka.ms/directx_x86_appx) and [x64](https://aka.ms/directx_x64_appx) which provide the original June 2010 DirectX SDK redistributable files using SHA-256 code signing. *The binaries themselves are unchanged and are not receiving security updates.*

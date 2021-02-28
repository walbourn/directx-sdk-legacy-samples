//----------------------------------------------------------------------------
// File: PlaySound.cpp
//
// Desc: The PlaySound sample shows how to load and play a wave file using
//       a DirectSound buffer.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKsound.h"
#include <commdlg.h>
#include "resource.h"




//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
VOID    OnInitDialog( HWND hDlg );
VOID    OnOpenSoundFile( HWND hDlg );
HRESULT OnPlaySound( HWND hDlg );
HRESULT PlayBuffer( BOOL bLooped );
VOID    OnTimer( HWND hDlg );
VOID    EnablePlayUI( HWND hDlg, BOOL bEnable );




//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
CSoundManager* g_pSoundManager = NULL;
CSound*        g_pSound = NULL;
BOOL           g_bBufferPaused;




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
INT APIENTRY wWinMain( HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR pCmdLine, 
                      INT nCmdShow )
{
    // Display the main dialog box.
    DialogBox( hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDlgProc );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: MainDlgProc()
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    HRESULT hr;

    switch( msg ) 
    {
        case WM_INITDIALOG:
            OnInitDialog( hDlg );
            break;

        case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDC_SOUNDFILE:
                    OnOpenSoundFile( hDlg );
                    break;

                case IDCANCEL:
                    EndDialog( hDlg, IDCANCEL );
                    break;

                case IDC_PLAY:
                    // The 'play'/'pause' button was pressed
                    if( FAILED( hr = OnPlaySound( hDlg ) ) )
                    {
                        DXTRACE_ERR_MSGBOX( TEXT("OnPlaySound"), hr );
                        MessageBox( hDlg, L"Error playing DirectSound buffer. "
                                    L"Sample will now exit.", L"DirectSound Sample", 
                                    MB_OK | MB_ICONERROR );
                        EndDialog( hDlg, IDABORT );
                    }
                    break;

                case IDC_STOP:
                    if( g_pSound )
                    {
                        g_pSound->Stop();
                        g_pSound->Reset();
                    }

                    EnablePlayUI( hDlg, TRUE );
                    break;

                default:
                    return FALSE; // Didn't handle message
            }
            break;

        case WM_TIMER:
            OnTimer( hDlg );
            break;

        case WM_DESTROY:
            // Cleanup everything
            KillTimer( hDlg, 1 );    
            SAFE_DELETE( g_pSound );
            SAFE_DELETE( g_pSoundManager );
            break; 

        default:
            return FALSE; // Didn't handle message
    }

    return TRUE; // Handled message
}




//-----------------------------------------------------------------------------
// Name: OnInitDialog()
// Desc: Initializes the dialogs (sets up UI controls, etc.)
//-----------------------------------------------------------------------------
VOID OnInitDialog( HWND hDlg )
{
    HRESULT hr;

    // Load the icon
#ifdef _WIN64
    HINSTANCE hInst = (HINSTANCE) GetWindowLongPtr( hDlg, GWLP_HINSTANCE );
#else
    HINSTANCE hInst = (HINSTANCE) GetWindowLong( hDlg, GWL_HINSTANCE );
#endif
    HICON hIcon = LoadIcon( hInst, MAKEINTRESOURCE( IDR_MAINFRAME ) );

    // Set the icon for this dialog.
    SendMessage( hDlg, WM_SETICON, ICON_BIG,   (LPARAM) hIcon );  // Set big icon
    SendMessage( hDlg, WM_SETICON, ICON_SMALL, (LPARAM) hIcon );  // Set small icon

    // Create a static IDirectSound in the CSound class.  
    // Set coop level to DSSCL_PRIORITY, and set primary buffer 
    // format to stereo, 22kHz and 16-bit output.
    g_pSoundManager = new CSoundManager();
    if( NULL == g_pSoundManager )
    {
        DXTRACE_ERR_MSGBOX( TEXT("Initialize"), E_OUTOFMEMORY );
        EndDialog( hDlg, IDABORT );
        return;
    }

    if( FAILED( hr = g_pSoundManager->Initialize( hDlg, DSSCL_PRIORITY ) ) )
    {
        DXTRACE_ERR_MSGBOX( TEXT("Initialize"), hr );
        MessageBox( hDlg, L"Error initializing DirectSound.  Sample will now exit.", 
                          L"DirectSound Sample", MB_OK | MB_ICONERROR );
        EndDialog( hDlg, IDABORT );
        return;
    }
    
    if( FAILED( hr = g_pSoundManager->SetPrimaryBufferFormat( 2, 22050, 16 ) ) )
    {
        DXTRACE_ERR_MSGBOX( TEXT("SetPrimaryBufferFormat"), hr );
        MessageBox( hDlg, L"Error initializing DirectSound.  Sample will now exit.", 
                          L"DirectSound Sample", MB_OK | MB_ICONERROR );
        EndDialog( hDlg, IDABORT );
        return;
    }

    g_bBufferPaused = FALSE;

    // Create a timer, so we can check for when the soundbuffer is stopped
    SetTimer( hDlg, 0, 250, NULL );

    // Set the UI controls
    SetDlgItemText( hDlg, IDC_FILENAME, TEXT("No file loaded.") );
}




//-----------------------------------------------------------------------------
// Name: OnOpenSoundFile()
// Desc: Called when the user requests to open a sound file
//-----------------------------------------------------------------------------
VOID OnOpenSoundFile( HWND hDlg ) 
{
    HRESULT hr;

    static TCHAR strFileName[MAX_PATH] = TEXT("");
    static TCHAR strPath[MAX_PATH] = TEXT("");

    // Setup the OPENFILENAME structure
    OPENFILENAME ofn = { sizeof(OPENFILENAME), hDlg, NULL,
                         TEXT("Wave Files\0*.wav\0All Files\0*.*\0\0"), NULL,
                         0, 1, strFileName, MAX_PATH, NULL, 0, strPath,
                         TEXT("Open Sound File"),
                         OFN_FILEMUSTEXIST|OFN_HIDEREADONLY, 0, 0,
                         TEXT(".wav"), 0, NULL, NULL };

    // Get the default media path (something like C:\WINDOWS\MEDIA)
    if( '\0' == strPath[0] )
    {
        if( GetWindowsDirectory( strPath, MAX_PATH ) != 0 )
        {
            wcscat_s( strPath, MAX_PATH, TEXT("\\MEDIA") );
        }
    }

    if( g_pSound )
    {
        g_pSound->Stop();
        g_pSound->Reset();
    }

    // Update the UI controls to show the sound as loading a file
    EnableWindow(  GetDlgItem( hDlg, IDC_PLAY ), FALSE);
    EnableWindow(  GetDlgItem( hDlg, IDC_STOP ), FALSE);
    SetDlgItemText( hDlg, IDC_FILENAME, TEXT("Loading file...") );

    // Display the OpenFileName dialog. Then, try to load the specified file
    if( TRUE != GetOpenFileName( &ofn ) )
    {
        SetDlgItemText( hDlg, IDC_FILENAME, TEXT("Load aborted.") );
        return;
    }

    SetDlgItemText( hDlg, IDC_FILENAME, TEXT("") );

    // Free any previous sound, and make a new one
    SAFE_DELETE( g_pSound );

    // Verify the file is small
    HANDLE hFile = CreateFile( strFileName, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if( hFile != NULL )
    {
        // If you try to open a 100MB wav file, you could run out of system memory with this
        // sample cause it puts all of it into a large buffer.  If you need to do this, then 
        // see the "StreamData" sample to stream the data from the file into a sound buffer.
        DWORD dwFileSizeHigh = 0;
        DWORD dwFileSize = GetFileSize( hFile, &dwFileSizeHigh );
        CloseHandle( hFile );

        if( dwFileSizeHigh != 0 || dwFileSize > 1000000 )
        {
            SetDlgItemText( hDlg, IDC_FILENAME, TEXT("File too large.  You should stream large files.") );
            return;
        }
    }

    // Load the wave file into a DirectSound buffer
    if( FAILED( hr = g_pSoundManager->Create( &g_pSound, strFileName, 0, GUID_NULL ) ) )
    {
        // Not a critical failure, so just update the status
        DXTRACE_ERR( TEXT("Create"), hr );
        SetDlgItemText( hDlg, IDC_FILENAME, TEXT("Could not create sound buffer.") );
        return; 
    }

    // Update the UI controls to show the sound as the file is loaded
    SetDlgItemText( hDlg, IDC_FILENAME, strFileName );
    EnablePlayUI( hDlg, TRUE );

    // Remember the path for next time
    wcscpy_s( strPath, MAX_PATH, strFileName );
    WCHAR* strLastSlash = wcsrchr( strPath, '\\' );
    if( strLastSlash )
        strLastSlash[0] = '\0';
}




//-----------------------------------------------------------------------------
// Name: OnPlaySound()
// Desc: User hit the "Play" button
//-----------------------------------------------------------------------------
HRESULT OnPlaySound( HWND hDlg ) 
{
    HRESULT hr;

    HWND hLoopButton = GetDlgItem( hDlg, IDC_LOOP_CHECK );
    BOOL bLooped = ( SendMessage( hLoopButton, BM_GETSTATE, 0, 0 ) == BST_CHECKED );

    if( g_bBufferPaused )
    {
        // Play the buffer since it is currently paused
        DWORD dwFlags = bLooped ? DSBPLAY_LOOPING : 0L;
        if( FAILED( hr = g_pSound->Play( 0, dwFlags ) ) )
            return DXTRACE_ERR( TEXT("Play"), hr );

        // Update the UI controls to show the sound as playing
        g_bBufferPaused = FALSE;
        EnablePlayUI( hDlg, FALSE );
    }
    else
    {
        if( g_pSound->IsSoundPlaying() )
        {
            // To pause, just stop the buffer, but don't reset the position
            if( g_pSound )
                g_pSound->Stop();

            g_bBufferPaused = TRUE;
            SetDlgItemText( hDlg, IDC_PLAY, L"Play" );
        }
        else
        {
            // The buffer is not playing, so play it again
            DWORD dwFlags = bLooped ? DSBPLAY_LOOPING : 0L;
            if( FAILED( hr = g_pSound->Play( 0, dwFlags ) ) )
                return DXTRACE_ERR( TEXT("Play"), hr );

            // Update the UI controls to show the sound as playing
            g_bBufferPaused = FALSE;
            EnablePlayUI( hDlg, FALSE );
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnTimer()
// Desc: When we think the sound is playing this periodically checks to see if 
//       the sound has stopped.  If it has then updates the dialog.
//-----------------------------------------------------------------------------
VOID OnTimer( HWND hDlg ) 
{
    if( IsWindowEnabled( GetDlgItem( hDlg, IDC_STOP ) ) )
    {
        // We think the sound is playing, so see if it has stopped yet.
        if( !g_pSound->IsSoundPlaying() ) 
        {
            // Update the UI controls to show the sound as stopped
            EnablePlayUI( hDlg, TRUE );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: EnablePlayUI( hDlg,)
// Desc: Enables or disables the Play UI controls 
//-----------------------------------------------------------------------------
VOID EnablePlayUI( HWND hDlg, BOOL bEnable )
{
    if( bEnable )
    {
        EnableWindow(   GetDlgItem( hDlg, IDC_LOOP_CHECK ), TRUE );
        EnableWindow(   GetDlgItem( hDlg, IDC_STOP ),       FALSE );

        EnableWindow(   GetDlgItem( hDlg, IDC_PLAY ),       TRUE );
        SetFocus(       GetDlgItem( hDlg, IDC_PLAY ) );
        SetDlgItemText( hDlg, IDC_PLAY, L"&Play" );
    }
    else
    {
        EnableWindow(  GetDlgItem( hDlg, IDC_LOOP_CHECK ), FALSE );
        EnableWindow(  GetDlgItem( hDlg, IDC_STOP ),       TRUE );
        SetFocus(      GetDlgItem( hDlg, IDC_STOP ) );

        EnableWindow(  GetDlgItem( hDlg, IDC_PLAY ),       TRUE );
        SetDlgItemText( hDlg, IDC_PLAY, L"&Pause" );
    }
}








//--------------------------------------------------------------------------------------
// File: ModelViewerTouchCamera.h
//
// This class extends the Modelview camera and enables multi touch control
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Multi-touch requires Windows 7 


#include "WinUser.h"
#include "DXUTCamera.h"

//--------------------------------------------------------------------------------------
// Extension of the Simple model viewing camera that handles Windows 7 touch input
//--------------------------------------------------------------------------------------

class CModelViewerCamera;

const float g_fCameraSelectionPadding = 0.01f;
const float g_fWheelMouseScale = 0.1f;

class CModelViewerTouchCamera : public CModelViewerCamera
{
public:
    CModelViewerTouchCamera ();
    ~CModelViewerTouchCamera ();
    bool HandleTouchMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, 
        INT cInputs, TOUCHINPUT *pInputs, const RECT& window );
};


CModelViewerTouchCamera::CModelViewerTouchCamera() {

};    

CModelViewerTouchCamera::~CModelViewerTouchCamera () {

};

enum TouchCameraState {
    TC_INIT,
    TC_SECOND_INPUT_RECEIVED,
    TC_ZOOM,
    TC_LOOK
};

//--------------------------------------------------------------------------------------
// This function moves the camera based on WM_TOUCH messages.   
// 
//--------------------------------------------------------------------------------------
bool CModelViewerTouchCamera::HandleTouchMessages( HWND hWnd, 
                                                   UINT uMsg,
                                                   WPARAM wParam, 
                                                   LPARAM lParam, 
                                                   INT nInputs, 
                                                   TOUCHINPUT *pInputs, 
                                                   const RECT &window ) {
   
    static D3DXVECTOR2 vLastTouchInputs[2];
    static TouchCameraState touchCameraState = TC_INIT;
    static D3DXVECTOR2 vCurrentTouchInputs[2];
    static float fpMouseWheelDelta;
     
    // When the number of touch inputs is 2 or greater the users is attempting to move the camera.
    // The help texts for this sample explains that that you must use 3 fingers. However this sample 
    // only looks at the frist two inputs.  Some camera based touch monitors will loose one finger due to
    // deficiencies in this technology.  When three fingers are used the sample works in a more robust way.
    
    if ( nInputs > 1 ) {
        TOUCHINPUT ti = pInputs[0];
        
        // The main block of camera code operates upon the Touch Move message. 
        if( ti.dwFlags & TOUCHEVENTF_MOVE )
        {   
            
            // truncate inputs to screen space and average them.
            
            INT iMouseX = (INT) ( pInputs[0].x * .005 + pInputs[1].x *.005 ) - window.left;
            INT iMouseY = (INT) ( pInputs[0].y * .005 + pInputs[1].y *.005 ) - window.top;
            
            // get a copy of the inputs
            vCurrentTouchInputs[0].x =(float)pInputs[0].x;
            vCurrentTouchInputs[0].y =(float)pInputs[0].y;
            vCurrentTouchInputs[1].x = (float)pInputs[1].x;
            vCurrentTouchInputs[1].y = (float)pInputs[1].y;

            // Calculate the zoom delta.  This is fed into the standard touch camera as the scroll wheel parameter
            D3DXVECTOR2 diff1 = vCurrentTouchInputs[0] - vCurrentTouchInputs[1];
            D3DXVECTOR2 diff2 = vLastTouchInputs[0] - vLastTouchInputs[1];
            float len1 = D3DXVec2Length(&diff1);
            float len2 = D3DXVec2Length(&diff2);
            fpMouseWheelDelta = len2 - len1;

            
            // On frame #2 the program can determine if the user is pinching to zoom, or moving 2 fingers to rotate the camera
            if ( touchCameraState == TC_SECOND_INPUT_RECEIVED ) {
                D3DXVECTOR2 finger1delta = vCurrentTouchInputs[0] - vLastTouchInputs[0];
                D3DXVECTOR2 finger2delta = vCurrentTouchInputs[1] - vLastTouchInputs[1];
                D3DXVECTOR2 finger1delta_norm, finger2delta_norm;
                D3DXVec2Normalize( &finger1delta_norm, &finger1delta );
                D3DXVec2Normalize( &finger2delta_norm, &finger2delta );
                
                float inphase = D3DXVec2Dot( &finger1delta_norm, &finger2delta_norm );
                // If the dot product is positive then the two touch inputs are moving in parallel.    
                if ( inphase > g_fCameraSelectionPadding ) 
                {
                    touchCameraState = TC_LOOK;
                    m_ViewArcBall.OnBegin( iMouseX, iMouseY );
                }
                // if the dot product is negative the two inputs are coming to gether or moving appart
                else if ( inphase < -g_fCameraSelectionPadding ) 
                {
                    touchCameraState = TC_ZOOM;
                }
                // one or both inputs did not register correctly.
                else 
                {
                    touchCameraState = TC_SECOND_INPUT_RECEIVED;
                }
            }
            
            // This class inherits from a mouse based camera.
            // the delta is used to simulate the mouse wheel scrolling
            if ( touchCameraState == TC_ZOOM ) 
            {
                if ( fabs ( fpMouseWheelDelta ) )
                m_nMouseWheelDelta -= ( INT) ( fpMouseWheelDelta * g_fWheelMouseScale );
            }
            // The touch locations are given to the arc ball camera.
            else if ( touchCameraState == TC_LOOK )
            {
                m_ViewArcBall.OnMove( iMouseX, iMouseY );
            }
            vLastTouchInputs[0].x = (float)pInputs[0].x;
            vLastTouchInputs[0].y = (float)pInputs[0].y;
            vLastTouchInputs[1].x = (float)pInputs[1].x;
            vLastTouchInputs[1].y = (float)pInputs[1].y;
            
            if ( touchCameraState == TC_INIT ) touchCameraState = TC_SECOND_INPUT_RECEIVED;
        }

        bool bTouchUpDetected = false;
        
        for ( INT index=0; index < nInputs; ++index ) {
            if ( pInputs[index].dwFlags & TOUCHEVENTF_UP ) bTouchUpDetected = true;
        }
        // If a TOUCHEVENTF_UP was received then send the end message to the camera.
        if ( bTouchUpDetected ) { 
            m_ViewArcBall.OnEnd();
            touchCameraState = TC_INIT;
        }
            
        if( 
            ti.dwFlags & TOUCHEVENTF_DOWN || ti.dwFlags & TOUCHEVENTF_UP || ti.dwFlags & TOUCHEVENTF_MOVE
            )
        {
            // This flag tells the camera to udpate.
            m_bDragSinceLastUpdate = true;
        }
    }   
    return touchCameraState > TC_SECOND_INPUT_RECEIVED;
}


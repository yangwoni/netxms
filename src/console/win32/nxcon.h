// nxcon.h : main header file for the NXCON application
//

#if !defined(AFX_NXCON_H__A5C08A87_42D8_47F3_8F69_9566830EF29E__INCLUDED_)
#define AFX_NXCON_H__A5C08A87_42D8_47F3_8F69_9566830EF29E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif


//
// Common inline functions
//

inline BOOL SafeFreeResource(HGLOBAL hRes)
{
   return (hRes != NULL) ? FreeResource(hRes) : FALSE;
}


#include <nxclapi.h>
#include <nms_util.h>
#include "resource.h"      // Main symbols
#include "globals.h"       // Global symbols
#include "ControlPanel.h"
#include "EventBrowser.h"
#include "EventEditor.h"
#include "ObjectBrowser.h"
#include "MapFrame.h"
#include "DebugFrame.h"
#include "ProgressDialog.h"	// Added by ClassView
#include "NodePropsGeneral.h"
#include "ObjectPropCaps.h"
#include "ObjectPropSheet.h"
#include "RequestProcessingDlg.h"
#include "ObjectPropsGeneral.h"
#include "ObjectPropsSecurity.h"
#include "UserEditor.h"


//
// Request waiting structure
//

struct RQ_WAIT_INFO
{
   HREQUEST hRequest;
   CWnd *pWnd;
};


/////////////////////////////////////////////////////////////////////////////
// CConsoleApp:
// See nxcon.cpp for the implementation of this class
//

class CConsoleApp : public CWinApp
{
public:
	CConsoleApp();
   virtual ~CConsoleApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConsoleApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
protected:
	DWORD m_dwClientState;
	CEventBrowser *m_pwndEventBrowser;
   CEventEditor *m_pwndEventEditor;
   CUserEditor *m_pwndUserEditor;
	CWnd* m_pwndObjectBrowser;
	CWnd *m_pwndCtrlPanel;
   CDebugFrame *m_pwndDebugWindow;

   BOOL m_bAuthFailed;
	CProgressDialog m_dlgProgress;
   DWORD m_dwRqWaitListSize;
   RQ_WAIT_INFO *m_pRqWaitList;
   MUTEX m_mutexRqWaitList;

   HMENU m_hMDIMenu;             // Default menu for MDI
	HACCEL m_hMDIAccel;           // Default accelerator for MDI
	HMENU m_hEventBrowserMenu;    // Menu for event browser
	HACCEL m_hEventBrowserAccel;  // Accelerator for event browser
	HMENU m_hObjectBrowserMenu;   // Menu for object browser
	HACCEL m_hObjectBrowserAccel; // Accelerator for object browser
	
public:
	void RegisterRequest(HREQUEST hRequest, CWnd *pWnd);
	void EventHandler(DWORD dwEvent, DWORD dwCode, void *pArg);
	void OnViewDestroy(DWORD dwView, CWnd *pWnd);
	void OnViewCreate(DWORD dwView, CWnd *pWnd);
   DWORD GetClientState(void) { return m_dwClientState; }
	//{{AFX_MSG(CConsoleApp)
	afx_msg void OnAppAbout();
	afx_msg void OnViewControlpanel();
	afx_msg void OnViewEvents();
	afx_msg void OnViewMap();
	afx_msg void OnConnectToServer();
	afx_msg void OnViewObjectbrowser();
	afx_msg void OnControlpanelEvents();
	afx_msg void OnViewDebug();
	afx_msg void OnControlpanelUsers();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	void OnRequestComplete(HREQUEST hRequest, DWORD dwRetCode);
	BOOL m_bEventBrowserActive;
	BOOL m_bEventEditorActive;
	BOOL m_bUserEditorActive;
	BOOL m_bObjectBrowserActive;
	BOOL m_bCtrlPanelActive;
   BOOL m_bDebugWindowActive;

   // Inline functions
public:
	DWORD WaitForRequest(HREQUEST hRequest, char *pszMessage = NULL);
	void ObjectProperties(DWORD dwObjectId);
	void DebugPrintf(char *szFormat, ...);
   void DebugCallback(char *pszMsg)
   {
      if (m_bDebugWindowActive)
         m_pwndDebugWindow->AddMessage(pszMsg);
   }
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NXCON_H__A5C08A87_42D8_47F3_8F69_9566830EF29E__INCLUDED_)

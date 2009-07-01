/*

Facebook plugin for Miranda Instant Messenger
_____________________________________________

Copyright � 2009 Michal Zelinka

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Revision       : $Revision$
Last change by : $Author$
Last change on : $Date$

*/

#include "common.h"

FacebookProto::FacebookProto(const char* proto_name,const TCHAR* username)
{
	m_szProtoName  = mir_strdup( proto_name );
	m_szModuleName = mir_strdup( proto_name );
	m_tszUserName  = mir_tstrdup( username );
	m_szAccountId  = "1627557216"; // TODO: set on login/cookie handling

	CreateProtoService(m_szModuleName,PS_CREATEACCMGRUI,
		&FacebookProto::SvcCreateAccMgrUI,this);
	CreateProtoService(m_szModuleName,PS_GETNAME,  &FacebookProto::GetName,   this);
	CreateProtoService(m_szModuleName,PS_GETSTATUS,&FacebookProto::GetStatus, this);

//	CreateProtoService(m_szModuleName,PS_JOINCHAT, &FacebookProto::OnJoinChat,    this);
//	CreateProtoService(m_szModuleName,PS_LEAVECHAT,&FacebookProto::OnLeaveChat,   this);

//	HookProtoEvent(ME_DB_CONTACT_DELETED,       &FacebookProto::OnContactDeleted,     this);
	HookProtoEvent(ME_CLIST_PREBUILDSTATUSMENU, &FacebookProto::OnBuildStatusMenu,    this);
	HookProtoEvent(ME_OPT_INITIALISE,           &FacebookProto::OnOptionsInit,        this);

	CreateProtoService("Facebook","/Test",
		&FacebookProto::Test,this);

	CLISTMENUITEM mi = {0};
	mi.cbSize = sizeof(CLISTMENUITEM);
	mi.flags = 0;
	mi.hIcon = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_FACEBOOK), IMAGE_ICON, 0, 0, 0);
	mi.pszContactOwner = NULL;
	mi.position = 1000000000;
	mi.pszName = Translate("Test Facebook");
	mi.pszService = "Facebook/Test";

	CallService(MS_CLIST_ADDMAINMENUITEM, 0, (LPARAM)&mi);
}

FacebookProto::~FacebookProto( )
{
	mir_free( m_szProtoName );
	mir_free( m_szModuleName );
	mir_free( m_tszUserName );

	if ( m_hNetlibUser )
		Netlib_CloseHandle( m_hNetlibUser );
	if ( m_hNetlibAvatar )
		Netlib_CloseHandle( m_hNetlibAvatar );
}

//////////////////////////////////////////////////////////////////////////////

DWORD FacebookProto::GetCaps( int type, HANDLE hContact )
{
	switch(type)
	{
	case PFLAGNUM_1:
		return PF1_IM | PF1_MODEMSGRECV | PF1_BASICSEARCH | PF1_SEARCHBYEMAIL;
	case PFLAGNUM_2:
		return PF2_ONLINE;
	case PFLAGNUM_3:
		return PF2_ONLINE;
	case PFLAGNUM_4:
		return PF4_NOCUSTOMAUTH | PF4_IMSENDUTF | PF4_AVATARS;
	case PFLAG_MAXLENOFMESSAGE:
		return 140;
	case PFLAG_UNIQUEIDTEXT:
		return (int) "Email";
	case PFLAG_UNIQUEIDSETTING:
		return (int) FACEBOOK_KEY_ID;
	}
	return 0;
}

HICON FacebookProto::GetIcon(int index)
{
	if(LOWORD(index) == PLI_PROTOCOL)
	{
		HICON ico = (HICON)CallService(MS_SKIN2_GETICON,0,(LPARAM)"Facebook_facebook");
		return CopyIcon(ico);
	}
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////////

int FacebookProto::SetStatus( int new_status )
{
	int old_status = m_iStatus;
	if ( new_status == m_iStatus )
		return 0;

	m_iDesiredStatus = new_status;

	if ( new_status == ID_STATUS_ONLINE )
	{
		if ( old_status == ID_STATUS_CONNECTING )
			return 0;

		m_iStatus = ID_STATUS_CONNECTING;
		ProtoBroadcastAck( m_szModuleName, 0, ACKTYPE_STATUS, ACKRESULT_SUCCESS,
			( HANDLE )old_status, m_iStatus );

		ForkThread( &FacebookProto::SignOn, this );
	}
	else if ( new_status == ID_STATUS_OFFLINE )
	{
		m_iStatus = m_iDesiredStatus;
		ProtoBroadcastAck( m_szModuleName, 0, ACKTYPE_STATUS, ACKRESULT_SUCCESS,
			( HANDLE ) old_status, m_iStatus );

		//SetAllContactStatuses( ID_STATUS_OFFLINE ); // TODO: Implement as a checking tool
		ForkThread( &FacebookProto::SignOff, this );
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

int FacebookProto::OnEvent(PROTOEVENTTYPE event,WPARAM wParam,LPARAM lParam)
{
	switch(event)
	{
	case EV_PROTO_ONLOAD:    return OnModulesLoaded(wParam,lParam);
	case EV_PROTO_ONEXIT:    return OnPreShutdown  (wParam,lParam);
	case EV_PROTO_ONOPTIONS: return OnOptionsInit  (wParam,lParam);
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////////
// EVENTS

int FacebookProto::GetName( WPARAM wParam, LPARAM lParam )
{
	lstrcpynA(reinterpret_cast<char*>(lParam),m_szProtoName,wParam);
	return 0;
}

int FacebookProto::GetStatus( WPARAM wParam, LPARAM lParam )
{
	return m_iStatus;
}

int FacebookProto::SvcCreateAccMgrUI(WPARAM wParam,LPARAM lParam)
{
	return (int)CreateDialogParam(g_hInstance,MAKEINTRESOURCE(IDD_FACEBOOKACCOUNT), 
		 (HWND)lParam, FBAccountProc, (LPARAM)this );
}

int FacebookProto::OnModulesLoaded(WPARAM wParam,LPARAM lParam)
{
	TCHAR descr[512];
    NETLIBUSER nlu = {sizeof(nlu)};
    nlu.flags = NUF_INCOMING | NUF_OUTGOING | NUF_HTTPCONNS | NUF_TCHAR;
    nlu.szSettingsModule = m_szModuleName;

	// Create standard network connection
	mir_sntprintf(descr,SIZEOF(descr),TranslateT("%s server connection"),m_tszUserName);
	nlu.ptszDescriptiveName = descr;
	m_hNetlibUser = (HANDLE)CallService(MS_NETLIB_REGISTERUSER,0,(LPARAM)&nlu);
	if(m_hNetlibUser == 0)
		MessageBoxA(0,"Unable to get Netlib connection for Facebook","",0);

	// Create avatar network connection (TODO: probably remove this)
	char module[512];
	mir_snprintf(module,SIZEOF(module),"%sAv",m_szModuleName);
	nlu.szSettingsModule = module;
	mir_sntprintf(descr,SIZEOF(descr),TranslateT("%s avatar connection"),m_tszUserName);
	nlu.ptszDescriptiveName = descr;
	m_hNetlibAvatar = (HANDLE)CallService(MS_NETLIB_REGISTERUSER,0,(LPARAM)&nlu);
	if(m_hNetlibAvatar == 0)
		MessageBoxA(0,"Unable to get avatar Netlib connection for Facebook","",0);

	facy.set_handle(m_hNetlibUser);

	// TODO: History events OR DB events

	return 0;
}

int FacebookProto::OnOptionsInit(WPARAM wParam,LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp = {sizeof(odp)};
	odp.position    = 271828;
	odp.hInstance   = g_hInstance;
	odp.ptszGroup   = LPGENT("Network");
	odp.ptszTitle   = m_tszUserName;
	odp.dwInitParam = LPARAM(this);
	odp.flags       = ODPF_BOLDGROUPS | ODPF_TCHAR;

	odp.ptszTab     = LPGENT("Basic");
    odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPTIONS);
	odp.pfnDlgProc  = FBOptionsProc;
	CallService(MS_OPT_ADDPAGE,wParam,(LPARAM)&odp);

	if(ServiceExists(MS_POPUP_ADDPOPUPT))
	{
		odp.ptszTab     = LPGENT("Popups");
		odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPTIONS_POPUPS);
		odp.pfnDlgProc  = FBPopupsProc;
		CallService(MS_OPT_ADDPAGE,wParam,(LPARAM)&odp);
	}

	return 0;
}

int FacebookProto::OnBuildStatusMenu(WPARAM wParam,LPARAM lParam)
{
	HGENMENU hRoot = pcli->pfnGetProtocolMenu(m_szModuleName);
	if (hRoot == NULL)
		return 0;

	CLISTMENUITEM mi = {sizeof(mi)};

	char text[200];
	strcpy(text,m_szModuleName);
	char *tDest = text+strlen(text);
	mi.pszService = text;

	mi.hParentMenu = hRoot;
	mi.flags = CMIF_ICONFROMICOLIB|CMIF_ROOTHANDLE;
	mi.position = 1001;

	HANDLE m_hMenuRoot = reinterpret_cast<HGENMENU>( CallService(
		MS_CLIST_ADDSTATUSMENUITEM,0,reinterpret_cast<LPARAM>(&mi)) );

	CreateProtoService(m_szModuleName,"/Mind",&FacebookProto::OnMind,this);
	strcpy(tDest,"/Mind");
	mi.pszName = LPGEN("Mind...");
	mi.popupPosition = 200001;
	mi.icolibItem = GetIconHandle("mind");
	HANDLE m_hMenuBookmarks = reinterpret_cast<HGENMENU>( CallService(
		MS_CLIST_ADDSTATUSMENUITEM,0,reinterpret_cast<LPARAM>(&mi)) );

	return 0;
}

int FacebookProto::OnMind(WPARAM,LPARAM)
{
	if ( !isOnline( ) ) // TODO: remove this, toggle icon in status menu
		return TRUE;

	HWND hDlg = CreateDialogParam( g_hInstance, MAKEINTRESOURCE( IDD_MIND ),
		 ( HWND )0, FBMindProc, reinterpret_cast<LPARAM>( this ) );
	ShowWindow( hDlg, SW_SHOW );
	return FALSE;
}

int FacebookProto::OnPreShutdown(WPARAM wParam,LPARAM lParam)
{
	Netlib_Shutdown( m_hNetlibUser );
	Netlib_Shutdown( m_hNetlibAvatar );
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

int FacebookProto::Test( WPARAM wparam, LPARAM lparam )
{
	facy.get_post_form_id( );
	facy.get_user_agent( );

	return FALSE;
}
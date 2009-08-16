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

File name      : $URL$
Revision       : $Revision$
Last change by : $Author$
Last change on : $Date$

*/

#pragma once

class FacebookProto : public PROTO_INTERFACE
{
public:
	FacebookProto( const char *proto_name, const TCHAR *username );
	~FacebookProto( );

	__inline void* operator new( size_t size )
	{
		return calloc( 1, size );
	}
	__inline void operator delete( void* p )
	{
		free( p );
	}

	inline const char* ModuleName( ) const
	{
		return m_szModuleName;
	}

	inline bool isOnline( )
	{
		return ( m_iStatus != ID_STATUS_OFFLINE && m_iStatus != ID_STATUS_CONNECTING );
	}

	//PROTO_INTERFACE

	virtual	HANDLE   __cdecl AddToList( int flags, PROTOSEARCHRESULT* psr ) { return NULL; }; //
	virtual	HANDLE   __cdecl AddToListByEvent( int flags, int iContact, HANDLE hDbEvent );

	virtual	int      __cdecl Authorize( HANDLE hDbEvent );
	virtual	int      __cdecl AuthDeny( HANDLE hDbEvent, const char* szReason );
	virtual	int      __cdecl AuthRecv( HANDLE hContact, PROTORECVEVENT* );
	virtual	int      __cdecl AuthRequest( HANDLE hContact, const char* szMessage );

	virtual	HANDLE   __cdecl ChangeInfo( int iInfoType, void* pInfoData );

	virtual	HANDLE   __cdecl FileAllow( HANDLE hContact, HANDLE hTransfer, const char* szPath );
	virtual	int      __cdecl FileCancel( HANDLE hContact, HANDLE hTransfer );
	virtual	int      __cdecl FileDeny( HANDLE hContact, HANDLE hTransfer, const char* szReason );
	virtual	int      __cdecl FileResume( HANDLE hTransfer, int* action, const char** szFilename );

	virtual	DWORD_PTR __cdecl GetCaps( int type, HANDLE hContact = NULL );
	virtual	HICON     __cdecl GetIcon( int iconIndex );
	virtual	int       __cdecl GetInfo( HANDLE hContact, int infoType ) { return NULL; };

	virtual	HANDLE    __cdecl SearchBasic( const char* id ) { return NULL; };
	virtual	HANDLE    __cdecl SearchByEmail( const char* email ) { return NULL; };
	virtual	HANDLE    __cdecl SearchByName( const char* nick, const char* firstName, const char* lastName );
	virtual	HWND      __cdecl SearchAdvanced( HWND owner );
	virtual	HWND      __cdecl CreateExtendedSearchUI( HWND owner );

	virtual	int       __cdecl RecvContacts( HANDLE hContact, PROTORECVEVENT* );
	virtual	int       __cdecl RecvFile( HANDLE hContact, PROTORECVFILE* );
	virtual	int       __cdecl RecvMsg( HANDLE hContact, PROTORECVEVENT* );
	virtual	int       __cdecl RecvUrl( HANDLE hContact, PROTORECVEVENT* );

	virtual	int       __cdecl SendContacts( HANDLE hContact, int flags, int nContacts, HANDLE* hContactsList );
	virtual	HANDLE    __cdecl SendFile( HANDLE hContact, const char* szDescription, char** ppszFiles );
	virtual	int       __cdecl SendMsg( HANDLE hContact, int flags, const char* msg );
	virtual	int       __cdecl SendUrl( HANDLE hContact, int flags, const char* url );

	virtual	int       __cdecl SetApparentMode( HANDLE hContact, int mode );
	virtual	int       __cdecl SetStatus( int new_status );

	virtual	HANDLE    __cdecl GetAwayMsg( HANDLE hContact );
	virtual	int       __cdecl RecvAwayMsg( HANDLE hContact, int mode, PROTORECVEVENT* evt );
	virtual	int       __cdecl SendAwayMsg( HANDLE hContact, HANDLE hProcess, const char* msg );
	virtual	int       __cdecl SetAwayMsg( int iStatus, const char* msg ); // TODO: Implement?

	virtual	int       __cdecl UserIsTyping( HANDLE hContact, int type );

	virtual	int       __cdecl OnEvent( PROTOEVENTTYPE iEventType, WPARAM wParam, LPARAM lParam );

	////////////////////////

	// Services
	int  __cdecl GetName( WPARAM, LPARAM );
	int  __cdecl GetStatus( WPARAM, LPARAM );
	int  __cdecl SvcCreateAccMgrUI( WPARAM, LPARAM );

	// Events
	int  __cdecl OnModulesLoaded( WPARAM, LPARAM );
	int  __cdecl OnOptionsInit( WPARAM, LPARAM );
	int  __cdecl OnBuildStatusMenu(WPARAM wParam,LPARAM lParam);
	int  __cdecl OnPrebuildContactMenu(WPARAM,LPARAM) { return NULL; };
	int  __cdecl OnContactDeleted(WPARAM,LPARAM) { return NULL; };
	int  __cdecl OnMind(WPARAM,LPARAM);
	int  __cdecl OnPreShutdown(WPARAM,LPARAM);

	// Worker threads
	void __cdecl AddToListWorker(void *p);
	void __cdecl SendSuccess(void *);
	void __cdecl DoSearch(void *);
	void __cdecl SignOn(void *);
	void __cdecl SignOff(void *);
	void __cdecl SendMindWorker(void *);
	void __cdecl GetAwayMsgWorker(void *);
	void __cdecl UpdateAvatarWorker(void *);
	void __cdecl UpdateInfoWorker(void *);

	// Processing threads
	void __cdecl ProcessUpdates( void* );
	void __cdecl ProcessMessages( void* );
	void         ProcessAvatar(HANDLE,const std::string &,bool force=false);

	// Message Loop
	bool    NegotiateConnection( );
	void __cdecl MessageLoop(void *);
	void __cdecl UpdateLoop(void *);
	BYTE    GetPollRate( );

	// Users handling
	bool IsMyContact(HANDLE);
	HANDLE UserIDToHContact(std::string);
	HANDLE AddToClientList(facebook_user*);
	void SetAllContactStatuses(int);
	void SetAllContactUpdates(std::map<std::string, facebook_user*>);

	// User
	facebook_communication  facy;
	int __cdecl Test( WPARAM, LPARAM );

	// Helpers
	std::string GetAvatarFolder();
	void ToggleStatusMenuItems( BOOL bEnable );

	// Handles, Locks

	HANDLE  m_hMenuRoot;
	HANDLE  m_hStatusMind;

	HANDLE  signon_lock_;
	HANDLE  avatar_lock_;
	HANDLE  facebook_lock_;
	HANDLE  log_lock_;

	HANDLE  m_hNetlibUser;
	HANDLE  m_hNetlibAvatar;
	HANDLE  m_hMsgLoop;
	HANDLE  m_hUpdLoop;

	std::string def_avatar_folder_;
	HANDLE hAvatarFolder_;

	static void CALLBACK APC_callback(ULONG_PTR p);

	int LOG(const char *fmt,...);
};
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

#include "common.h"

extern DWORD g_mirandaVersion;

facebook_client::facebook_client( )
{
	username_ = password_ = user_id_ = real_name_ = status_ = post_form_id_ = dtsg_ = "";
	chat_sequence_num_ = 0;
	first_touch_ = false;
}

http::response facebook_client::flap( const int request_type, char* request_data )
{
	NETLIBHTTPREQUEST nlhr = {sizeof( nlhr )};
	nlhr.requestType = choose_method( request_type );
	nlhr.szUrl = choose_request_url( request_type, request_data );
	nlhr.flags = NLHRF_NODUMP | choose_security_level( request_type ) | NLHRF_GENERATEHOST;
	nlhr.headers = get_request_headers( request_type, &nlhr.headersCount );

	if ( request_data != NULL )
	{
		nlhr.pData = request_data;
		nlhr.dataLength = strlen( nlhr.pData );
	}

	NETLIBHTTPREQUEST* pnlhr = ( NETLIBHTTPREQUEST* )CallService( MS_NETLIB_HTTPTRANSACTION, (WPARAM)handle_, (LPARAM)&nlhr );

	http::response resp;

	if ( pnlhr )
	{
		store_cookies( pnlhr->headers, pnlhr->headersCount );
		resp.code = pnlhr->resultCode;
		resp.data = pnlhr->pData ? pnlhr->pData : "";

		CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)pnlhr);
	}
	else
		resp.code = HTTP_CODE_FAKE_DISCONNECTED;
		// Better to have something set explicitely as this value
	    // is compaired in all communication requests

	// free data
	utils::mem::detract( &nlhr.szUrl );

//// TODO: headers leak
//	for ( int i = 0; i < nlhr.headersCount; i++ )
//	{
//		if ( !nlhr.headers[i].szName && !nlhr.headers[i].szValue )
//			break;
//
//		utils::mem::detract( &nlhr.headers[i].szName );
//		utils::mem::detract( &nlhr.headers[i].szValue );
//	}

	return resp;
}

bool facebook_client::validate_response( http::response* resp )
{
	if ( resp->code == HTTP_CODE_FAKE_DISCONNECTED )
		return false;
	
	if ( resp->data.find( "\"errorSummary\":\"Not Logged In\"" ) != string::npos )
	{
		resp->code = HTTP_CODE_FAKE_LOGGED_OUT;
		return false;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////

DWORD facebook_client::choose_security_level( int request_type )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
		return NLHRF_SSL;

	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_STATUS_GET:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
	case FACEBOOK_REQUEST_LOGOUT:
	default:
		return ( DWORD )0;
	}
}

int facebook_client::choose_method( int request_type )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
		return REQUEST_POST;

	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_STATUS_GET:
	case FACEBOOK_REQUEST_LOGOUT:
	default:
		return REQUEST_GET;
	}
}

char* facebook_client::choose_server( int request_type, char* data )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
		return FACEBOOK_SERVER_LOGIN;

	case FACEBOOK_REQUEST_MESSAGES_RECEIVE:
	{
		char* chatServer = ( char* )utils::mem::allocate( 64 * sizeof( char ) );
		mir_snprintf( chatServer, 64, FACEBOOK_SERVER_CHAT, 0, chat_channel_num_ );
		return chatServer;
	}

	case FACEBOOK_REQUEST_STATUS_GET:
		return FACEBOOK_SERVER_MOBILE;

	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
	case FACEBOOK_REQUEST_LOGOUT:
	default:
		return FACEBOOK_SERVER_REGULAR;
	}
}

char* facebook_client::choose_action( int request_type, char* data )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
		return "login.php?login_attempt=1";

	case FACEBOOK_REQUEST_HOME:
		return "home.php?";

	case FACEBOOK_REQUEST_BUDDY_LIST:
		return "ajax/chat/buddy_list.php";

	case FACEBOOK_REQUEST_RECONNECT: {
		char* action = ( char* )utils::mem::allocate( 128 * sizeof( char ) );
		unsigned int reason = ( this->first_touch_ ) ? FACEBOOK_RECONNECT_LOGIN : FACEBOOK_RECONNECT_KEEP_ALIVE;
		mir_snprintf( action, 128, "ajax/presence/reconnect.php?reason=%d&post_form_id=%s&iframe_loaded=false&__a=1&nctr[n]=1", reason, post_form_id_.c_str( ) );
		return action; }

	case FACEBOOK_REQUEST_STATUS_GET: {
		char* action = ( char* )utils::mem::allocate( 48 * sizeof( char ) );
		mir_snprintf( action, 48, "profile.php?id=%s&v=info", data );
		return action; }

	case FACEBOOK_REQUEST_STATUS_SET:
		return "ajax/updatestatus.php";

	case FACEBOOK_REQUEST_MESSAGE_SEND:
		return "ajax/chat/send.php";

	case FACEBOOK_REQUEST_MESSAGES_RECEIVE: {
		char* action = ( char* )utils::mem::allocate( 48 * sizeof( char ) );
		std::string first_time;
		if ( this->first_touch_ ) { first_time = "true"; this->first_touch_ = false; }
		else first_time = "false";
		mir_snprintf( action, 48, "x/%s/%s/p_%s=%d", utils::time::unix_timestamp().c_str(), first_time.c_str(), user_id_.c_str(), chat_sequence_num_ );
		return action; }

	case FACEBOOK_REQUEST_LOGOUT: {
		char* action = ( char* )utils::mem::allocate( 128 * sizeof( char ) );
		mir_snprintf( action, 128, "logout.php%s", this->logout_action_.c_str() );
		return action; }

	default:
		return "";
	}
}

char* facebook_client::choose_request_url( int request_type, char* data )
{
	char* url = ( char* )utils::mem::allocate( 255 * sizeof( char ) );
	char* server = choose_server( request_type, data );
	char* action = choose_action( request_type, data );
	mir_snprintf( url, 255, "%s%s", server, action );
//// TODO: server + action leaks
//	utils::mem::detract( &server );
//	utils::mem::detract( &action );
	return url;
}


NETLIBHTTPHEADER* facebook_client::get_request_headers( int request_type, int* headers_count )
{
	// TODO: cookies leak
	// TODO: headers could be static

	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_STATUS_GET:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
		*( headers_count ) = 7;
		break;
	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_MESSAGES_RECEIVE:
	case FACEBOOK_REQUEST_NOTIFICATIONS_RECEIVE:
	default:
		*( headers_count ) = 6;
		break;
	}

	NETLIBHTTPHEADER* headers = ( NETLIBHTTPHEADER* )utils::mem::allocate( sizeof( NETLIBHTTPHEADER )*( *headers_count ) );

	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_STATUS_GET:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
		set_header( &headers[6], "Content-Type", "application/x-www-form-urlencoded" );
	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_MESSAGES_RECEIVE:
	case FACEBOOK_REQUEST_NOTIFICATIONS_RECEIVE:
	default:
		set_header( &headers[0], "Connection", "close" );
		set_header( &headers[1], "User-Agent", get_user_agent( ) );
		set_header( &headers[2], "Accept", "*/*" );
		set_header( &headers[3], "Cookie", load_cookies( ) );
		set_header( &headers[4], "Accept-Encoding", "gzip" );
		set_header( &headers[5], "Accept-Language", "en, C" );
		break;
	}

	return headers;
}

void facebook_client::set_header( NETLIBHTTPHEADER* header, char* header_name, char* header_value )
{
	header->szName  = header_name;
	header->szValue = header_value;
}

char* facebook_client::get_user_agent( )
{
	extern char* g_szUserAgent;
	BYTE user_agent = DBGetContactSettingByte(NULL, parent->m_szModuleName, FACEBOOK_KEY_USER_AGENT, 0);
	if (user_agent) return user_agents[user_agent];
	else return g_szUserAgent;
}

char* facebook_client::load_cookies( )
{
	// TODO: rewrite to use input string pointer (partially/fully avoid leak?)

	string cookieString = "isfbe=false;";

	if ( cookies.size() )
		for ( map< string, string >::iterator iter = cookies.begin(); iter != cookies.end(); ++iter ) {
			cookieString.append( iter->first );
			cookieString.append( "=" );
			cookieString.append( iter->second );
			cookieString.append( ";" ); }

	char* cookieCharString = ( char* )utils::mem::allocate( ( cookieString.length( ) ) * sizeof( char ) );
	strcpy( cookieCharString, cookieString.c_str( ) );
	return cookieCharString;
}

void facebook_client::store_cookies( NETLIBHTTPHEADER* headers, int headersCount )
{
	for ( int i = 0; i < headersCount; i++ )
	{
		if ( std::string( headers[i].szName ) == std::string( "Set-Cookie" ) )
		{
			string cookieString = headers[i].szValue;
			string cookieName = cookieString.substr( 0, cookieString.find( "=" ) );
			string cookieValue = cookieString.substr( cookieString.find( "=" ) + 1, cookieString.find( ";" ) - cookieString.find( "=" ) - 1 );
			if ( cookieValue == std::string( "deleted" ) ) {
				parent->LOG("      Deleted cookie '%s'", cookieName.c_str());
				cookies.erase( cookieName ); }
			else {
				parent->LOG("      New cookie '%s': %s", cookieName.c_str(), cookieValue.c_str());
				cookies[cookieName] = cookieValue; }
		}
	}
}

void facebook_client::clear_cookies( )
{
	if ( cookies.size() )
		cookies.clear( );
}

//////////////////////////////////////////////////////////////////////////////

bool facebook_client::login(const std::string &username,const std::string &password)
{
	username_ = username;
	password_ = password;

	// Prepare validation data
	clear_cookies( );
	cookies.insert( make_pair( "isfbe", "false" ) );
	cookies.insert( make_pair( "test_cookie", "1" ) );

	string data = "charset_test=%e2%82%ac%2c%c2%b4%2c%e2%82%ac%2c%c2%b4%2c%e6%b0%b4%2c%d0%94%2c%d0%84&locale=en&email=";
	data += utils::url::encode( username );
	data += "&pass=";
	data += utils::url::encode( password );
	data += "&pass_placeHolder=Password&persistent=1&login=Login&charset_test=%e2%82%ac%2c%c2%b4%2c%e2%82%ac%2c%c2%b4%2c%e6%b0%b4%2c%d0%94%2c%d0%84";

	// Send validation

	http::response resp = flap( FACEBOOK_REQUEST_LOGIN, (char*)data.c_str( ) );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK: // OK page returned, but that is regular login page we don't want in fact
		if ( resp.data.find( "<title>Login | Facebook</title>" ) != string::npos )
			MessageBox( NULL, TEXT( "Wrong password entered, please try again." ), TEXT( FACEBOOK_NAME ), MB_OK );
	case HTTP_CODE_FORBIDDEN: // Forbidden
	case HTTP_CODE_NOT_FOUND: // Not Found
	default:
		parent->LOG("!!!!! login: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	case HTTP_CODE_FOUND: // Found and redirected to Home, Logged in, everything is OK
		user_id_ = cookies["c_user"]; // TODO: Buffer overrun fix?
		return true;

	}
}

bool facebook_client::logout( )
{
	http::response resp = flap( FACEBOOK_REQUEST_LOGOUT );

	// Process result

	username_ = password_ = user_id_ = "";

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
	case HTTP_CODE_FOUND:
		return true;

	default:
		parent->LOG("!!!!! logout: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}
}

bool facebook_client::home( )
{
	http::response resp = flap( FACEBOOK_REQUEST_HOME );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		// Get real_name
		{
			int start = resp.data.find( "id=\"fb_menu_account\"" );
			int end = resp.data.find( "</a>", start );
			start = resp.data.rfind( "\">", end ) + 2;
			this->real_name_ = resp.data.substr( start, end - start );
			DBWriteContactSettingUTF8String(NULL,parent->m_szModuleName,FACEBOOK_KEY_NAME,real_name_.c_str());
		}

		// Get post_form_id
		this->post_form_id_ = resp.data.substr( resp.data.find( "post_form_id:" ) + 14, 32 );
// If something would go wrong:
//		{
//			this->post_form_id_ = resp.data.substr( resp.data.find( "post_form_id:" ) + 14, 48 );
//			this->post_form_id_ = this->post_form_id_.substr( 0, this->post_form_id_.find( "\"" ) );
//		}

		// Get dtsg
		{
			this->dtsg_ = resp.data.substr( resp.data.find( "fb_dtsg:" ) + 9, 48 );
			this->dtsg_ = this->dtsg_.substr( 0, this->dtsg_.find( "\"" ) );
		}

		// Get logout action
		{
			this->logout_action_ = resp.data.substr( resp.data.find( "id=\"fb_menu_logout\"" ), 256 );
			int start = this->logout_action_.find( "<a href=\"" ) + 9;
			start = this->logout_action_.find( "?", start );
			int end = this->logout_action_.find( "\" class=\"fb_menu_link\"", start );
			this->logout_action_ = this->logout_action_.substr( start, end - start );
			utils::text::replace_all( &this->logout_action_, "&amp;", "&" );
		}
		return true;

	default:
		parent->LOG("!!!!! popout: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}
}

bool facebook_client::reconnect( )
{
	http::response resp = flap( FACEBOOK_REQUEST_RECONNECT );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK: {
		std::string channel_num = resp.data.substr( resp.data.find( "\"host\":" ) + 8 + 7, 16 );
		int end = channel_num.find( "\"" );
		channel_num = channel_num.substr( 0, end );

		std::string sequence_num = resp.data.substr( resp.data.find( "\"seq\":" ) + 6, 16 );
		end = sequence_num.find( "," );
		sequence_num = sequence_num.substr( 0, end );

		this->chat_channel_num_ = atoi( channel_num.c_str( ) );
		this->chat_sequence_num_ = atoi( sequence_num.c_str( ) ); }

		return true;

	default:
		parent->LOG("!!!!! reconnect: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}
}

bool facebook_client::buddy_list( )
{
	// Prepare update data

	// TODO: Request & process notifications by adding &notifications=1
	string data = "user=" + this->user_id_ + "&popped_out=false&force_render=true&buddy_list=1&notifications=0&post_form_id=" + this->post_form_id_ + "&fb_dtsg=" + this->dtsg_ + "&post_form_id_source=AsyncRequest&__a=1&nctr[n]=1";

	for (std::map< std::string, facebook_user* >::iterator i = buddies.begin(); i != buddies.end(); ++i )
	{
		if ( i->first == this->user_id_ )
			continue;

		data += "&available_list[";
		data += i->first;
		data += "][i]=";
		data += ( i->second->is_idle ) ? "1" : "0";
	}

	// Get buddy list

	http::response resp = flap( FACEBOOK_REQUEST_BUDDY_LIST, (char*)data.c_str( ) );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		if ( resp.data.find( "\"listChanged\":false" ) == string::npos ) {
			std::string* response_data = new std::string( resp.data );
			ForkThreadEx( &facebook_client::process_updates, this, ( void* )response_data ); }
	case HTTP_CODE_FAKE_LOGGED_OUT:
		return true;

	default:
		parent->LOG("!!!!! update: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}
}

bool facebook_client::channel( )
{
	// Get update

	http::response resp = flap( FACEBOOK_REQUEST_MESSAGES_RECEIVE );

	// Process result data

	validate_response(&resp);

	if ( resp.code != HTTP_CODE_OK )
	{
		// Something went wrong
	}
	else if ( resp.data.find( "\"t\":\"continue\"" ) != string::npos )
	{
		// Everything is OK, no new message received
	}
	else if ( resp.data.find( "\"t\":\"refresh\"" ) != string::npos )
	{
		// Something went wrong with the session, refresh it

		this->reconnect( );
	}
	else
	{
		// Something has been received, throw to new thread to process

		string* response_data = new std::string( resp.data );
		ForkThreadEx( &facebook_client::process_messages, this, ( void* )response_data );

		// Increment sequence number

		this->chat_sequence_num_ += utils::text::find_all( &resp.data, "{\"type\":\"" );
	}

	// Return

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		return true;

	default:
		parent->LOG("!!!!! channel: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}
}

bool facebook_client::send_message( string message_recipient, string message_text )
{
	string data = "msg_text=";
	data += utils::url::encode( message_text );
	data += "&msg_id=";
	data += utils::number::random( );
	data += "&to=";
	data += message_recipient;
	data += "&client_time=";
	data += utils::time::unix_timestamp( ) + "000"; // TODO: Vary
	data += "&post_form_id=";
	data += ( post_form_id_.length( ) ) ? post_form_id_ : "0";

	http::response resp = flap( FACEBOOK_REQUEST_MESSAGE_SEND, (char*)data.c_str( ) );

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		return true;

	default:
		parent->LOG("!!!!! send_message: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}

}

bool facebook_client::get_status(facebook_user* fbu)
{
	http::response resp = flap( FACEBOOK_REQUEST_STATUS_GET, (char*)fbu->user_id.c_str() );

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK: {
		int start = resp.data.find( "<div class=\"sectitle\">" );
		start = resp.data.find( "class=\"nopad\">", start );
		if ( start > 0 ) {
			start += 14;
			int end = resp.data.find( "&nbsp;<small>", start ) - 1; // hopefuly remove the "evil" dot. ^^
			fbu->status = resp.data.substr( start, end - start ); }
		else fbu->status = ""; }
	case HTTP_CODE_FOUND:
		return true;

	default:
		parent->LOG("!!!!! get_status: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}
}

bool facebook_client::set_status(const std::string &status_text)
{
	string data = "action=HOME_UPDATE&home_tab_id=1&profile_id=";
	data += this->user_id_;
	if ( status_text.length( ) )
	{
		data += "&status=";
		data += utils::url::encode( status_text );
	}
	else
	{
		data += "&clear=1";
	}
	data += "&post_form_id=";
	data += ( post_form_id_.length( ) ) ? post_form_id_ : "0";
	data += "&target_id=0&app_id=&post_form_id_source=AsyncRequest";

	http::response resp = flap( FACEBOOK_REQUEST_STATUS_SET, (char*)data.c_str() );

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		return true;

	default:
		parent->LOG("!!!!! set_status: Something went wrong...");
		parent->SetStatus(ID_STATUS_OFFLINE);
		return false;

	}
}

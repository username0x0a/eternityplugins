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

http::response facebook_client::flap( const int request_type, std::string* request_data )
{
	_APP("flap");
	NETLIBHTTPREQUEST nlhr = {sizeof( nlhr )};
	nlhr.requestType = choose_method( request_type );
	std::string url = choose_request_url( request_type, request_data );
	nlhr.szUrl = (char*)url.c_str( );
	nlhr.flags = NLHRF_NODUMP | choose_security_level( request_type ) | NLHRF_GENERATEHOST;
	nlhr.headers = get_request_headers( request_type, &nlhr.headersCount );

	if ( request_data != NULL )
	{
		nlhr.pData = (char*)(*request_data).c_str();
		nlhr.dataLength = strlen( nlhr.pData );
	}

	parent->LOG("@@@@@ Sending request to '%s'", nlhr.szUrl);

	NETLIBHTTPREQUEST* pnlhr = ( NETLIBHTTPREQUEST* )CallService( MS_NETLIB_HTTPTRANSACTION, (WPARAM)handle_, (LPARAM)&nlhr );

	http::response resp;

	if ( pnlhr )
	{
		parent->LOG("@@@@@ Got response with code %d", pnlhr->resultCode);
		for ( int i = 0; i < pnlhr->headersCount; i++ )
			parent->LOG("----- Got header '%s': %s",pnlhr->headers[i].szName, pnlhr->headers[i].szValue );
		store_cookies( pnlhr->headers, pnlhr->headersCount );
		resp.code = pnlhr->resultCode;
		resp.data = pnlhr->pData ? pnlhr->pData : "";

		CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)pnlhr);
	}
	else
	{
		parent->LOG("!!!!! No response from server (time-out)");
		resp.code = HTTP_CODE_FAKE_DISCONNECTED;
		// Better to have something set explicitely as this value
	    // is compaired in all communication requests
	}

	return resp;
}

bool facebook_client::validate_response( http::response* resp )
{
	_APP("validate_response");
	if ( resp->code == HTTP_CODE_FAKE_DISCONNECTED )
		parent->LOG(" ! !  Request has timed out, connection or server error");
		return false;

	if ( resp->data.find( "{\"error\":" ) != string::npos ) try
	{
		std::string error = resp->data.substr( resp->data.find( "{\"error\":" ) + 9, 64 );
		int error_num = atoi( error.substr( 0, error.find( "," ) ).c_str() );
		if ( error_num != 0 )
		{
			error = error.substr( error.find( "\"errorDescription\":\"" ) + 20 );
			error = error.substr( 0, error.find( "\"," ) );
			parent->LOG(" ! !  Received Facebook error: %d -- %s", error_num, error.c_str());
			resp->code = HTTP_CODE_FAKE_LOGGED_OUT;
			return false;
		}
	} catch (const std::exception &e) {
		parent->LOG(" @ @  validate_response: Exception: %s",e.what());return false; }

	return true;
}

bool facebook_client::handle_entry( std::string method )
{
	parent->LOG("   >> Entering %s()", method.c_str());
	return true;
}

bool facebook_client::handle_success( std::string method )
{
	parent->LOG("   << Quitting %s()", method.c_str());
	decrement_error();
	return true;
}

bool facebook_client::handle_error( std::string method, bool force_disconnect )
{
	bool result;
	increment_error();
	parent->LOG("!!!!! %s(): Something with Facebook went wrong", method.c_str());

	if ( force_disconnect )
		result = false;
	else if ( error_count_ <= (UINT)DBGetContactSettingByte(NULL,parent->m_szModuleName,FACEBOOK_KEY_TIMEOUTS_LIMIT,FACEBOOK_TIMEOUTS_LIMIT))
		result = true;
	else
		result = false;

	if ( result == false ) {
		reset_error(); parent->SetStatus(ID_STATUS_OFFLINE); }

	return result;
}

//////////////////////////////////////////////////////////////////////////////

DWORD facebook_client::choose_security_level( int request_type )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
		return NLHRF_SSL;

	case FACEBOOK_REQUEST_LOGOUT:
	case FACEBOOK_REQUEST_KEEP_ALIVE:
	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_PROFILE_GET:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
	default:
		return ( DWORD )0;
	}
}

int facebook_client::choose_method( int request_type )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
	case FACEBOOK_REQUEST_KEEP_ALIVE:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
		return REQUEST_POST;

	case FACEBOOK_REQUEST_LOGOUT:
	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_PROFILE_GET:
	default:
		return REQUEST_GET;
	}
}

std::string facebook_client::choose_server( int request_type, std::string* data )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
		return FACEBOOK_SERVER_LOGIN;

	case FACEBOOK_REQUEST_MESSAGES_RECEIVE: {
		std::string server = FACEBOOK_SERVER_CHAT;
		utils::text::replace_first( &server, "%s", "0" );
		utils::text::replace_first( &server, "%s", this->chat_channel_num_ );
		return server; }

	case FACEBOOK_REQUEST_PROFILE_GET:
		return FACEBOOK_SERVER_MOBILE;

	case FACEBOOK_REQUEST_LOGOUT:
	case FACEBOOK_REQUEST_KEEP_ALIVE:
	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
	default:
		return FACEBOOK_SERVER_REGULAR;
	}
}

std::string facebook_client::choose_action( int request_type, std::string* data )
{
	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
		return "login.php?login_attempt=1";

	case FACEBOOK_REQUEST_LOGOUT: {
		std::string action = "logout.php%s";
		utils::text::replace_first( &action, "%s", this->logout_action_ );
		return action; }

	case FACEBOOK_REQUEST_KEEP_ALIVE:
		return "ajax/presence/update.php";

	case FACEBOOK_REQUEST_HOME:
		return "home.php?";

	case FACEBOOK_REQUEST_BUDDY_LIST:
		return "ajax/chat/buddy_list.php";

	case FACEBOOK_REQUEST_RECONNECT: {
		std::string action = "ajax/presence/reconnect.php?reason=%s&post_form_id=%s&iframe_loaded=false&__a=1&nctr[n]=1";
		std::string reason = ( this->chat_first_touch_ ) ? FACEBOOK_RECONNECT_LOGIN : FACEBOOK_RECONNECT_KEEP_ALIVE;
		utils::text::replace_first( &action, "%s", reason );
		utils::text::replace_first( &action, "%s", this->post_form_id_ );
		return action; }

	case FACEBOOK_REQUEST_PROFILE_GET: {
		std::string action = "profile.php?id=%s&v=info";
		utils::text::replace_first( &action, "%s", (*data) );
		return action; }

	case FACEBOOK_REQUEST_STATUS_SET:
		return "ajax/updatestatus.php";

	case FACEBOOK_REQUEST_MESSAGE_SEND:
		return "ajax/chat/send.php";

	case FACEBOOK_REQUEST_MESSAGES_RECEIVE: {
		std::string action = "x/%s/%s/p_%s=%d";
		std::string first_time;
		if ( this->chat_first_touch_ ) { first_time = "true"; this->chat_first_touch_ = false; }
		else first_time = "false";
		utils::text::replace_first( &action, "%s", utils::time::unix_timestamp() );
		utils::text::replace_first( &action, "%s", first_time );
		utils::text::replace_first( &action, "%s", self_.user_id );
		utils::text::replace_first( &action, "%d", utils::conversion::to_string( (void*)&chat_sequence_num_, UTILS_CONV_UNSIGNED_INT ) );
		return action; }

	default:
		return "";
	}
}

std::string facebook_client::choose_request_url( int request_type, std::string* data )
{
	std::string url = choose_server( request_type, data );
	url.append( choose_action( request_type, data ) );
	return url;
}


NETLIBHTTPHEADER* facebook_client::get_request_headers( int request_type, int* headers_count )
{
	_APP("get_request_headers");

	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_PROFILE_GET:
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

	refresh_headers( ); // TODO: Refresh only when required

	switch ( request_type )
	{
	case FACEBOOK_REQUEST_LOGIN:
	case FACEBOOK_REQUEST_BUDDY_LIST:
	case FACEBOOK_REQUEST_PROFILE_GET:
	case FACEBOOK_REQUEST_STATUS_SET:
	case FACEBOOK_REQUEST_MESSAGE_SEND:
		set_header( &headers[6], "Content-Type" );
	case FACEBOOK_REQUEST_HOME:
	case FACEBOOK_REQUEST_RECONNECT:
	case FACEBOOK_REQUEST_MESSAGES_RECEIVE:
	case FACEBOOK_REQUEST_NOTIFICATIONS_RECEIVE:
	default:
		set_header( &headers[0], "Connection" );
		set_header( &headers[1], "User-Agent" );
		set_header( &headers[2], "Accept" );
		set_header( &headers[3], "Cookie" );
		set_header( &headers[4], "Accept-Encoding" );
		set_header( &headers[5], "Accept-Language" );
		break;
	}

	return headers;
}

void facebook_client::set_header( NETLIBHTTPHEADER* header, char* name )
{
	header->szName  = name;
	header->szValue = (char*)this->headers[name].c_str();
}

void facebook_client::refresh_headers( )
{
	if ( headers.size() < 5 )
	{
		this->headers["Connection"] = "close";
		this->headers["Accept"] = "*/*";
		this->headers["Accept-Encoding"] = "gzip";
		this->headers["Accept-Language"] = "en, C";
		this->headers["Content-Type"] = "application/x-www-form-urlencoded";
	}
	this->headers["User-Agent"] = get_user_agent( );
	this->headers["Cookie"] = load_cookies( );
}

std::string facebook_client::get_user_agent( )
{
	BYTE user_agent = DBGetContactSettingByte(NULL, parent->m_szModuleName, FACEBOOK_KEY_USER_AGENT, 0);
	if (user_agent) return user_agents[user_agent];
	else return g_strUserAgent;
}

std::string facebook_client::load_cookies( )
{
	string cookieString = "isfbe=false;";

	if ( cookies.size() )
		for ( map< string, string >::iterator iter = cookies.begin(); iter != cookies.end(); ++iter ) {
			_APP("load_cookies::for");
			cookieString.append( iter->first );
			cookieString.append( "=" );
			cookieString.append( iter->second );
			cookieString.append( ";" ); }

	return cookieString;
}

void facebook_client::store_cookies( NETLIBHTTPHEADER* headers, int headersCount )
{
	_APP("store_cookies");
	for ( int i = 0; i < headersCount; i++ )
	{
		if ( std::string( headers[i].szName ) == std::string( "Set-Cookie" ) )
		{
			_APP("store_cookies::for");
			string cookieString = headers[i].szValue;
			string cookieName = cookieString.substr( 0, cookieString.find( "=" ) );
			string cookieValue = cookieString.substr( cookieString.find( "=" ) + 1, cookieString.find( ";" ) - cookieString.find( "=" ) - 1 );
			if ( cookieValue == std::string( "deleted" ) ) {
				parent->LOG("      Deleted cookie '%s'", cookieName.c_str());
				cookies.erase( cookieName ); }
			else {
				parent->LOG("      New cookie '%s': %s", cookieName.c_str(), cookieValue.c_str());
				// TODO: append+set?
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
	handle_entry( "login" );

	username_ = username;
	password_ = password;

	// Prepare validation data
	clear_cookies( );
	cookies.insert( make_pair( "isfbe", "false" ) );
	cookies.insert( make_pair( "test_cookie", "1" ) );

	std::string data = "charset_test=%e2%82%ac%2c%c2%b4%2c%e2%82%ac%2c%c2%b4%2c%e6%b0%b4%2c%d0%94%2c%d0%84&locale=en&email=";
	data += utils::url::encode( username );
	data += "&pass=";
	data += utils::url::encode( password );
	data += "&pass_placeHolder=Password&persistent=1&login=Login&charset_test=%e2%82%ac%2c%c2%b4%2c%e2%82%ac%2c%c2%b4%2c%e6%b0%b4%2c%d0%94%2c%d0%84";

	// Send validation

	http::response resp = flap( FACEBOOK_REQUEST_LOGIN, &data );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK: // OK page returned, but that is regular login page we don't want in fact
//		if ( resp.data.find( "<title>Login | Facebook</title>" ) != string::npos )
			MessageBox( NULL, LPGENT( "Probably wrong password entered, please try again." ), TEXT( FACEBOOK_NAME ), MB_OK );
	case HTTP_CODE_FORBIDDEN: // Forbidden
	case HTTP_CODE_NOT_FOUND: // Not Found
	default:
		return handle_error( "login", FORCE_DISCONNECT );

	case HTTP_CODE_FOUND: // Found and redirected to Home, Logged in, everything is OK
		if ( cookies.find("c_user") != cookies.end() )
		{
			this->self_.user_id = cookies.find("c_user")->second;
			DBWriteContactSettingString(NULL,parent->m_szModuleName,FACEBOOK_KEY_ID,this->self_.user_id.c_str());
			parent->LOG("      Got self user id: %s", this->self_.user_id.c_str());
			return handle_success( "login" );
		}
		else
			return handle_error( "login", FORCE_DISCONNECT );
	}
}

bool facebook_client::logout( )
{
	if ( parent->isOffline() )
		return true;

	if ( DBGetContactSettingByte(NULL, parent->m_szModuleName, FACEBOOK_KEY_DISABLE_LOGOUT, 0) )
		return true;

	handle_entry( "logout" );

	http::response resp = flap( FACEBOOK_REQUEST_LOGOUT );

	// Process result

	username_ = password_ = self_.user_id = "";

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
	case HTTP_CODE_FOUND:
		return handle_success( "logout" );

	default:
		return handle_error( "logout", FORCE_DISCONNECT );

	}
}

bool facebook_client::keep_alive( )
{
	if ( parent->isOffline() )
		return false;

	handle_entry( "keep_alive" );

	std::string data = "user=" + this->self_.user_id + "&popped_out=false&force_render=true&buddy_list=1&notifications=0&post_form_id=" + this->post_form_id_ + "&fb_dtsg=" + this->dtsg_ + "&post_form_id_source=AsyncRequest&__a=1&nctr[n]=1";

	for (std::map< std::string, facebook_user* >::iterator i = buddies.begin(); i != buddies.end(); ++i )
	{
		_APP("keep_alive::for");
		data += "&available_list[";
		data += i->first;
		data += "][i]=";
		data += ( i->second->is_idle ) ? "1" : "0";
	}

	http::response resp = flap( FACEBOOK_REQUEST_KEEP_ALIVE, &data );

	// Process result

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
	case HTTP_CODE_FOUND:
		return handle_success( "keep_alive" );

	default:
		return handle_error( "keep_alive" );

	}
}

bool facebook_client::home( )
{
	handle_entry( "home" );

	http::response resp = flap( FACEBOOK_REQUEST_HOME );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		// Get real_name
		{
			std::string::size_type start = resp.data.find( "id=\"fb_menu_account\"" );
			std::string::size_type end = resp.data.find( "</a>", start );
			start = resp.data.rfind( "\">", end ) + 2;
			this->self_.real_name = resp.data.substr( start, end - start );
			this->self_.real_name = utils::text::html_special_chars_decode( this->self_.real_name );
			DBWriteContactSettingUTF8String(NULL,parent->m_szModuleName,FACEBOOK_KEY_NAME,this->self_.real_name.c_str());
			DBWriteContactSettingUTF8String(NULL,parent->m_szModuleName,"Nick",this->self_.real_name.c_str());
			parent->LOG("      Got self real name: %s", this->self_.real_name.c_str());
		}

		// Get post_form_id
		this->post_form_id_ = resp.data.substr( resp.data.find( "post_form_id:" ) + 14, 32 );
		parent->LOG("      Got self post form id: %s", this->post_form_id_.c_str());
// If something would go wrong:
//		{
//			this->post_form_id_ = resp.data.substr( resp.data.find( "post_form_id:" ) + 14, 48 );
//			this->post_form_id_ = this->post_form_id_.substr( 0, this->post_form_id_.find( "\"" ) );
//		}

		// Get dtsg
		{
			this->dtsg_ = resp.data.substr( resp.data.find( "fb_dtsg:" ) + 9, 48 );
			this->dtsg_ = this->dtsg_.substr( 0, this->dtsg_.find( "\"" ) );
			parent->LOG("      Got self dtsg: %s", this->dtsg_.c_str());
		}

		// Get logout action
		{
			this->logout_action_ = resp.data.substr( resp.data.find( "id=\"fb_menu_logout\"" ), 256 );
			std::string::size_type start = this->logout_action_.find( "<a href=\"" ) + 9;
			start = this->logout_action_.find( "?", start );
			std::string::size_type end = this->logout_action_.find( "\" class=\"fb_menu_link\"", start );
			this->logout_action_ = this->logout_action_.substr( start, end - start );
			utils::text::replace_all( &this->logout_action_, "&amp;", "&" );
		}

		// Set first touch flag
		this->chat_first_touch_ = true;

		// Update self-contact + own data in the DB
		ForkThreadEx(&FacebookProto::UpdateContactWorker, this->parent, (void*)&this->self_);

		return handle_success( "home" );

	case HTTP_CODE_FOUND:
		// work-around for replica_down? f**king hell what's that?
		return this->home();

	default:
		return handle_error( "home", FORCE_DISCONNECT );

	}
}

bool facebook_client::reconnect( )
{
	handle_entry( "reconnect" );

	http::response resp = flap( FACEBOOK_REQUEST_RECONNECT );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK: {
		std::string channel_num = resp.data.substr( resp.data.find( "\"host\":" ) + 8 + 7, 16 );
		std::string::size_type end = channel_num.find( "\"" );
		this->chat_channel_num_ = channel_num.substr( 0, end );
		parent->LOG("      Got self channel number: %s", this->chat_channel_num_.c_str());

		std::string sequence_num = resp.data.substr( resp.data.find( "\"seq\":" ) + 6, 16 );
		end = sequence_num.find( "," );
		sequence_num = sequence_num.substr( 0, end );
		this->chat_sequence_num_ = atoi( sequence_num.c_str( ) );
		parent->LOG("      Got self sequence number: %d", this->chat_sequence_num_); }

		return handle_success( "reconnect" );

	default:
		return handle_error( "reconnect", FORCE_DISCONNECT );

	}
}

bool facebook_client::buddy_list( )
{
	handle_entry( "buddy_list" );

	// Prepare update data

	// TODO: Request & process notifications
	std::string data = "user=" + this->self_.user_id + "&popped_out=false&force_render=true&buddy_list=1&notifications=0&post_form_id=" + this->post_form_id_ + "&fb_dtsg=" + this->dtsg_ + "&post_form_id_source=AsyncRequest&__a=1&nctr[n]=1";

	for (std::map< std::string, facebook_user* >::iterator i = buddies.begin(); i != buddies.end(); ++i )
	{
		_APP("buddy_list::for");
		data += "&available_list[";
		data += i->first;
		data += "][i]=";
		data += ( i->second->is_idle ) ? "1" : "0";
	}

	// Get buddy list

	http::response resp = flap( FACEBOOK_REQUEST_BUDDY_LIST, &data );

	// Process result data

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		if ( resp.data.find( "\"listChanged\":true" ) != string::npos ) {
			std::string* response_data = new std::string( resp.data );
			ForkThreadEx( &FacebookProto::ProcessBuddyList, this->parent, ( void* )response_data ); }
		return handle_success( "buddy_list" );

	case HTTP_CODE_FAKE_LOGGED_OUT:
	case HTTP_CODE_FAKE_DISCONNECTED:
	default:
		return handle_error( "buddy_list" );

	}
}

bool facebook_client::channel( )
{
	handle_entry( "channel" );

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
		// Something went wrong with the session (has expired?), refresh it

		this->reconnect( );
	}
	else
	{
		// Something has been received, throw to new thread to process

		string* response_data = new std::string( resp.data );
		ForkThreadEx( &FacebookProto::ProcessMessages, this->parent, ( void* )response_data );

		// Increment sequence number

		this->chat_sequence_num_ += utils::text::count_all( &resp.data, "\"type\":\"" );
	}

	// Return

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		return handle_success( "channel" );

	case HTTP_CODE_FAKE_LOGGED_OUT:
	case HTTP_CODE_FAKE_DISCONNECTED:
	default:
		return handle_error( "channel" );

	}
}

bool facebook_client::send_message( string message_recipient, string message_text )
{
	handle_entry( "send_message" );

	std::string data = "msg_text=";
	data += utils::url::encode( message_text );
	data += "&msg_id=";
	data += utils::time::unix_timestamp( );
	data += "&to=";
	data += message_recipient;
	data += "&client_time=";
	data += utils::time::mili_timestamp( );
	data += "&post_form_id=";
	data += ( post_form_id_.length( ) ) ? post_form_id_ : "0";

	http::response resp = flap( FACEBOOK_REQUEST_MESSAGE_SEND, &data );

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		return handle_success( "send_message" );

	case HTTP_CODE_FAKE_LOGGED_OUT:
	case HTTP_CODE_FAKE_DISCONNECTED:
	default:
		return handle_error( "send_message" );

	}

}

bool facebook_client::get_profile(facebook_user* fbu)
{
	handle_entry( "get_profile" );

	http::response resp = flap( FACEBOOK_REQUEST_PROFILE_GET, &fbu->user_id );

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		if ( fbu->status.length() == 0 )
		{
			std::string::size_type start = resp.data.find( "<div class=\"sectitle\">" );
			start = resp.data.find( "class=\"nopad\">", start );
			if ( start != std::string::npos ) {
				start += 14;
				std::string::size_type end = resp.data.find( "&nbsp;<small>", start );
				if ( end != std::string::npos ) {
					if ( (BYTE)resp.data.at(end-1) == (BYTE)'.' ) end--; // This will hopefuly remove the "evil" dot ^^.
					fbu->status = resp.data.substr( start, end - start );
					fbu->status = utils::text::html_special_chars_decode( fbu->status ); } }
		}
		if ( fbu->image_url.length() == 0 )
		{
			std::string::size_type start = resp.data.find( "<img src=\"http://profile.ak.fbcdn.net" );
			if ( start != std::string::npos ) {
				start += 10;
				std::string::size_type end = resp.data.find( "\" alt=\"\"", start );
				if ( end != std::string::npos ) {
					fbu->image_url = resp.data.substr( start, end - start );
					fbu->image_url = fbu->image_url.replace( fbu->image_url.rfind( "/s" ), 2, "/q" ); } }
			else
				fbu->image_url = FACEBOOK_DEFAULT_AVATAR_URL;
		}
	case HTTP_CODE_FOUND:
		return handle_success( "get_profile" );

	case HTTP_CODE_FAKE_LOGGED_OUT:
	case HTTP_CODE_FAKE_DISCONNECTED:
	default:
		return handle_error( "get_profile" );

	}
}

bool facebook_client::set_status(const std::string &status_text)
{
	handle_entry( "set_status" );

	std::string data = "action=HOME_UPDATE&home_tab_id=1&profile_id=";
	data += this->self_.user_id;
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

	http::response resp = flap( FACEBOOK_REQUEST_STATUS_SET, &data );

	validate_response(&resp);

	switch ( resp.code )
	{

	case HTTP_CODE_OK:
		return handle_success( "set_status" );

	case HTTP_CODE_FAKE_LOGGED_OUT:
	case HTTP_CODE_FAKE_DISCONNECTED:
	default:
		return handle_error( "set_status" );

	}
}

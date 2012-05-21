/// \file waMysqlClient.cpp
/// MysqlClient,MysqlData类实现文件

// 编译参数:
// (CC) -I /usr/local/include/mysql/ -L /usr/local/lib/mysql -lmysqlclient -lm

#include "waMysqlClient.h"

using namespace std;

// WEB Application Library namaspace
namespace webapp {

/// \ingroup waMysqlClient
/// \fn string escape_sql( const string &str )
/// SQL语句字符转义
/// \param 要转换的SQL字符串
/// \return 转义过的字符串
string escape_sql( const string &str ) {
	char *p = new char[str.length()*2+1];
	mysql_escape_string( p, str.c_str(), str.length() );
	string s = p;
	delete[] p;
	return s;
}

////////////////////////////////////////////////////////////////////////////
// MysqlData

/// MysqlData析构函数
MysqlData::~MysqlData() {
	if ( _mysqlres != NULL )
		mysql_free_result( _mysqlres );
	_mysqlres = 0;
	_mysqlfields = 0;
}

/// 返回指定位置的MysqlData数据
/// \param row 数据行位置,默认为0
/// \param col 数据列位置,默认为0
/// \return 返回数据,不存在则返回空字符串
string MysqlData::get_data( const unsigned long row, const unsigned int col ) {
	if( _mysqlres!=NULL && row<_rows && col<_cols ) {
		if ( static_cast<long>(row) != _fetched ) {
			if ( row != _curpos+1 )
				mysql_data_seek( _mysqlres, row );
			_mysqlrow = mysql_fetch_row( _mysqlres );
			_fetched = static_cast<long>( row ); 
		}
		
		if ( _mysqlrow!=NULL && _mysqlrow[col]!=NULL ) {
			_curpos = row; // log current cursor
			return  string( _mysqlrow[col] );
		}
	}
	return string( "" );
}

/// 返回指定字段的MysqlData数据
/// \param row 行位置
/// \param field 字段名
/// \return 数据字符串,不存在返回空字符串
string MysqlData::get_data( const unsigned long row, const string &field ) {
	int col = this->field_pos( field );
	if ( col != -1 )
		return this->get_data( row, col );
	else
		return string( "" );
}

/// 返回指定位置的MysqlData数据行
/// \param row 数据行位置,默认为当前纪录位置,
/// 当前纪录位置由first(),prior(),next(),last(),find()函数操纵,默认为0
/// \return 返回值类型为MysqlDataRow,即map<string,string>
MysqlDataRow MysqlData::get_row( const long row ) {
	MysqlDataRow datarow;
	string field;
	unsigned long rowpos;
	
	if ( row < 0 ) 
		rowpos = _curpos;
	else
		rowpos = row;
		
	if( _mysqlres!=NULL && rowpos<_rows ) {
		if ( rowpos != _curpos ) {
			if ( rowpos != _curpos+1 )
				mysql_data_seek( _mysqlres, rowpos );
			_mysqlrow = mysql_fetch_row( _mysqlres );
		}
		
		if ( _mysqlrow != NULL ) {
			_curpos = rowpos; // log current cursor
			for ( size_t i=0; i<_cols; ++i ) {
				field = this->field_name( i );
				if ( field!="" && _mysqlrow[i]!=NULL )
					datarow.insert( MysqlDataRow::value_type(field,_mysqlrow[i]) );
			}
		}
	}
	
	return datarow;
}

/// 设置当前位置为前一条数据
/// \retval true 成功 
/// \retval false 已经为第一条数据
bool MysqlData::prior() {
	if ( _curpos > 0 ) {
		--_curpos;
		return true;
	}
	return false;
}

/// 设置当前位置为后一条数据
/// \retval true 成功 
/// \retval false 已经为最后一条数据
bool MysqlData::next() {
	if ( _curpos < _rows-1 ) {
		++_curpos;
		return true;
	}
	return false;
}

/// 查询数据位置 
/// \param field 数据字段名
/// \param value 要查找的数据值
/// \param mode 查找方式
/// - MysqlData::FIND_FIRST 从头开始查找,
/// - MysqlData::FIND_NEXT 从当前位置开始继续查找
/// - 默认为MysqlData::FIND_FIRST
/// \return 若查找到相应纪录返回纪录条数位置并设为当前数据位置，否则返回-1
long MysqlData::find( const string &field, const string &value, 
					 const find_mode mode ) {
	// confirm
	if ( _rows==0 || _cols==0 )
		return -1;
	
	int col = this->field_pos( field );
	if ( col==-1 || static_cast<size_t>(col)>_cols ) 
		return -1;
		
	// init
	unsigned long pos;
	if ( mode == FIND_FIRST )
		pos = 0;
	else if ( _curpos < _rows )
		pos = _curpos + 1;
	else
		return -1;	
		
	// find
	for ( ; pos<_rows; ++pos ) {
		if ( this->get_data(pos,col) == value ) {
			_curpos = pos;
			return _curpos;
		}
	}
	return -1;
}

/// 填充MysqlData数据
/// \param mysql MYSQL*参数
/// \retval true 成功
/// \retval false 失败
bool MysqlData::fill_data( MYSQL *mysql ) {
	if ( mysql == NULL )
		return false;
	
	// clean		
	if ( _mysqlres != NULL )
		mysql_free_result( _mysqlres );
	_mysqlres = 0;
	this->first(); // return to first position
	_field_pos.clear(); // clean field pos cache

	// fill data
	_mysqlres = mysql_store_result( mysql );
	if ( _mysqlres != NULL ) {
		_rows = mysql_num_rows( _mysqlres );
		_cols = mysql_num_fields( _mysqlres );
		_mysqlfields = mysql_fetch_fields( _mysqlres );
		
		// init first data
		mysql_data_seek( _mysqlres, 0 );
		_mysqlrow = mysql_fetch_row( _mysqlres );		
		
		return true;
	}
	return false;
}

/// 返回字段位置
/// \param field 字段名
/// \return 若数据结果中存在该字段则返回字段位置,否则返回-1
int MysqlData::field_pos( const string &field ) {
	if ( _mysqlfields==0 || field=="" )
		return -1;
	
	// check cache
	if ( _field_pos.find(field) != _field_pos.end() )
		return _field_pos[field];

	for( size_t i=0; i<_cols; ++i ) {
		if ( strcmp(field.c_str(),_mysqlfields[i].name) == 0 ) {
			_field_pos[field] = i;
			return i;
		}
	}
	_field_pos[field] = -1;
	return -1;
}

/// 返回字段名称
/// \param col 字段位置
/// \return 若数据结果中存在该字段则返回字段名称,否则返回空字符串
string MysqlData::field_name( unsigned int col ) const {
	if ( _mysqlfields!=0 && col<=_cols )
		return string( _mysqlfields[col].name );
	else
		return string( "" );
}

////////////////////////////////////////////////////////////////////////////
// MysqlClient

/// 连接数据库
/// \param host MySQL主机IP
/// \param user MySQL用户名
/// \param pwd 用户口令
/// \param database 要打开的数据库
/// \param port 数据库端口，默认为0
/// \param socket UNIX_SOCKET，默认为NULL
/// \retval true 成功
/// \retval false 失败
bool MysqlClient::connect( const string &host, const string &user, const string &pwd, 
					 const string &database, const int port, const char* socket ) {
	this->disconnect();
	
	if ( mysql_init(&_mysql) ) {
		if ( mysql_real_connect( &_mysql, host.c_str(), user.c_str(),
								 pwd.c_str(), database.c_str(),
								 port, socket, CLIENT_COMPRESS ) )
			_connected = true;
	}
	
	return _connected;
}

/// 断开数据库连接
void MysqlClient::disconnect() {
	if( _connected ) {
		mysql_close( &_mysql );
		_connected = false;
	}
}

/// 判断是否连接数据库
/// \retval true 连接
/// \retval false 断开
bool MysqlClient::is_connected() {
	if ( _connected ) {
		if ( mysql_ping(&_mysql) == 0 )
			_connected = true;
		else
			_connected = false;
	}
	
	return _connected;
}

/// 选择数据库
/// \param database 数据库名
/// \retval true 成功
/// \retval false 失败
bool MysqlClient::select_db( const string &database ) {
	if ( _connected && mysql_select_db(&_mysql,database.c_str())==0 )
		return true;
	else
		return false;
}

/// 执行SQL语句,取得查询结果
/// \param sqlstr 要执行的SQL语句
/// \param records 保存数据结果的MysqlData对象
/// \retval true 成功
/// \retval false 失败
bool MysqlClient::query( const string &sqlstr, MysqlData &records ) {
	if ( _connected && mysql_real_query(&_mysql,sqlstr.c_str(),sqlstr.length())==0 ) {
		if( records.fill_data(&_mysql) )
			return true;
	}
	return false;
}

/// 执行SQL语句
/// \param sqlstr 要执行的SQL语句
/// \retval true 成功
/// \retval false 失败
bool MysqlClient::query( const string &sqlstr ) {
	if ( _connected && mysql_real_query(&_mysql,sqlstr.c_str(),sqlstr.length())==0 )
		return true;			
	else
		return false;
}

/// 返回查询结果中指定位置的字符串值
/// \param sqlstr SQL查询字符串
/// \param row 数据行位置,默认为0
/// \param col 数据列位置,默认为0
/// \return 查询成功返回字符串,否则返回空字符串
string MysqlClient::query_val( const string &sqlstr, const unsigned long row, 
						 const unsigned int col ) {
	MysqlData res;
	if ( this->query(sqlstr,res) ) {
		if ( res.rows()>row && res.cols()>col )
			return res(row,col);
	}
	return string( "" );
}

/// 返回查询结果中指定行
/// \param sqlstr SQL查询字符串
/// \param row 数据行位置,默认为0
/// \return 返回值类型为MysqlDataRow,即map<string,string>
MysqlDataRow MysqlClient::query_row( const string &sqlstr, const unsigned long row ) {
	MysqlData res;
	MysqlDataRow resrow;
	if ( this->query(sqlstr,res) ) {
		if ( row < res.rows() )
			resrow = res.get_row( row );
	}
	
	return resrow;
}

/// 上次查询动作所影响的记录条数
/// \return 返回记录条数,类型unsigned long
unsigned long MysqlClient::affected() {
	if ( _connected )
		return mysql_affected_rows( &_mysql );
	else
		return 0;
}

/// 取得上次查询的一个AUTO_INCREMENT列生成的ID
/// 一个Mysql表只能有一个AUTO_INCREMENT列,且必须为索引
/// \return 返回生成的ID
unsigned long MysqlClient::last_id() {
	if ( _connected )
		return mysql_insert_id( &_mysql );
	else
		return 0;
}

/// 取得更新信息
/// \return 返回更新信息
string MysqlClient::info() {
	if ( _connected )
		return string( mysql_info(&_mysql) );
	else
		return string( "" );
}

} // namespace


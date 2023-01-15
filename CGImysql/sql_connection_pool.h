#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <string>
#include <iostream>
#include "../lock/locker.h"

using namespace std;

class connection_pool
{
public:
    MYSQL *GetConnection();
    bool ReleaseConnection(MYSQL *conn);
    int GetFreeConn();
    void DestroyPool();

    static connection_pool *GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);
private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;
    int m_CurConn;
    int m_FreeConn;
    Locker lock;
    Sem reserve;
    list<MYSQL *> connList;
public:
    string m_url;
    string m_Port;
    string m_User;
    string m_PassWord;
    string m_DatabaseName;
    int m_close_log;
};

class connectionRAII
{
public:
    connectionRAII(MYSQL **conn, connection_pool *connPool);
    ~connectionRAII();
private:
    MYSQL *connRAII;
    connection_pool *poolRAII;
};


#endif
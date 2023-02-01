#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <map>
#include <sys/stat.h>
#include <fcntl.h>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

class http_conn{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD {GET = 0, HEAD, POST, OPTIONS, PUT, DELETE, TRACE, CONNECT};
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};
    enum LINE_STATUS{LINE_OK = 0, LINE_BAD, LINE_OPEN};
public:
    http_conn();
    ~http_conn();

    void init(int sockfd, const sockaddr_in &addr);
    void close_conn(bool real_close = true);        // turn off http connect
    void process();
    bool read_once();       // accept all message fron browser
    bool write();
    sockaddr_in *get_address();
    void initmysql_result();
    void init_resultFile(connection_pool *connPool);
private:
    void init();
    HTTP_CODE process_read();       // read and handle requests from m_read_buf
    bool process_write(HTTP_CODE ret);      // write response into m_write_buf
    HTTP_CODE parse_request_line(char *text);       // main state machine parse request line
    HTTP_CODE parse_headers(char *text);        // main state machine parse request header
    HTTP_CODE parse_content(char *text);        // main state machine parse request content
    HTTP_CODE do_request();     // generate response

    char* get_line();

    LINE_STATUS parse_line();       // slave state machine get one line and analyse which part it is

    void unmap();

    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_bland_line();
public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];  // request message
    int m_read_idx;     // last index +1 in m_read_buf
    int m_checked_idx;    // reading index in m_read_buf
    int m_start_line;   // number of parsed char in m_read_buf 

    char m_write_buf[WRITE_BUFFER_SIZE];    // response message
    int m_write_idx;    // index in m_write_buf

    CHECK_STATE m_check_state;  // state of main state machine
    METHOD m_method;            //

    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;      // flag for persistent connection

    char *m_file_address;
    struct stat m_flie_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;
    char *m_string;
    int btyes_to_send;
    int bytes_have_send;
};


#endif
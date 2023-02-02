#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";


int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);

    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot){
    epoll_event event;
    event.data.fd = fd;

#ifdef ET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef LT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
#ifdef ET
    event.events = EPOLLONESHOT | EPOLLET | EPOLLRDHUP | ev;
#endif

#ifdef LT
    event.events = EPOLLONESHOT | EPOLLRDHUP | ev;
#endif
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::init(int sockfd, const sockaddr_in &addr){
    m_sockfd = sockfd;
    m_address = addr;
}

void http_conn::close_conn(bool real_close){
    if (real_close && (m_sockfd != -1)){
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::process(){
    HTTP_CODE read_ret = process_read();

    // NO_REQUEST, request is incomplete, shall continue receiving request
    if (read_ret == NO_REQUEST){
        // register and listen for read event
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    // call process_write() to complete message response
    bool write_ret = process_write(read_ret);
    if (!write_ret){
        close_conn();
    }

    // register and listen for write event
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

bool http_conn::read_once(){
    if (m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;
    while (true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1){
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }else if (bytes_read == 0){
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

bool http_conn::write(){}

sockaddr_in* http_conn::get_address(){}

void http_conn::initmysql_result(){}

void http_conn::init_resultFile(connection_pool *connPool){}



char* http_conn::get_line(){
    return m_read_buf + m_start_line;
}

http_conn::HTTP_CODE http_conn::process_read(){
    // init slave state machine, parse http request
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while ( (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK)
    || ( (line_status = parse_line()) == LINE_OK)){
        text = get_line();

        m_start_line = m_checked_idx;

        switch(m_check_state){
            case CHECK_STATE_REQUESTLINE:{
                // parse request line
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                
                break;
            }
            case CHECK_STATE_HEADER:{
                // parse request header
                ret = parse_headers(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                // if parse success, call do_request()
                else if (ret == GET_REQUEST)
                    return do_request();
                
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret = parse_content(text);
                if (ret == GET_REQUEST)
                    return do_request();
                
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;
}

// parse one line
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx){
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r'){
            if ( (m_checked_idx + 1) == m_read_idx){
                return LINE_OPEN;
            }else if (m_read_buf[m_checked_idx + 1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
        }else if (temp == '\n'){
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx-1] == '\r'){
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}

// parse request line 
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
// request line continues request type, resource to access, http edition, saparate by ' ' of '\t'
    m_url = strpbrk(text, " \t");
    if (!m_url)
        return BAD_REQUEST;
    
    *m_url++ = '\0';
    char* method = text;
    if (strcasecmp(method, "GET") == 0){
        m_method = GET;
    }else if (strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }else
        return BAD_REQUEST;

    m_url += strspn(m_url, " \t");

    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    m_version += strspn(m_version, " \t");
    
    if (strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    // request line parsed successfully, main state machine turn to parse request header
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    // judge empty line or request headers
    if (text[0] == '\0'){
        // judge POST or GET
        if (m_content_length != 0){
            // method POST shall change to content parse state 
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if (strncasecmp(text, "Connection:", 11) == 0){
        text += 11;

        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0){
            // persistent connect
            m_linger = true;
        }
    }else if (strncasecmp(text, "Content-length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }else if (strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }else{
        printf("oop! unknow header: %s\n", text);
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text){
    // if readed message content
    if (m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';

        m_string = text;

        return GET_REQUEST;
    }
    return NO_REQUEST;
}

const char* doc_root = "./webserver/root";
http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    const char *p = strrchr(m_url, '/');
    if (cgi == 1 && (*(p+1) == '2' || *(p+1) == '3')){
        // login or register
        // synchronize thread login verification
        // CGI muti-process login verification
    }

    // /0, jump register interface
    if (*(p+1) == '0'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if (*(p+1) == '1'){
    // /1, jump login interface
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/login.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else{
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }

    // resource don't exist
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    // resource not readable
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    // resource type
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    close(fd);

    return FILE_REQUEST;
}
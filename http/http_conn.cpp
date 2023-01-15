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
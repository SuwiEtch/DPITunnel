#include "dpi-bypass.h"
#include "base64.h"
#include "socket.h"
#include "sni.h"
#include "dns.h"
#include "hostlist.h"

extern struct Settings settings;

int recv_string(int & socket, std::string & message, unsigned int & last_char)
{
    std::string log_tag = "CPP/recv_string";

    ssize_t read_size;
    size_t message_offset = 0;

    // Set receive timeout on socket
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50;
    if(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        log_error(log_tag.c_str(), "Can't setsockopt on socket");
        return -1;
    }

    while(true)
    {
        if(message.size() - message_offset < 1024) // If there isn't any space in message string - just increase it
        {
            message.resize(message.size() + 1024);
        }

        read_size = recv(socket, &message[0] + message_offset, message.size() - message_offset, 0);
        if(read_size < 0)
        {
            if(errno == EWOULDBLOCK)	break;
            if(errno == EINTR)      continue; // All is good. This is just interrrupt.
            else
            {
                log_error(log_tag.c_str(), "There is critical read error. Can't process client. Errno: %s", std::strerror(errno));
                return -1;
            }
        }
        else if(read_size == 0) return -1;

        message_offset += read_size;
    }

    // Set position of last character
    last_char = message_offset;

    return 0;
}

int recv_string(int & socket, std::string & message, struct timeval timeout, unsigned int & last_char)
{
    std::string log_tag = "CPP/recv_string";

    ssize_t read_size;
    size_t message_offset = 0;

    if(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        log_error(log_tag.c_str(), "Can't setsockopt on socket");
        return -1;
    }

    bool is_first_time = true;

    while(true)
    {
        if(message.size() - message_offset < 1024) // If there isn't any space in message string - just increase it
        {
            message.resize(message.size() + 1024);
        }

        read_size = recv(socket, &message[0] + message_offset, message.size() - message_offset, 0);
        if(read_size < 0)
        {
            if(errno == EWOULDBLOCK)	break;
            if(errno == EINTR)      continue; // All is good. This is just interrrupt.
            else
            {
                log_error(log_tag.c_str(), "There is critical read error. Can't process client. Errno: %s", std::strerror(errno));
                return -1;
            }
        }
        else if(read_size == 0) return -1;

        message_offset += read_size;

        if(is_first_time)
        {
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 50;
            if(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
            {
                log_error(log_tag.c_str(), "Can't setsockopt on socket");
                return -1;
            }

            is_first_time = false;
        }
    }

    // Set position of last character
    last_char = message_offset;

    return 0;
}

int send_string(int & socket, const std::string & string_to_send, unsigned int last_char)
{
    std::string log_tag = "CPP/send_string";

    // Check if string is empty
    if(last_char == 0)
        return 0;

    size_t offset = 0;

    // Set send timeout on socket
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    if(setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        log_error(log_tag.c_str(), "Can't setsockopt on socket");
        return -1;
    }

    while(last_char - offset != 0)
    {
        ssize_t send_size = send(socket, string_to_send.c_str() + offset, last_char - offset, 0);
        if(send_size < 0)
        {
            if(errno == EINTR)      continue; // All is good. This is just interrrupt.
            else {
                log_error(log_tag.c_str(), "There is critical send error. Can't process client. Errno: %s", std::strerror(errno));
                return -1;
            }
        }
        if(send_size == 0)
        {
            return -1;
        }
        offset += send_size;
    }

    return 0;
}

int send_string(int & socket, const std::string & string_to_send, unsigned int split_position, unsigned int last_char)
{
    std::string log_tag = "CPP/send_string";

    // Check if string is empty
    if(last_char == 0)
        return 0;

    size_t offset = 0;

    // Set send timeout on socket
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    if(setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        log_error(log_tag.c_str(), "Can't setsockopt on socket");
        return -1;
    }

    while(last_char - offset != 0)
    {
        ssize_t send_size = send(socket, string_to_send.c_str() + offset, last_char - offset < split_position ? last_char - offset < split_position : split_position, 0);
        if(send_size < 0)
        {
            if(errno == EINTR)	continue; // All is good. This is just interrrupt.
            else
            {
                log_error(log_tag.c_str(), "There is critical send error. Can't process client. Errno: %s", std::strerror(errno));
                return -1;
            }
        }
        if(send_size == 0)
        {
            return -1;
        }
        offset += send_size;
    }

    return 0;
}

int init_remote_server_socket(int & remote_server_socket, std::string & remote_server_host, int remote_server_port, bool is_https, bool hostlist_condition, SSL *& client_context)
{
    std::string log_tag = "CPP/init_remote_server_socket";

    // First task is host resolving
    std::string remote_server_ip(50, ' ');
    if(resolve_host(remote_server_host, remote_server_ip, hostlist_condition) == -1)
    {
        return -1;
    }

    // Init remote server socker
    struct sockaddr_in remote_server_address;

    if((remote_server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        log_error(log_tag.c_str(), "Can't create remote server socket");
        return -1;
    }

    // Check if socks5 is need
    if(hostlist_condition && ((settings.https.is_use_socks5 && is_https) || (settings.http.is_use_socks5 && !is_https)))
    {
        // Parse socks5 server string
        size_t splitter_position = settings.other.socks5_server.find(':');
        if(splitter_position == std::string::npos)
        {
            log_error(log_tag.c_str(), "Failed to parse SOKCS5 server");
        }
        std::string proxy_ip = settings.other.socks5_server.substr(0, splitter_position);
        // If address is hostname, resolve it
        resolve_host(proxy_ip, proxy_ip, false);
        std::string proxy_port = settings.other.socks5_server.substr(splitter_position + 1, settings.other.socks5_server.size() - splitter_position - 1);

        // Add port and address
        remote_server_address.sin_family = AF_INET;
        remote_server_address.sin_port = htons(atoi(proxy_port.c_str()));

        if(inet_pton(AF_INET, proxy_ip.c_str(), &remote_server_address.sin_addr) <= 0)
        {
            log_error(log_tag.c_str(), "Invalid proxy server ip address");
            return -1;
        }

        // Connect to remote server
        if(connect(remote_server_socket, (struct sockaddr *) &remote_server_address, sizeof(remote_server_address)) < 0)
        {
            log_error(log_tag.c_str(), "Can't connect to proxy server. Errno: %s", strerror(errno));
            return -1;
        }

        std::string proxy_message_buffer(" ", 3);
        // Send hello packet to proxy server
        proxy_message_buffer[0] = 0x05; // set socks protocol version
        proxy_message_buffer[1] = 0x01; // set number of auth methods
        proxy_message_buffer[2] = 0x00; // set noauth method

        if(send_string(remote_server_socket, proxy_message_buffer, proxy_message_buffer.size()) == -1)
        {
            log_error(log_tag.c_str(), "Failed to send hello packet to SOCKS5 proxy server");
            return -1;
        }

        // Receive response from proxy server
        // last_char indicates string end position
        unsigned int last_char;
        proxy_message_buffer.resize(0);
        do
        {
            if(recv_string(remote_server_socket, proxy_message_buffer, last_char) == -1)
            {
                log_error(log_tag.c_str(), "Failed to receive response from proxy server");
                return -1;
            }
        } while(last_char == 0);

        // Check auth method selected by proxy server
        if(proxy_message_buffer[1] != 0x00)
        {
            log_error(log_tag.c_str(), "Proxy server don't support noauth method");
            return -1;
        }

        // Ask proxy server to connect to remote server ip with command packet
        proxy_message_buffer.resize(10);
        proxy_message_buffer[0] = 0x05; // set socks protocol version
        proxy_message_buffer[1] = 0x01; // set tcp protocol
        proxy_message_buffer[2] = 0x00; // reserved field always must be zero
        proxy_message_buffer[3] = 0x01; // ask proxy server to connect to ipv4 address

        // Convert server ip string to int
        uint32_t remote_server_ip_bits = inet_addr(remote_server_ip.c_str());

        // Set remote server ip by 8 bits
        proxy_message_buffer[4] = remote_server_ip_bits & 0xFF;
        proxy_message_buffer[5] = (remote_server_ip_bits & 0xFF00) >> 8;
        proxy_message_buffer[6] = (remote_server_ip_bits & 0xFF0000) >> 16;
        proxy_message_buffer[7] = (remote_server_ip_bits & 0xFF000000) >> 24;

        // Set remote server port by 8 bits
        proxy_message_buffer[8] = remote_server_port >> 8;
        proxy_message_buffer[9] = remote_server_port & 0xFF;

        // Send command packet to proxy server
        if(send_string(remote_server_socket, proxy_message_buffer, proxy_message_buffer.size()) == -1)
        {
            log_error(log_tag.c_str(), "Failed to send command packet to proxy server");
            return -1;
        }

        // Receive response from proxy server
        proxy_message_buffer.resize(0);
        do
        {
            if(recv_string(remote_server_socket, proxy_message_buffer, last_char) == -1)
            {
                log_error(log_tag.c_str(), "Failed to receive response from proxy server");
                return -1;
            }
        } while(last_char == 0);

        // Check response code
        if(proxy_message_buffer[1] != 0x00)
        {
            log_error(log_tag.c_str(), "Proxy server returned bad response code");
            return -1;
        }
    }
    // Check if HTTP proxy is need
    else if(hostlist_condition && ((settings.https.is_use_http_proxy && is_https) || (settings.http.is_use_http_proxy && !is_https)))
    {
        // Parse http server string
        size_t splitter_position = settings.other.http_proxy_server.find(':');
        if(splitter_position == std::string::npos)
        {
            log_error(log_tag.c_str(), "Failed to parse HTTP server");
        }
        std::string proxy_ip = settings.other.http_proxy_server.substr(0, splitter_position);
        // If address is hostname, resolve it
        std::string host = proxy_ip;
        resolve_host(proxy_ip, proxy_ip, false);
        std::string proxy_port = settings.other.http_proxy_server.substr(splitter_position + 1, settings.other.http_proxy_server.size() - splitter_position - 1);

        // Add port and address
        remote_server_address.sin_family = AF_INET;
        remote_server_address.sin_port = htons(atoi(proxy_port.c_str()));

        if(inet_pton(AF_INET, proxy_ip.c_str(), &remote_server_address.sin_addr) <= 0)
        {
            log_error(log_tag.c_str(), "Invalid proxy server ip address");
            return -1;
        }

        // Connect to remote server
        if(connect(remote_server_socket, (struct sockaddr *) &remote_server_address, sizeof(remote_server_address)) < 0)
        {
            log_error(log_tag.c_str(), "Can't connect to proxy server. Errno: %s", strerror(errno));
            return -1;
        }

        // Ask proxy server to connect to remote host
        std::string proxy_message_buffer = "CONNECT " + remote_server_ip +
                                           ":" + std::to_string(remote_server_port) + " HTTP/1.1\r\n";

        // Add Proxy-Authorization header if authorization is need
        if (!settings.other.proxy_credentials.empty()
        && settings.other.proxy_credentials != "login:password")
        {
            proxy_message_buffer += "Proxy-Authorization: Basic " +
            macaron::Base64::Encode(settings.other.proxy_credentials) +
            "\r\n";
        }

        // Add host
        proxy_message_buffer += "Host: " + host + "\r\n\r\n";

        // Send CONNECT request
        if(send_string(remote_server_socket, proxy_message_buffer, proxy_message_buffer.size()) == -1)
        {
            log_error(log_tag.c_str(), "Failed to send connect packet to HTTP proxy server");
            return -1;
        }

        // Receive reply
        // last_char indicates string end position
        unsigned int last_char;
        proxy_message_buffer.resize(0);
        do
        {
            if(recv_string(remote_server_socket, proxy_message_buffer, last_char) == -1)
            {
                log_error(log_tag.c_str(), "Failed to receive response from proxy server");
                return -1;
            }
        } while(last_char == 0);

        // Check response code
        size_t success_response_position = proxy_message_buffer.find("200");
        if(success_response_position == std::string::npos)
        {
            log_error(log_tag.c_str(), "Proxy server failed to connect to remote host");
            return -1;
        }
    }
    // Check if HTTPS proxy is need
    else if(hostlist_condition && ((settings.https.is_use_https_proxy && is_https) || (settings.http.is_use_https_proxy && !is_https)))
    {
        // Parse http server string
        size_t splitter_position = settings.other.https_proxy_server.find(':');
        if(splitter_position == std::string::npos)
        {
            log_error(log_tag.c_str(), "Failed to parse HTTPS server");
        }
        std::string proxy_ip = settings.other.https_proxy_server.substr(0, splitter_position);
        // If address is hostname, resolve it
        std::string host = proxy_ip;
        resolve_host(proxy_ip, proxy_ip, false);
        std::string proxy_port = settings.other.https_proxy_server.substr(splitter_position + 1, settings.other.https_proxy_server.size() - splitter_position - 1);

        // Add port and address
        remote_server_address.sin_family = AF_INET;
        remote_server_address.sin_port = htons(atoi(proxy_port.c_str()));

        if(inet_pton(AF_INET, proxy_ip.c_str(), &remote_server_address.sin_addr) <= 0)
        {
            log_error(log_tag.c_str(), "Invalid proxy server ip address");
            return -1;
        }

        // Connect to remote server
        if(connect(remote_server_socket, (struct sockaddr *) &remote_server_address, sizeof(remote_server_address)) < 0)
        {
            log_error(log_tag.c_str(), "Can't connect to proxy server. Errno: %s", strerror(errno));
            return -1;
        }

        // Init tls connection
        std::string empty_str = "";
        client_context = init_tls_client(remote_server_socket, empty_str, false);
        if(client_context == NULL){
            SSL_shutdown(client_context);
            close(remote_server_socket);
            SSL_CTX_free(client_context);
            return -1;
        }

        // Ask proxy server to connect to remote host
        std::string proxy_message_buffer = "CONNECT " + remote_server_ip +
                                           ":" + std::to_string(remote_server_port) + " HTTP/1.1\r\n";

        // Add Proxy-Authorization header if authorization is need
        if (!settings.other.proxy_credentials.empty()
            && settings.other.proxy_credentials != "login:password")
        {
            proxy_message_buffer += "Proxy-Authorization: Basic " +
                                    macaron::Base64::Encode(settings.other.proxy_credentials) +
                                    "\r\n";
        }

        // Add host
        proxy_message_buffer += "Host: " + host + "\r\n\r\n";

        // Send CONNECT request
        if(send_string_tls(remote_server_socket, client_context, proxy_message_buffer, proxy_message_buffer.size()) == -1)
        {
            log_error(log_tag.c_str(), "Failed to send connect packet to HTTPS proxy server");
            return -1;
        }

        // Receive reply
        // last_char indicates string end position
        unsigned int last_char = 0;
        proxy_message_buffer.resize(0);
        do
        {
            if(recv_string_tls(remote_server_socket, client_context, proxy_message_buffer, last_char) == -1)
            {
                log_error(log_tag.c_str(), "Failed to receive response from proxy server");
                return -1;
            }
        } while(last_char == 0);

        // Check response code
        size_t success_response_position = proxy_message_buffer.find("200");
        if(success_response_position == std::string::npos)
        {
            log_error(log_tag.c_str(), "Proxy server failed to connect to remote host");
            return -1;
        }
    }
    else
    {
        // Add port and address
        remote_server_address.sin_family = AF_INET;
        remote_server_address.sin_port = htons(remote_server_port);

        if(inet_pton(AF_INET, remote_server_ip.c_str(), &remote_server_address.sin_addr) <= 0)
        {
            log_error(log_tag.c_str(), "Invalid remote server ip address");
            return -1;
        }

        // Connect to remote server
        if(connect(remote_server_socket, (struct sockaddr *) &remote_server_address, sizeof(remote_server_address)) < 0)
        {
            log_error(log_tag.c_str(), "Can't connect to remote server. Errno: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}
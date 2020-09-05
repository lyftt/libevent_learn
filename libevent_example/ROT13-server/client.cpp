#include <iostream>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

using namespace std;

int set_noblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int main()
{
    const char ip[] = "127.0.0.1";
    const short port = 5556;

    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET,ip,&server_address.sin_addr);

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0)
    {
        perror("socket error");
        return -1;
    }
    
    int ret = connect(sockfd,(struct sockaddr*)&server_address,sizeof(server_address));
    if(ret < 0)
    {
        perror("connect error");
        return -1;
    }

    set_noblocking(sockfd);    //set no blocking

    fd_set read_fds;
    fd_set exception_fds;
    fd_set write_fds;

    struct timeval wait_time = {5,0};

    while(1)
    {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exception_fds);

        FD_SET(sockfd,&read_fds);
        FD_SET(sockfd,&write_fds);
        FD_SET(sockfd,&exception_fds);

        if(select(sockfd+1,&read_fds,&write_fds,&exception_fds,&wait_time) <= 0)
        {
            switch(errno)
            {
                case EINTR:
                {
                    cout<<"select failed by signal..."<<endl;
                    break;
                }
                default:
                {
                    cout<<"select time out"<<endl;
                    break;
                }
            }

            return -1;
        }

        //can read
        if(FD_ISSET(sockfd,&read_fds))
        {
            char buffer[1024];
            bzero(buffer,sizeof(buffer));

            int cur_recv = 0;
            int temp = 0;
            
            while(1)
            {
                temp = recv(sockfd,buffer+cur_recv,sizeof(buffer)-1-cur_recv,0);
                if(temp <= 0)
                    break;
                else
                    cur_recv += temp;
            }
            
            buffer[cur_recv] = '\0';
            cout<<buffer<<endl;

            cout<<"cur_recv:"<<cur_recv<<endl;
            if(temp < 0)
            {
                if(errno == EAGAIN)
                {
                    cout<<"no data to read"<<endl;
                }else
                    break;
            }else
            {
                cout<<"peer out"<<endl;
                break;
            }
        }
        //can write
        if(FD_ISSET(sockfd,&write_fds))
        {
            string input_str;
            cin>>input_str;

            if(input_str == "end")
                break;
            input_str += "\n";
            int cur_send = 0;
            
            while(cur_send < input_str.length())
            {
                cur_send += send(sockfd,input_str.c_str()+cur_send,input_str.length()-cur_send,0);
            }

            cout<<"already send "<<cur_send<<" bytes"<<endl;
        }


        if(FD_ISSET(sockfd,&exception_fds))
        {
            cout<<"exception"<<endl;
            break;
        }
    }

    close(sockfd);
    return 0;
}


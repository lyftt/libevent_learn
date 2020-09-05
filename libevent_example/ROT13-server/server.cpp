/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
/* For fcntl */
#include <fcntl.h>

#include <event2/event.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>

using namespace std;

#define MAX_LINE 16384

void do_read(evutil_socket_t fd, short events, void *arg);
void do_write(evutil_socket_t fd, short events, void *arg);

char
rot13_char(char c)
{
    /* We don't want to use isalpha here; setting the locale would change
     * which characters are considered alphabetical. */
    /* if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M')) */
    /*     return c + 13; */
    /* else if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z')) */
    /*     return c - 13; */
    /* else */
        return c;
}

struct fd_state {
    char buffer[MAX_LINE];    //buffer for fd to store after read
    size_t buffer_used;       //how much space have used in buffer

    size_t n_written;
    size_t write_upto;

    struct event *read_event;   //event handler for read
    struct event *write_event;  //event handler for write
};

/*
 *
 *
 *
 */
struct fd_state *
alloc_fd_state(struct event_base *base, evutil_socket_t fd)
{
    struct fd_state *state = (struct fd_state *)malloc(sizeof(struct fd_state));
    if (!state)
        return NULL;
    //alloc read event
    state->read_event = event_new(base, fd, EV_READ|EV_PERSIST, do_read, state);
    if (!state->read_event) {
        free(state);
        return NULL;
    }
    //alloc write event
    state->write_event = event_new(base, fd, EV_WRITE|EV_PERSIST, do_write, state);
    if (!state->write_event) {
        event_free(state->read_event);        //free read_event  first
        free(state);
        return NULL;
    }

    //initialize variables in fd_state
    state->buffer_used = state->n_written = state->write_upto = 0;

    assert(state->write_event);
    return state;
}

void
free_fd_state(struct fd_state *state)
{
    event_free(state->read_event);
    event_free(state->write_event);
    free(state);
}

void
do_read(evutil_socket_t fd, short events, void *arg)
{
    struct fd_state *state = (struct fd_state *)arg;
    char buf[1024];
    int i;
    ssize_t result;
    while (1) 
    {
        assert(state->write_event);
        result = recv(fd, buf, sizeof(buf), 0);

        if (result <= 0)
            break;

        for (i = 0; i < result; ++i)  
        {
            if (state->buffer_used < sizeof(state->buffer))
                state->buffer[state->buffer_used++] = rot13_char(buf[i]);
            if (buf[i] == '\n') 
            {
                assert(state->write_event);
                event_add(state->write_event, NULL);      //add write event
                state->write_upto = state->buffer_used;
            }
        }
    }

    cout<<errno<<endl;
    if (result == 0) 
    {
        free_fd_state(state);
    } else if (result < 0) 
    {
        if (errno == EAGAIN) // have read all data this time
        {
            cout<<"no data to read"<<endl;
            return;
        }

        perror("recv");
        free_fd_state(state);
    }
}

void
do_write(evutil_socket_t fd, short events, void *arg)
{
    struct fd_state *state = (struct fd_state *)arg;

    while (state->n_written < state->write_upto) 
    {
        ssize_t result = send(fd, state->buffer + state->n_written,
                              state->write_upto - state->n_written, 0);
        if (result < 0) 
        {
            if (errno == EAGAIN) // XXX use evutil macro
            {
                cout<<"send buffer have no space"<<endl;
                return;
            }
            free_fd_state(state);
            return;
        }
        assert(result != 0);

        state->n_written += result;
    }
    
    cout<<"send ok"<<endl;
    if (state->n_written == state->buffer_used)
        state->n_written = state->write_upto = state->buffer_used = 1;

    event_del(state->write_event);   //delete
}

void
do_accept(evutil_socket_t listener, short event, void *arg)
{
    struct event_base *base = (struct event_base *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);

    //accept
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);

    if (fd < 0) 
    { // XXXX eagain??
        perror("accept");
    } 
    else if (fd > FD_SETSIZE) 
    {
        close(fd); // XXX replace all closes with EVUTIL_CLOSESOCKET */
    } 
    else 
    {
        struct fd_state *state;
        evutil_make_socket_nonblocking(fd);
        state = alloc_fd_state(base, fd);
        assert(state); /*XXX err*/
        assert(state->write_event);
        event_add(state->read_event, NULL);
    }
}

void
run(void)
{
    evutil_socket_t listener;          //socket listener(evutil_socket_t -> int)
    struct sockaddr_in sin;
    struct event_base *base;
    struct event *listener_event;

    base = event_base_new();    //reactor
    if (!base)
        return; /*XXXerr*/

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;          //ip address
    sin.sin_port = htons(5556);      //port

    listener = socket(AF_INET, SOCK_STREAM, 0);   //get socket fd
    evutil_make_socket_nonblocking(listener);     //set socket fd not blocking

#ifndef WIN32
    {
        int one = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));   //to reuse the socket in TIME_WAIT 
    }
#endif

    //bind
    if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) 
    {
        perror("bind");
        return;
    }

    //listen for SYN 
    if (listen(listener, 16)<0) 
    {
        perror("listen");
        return;
    }

    listener_event = event_new(base, listener, EV_READ|EV_PERSIST, do_accept, (void*)base);
    /*XXX check it */
    event_add(listener_event, NULL);

    event_base_dispatch(base);
}

int
main(int c, char **v)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    run();
    return 0;
}

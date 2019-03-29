#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 56970
#endif
#define MAX_QUEUE 5


void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);

/* These are some of the function prototypes that we used in our solution 
 * You are not required to write functions that match these prototypes, but
 * you may find the helpful when thinking about operations in your program.
 */

/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf) {
    for (struct client *d = game->head; d != NULL; d = d->next) {
        if(write(d->fd, outbuf, strlen(outbuf)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(d->ipaddr));
            remove_player(&(game->head), d->fd);
        }
    }
}


void announce_turn(struct game_state *game);
void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);


/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char *buf, int n) {
    for (int i = 0; i < n - 1; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            return i + 2;
        }
    }
    return -1;
}

/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));

    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

void move_player(struct client **dest, struct client **from, struct client *player) {
    struct client **p;
    for (p = from; *p && (*p)->fd != player->fd; p = &(*p)->next);
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        *p = t;
        player->next = *dest;
        *dest = player;
        printf("Moving client %d %s\n", player->fd, inet_ntoa(player->ipaddr));
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 player->fd);
    }
}

int read_in(struct client *player, struct client **list) {
    int nbytes;
    if ((nbytes = read(player->fd, player->in_ptr, MAX_BUF)) == -1) {
        fprintf(stderr, "Read from client %s failed\n", inet_ntoa(player->ipaddr));
        remove_player(list, player->fd);
        return -1;
    }
    player->in_ptr = (player->in_ptr + nbytes);
    
    int where;
    if ((where = find_network_newline(player->inbuf, MAX_BUF)) == -1) {
        return -1;
    } 
    player->inbuf[where - 2] = '\0';
    printf("--- Received %s\n", player->inbuf);
    return 0;
}

void sign_in(struct client *player, struct client **new_players, struct client **active_players, struct game_state *game) {
    if (read_in(player, new_players) == -1) {
        return ;
    }
    // Full line read in by read_in, continue to set up new player
    int name_check = 1;
    if (strlen(player->inbuf) == 0) {
        name_check = 0;
    }
    if (name_check != 0) {
        struct client *p;
        for (p = *active_players; p != NULL; p = p->next) {
            if (strcmp(player->inbuf, p->name) == 0) {
                name_check = 0;
                break;
            }
        }
    }
    for (struct client *p = *new_players; p != NULL; p = p->next) {
        printf("In new players: %d, %s\n", p->fd, inet_ntoa(p->ipaddr));
    }
    for (struct client *p = *active_players; p != NULL; p = p->next) {
        printf("In active players: %d, %s\n", p->fd, inet_ntoa(p->ipaddr));
    }
    if (name_check == 1) {
        move_player(active_players, new_players, player);
        strncat(player->name, player->inbuf, strlen(player->inbuf));

        char msg[MAX_MSG];
        char state[MAX_MSG];
        status_message(state, game);
        if (sprintf(msg, "%s Just Joined\n", player->name) < 0) {
            perror("sprintf");
            exit(1);
        }
        broadcast(game, msg);
        broadcast(game, state);
    } else {
        char *retry = UNACCEPTABLE_NAME;
        if (write(player->fd, retry, strlen(retry)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            remove_player(new_players, player->fd);
            return ;
        }
    }
    memset(player->inbuf, 0, MAX_BUF);
    player->in_ptr = player->inbuf;
}


int main(int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;
    
    if(argc != 2){
        fprintf(stderr,"Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }

    // Code to fix SIGPIPE problem
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to 
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);
    
    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;
    
    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;
    
    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);
    
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if(write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&new_players, p->fd);
            };
        }
        
        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be 
         * valid.
         */
        int cur_fd;
        for(cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if(FD_ISSET(cur_fd, &rset)) {
                // Check if this socket descriptor is an active player
                for(p = game.head; p != NULL; p = p->next) {
                    if (cur_fd == p->fd) {
                        
                        printf("_____%d, %s SIGNED IN\n", p->fd, p->name);

                        break;
                    }
                }
        
                // Check if any new players are entering their names
                for(p = new_players; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        // TODO - handle input from an new client who has
                        // not entered an acceptable name.
                        sign_in(p, &new_players, &(game.head), &game);
                        printf("_____%d, %s SIGNED IN\n", p->fd, p->name);
                        for (struct client *d = new_players; d != NULL; d = d->next) {
                            printf("In new players: %d, %s\n", d->fd, inet_ntoa(d->ipaddr));
                        }
                        for (struct client *d = game.head; d != NULL; d = d->next) {
                            printf("In new players: %d, %s\n", d->fd, inet_ntoa(d->ipaddr));
                        }
                        break;
                    }
                }   
            }
        }
    }
    return 0;
}



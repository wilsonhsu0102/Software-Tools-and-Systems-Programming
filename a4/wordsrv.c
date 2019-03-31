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
        return 0;
    } 
    player->inbuf[where - 2] = '\0';
    printf("--- Received %s ---\n", player->inbuf);
    return 1;
}

int sign_in(struct client *player, struct client **new_players, struct game_state *game) {
    struct client **active_players = &(game->head);
    int readIn;
    if ((readIn = read_in(player, new_players)) != 1) {
        return readIn;
    }
    // Full line read in by read_in, continue to set up new player
    int name_check = 1;
    // Checks for null string
    if (strlen(player->inbuf) == 0) {
        name_check = 0;
    }
    // Checks if name already been used
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
        printf("Before in new players: %d, %s\n", p->fd, inet_ntoa(p->ipaddr));
    }
    for (struct client *p = *active_players; p != NULL; p = p->next) {
        printf("Before in active players: %d, %s\n", p->fd, inet_ntoa(p->ipaddr));
    }
    if (name_check) {
        move_player(active_players, new_players, player);
        strncat(player->name, player->inbuf, strlen(player->inbuf));

        char msg[MAX_MSG];
        if (sprintf(msg, "%s Just Joined\n", player->name) < 0) {
            perror("sprintf");
            exit(1);
        }
        broadcast(game, msg);
        char state[MAX_MSG];
        status_message(state, game);
        if (write(player->fd, state, strlen(state)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            remove_player(&(game->head), player->fd);
            return -1;
        }
        memset(player->inbuf, 0, MAX_BUF);
        player->in_ptr = player->inbuf;
        return 1;
    } else {
        char *retry = UNACCEPTABLE_NAME;
        if (write(player->fd, retry, strlen(retry)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            remove_player(new_players, player->fd);
            return -1;
        }
        memset(player->inbuf, 0, MAX_BUF);
        player->in_ptr = player->inbuf;
        return  2;
    }
}

int make_move(struct game_state *game, struct client *player, char *dict_name) {
    int readIn;
    if ((readIn = read_in(player, &(game->head))) != 1) {
        return readIn;
    }
    // Incorrect input
    char *retry;
    if (strlen(player->inbuf) != 1 || player->inbuf[0] < 'a' || player->inbuf[0] > 'z') {
        retry = INCORRECT_INPUT;
        printf("-------- INCORRECT INPUT %s -----------\n", player->inbuf);
        if (write(player->fd, retry, strlen(retry)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            remove_player((&game->head), player->fd);
            return -1;
        }
        memset(player->inbuf, 0, MAX_BUF);
        player->in_ptr = player->inbuf;
        return 2;
    }
    int letter_index = player->inbuf[0] - 'a';
    if (game->letters_guessed[letter_index] == 1) {
        retry = CHAR_GUESSED;
        if (write(player->fd, retry, strlen(retry)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            remove_player((&game->head), player->fd);
            return -1;
        }
        memset(player->inbuf, 0, MAX_BUF);
        player->in_ptr = player->inbuf;
        return 2;
    }
    int guessed_right = 0;
    int game_solved = 1;
    game->letters_guessed[letter_index] = 1;
    for (int i = 0; i < strlen(game->word); i++) {
        if (game->word[i] == player->inbuf[0]) {
            game->guess[i] = game->word[i];
            guessed_right = 1;
        } else {
            if (game->guess[i] == '-') { 
                game_solved = 0;
            }
        }
    }
    if (!guessed_right) {
        game->guesses_left -= 1; 
    }
    char msg[MAX_MSG];
    if (sprintf(msg, "Let's Start a New Game!\n") < 0) {
        perror("sprintf");
        exit(1);
    }
    char state[MAX_MSG];
    char game_over[MAX_MSG];
    if (game_solved == 1) {
        char you_win[MAX_MSG];
        if (sprintf(you_win, "You win!\nThe word is %s\n", game->word) < 0) {
            perror("sprintf");
            exit(1);
        }
        if (sprintf(game_over, "Game Over! %s Won!\nThe word was %s\n", player->name, game->word) < 0) {
            perror("sprintf");
            exit(1);
        }
        for (struct client *p = game->head; p != NULL; p = p->next) {
            if (p == player) {
                if (write(p->fd, you_win, strlen(you_win)) == -1) {
                    fprintf(stderr, "Write to client %s failed\n", p->name);
                    remove_player(&(game->head), p->fd);
                }
            } else {
                if (write(p->fd, game_over, strlen(game_over)) == -1) {
                    fprintf(stderr, "Write to client %s failed\n", p->name);
                    remove_player(&(game->head), p->fd);
                }
            }
        }
        init_game(game, dict_name);
        broadcast(game, msg);
        status_message(state, game);
        broadcast(game, state);
    } else if (game->guesses_left == 0) {
        if (sprintf(game_over, "Game Over! No more moves left\nThe word was %s\n", game->word) < 0) {
            perror("sprintf");
            exit(1);
        }
        broadcast(game, game_over);
        init_game(game, dict_name);
        broadcast(game, msg);
        status_message(state, game);
        broadcast(game, state);
    } else {
        if (sprintf(msg, "%s Guessed %c\n", player->name, player->inbuf[0]) < 0) {
            perror("sprintf");
            exit(1);
        }
        broadcast(game, msg);
        if (!guessed_right) {
            if (sprintf(msg, "%c is not in the word\n", player->inbuf[0]) < 0) {
                perror("sprintf");
                exit(1);
            }
            broadcast(game, msg);
            if (player->next == NULL) {
                game->has_next_turn = game->head;
            } else {
                game->has_next_turn = player->next;
            }
        } else {
            if (sprintf(msg, "%c is in the word\n", player->inbuf[0]) < 0) {
                perror("sprintf");
                exit(1);
            }
        }
        status_message(state, game);
        broadcast(game, state);
        char msg[MAX_MSG];
        char *your_turn = "It's your turn! Guess?\n";
        if (sprintf(msg, "It's %s's turn\n", game->has_next_turn->name) < 0) {
            perror("sprintf");
            exit(1);
        }
        for (struct client *k = game->head; k != NULL; k = k->next) {
            if (k == game->has_next_turn) {
                if(write(k->fd, your_turn, strlen(your_turn)) == -1) {
                    fprintf(stderr, "Write to client %s failed\n", k->name);
                    remove_player(&(game->head), k->fd); // break or not?
                }
            } else {
                if(write(k->fd, msg, strlen(msg)) == -1) {
                    fprintf(stderr, "Write to client %s failed\n", k->name);
                    remove_player(&(game->head), k->fd); //break or not?
                }
            }
        }
    }
    memset(player->inbuf, 0, MAX_BUF); // might need to clear all inbuf for all active player
    player->in_ptr = player->inbuf;
    return 1;
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
                        if (game.has_next_turn == p) {
                            int makeMove = 0;
                            if ((makeMove = make_move(&(game), p, argv[1])) == -1 || makeMove == 2) {
                                printf("First\n");
                                break;
                            } else if (makeMove == 0) {
                                if(write(p->fd, "", 1) == -1) {
                                    fprintf(stderr, "Write to client %d failed\n", p->fd);
                                    if (!p->next) {
                                        game.has_next_turn = game.head;
                                    } else {
                                        game.has_next_turn = p->next;
                                    }
                                    remove_player(&(game.head), p->fd); // break or not?
                                    printf("Second\n");
                                    char msg[MAX_MSG];
                                    char *your_turn = "It's your turn! Guess?\n";
                                    if (sprintf(msg, "It's %s's turn\n", game.has_next_turn->name) < 0) {
                                        perror("sprintf");
                                        exit(1);
                                    }
                                    for (struct client *k = game.head; k != NULL; k = k->next) {
                                        if (k == game.has_next_turn) {
                                            if(write(k->fd, your_turn, strlen(your_turn)) == -1) {
                                                fprintf(stderr, "Write to client %s failed\n", k->name);
                                                remove_player(&(game.head), k->fd); // break or not?
                                            }
                                        } else {
                                            if(write(k->fd, msg, strlen(msg)) == -1) {
                                                fprintf(stderr, "Write to client %s failed\n", k->name);
                                                remove_player(&(game.head), k->fd); //break or not?
                                            }
                                        }
                                    }
                                    break;
                                }
                                printf("Third\n");
                                break;
                            }
                            
                        } else {
                            // If player types something, read in the msg, clear the buf
                            // tell player to wait for their turn.
                            int readIn;
                            if ((readIn = read_in(p, (&game.head))) == -1) {
                                break;
                            } else if (readIn == 0) {
                                if(write(p->fd, "", 1) == -1) {
                                    fprintf(stderr, "Write to client %d failed\n", p->fd);
                                    remove_player(&(game.head), p->fd); // break or not?
                                    printf("Second\n");
                                    break;
                                }
                                printf("Third\n");
                                break;
                            } else {
                                char *wait = WAIT_FOR_TURN;
                                memset(p->inbuf, 0, MAX_BUF);
                                p->in_ptr = p->inbuf;
                                if(write(p->fd, wait, strlen(wait)) == -1) {
                                    fprintf(stderr, "Write to client %s failed\n", p->name);
                                    remove_player(&(game.head), p->fd);
                                }
                            }
                        }

                        break;
                    }
                }
        
                // Check if any new players are entering their names
                for(p = new_players; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        // TODO - handle input from an new client who has
                        // not entered an acceptable name.
                        int signIn = 0;
                        if ((signIn = sign_in(p, &new_players, &game)) == -1 || signIn == 2) {
                            printf("First\n");
                            break;
                        } else if (signIn == 0) {
                            if(write(p->fd, "", 1) == -1) {
                                fprintf(stderr, "Write to client %d failed\n", p->fd);
                                remove_player(&new_players, p->fd); // break or not?
                                printf("Second\n");
                                break;
                            }
                            printf("Third\n");
                            break;
                        }
                        if (game.has_next_turn == NULL) { 
                            game.has_next_turn = game.head;
                        }
                        char msg[MAX_MSG];
                        char *your_turn = "It's your turn! Guess?\n";
                        if (sprintf(msg, "It's %s's turn\n", game.has_next_turn->name) < 0) {
                            perror("sprintf");
                            exit(1);
                        }
                        for (struct client *k = game.head; k != NULL; k = k->next) {
                            if (k == game.has_next_turn) {
                                if(write(k->fd, your_turn, strlen(your_turn)) == -1) {
                                    fprintf(stderr, "Write to client %s failed\n", k->name);
                                    remove_player(&(game.head), k->fd); // break or not?
                                }
                            } else {
                                if(write(k->fd, msg, strlen(msg)) == -1) {
                                    fprintf(stderr, "Write to client %s failed\n", k->name);
                                    remove_player(&(game.head), k->fd); //break or not?
                                }
                            }
                        }
                        for (struct client *d = new_players; d != NULL; d = d->next) {
                            printf("In new players: %d, %s\n", d->fd, inet_ntoa(d->ipaddr));
                        }
                        for (struct client *d = game.head; d != NULL; d = d->next) {
                            printf("In active players: %s, %s\n", d->name, inet_ntoa(d->ipaddr));
                        }
                        break;
                    }
                }   
            }
        }
    }
    return 0;
}



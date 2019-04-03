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
void special_broadcast(struct game_state *game, struct client *player, char *msg1, char *msg2);
void announce_remove_leaver(struct game_state *game, struct client *leaver);

/* These are some of the function prototypes that we used in our solution 
 * You are not required to write functions that match these prototypes, but
 * you may find the helpful when thinking about operations in your program.
 */

/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf) {
    for (struct client *d = game->head; d != NULL; d = d->next) {
        if(write(d->fd, outbuf, strlen(outbuf)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(d->ipaddr));
            printf("Player %s has disconnected\n", d->name);
            announce_remove_leaver(game, d);
        }
    }
}

/* Announce to all active player about who's turn it is */
void announce_turn(struct game_state *game) {
    char msg[MAX_MSG];
    char *your_turn = "It's your turn! Guess?\r\n";
    if (game->has_next_turn == NULL) {
        return ;
    }
    if (sprintf(msg, "It's %s's turn\r\n", game->has_next_turn->name) < 0) {
        perror("sprintf");
        exit(1);
    }
    special_broadcast(game, game->has_next_turn, msg, your_turn);
}

/* Announce to all active players about who's the winner */
void announce_winner(struct game_state *game, struct client *winner) {
    char you_win[MAX_MSG];
    char game_over[MAX_MSG];
    if (sprintf(you_win, "You win!\r\nThe word is %s\r\n", game->word) < 0) {
        perror("sprintf");
        exit(1);
    }
    if (sprintf(game_over, "Game Over! %s Won!\r\nThe word was %s\r\n", winner->name, game->word) < 0) {
        perror("sprintf");
        exit(1);
    }
    special_broadcast(game, winner, game_over, you_win);
    char *new_game = "Let's Start a New Game!\r\n";
    broadcast(game, new_game);
}
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);


/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;

/* 
 * Broadcast to all active players about that leaver left the game, after
 * removing the leaver.
 */
void announce_remove_leaver(struct game_state *game, struct client *leaver) {
    char left_game[MAX_MSG];
    if (sprintf(left_game, "%s just left the game\r\n", leaver->name) < 0) {
        perror("sprintf");
        exit(1);
    }
    remove_player(&(game->head), leaver->fd);
    broadcast(game, left_game);
}

/*
 * Broadcast to all active players except specified player about msg1.
 * Send msg2 to specified player.
 */
void special_broadcast(struct game_state *game, struct client *player, char *msg1, char *msg2) {
    for (struct client *k = game->head; k != NULL; k = k->next) {
        if (k == player) {
            if(write(k->fd, msg2, strlen(msg2)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", k->name);
                printf("Player %s has disconnected\n", k->name);
                announce_remove_leaver(game, k);
            }
        } else {
            if(write(k->fd, msg1, strlen(msg1)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", k->name);
                printf("Player %s has disconnected\n", k->name);
                announce_remove_leaver(game, k);
            }
        }
    }
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * 
 * Return 1 + the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
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

/* 
 * Move player from new player list to active player list. 
 */
void move_player(struct client **active_player, struct client **new_player, struct client *player) {
    struct client **p;
    for (p = new_player; *p && (*p)->fd != player->fd; p = &(*p)->next);
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        *p = t;
        player->next = *active_player;
        *active_player = player;
        printf("Moving client %s to active player\n", player->name);
    } else {
        fprintf(stderr, "Trying to move fd %d, but I don't know about it\n",
                 player->fd);
    }
}

/* 
 * Function to read in strings from client side. Save 
 * read strings in player's inbuf.
 *
 * Return -1 if socket closes or error, 0 if network new 
 * line not found, 1 if network new line found.
 */
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
    return 1;
}

/* 
 * Sign in new players, after signing in successfully move them to active player list.
 * 
 * Return -1 if socket closes or error, 0 if read_in process has not yet been completed,
 * 1 if sign in successfully, 2 if user entered an invalid name.
 */
int sign_in(struct client *player, struct client **new_players, struct game_state *game) {
    struct client **active_players = &(game->head);
    int readIn;
    if ((readIn = read_in(player, new_players)) == -1) {
        return -1;
    } else if (readIn == 0) {
        // Checks if socket is still open.
        if(write(player->fd, "", 1) == -1) {
            fprintf(stderr, "Write to client %d failed\n", player->fd);
            printf("Player with fd: %d has disconnected\n", player->fd);
            remove_player(new_players, player->fd);
            return -1;
        }
        return 0;
    }
    // Full line read in by read_in, continue to set up new player
    int name_check = 1;
    // Checks for null string
    if (strlen(player->inbuf) == 0) {
        name_check = 0;
    }
    // Checks if name already been used by active players
    if (name_check != 0) {
        struct client *p;
        for (p = *active_players; p != NULL; p = p->next) {
            if (strcmp(player->inbuf, p->name) == 0) {
                name_check = 0;
                break;
            }
        }
    }
    if (name_check) {
        // Valid name given, move player from new player list
        // to active player list
        strncat(player->name, player->inbuf, strlen(player->inbuf));
        move_player(active_players, new_players, player);
        printf("Player %s has joined the game\n", player->name);
        // Broadcast player joined message
        char msg[MAX_MSG];
        if (sprintf(msg, "%s Just Joined\r\n", player->name) < 0) {
            perror("sprintf");
            exit(1);
        }
        broadcast(game, msg);
        char state[MAX_MSG];
        status_message(state, game);
        if (write(player->fd, state, strlen(state)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            printf("Player %s has disconnected\n", player->name);
            remove_player(&(game->head), player->fd);
            return -1;
        }
        // Clean inbuf
        memset(player->inbuf, 0, MAX_BUF);
        player->in_ptr = player->inbuf;
        // If first player to sign in, player has next turn.
        if (game->has_next_turn == NULL) { 
            game->has_next_turn = game->head;
        }
        return 1;
    } else {
        // player entered invalid name
        char *retry = UNACCEPTABLE_NAME;
        if (write(player->fd, retry, strlen(retry)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            printf("Player with fd: %d has disconnected\n", player->fd);
            remove_player(new_players, player->fd);
            return -1;
        }
        memset(player->inbuf, 0, MAX_BUF);
        player->in_ptr = player->inbuf;
        return  2;
    }
}

/* 
 * Read in the input from the current_player, if it's not a single lower case 
 * character from a - z or it had been guessed before, we prompt the user to
 * retry. If it's none of the above, then it's a valid input, and we change 
 * the current game state base on the input.
 * 
 * Return -1 if socket is closed or error, 0 if read in process is not finished
 * yet, 1 if player successfully made a move, 2 if player gave invalid input.
 */
int make_move(struct game_state *game, struct client *player, char *dict_name) {
    int readIn;
    char left_game[MAX_MSG];
    if (sprintf(left_game, "%s just left the game\r\n", player->name) < 0) {
        perror("sprintf");
        exit(1);
    }
    struct client *next_player = player->next;
    if ((readIn = read_in(player, &(game->head))) == -1) {
        // current player removed during read_in, advance turn.
        if (next_player == NULL) {
            game->has_next_turn = game->head;
        } else {
            game->has_next_turn = next_player;
        }
        broadcast(game, left_game);
        return -1;
    } else if (readIn == 0) {
        // Checks if socket is still open
        if(write(player->fd, "", 1) == -1) {
            fprintf(stderr, "Write to client %d failed\n", player->fd);
            printf("Player %s has disconnected\n", player->name);
            announce_remove_leaver(game, player);
            if (next_player == NULL) {
                game->has_next_turn = game->head;
            } else {
                game->has_next_turn = next_player;
            }
            announce_turn(game);
            return -1;
        }
        return 0;
    }
    // Checks for invalid inputs
    int letter_index = player->inbuf[0] - 'a';
    if (strlen(player->inbuf) != 1 || game->letters_guessed[letter_index] || player->inbuf[0] < 'a' || player->inbuf[0] > 'z') {
        char *retry = INCORRECT_INPUT;
        if (write(player->fd, retry, strlen(retry)) == -1) {
            fprintf(stderr, "Write to client %s failed\n", inet_ntoa(player->ipaddr));
            printf("Player %s has disconnected\n", player->name);
            announce_remove_leaver(game, player);
            if (next_player == NULL) {
                game->has_next_turn = game->head;
            } else {
                game->has_next_turn = next_player;
            }
            return -1;
        }
        memset(player->inbuf, 0, MAX_BUF);
        player->in_ptr = player->inbuf;
        return 2;
    }
    // Reveals the char in word, if guessed right
    // Checks if the whole word is guessed.
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
    printf("Player %s made move %c\n", player->name, player->inbuf[0]);
    char guessed[MAX_MSG];
    if (sprintf(guessed, "%s Guessed %c\r\n", player->name, player->inbuf[0]) < 0) {
        perror("sprintf");
        exit(1);
    }
    broadcast(game, guessed);
    if (!guessed_right) {
        game->guesses_left -= 1; 
        if (sprintf(guessed, "%c is not in the word\r\n", player->inbuf[0]) < 0) {
            perror("sprintf");
            exit(1);
        }
        if (player->next == NULL) {
            game->has_next_turn = game->head;
        } else {
            game->has_next_turn = player->next;
        }
    } else {
        if (sprintf(guessed, "%c is in the word\r\n", player->inbuf[0]) < 0) {
            perror("sprintf");
            exit(1);
        }
    }
    broadcast(game, guessed);
    if (game_solved) {
        announce_winner(game, player);
        init_game(game, dict_name);
    } else if (game->guesses_left == 0) {
        char game_over[MAX_MSG];
        if (sprintf(game_over, "Game Over! No more moves left\r\nThe word was %s\r\nLet's Start a New Game!\r\n", game->word) < 0) {
            perror("sprintf");
            exit(1);
        }
        broadcast(game, game_over);
        init_game(game, dict_name);
    }
    char state[MAX_MSG];
    status_message(state, game);
    broadcast(game, state);
    announce_turn(game);
    memset(player->inbuf, 0, MAX_BUF);
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

    // Sig Handler to handle SIGPIPE
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
                            // current player's turn
                            make_move(&(game), p, argv[1]);
                        } else {
                            // Not current player's turn
                            // If player types something, read in the msg, clear the buf
                            // tell player to wait for their turn.
                            int readIn;
                            if ((readIn = read_in(p, (&game.head))) == 0) {
                                // check if socket is still open.
                                if(write(p->fd, "", 1) == -1) {
                                    fprintf(stderr, "Write to client %d failed\n", p->fd);
                                    printf("Player %s has disconnected\n", p->name);
                                    announce_remove_leaver(&game, p);
                                }
                            } else if (readIn == 1) {
                                char *wait = WAIT_FOR_TURN;
                                memset(p->inbuf, 0, MAX_BUF);
                                p->in_ptr = p->inbuf;
                                if(write(p->fd, wait, strlen(wait)) == -1) {
                                    fprintf(stderr, "Write to client %s failed\n", p->name);
                                    printf("Player %s has disconnected\n", p->name);
                                    announce_remove_leaver(&game, p);
                                }
                            }
                        }
                        break;
                    }
                }
        
                for(p = new_players; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        if (sign_in(p, &new_players, &game)) {
                            announce_turn(&game);
                        }
                        break;
                    }
                }   
            }
        }
    }
    return 0;
}

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define MAX_CLIENTS 10
#define BUFFER_SZ 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

static char* word;
static char* liar_name; 
static int liar_uid;
static int game_status = 0; // 1이면 게임 중으로 다시 게임을 시작할 수 없다.
static int vote_count = 0; // 현재까지 투표를 진행한 사람
static int pros = 0; // 찬성

static char vote[MAX_CLIENTS] = "\0";
static char vote_setting[MAX_CLIENTS] = "\0";
static int timer = 60;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

typedef struct{
    char m_username[32];
    char m_buffer[100];
} MESSAGE;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

char *ltrim(char *str, const char *seps)
{
    size_t totrim;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    totrim = strspn(str, seps);
    if (totrim > 0) {
        size_t len = strlen(str);
        if (totrim == len) {
            str[0] = '\0';
        }
        else {
            memmove(str, str + totrim, len + 1 - totrim);
        }
    }
    return str;
}

char *rtrim(char *str, const char *seps)
{
    int i;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    i = strlen(str) - 1;
    while (i >= 0 && strchr(seps, str[i]) != NULL) {
        str[i] = '\0';
        i--;
    }
    return str;
}

char *trim(char *str, const char *seps)
{
    return ltrim(rtrim(str, seps), seps);
}

void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void initialize_vote_setting(){
    vote_count = 0;
    pros = 0;
    timer = 60;
    strcpy(vote, vote_setting);
}
/* Add clients to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void send_to_all_message(char * s){
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0 ; i < MAX_CLIENTS; ++i){
        if(clients[i]){
            if(write(clients[i]->sockfd, s, strlen(s)) < 0){
				perror("ERROR: write to descriptor failed");
				break;
			}
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_to_self_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void start_game(client_t * cli, char* buff_out){ // buff_out을 인자로 받아야 하는가?
    //char buff_out[BUFFER_SZ];

    sprintf(buff_out, "\n\n>>>> Liar Game Start!! <<<\n");
    initialize_vote_setting();
    printf("%s", buff_out);
    send_to_all_message(buff_out);

    srand(time(NULL));
    client_t* cli_liar = clients[rand() % MAX_CLIENTS];
    while(cli_liar == NULL){
        cli_liar = clients[rand() % MAX_CLIENTS];
    }

    liar_name = cli_liar->name;
    liar_uid = cli_liar->uid;

    word = "banana"; // 원래는 파일에서 읽어올 예정

    sprintf(buff_out, ">>> The word is %s <<<\n", word);
    printf("%s", buff_out);
    printf("uid: %d, liar_uid: %d, liar_name: %s\n",cli->uid, liar_uid, liar_name);
    
    send_message(buff_out, liar_uid);


    // name이라는 전역변수를 만들긴 했는데, 서로 다른 쓰레드인데 접근 가능한 지 의문
    
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ];
	char name[32];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Name
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined\n", cli->name);
		printf("%s", buff_out);
		send_message(buff_out, cli->uid);
	}

	bzero(buff_out, BUFFER_SZ);

	while(1){
		if (leave_flag) {
			break;
		}

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){

                char * word_slice = malloc(sizeof(char) * BUFFER_SZ);
                strcpy(word_slice, buff_out);

                char* ptr_word_slice = strtok(word_slice, ":");
                ptr_word_slice = strtok(NULL, ":");

                trim(ptr_word_slice, NULL); // trim 처리 중요
                /*
                현재의 방식보단 '/'키워드를 만들어서 게임을 시작하고 게임을 끝내고 liar, word를 파악할 수 있도록 짜자.
                */
                if(ptr_word_slice[0] == '/'){
                    switch(ptr_word_slice[1]){
                    case 's':
                        if(game_status == 0){
                            game_status = 1;
                            start_game(cli, buff_out);
                        } else {
                            sprintf(buff_out, "The game has already begun.\n");
                            send_to_self_message(buff_out, cli->uid);
                        }
                        break;
                    /*
                        코드가 조잡해보이는데 괜찮은가...
                        과반수 이상이 찬성해야 투표하게 만들고 싶은데
                    */
                    case 'v':
                        if(game_status == 1){
                            if(ptr_word_slice[3] != 'o' && ptr_word_slice[3] != 'x'){
                                sprintf(buff_out, "You must enter o or x\n");
                                send_to_self_message(buff_out, cli->uid);
                                break;
                            }
                            char ox[2] = "\0";
                            ox[0] = ptr_word_slice[3];
                            
                            if(strcmp(ox, "o") == 0) pros++;
                            strcat(vote, ox);// ox 선택도 둬야하는구나
                            ++vote_count;
                            if(cli_count == vote_count) sprintf(buff_out, "The vote is finished.\n");
                            else sprintf(buff_out, "The vote is in progress.\n> [Voting Status: %s]\n> The number of last: %d\n", vote, cli_count - vote_count);
                            send_to_all_message(buff_out);
                            printf("%s\n", buff_out);
                            sleep(1);
                                
                            while(vote_count != cli_count && timer > 0){ // 시간이 지나면 취소
                                sleep(5);
                                timer -= 5;
                                int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
                                // sprintf(buff_out, "Waiting until the vote is over...\n> [Voting Status: %s]\n> Time Remaining : %dsec\n", vote, timer);
                                // send_to_all_message(buff_out);
        
                            }
                            if(vote_count != cli_count || pros < vote_count / 2){
                                sprintf(buff_out, "The vote is cancelled.\n");
                                send_to_all_message(buff_out);
                                printf("%s\n", buff_out);
                                initialize_vote_setting();
                                break;
                            } else {
                                sprintf(buff_out,"Who is the liar?");
                                for(int i = 0; i < MAX_CLIENTS; i++){
                                    if(clients[i]){
                                        strcat(buff_out, "\n>>>> ");
                                        strcat(buff_out, clients[i]->name);
                                    }
                                }
                                strcat(buff_out,"\n");
                                send_to_self_message(buff_out, cli->uid); 
                                
                                /*
                                    엄청 비효율적, 각각의 스레드가 자신에게 보내도록 해야하기 때문이다 
                                    poll의 경우 하나의 메인쓰레드에서 관리하면 좋을텐데
                                */
                                int receive = recv(cli->sockfd, buff_out,BUFFER_SZ, 0);
                                if(receive > 0){

                                }
                                /*
                                    liar_game.md To - Do List 2번 내용 참고
                                */

                                sprintf(buff_out,"The game is finished. The word is %s. The liar is %s\n", word, liar_name); 
                                send_to_self_message(buff_out, cli->uid);
                                game_status = 0;
                            }   
                        } else {
                            sprintf(buff_out, "The game hasn't started yet.\n");
                            send_to_self_message(buff_out, cli->uid);
                        }
                        break;
                    case 'h':
                        sprintf(buff_out, "/s : game start\n> /v [o or x]: vote start\n> /h : help\n> exit\n");
                        send_to_self_message(buff_out, cli->uid);
                        break;
                    }
                } else {
                    send_message(buff_out, cli->uid);

                    str_trim_lf(buff_out, strlen(buff_out));
                    printf("%s -> %s\n", buff_out, cli->name);
                }
                free(word_slice);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}
        
		bzero(buff_out, BUFFER_SZ);
	}

  /* Delete client from queue and yield thread */
	close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

  /* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
	}

	/* Bind */
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

  /* Listen */
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE CHATROOM ===\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}
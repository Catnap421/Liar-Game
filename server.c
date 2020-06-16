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

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

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

void start_game(client_t * cli, char* buff_out){ // buff_out을 인자로 받아야 하는가?
    //char buff_out[BUFFER_SZ];

    sprintf(buff_out, "liar game start!!\n");
    word = "target";
    printf("%s", buff_out);
    printf("The word: %s\n", word);
    printf("uid: %d\n",cli->uid);
    send_to_all_message(buff_out);

    srand(time(NULL));
    client_t* cli_liar = clients[rand() % MAX_CLIENTS];
    while(cli_liar == NULL){
        cli_liar = clients[rand() % MAX_CLIENTS];
    }
    // 한 명을 제외하고 모두에게 제시어(word)를 전달해야 한다
    // clients[i] 인데 여기서 임의의 i값만 제거해서 하면 될듯?
    // cli_count를 통해서 클라 수도 확인하고 있으니
    // name이라는 전역변수를 만들긴 했는데, 서로 다른 쓰레드인데 접근 가능한 지 의문
    
    //int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
    //if(receive > 0){
        // if(strstr(buff_out, word) != NULL){
        //     word = NULL;
        //     sprintf(buff_out, "Game Finished!!");
        //     printf("%s", buff_out);
        //     send_message(buff_out, cli->uid);
        // }  
         // 이 부분 같은 경우 : 라이어가 제시어를 맞추는 부분이기 때문에 나중에 추가
    //}

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

                if(strcmp(ptr_word_slice, "game start") == 0){
                    start_game(cli, buff_out);
                } else if(strcmp(ptr_word_slice, "what is word") == 0){
                    sprintf(buff_out,"The word is %s\n", word);
                    printf("%s", buff_out);
                    send_to_all_message(buff_out);
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
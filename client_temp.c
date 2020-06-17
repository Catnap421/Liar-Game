#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048
typedef struct {
    char m_username[32];
    char m_buffer[100];
} MESSAGE;
// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];
int vote_flag = 0; // while voting, user can't chat!

void str_overwrite_stdout() {
  printf("%s", "> ");
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

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

void timer(){
    int timer = 0;

    while(timer <= 60 && vote_flag == 1){
        printf("> Remaining Time: %dsec\n", timer);
        timer += 5;
        sleep(5);
    }
    if(vote_flag == 1){
        printf("The time is done.\n> ");
        send(sockfd, "*fail\n", strlen("*fail\n"), 0);
    }
    timer = 0;
}

void send_msg_handler() {
    char message[LENGTH] = {};
	char buffer[LENGTH + 32] = {};
    while(1) {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);
        if(vote_flag == 1){
            if(message[0] == '!'){
                switch(message[1]){
                    case 'o':
                    case 'x':
                        printf("Right Input!!\n");
                        break;
                    case 'n': // 이건 vote_flag 2로 설정하고 하자
                        printf("name\n");
                        break;
                    default:
                        printf("Wrong Input!!\n");
                        break;
                }
            } else {
                printf("Wrong Input!!\n");
            } //sprintf(buffer, "%s: %s\n", name, message);
        } else {
            if (strcmp(message, "exit") == 0) {
                    break;
            } else {
                sprintf(buffer, "%s: %s\n", name, message); // 이 형태로 고정되서 나오는데 
                send(sockfd, buffer, strlen(buffer), 0);
            }
        }

        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 32);
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {
	char message[LENGTH] = {};
    while (1) {
		int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0) {
            if(message[0] == '/'){
                switch(message[1]){
                    case 'v':
                        vote_flag = 1;
                        printf("> Enter o or x\n> Enter >> ![o or x]\n");
                        pthread_t timer_thread;
                        if(pthread_create(&timer_thread,  NULL, (void *)timer, NULL) != 0){
                            printf("ERROR: pthread\n");
                        }
                        break;
                }
            }
            else 
                printf("%s", message);
            str_overwrite_stdout();
        } else if (receive == 0) {
			break;
        } else {
			// -1
		}
		memset(message, 0, sizeof(message));
    }
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("Please enter your name: ");
    fgets(name, 32, stdin);
    str_trim_lf(name, strlen(name));


	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);


  // Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
        return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(flag){
			printf("\nBye\n");
			break;
        }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
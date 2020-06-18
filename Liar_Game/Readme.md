# ChatRoom의 구조

## server.c

### main

랑데뷰 소켓 -> 커뮤니케이션 소켓을 생성해 통신

포트를 전달 받고, 소켓 환경을 설정한다.

<u>pipe signal을 무시하는 코드가 존재</u> 

![image-20200615212424080](C:\Users\user\AppData\Roaming\Typora\typora-user-images\image-20200615212424080.png)

이후에 bind와 listen을 연결하여 소켓 환경을 구성한다.

> bind  - 생성한 소켓을 서버 소켓으로 등록
>
> listen - 서버 소켓을 통해 클라이언트의 접속 요청을 확인

그리고 이후엔 while문이 반복해서 돌아간다. while 문 안에는 새로운 클라이언트를 accept하고, 새로운 클라이언트에게 구조체(client_t)를 할당하여 서버에서 client의 uid와 sockfd(socket file descriptor)를 통해 클라이언트를 구분하고, 통신한다.

마지막으로 그렇게 만든 클라이언트에 대해 **새로운 쓰레드를 생성해** handle_client 함수를 통해 관리한다.



### handle_client

handle_client에선 클라이언트의 초기 설정(**recv를 통한 name 메시지 수신 대기**)과 첫 연결 성공 이후에 해당 클라이언트가 입장했다는 메시지를 모두에게 전달해주게 된다.

그리고 이후에 while문을 통해 지속적인 상태를 감시한다. 

recv를 통해 cli->sockfd를 항상 확인**(대기 상태)**하고 있는데, 이는 client.c에서 메시지를 전달하면 recv의 리턴 값이 변하게 되고, if 문이 수행된다.

if 문에선 기본적으로 클라이언트의 입력에 따른 동작을 설정하고 있다.

1. 정상 입력 값

   정상적인 입력 값일 경우, 해당 메시지(buff_out)를 서버에 출력하고, send_message 함수를 통해 다른 모든 클라이언트에게 해당 내용을 전달한다.

2. exit 

   exit의 경우, 다른 클라이언트들에게 퇴장한다는 메시지를 보내고, leave_flag의 값을 1로 만들어 while문에서 빠져나오도록 한다.

while문을 빠져나오면 close와 free, pthread_detach를 통해서 해당 클라이언트의 존재를 지워버린다.



> #### **recv 함수의 동작**
>
> 1**.소켓 버퍼의 내용보다 많이 요구할 경우**
>
> 소켓 버퍼의 내용보다 recv에서 요구하는 크기가 더 클 경우 소켓 버퍼의 내용을 모두 가져가고 recv는 리턴된다.
>
> **2.소켓 버퍼의 내용보다 요구한 내용이 적을 경우**
>
> 소켓 버퍼의 내용보다 recv에서 요구한 내용이 적을 경우는 요구한 만큼만 가져가고 recv는 리턴된다.
>
> **3.소켓 버퍼에 데이터가 없을 경우**
>
> 소켓 버퍼에 데이터가 없을 경우(상대편에서 아직 하나도 데이터를 보내지 않은 경우)는 한 바이트라도 소켓 버퍼에 들어올 때까지 기다린다.
>
> ※Blocking Socket과 Non-Blocking Socket의 동작이 다른데 Blocking Socket에서는 데이터가 들어올 때까지 무한정 대기하고, Non-Blocking Socket에서는 데이터가 없으면 바로 리턴한다.



### send_message

send_message에선 메시지를 송신한 클라이언트의 uid값을 파악하고, 임계구역을 설정해 다른 쓰레드가 동일하게 호출할 수 없도록 한다.

파악한 클라이언트의 uid는 해당 클라이언트를 제외한 나머지 클라이언트들에게 write() 함수를 통해서 메시지를 전달하기 위함이다.



## client.c

### main

main에선 먼저 포트를 입력받아, 해당 포트를 통해 연결된 서버 소켓에 connect를 시도한다. connect가 성공하면 해당 이름을 send를 통해 보내게 되는데, 서버에선 handle_client에서 이름과 함께 채팅방에 입장했다는 알림을 모두에게 보내게 된다.

그리고 각각의 스레드를 만들어 send_msg_handler와 recv_msg_handler를 통해  지속적으로 클라이언트 프로세스의 상황을 감시하게 된다.



### send_msg_handler

while문을 통해 fgets 상태를 유지하고 있는다. 그러다가 입력이 존재하게 되면 해당 입력값이 "exit"이라면 break를 통해 클라이언트 프로세스를 종료시키고, 마약 그게 아니라면 클라이언트의 name과 함께 메시지를 send를 통해 서버로 전달한다.



### recv_msg_handler

send_msg_handler와 비슷하게 동작한다. recv를 계속 감시하고 있다가 recv의 리턴 값이 0보다 커질 때(즉, 메시지가 전송됐을 때), 해당 메시지를 화면에 출력한다.



## 라이어 게임

라이어 게임 스타트
-> 서버에서 해당 워드를 기록하고 있어야 하는데, 서버 또한 스레드 형식으로 돌아간다는 걸 명시하자 -> static?
게임을 시작하면 word를 저장한다
게임을 시작하고 나면 새로운 게임을 시작할 수 없다 -> flag 설정? 혹은 word의 유무
이후 시간이 지나거나 게임이 끝나 라이어를 밝혀낼 때가 되면 라이어가 누구인지 맞춰야 한다. -> 라이어도 저장쓰
이 때 라이어는 참가자들의 라이어 발견 시 라이어는, word를 맞춰야 한다.
word를 맞추고 나면 라이어 승리
만약 라이어를 발견하지 못할 시 
라이어 승리
만약 라이어 발견 && word도 못 맞출 경우 참가자들 승리 

<u>모든 클라이언트가 start_game 함수를 호출해야 한다?</u>

### To - Do List

1. ~~game start를 외치면 게임이 끝날 때까지는 외쳐도 소용이 없다. -> flag  이용??~~

2. 이야기를 마친 후 투표 시간이 존재해야하는데 이를 어떻게 할당할 것인가 -> 타이머??

   과반수 투표를 통해 취소를 진행해도 좋다

   /p를 했을 때,  투표(종료)를 원하는 사람의 상태를 배열로 만들어서 찬반을 기록해서 과반수 이상 넘어가면 투표(종료)가 진행되고, **(투표가 되면 liar를 맞추는 단계와 이제 liar가 word를 맞추는 단계를 진행하고!!**) 이후에 liar와 word를 말해주고 끝내자. -> 투표 진행에서 일정 시간이 지나면 취소되도록

   &&& 만약 투표가 취소되면 모두 다 취소를 해줘야하는데 이건 어떻게?? **그건 flag를 두면 돼!!**

   

   사실 라이어가 대답을 해야하는데 /와 같은 키워드를 통해서 답을 말하도록 할까? (이 때 기회는??)

   모두가 다같이 라이어를 맞추는 상황도 존재해야 하는데 이는 어떻게 처리 할까??

3. ~~모두에게 같은 상황을 만들어주고 싶은데(서로 다른 스레드가 똑같은 실행 환경) 이를 어떻게하면 만들 수 있는가?~~

4. MESSAGE라는 구조체를 만들어서 메시지를 전송할까?? -> 구조체로 만들었을 때의 장점은?? 음.. 좀 더 클라이언트 관리하기가 편하다?? 클라이언트의 관리가 어떤 면에서 필요한가?  라이어로 변한 클라이언트가 존재한다. 나중에 답을 맞출 때 이 라이어가 word를 답해야 정답인데... 이 때 필요하지 않을까? 하지만 서버에서 직접 1대1로 클라를 관리하고 있으며, 라이어의 uid도 기록하고 있으면 되지 않을까? -> O

   message 구조체의 경우 단어를 뽑아내기가 좋다는 장점이 있다는 것?? 구조체 사이즈의 경우 어떡하지?

   MESSAGE message ; 이렇게 하고 **recv(sockfd, &message, sizeof(message), 0);** 으로 처리 가능

5. 투표 과정에서 각 스레드의 타이머가 따로 돌아가기  때문에 문제가 생긴다.  while을 통해 진행하는게 무리인듯하다 -> while을 빼려면 투표의 상황은 계속 지켜보고 있어야 하고, 새로운 쓰레드를 만든다??

   <u>시그널 관련해서 개념을 알아두고 이용가능한지 생각해보기</u>

   이름을 받아서 uid로 변환할 필욘 없겟구만유 (uid를 기록하고 있으므로!!)

   <u>line 302: recv를 통해서 받지 않아도 시간이 지나면 어떻게 처리할까?</u>

   처음 투표를 시작하면 브로드캐스트로 똑같은 이벤트를 받도록 처리 -> 클라단에서 처리해보도록 하자

   -> **<u>그렇게 해야하는 이유? 투표중에 사용자가 어떠한 입력이 잇어도 무시가 되어야 하기 때문!!</u>**

   혹은???

   대답한 사람은 게임을 끝내!! 그리고 vote_status를 통해 새로운 투표는 시작될 수 없게 하고!!

   누군가 마지막으로 투표를 하면!!!  모두에게 투표가 끝났다는 방송을 하고!! 함수를 진행시킬까?

   ```c
   if(vote_count != cli_count || pros < vote_count / 2){
   	sprintf(buff_out, "The vote is cancelled.\n");
       send_to_all_message(buff_out);
       printf("%s\n", buff_out);
       initialize_vote_setting();
       break;
   } else {
       if(cli->uid != liar_uid){
       	sprintf(buff_out,"Who is the liar?\n");
           for(int i = 0; i < MAX_CLIENTS; i++){
           	if(clients[i]){
               	strcat(buff_out, ">>>> ");
                   strcat(buff_out, clients[i]->name);
                   strcat(buff_out,"\n");
               }
           } 
       }
       else {
       	sprintf(buff_out, "What is the word?\n");
       }
                                       
       send_to_self_message(buff_out, cli->sockfd); 
                                   
       int receive = recv(cli->sockfd, buff_out,BUFFER_SZ, 0);
       if(receive > 0){
                                       
       }
       /*
            liar_game.md To - Do List 2번 내용 참고
        */
   
       sprintf(buff_out,"The game is finished. The word is %s. The liar is %s\n", word, liar_name); 
       send_to_self_message(buff_out, cli->sockfd);
       game_status = 0;
   }   
   ```

   

   ## wt 사용법

   - **Create a new pane, splitting horizontally**: Alt+Shift+- (Alt, Shift, and a minus sign)
   - **Create a new pane, splitting vertically**: Alt+Shift++ (Alt, Shift, and a plus sign)
   - **Move pane focus**: Alt+Left, Alt+Right, Alt+Down, Alt+Up
   - **Resize the focused pane**: Alt+Shift+Left, Alt+Shift+Right, Alt+Shift+Down, Alt+Shift+Up
   - **Close a pane**: Ctrl+Shift+W

   






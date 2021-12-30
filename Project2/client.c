#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h> //localtime(), tm structure
#include <sys/timeb.h> //timeb structure, ftime()
#include <sys/types.h> //time_t structure in timeb
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#define MAX_MSG 65535

//현재 시간 h:m:s.ms 형식으로 저장
void get_time(char * time) {
	struct timeb tb;
	struct tm *tm;
	ftime(&tb); // sets the time and millitm members of the timeb structure
	tm = localtime(&tb.time); //달력시간(년,월,일,시,분,초)로 변환 (returns a pointer to borken-down time structure)
	sprintf(time, "%02d:%02d:%02d.%03d", tm->tm_hour, tm->tm_min, tm->tm_sec, tb.millitm);
	//0은 채워질 문자, 2는 총 자리수. time에 형식에 맞춰 저장한다
}

//pthread_create에 인자로 넘겨줄 함수. 소켓 생성해서 서버와 연결하고 메세지 수신, 로그 저장. 매개변수:포트번호
void * start_routine(void * arg) {
	unsigned int port = *(unsigned int *)arg;
	
	//소켓 생성
	int client_socket = socket(AF_INET, SOCK_STREAM, 0); //IPv4 Internet protocols, TCP
    //마지막 인자가 0인 이유: 'IPv4 인터넷 프로토콜 체계'에서 동작하는 '연결지향형 데이터 소켓'은 IPPROTO_TCP하나밖에 없기 때문.
	//그래서 그냥 TCP 소켓이라고도 부르는 것
	if(client_socket < 0){//socket함수는 실패 시 -1을 리턴하므로
		printf("Client: Can't open stream socket\n");
        exit(1);
    }
	
	//서버 주소 설정
	struct sockaddr_in server_addr; //서버 주소 저장 예정
	memset(&server_addr, 0, sizeof(server_addr)); //initialize
    server_addr.sin_family = AF_INET; //IPv4 Internet protocols
    server_addr.sin_port = htons(port); // host byte order to network byte order. big endian방식으로 정렬
    server_addr.sin_addr.s_addr = inet_addr("192.168.56.102"); //ifconfig로 확인한 서버의 네트워크 주소. 문자열로 표현한 주소를 network byte로
	
	//connection 요청
	if(connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){ //connect()는 실패 시 -1을 리턴하므로
        printf("port %u connection failed\n", port);
		exit(1);
    }
	
	//로그 저장할 파일 오픈
	char filename[10];
	sprintf(filename, "%u.txt", port); //<포트번호>.txt 파일명 지정
	FILE * fp = fopen(filename, "wa"); //write(쓰기), append(추가쓰기) 옵션
	if (fp == NULL) {
		printf("File %s open failed\n", filename);
		exit(1); // EXIT_FAILURE
	}
	
	//패킷 수신할 때마다 파일에 기록
	unsigned int msg_cnt = 0; //수신 횟수 제한
	char msg[MAX_MSG+1]; //받은 메시지 저장할 버퍼
	unsigned int msg_len; //받은 메시지 길이
	char time[15]; //h:m:s.ms
	while (msg_cnt < 100) { //수신 횟수를 100회로 제한
		memset(msg, 0, sizeof(msg)); 
		msg_len = recv(client_socket, msg, MAX_MSG, 0); //socket으로부터 data를 받아오는 함수. 성공 시 실제 데이터 길이, 실패 시 -1 리턴
		if(msg_len < 0){
			printf("receive failed\n");
		}
		get_time(time); //현재 시간 h:m:s.ms 형식으로 저장
		fprintf(fp, "%s %d %s\n", time, msg_len, msg); //파일에 시간, data 길이, data 쓰기
		msg_cnt++;
	}
	fclose(fp);
	close(client_socket); //참조 카운터가 0이면 socket 닫고 통신 종료
	pthread_exit(NULL); //pthread_join에 NULL값을 넘겨주고 thread 종료
	//exit은 program을 종료, pthread_exit은 해당 thread만 종료
	//return은 pthread_cleanup_push로 등록된 handler들을 호출하지 않지만, pthread_exit은 handler들을 호출하여 pthread_clenaup_pop을 수행
}

int main(){
	pthread_t thread[5];
	unsigned int server_port[5] = {4444, 5555, 6666, 7777, 8888}; //사용할 port번호 다섯 개. 임의 지정함
	
	
	for (int i = 0; i < 5; i++) {
		if (pthread_create(&thread[i], NULL, start_routine, &server_port[i]) != 0) {
			printf("pthread %d create failed\n", i);
			exit(1); //EXIT_FAILURE
		}
	}
	for (int i = 0; i < 5; i++) {
		if (pthread_join(thread[i], NULL) != 0) {
			printf("pthread %d join failed\n", i);
			exit(1); // EXIT_FAILURE
		}
	}
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <limits.h>


#define MAX_FILE_NAME 50
#define LOG_FILE_NAME "integralGen.log"
#define MAIN_SERVER_FIFO_NAME "hmenn.ff"
#define RW_FLAG O_RDWR

typedef struct{
  int iTimeInterval;
  char cOperator;
  char strFiName[MAX_FILE_NAME];
  char strFjName[MAX_FILE_NAME];
}calculate_t;

typedef enum {
  FALSE=0,TRUE=1
}bool;


/* PREPROCESSORS DEFINE */




/* GLOBAL DEGISKENLER */
FILE *fpLog =NULL;
pid_t pid;
pid_t *pPidClients = NULL;
int iMaxClient=0;
int iCurrentClientNumber=0;
char strFromClientFifo[MAX_FILE_NAME];
char strToClientFifo[MAX_FILE_NAME];
pid_t pidConnectedClient=0;
calculate_t t_client;

void sigHandler(int signalNo);
void sigHandlerMini(int signalNo);
void sigDeadHandler(int signalNo);
void exitHmenn(int exitStatus);


int main(int argc,char *argv[]){
  int fdMainServerRead=0;
  pid_t pidChild;

  if(argc != 3 || argv[1][0]!='-' || argv[2][0]!='-'){
    fprintf(stderr,"Command-Line arguments failed.\n");
    fprintf(stderr,"USAGE: ./integralGen -resolution -max#OfClients\n");
    exit(0);
  }

  signal(SIGINT,sigHandler);
  signal(SIGCHLD,sigDeadHandler); // cocuk olunce yollanacak sinyali tutacak
  fpLog = fopen(LOG_FILE_NAME,"a");
  if(NULL == fpLog){
    fprintf(stderr,"Failed to open %s. [Errno : %s]",LOG_FILE_NAME,strerror(errno));
    exit(0);
  }

  pid = getpid();

  // TODO : UNLINK EMEYI UNUTMA
  mkfifo(MAIN_SERVER_FIFO_NAME,0666);

  fdMainServerRead = open(MAIN_SERVER_FIFO_NAME,O_RDWR);
  if(fdMainServerRead == -1){
    fprintf(stderr,"Failed to open MainServer FIFO\n");
    fprintf(fpLog, "Failed to open MainServer FIFO\n");
    exitHmenn(0);
  }

  iMaxClient = atoi(&argv[2][1]);

  // TODO : ERROR CHECKS - ARGUMENT CONTROLS
  if(iMaxClient<=0){
    fprintf(stderr,"Max Client Number[%d] is invalid. Program aborted.",iMaxClient);
    fprintf(fpLog,"Max Client Number[%d] is invalid. Program aborted.",iMaxClient);
    exitHmenn(0);
  }

  pPidClients = (pid_t*)calloc(sizeof(pid_t),iMaxClient);
  printf("Server[%ld] Started.\n",(long)pid);
  while(1){
    printf("Main Server waits for clients.\n");
    if(0 < read(fdMainServerRead,&pidConnectedClient,sizeof(pid_t) )){

      if(iCurrentClientNumber >= iMaxClient){
        fprintf(stderr,"All[%d] servers served. Please try again. Good Bye!",iCurrentClientNumber);
        kill(pidConnectedClient,SIGINT);
        continue;
      }
      pPidClients[iCurrentClientNumber]=pidConnectedClient;
      ++iCurrentClientNumber;
      printf("Client[%ld] sent request.\n",(long)pidConnectedClient);


      if(-1 == (pidChild = fork())){
        fprintf(stderr, "Failed to fork operation.\n");
        fprintf(fpLog,"Failed to fork operation.\n");
        exitHmenn(0);
      }

      // child-server
      if(pidChild == 0){

        signal(SIGINT,sigHandlerMini);
        pid_t pidChild;
        int fdMiniServerRead;
        int fdMiniServerWrite; // for send result to client


        pidChild=getpid();

        // client ile haberlesmek icin client pid ile fifo ac
        sprintf(strFromClientFifo,"Logs/%ld-cwsr",(long)pidConnectedClient);
        if(mkfifo(strFromClientFifo,0666) != 0){
          if(errno != EEXIST){
            fprintf(stderr,"Failed to open fifo : %s\n",strFromClientFifo);
            fprintf(fpLog,"Failed to open fifo : %s\n",strFromClientFifo);
            exitHmenn(0);
          }
        }

        if(-1 == (fdMiniServerRead = open(strFromClientFifo,O_RDWR))){
          fprintf(stderr, "Failed to open MiniServerFifo to read.\n");
          fprintf(fpLog, "Failed to open MiniServerFifo[%ld] to read.\n",(long)pidChild);
          exitHmenn(0);
        }
        printf("MiniServer[%2d-%ld] served Client[%ld] on %s\n",
            iCurrentClientNumber,(long)pidChild,(long)pidConnectedClient,
                                          strFromClientFifo);


        // ######### READING INFORMATION FROM MINI SERVER FIFO ############# //
        calculate_t t_client;
        read(fdMiniServerRead,&t_client,sizeof(calculate_t));
        close(fdMiniServerRead);


        // ######### READING INFORMATION TO MINI SERVER FIFO ############# //
        double result=9.99;
        sprintf(strToClientFifo,"Logs/%ld-crsw",(long)pidConnectedClient);
        if(mkfifo(strToClientFifo,0666) != 0){
          if(errno != EEXIST){
            fprintf(stderr,"Failed to open fifo : %s\n",strToClientFifo);
            fprintf(fpLog,"Failed to open fifo : %s\n",strToClientFifo);
            exitHmenn(0);
          }
        }

        if(-1 == (fdMiniServerWrite = open(strToClientFifo,O_WRONLY))){
          fprintf(stderr, "Failed to open MiniServerFifo to read.\n");
          fprintf(fpLog, "Failed to open MiniServerFifo[%ld] to read.\n",(long)pidChild);
          exitHmenn(0);
        }

        write(fdMiniServerWrite,&pidChild,sizeof(pid_t));
        int h=0;
        for(h=0;h<3;++h)
          write(fdMiniServerWrite,&result,sizeof(double));
        close(fdMiniServerWrite);

        #ifdef DEBUG
        fprintf(stdout,"MiniServer read Fi=%s\n",t_client.strFiName);
        fprintf(stdout,"MiniServer read Fj=%s\n",t_client.strFjName);
        fprintf(stdout,"MiniServer read iTimeInterval = %d\n",t_client.iTimeInterval);
        fprintf(stdout,"MiniServer read cOperator = %c\n",t_client.cOperator);
        #endif

        printf("MiniServer ended.\n");

        //TODO : ADD MY EXIT HERE
        while(1){sleep(2);}

        exitHmenn(0);
      }else{
        //sleep(6);
        //exitHmenn(0);
        // TODO : PARENT YENI CLIENTLER ICIN BEKLEME DURUMUNA GECECEK
      }
    }
  }


  unlink(MAIN_SERVER_FIFO_NAME);
  printf("Exit Here");
  return 0;
}

void exitHmenn(int exitStatus){

  free(pPidClients);
  pPidClients=NULL;
  fclose(fpLog);
  fpLog=NULL;
  exit(exitStatus);
}

void sigHandler(int signalNo){

  int i=0;

  for(i=0;i<iCurrentClientNumber;++i){
    printf("Killed %d.[%ld].\n",i,(long)pPidClients[i]);
    kill(pPidClients[i],SIGINT);
  }

  free(pPidClients);
  pPidClients=NULL;

  unlink(MAIN_SERVER_FIFO_NAME);

  printf("CTRL-C Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  fprintf(fpLog, "CTRL-C Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  fclose(fpLog);
  exit(signalNo);
}

void sigHandlerMini(int signalNo)
{
    free(pPidClients);
    pPidClients=NULL;
	  unlink(strFromClientFifo);
    unlink(strToClientFifo);
    kill(pidConnectedClient,SIGINT);
    printf("CTRL-C Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
    fprintf(fpLog, "CTRL-C Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
		fclose(fpLog);
    exit(signalNo);
}

// sadece parente sinyal giderse tutulur
void sigDeadHandler(int signalNo){
  signal(SIGCHLD,sigDeadHandler); // sinyali resetle
  --iCurrentClientNumber;
  printf("ChildDead\n");
  pid_t child=wait(NULL);
  printf("ID  : %ld and child %ld\n",(long)getpid(),(long)child);
  //exitHmenn(signalNo);
}

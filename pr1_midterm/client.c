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
#include <time.h>
#include <math.h>

#define SERVER_FIFO_NAME "hmenn.ff"
#define MAX_FILE_NAME 20
#define ALARM_TIME 10


typedef struct{
  int iTimeInterval;
  int iFiSize;
  int iFjSize;
  char cOperator;
}calculate_t;

typedef struct{
    double dResult;
    double dStartTime;
    double dEndTime;
}t_integralRes;

char strFiName[MAX_FILE_NAME];
char strFjName[MAX_FILE_NAME];
char *cpOperator=NULL;
char cOperator;

calculate_t t_client;

char strClientLog[MAX_FILE_NAME]; // max 127 in limits.h
char strToMiniServerFifo[MAX_FILE_NAME];
char strFromMiniServerFifo[MAX_FILE_NAME];
pid_t pidConnectedServer=-1;
FILE *fpClientLog=NULL;


void sigHandler(int signalNo);

char giveOperator(const char * cpOperator);

void myExit(int exitStatus);

long double getdifMil(struct timeval *start,struct timeval *end){
  return 1000*(end->tv_sec - start->tv_sec) +(end->tv_usec - start->tv_usec)/1000.0;
}

int main(int argc,char* argv[]){

  struct timeval tClientStart;
  struct timeval tClientConnect;
  struct timeval tClientRead;
  int iTimeInterval=0;
  int fdMainServerRead;
  int fdMainServerWrite;
  int fdMiniServerWrite;
  int fdMiniServerRead;
  pid_t pidClient;
  int fdFij;

  gettimeofday(&tClientStart,NULL);
  // initialize signal handler
  signal(SIGINT,sigHandler);
  signal(SIGTSTP,sigHandler);
  signal(SIGTERM,sigHandler);
  signal(SIGQUIT,sigHandler);
  signal(SIGHUP,sigHandler);
  signal(SIGALRM,sigHandler);

  // ################ Control line arguments ################## //
  if(argc != 5 ||argv[1][0] !='-' || argv[2][0] !='-' || argv[3][0] !='-'
          ||  argv[4][0] !='-'){
    fprintf(stderr,"Command-Line arguments failed.\n");
    fprintf(stderr,"USAGE : ./client -fi -fj -internal -operation\n");
    exit(0);
  }

  pidClient = getpid();
  // TODO : CHANGE CLIENT LOG NAME
  sprintf(strClientLog,"Logs/c-%ld.log",(long)pidClient);
  if(NULL == (fpClientLog = fopen(strClientLog,"w"))){
    fprintf(stderr,"FAILED TO CREATE CLIENT LOG FILE. Errno : %s\n",strerror(errno));
    exit(0);
  }
  // ********** END OF Line Arguments control ************** //



  // ############ Time Reveal control ############ //

  iTimeInterval = atoi(&argv[3][1]);
  if(iTimeInterval<=0){
    fprintf(stderr,"Time Reveal must be bigger than zero.\n");
    myExit(0);
  }
  // *********** end of time Interveal control ******** //

  // ############ Operator control ############ //

  cpOperator = argv[4];
  if(strlen(cpOperator)>1){
    cOperator = giveOperator(&cpOperator[1]);
    if(cOperator == '\0'){
      fprintf(stderr,"Undefined operator. Please use +,-,/,* operators.");
      myExit(0);
    }
  }else{
    fprintf(stderr,"Unentered Operator.\n");
    myExit(0);
  }

   /* Set inormations to send */
    t_client.iTimeInterval = iTimeInterval;
    t_client.cOperator= cOperator;

    t_client.iFiSize = strlen(&argv[1][1])+1;
    strcpy(strFiName,&argv[1][1]);

    t_client.iFjSize = strlen(&argv[2][1])+1;
    strcpy(strFjName , &argv[2][1]);

  // ********* END OF OPERATOR CONTROL ******** * //

  #ifdef DEBUG
    printf("# CLIENT COMMAND-LINE DEBUG\n");
    printf("Client[%ld] started.\n",(long)pidClient);
    printf("--Fi %d file : %s\n",t_client.iFiSize,strFiName);
    printf("--Fj %d file : %s\n",t_client.iFjSize,  strFjName);
    printf("--Tİme Interval : %d\n",t_client.iTimeInterval);
    printf("--Operator : %c\n",t_client.cOperator);
  #endif

  gettimeofday(&tClientConnect,NULL);
  long double ldConnectTime;
  ldConnectTime = getdifMil(&tClientStart,&tClientConnect);
  if(-1 == (fdMainServerWrite = open(SERVER_FIFO_NAME,O_WRONLY))){
    fprintf(stderr,"Client[%ld] failed to connect MainServer at %Lf ms.\n",(long)pidClient,ldConnectTime);
    fprintf(fpClientLog,"Client[%ld] failed to connect MainServer at %Lf ms.\n",(long)pidClient,ldConnectTime);
    myExit(0);
  }


  fprintf(stderr,"Client[%ld] connecced  MainServer at %Lf ms.\n",(long)pidClient,ldConnectTime);
  fprintf(fpClientLog,"Client[%ld] connected connect MainServer at %Lf ms..\n",(long)pidClient,ldConnectTime);


  int iWriteCheck=0;
  // send client pid to server. Server will open fifo with client pid
  iWriteCheck = write(fdMainServerWrite,&pidClient,sizeof(pid_t));
  #ifdef DEBUG
  printf("Client[%ld] wrote %d bytes main server.\n",(long)pidClient,iWriteCheck);
  #endif
  close(fdMainServerWrite);

  // client write server read
  sprintf(strToMiniServerFifo,"Logs/%ld-cwsr",(long)pidClient);
  if(-1 == mkfifo(strToMiniServerFifo,0666) && errno != EEXIST){
    fprintf(stderr, "Client-MiniServer mkfifo error. %s\n",strerror(errno));
    fprintf(fpClientLog, "Client-MiniServer mkfifo error. %s\n",strerror(errno));
    myExit(0);
  }

  // client read server write
  sprintf(strFromMiniServerFifo,"Logs/%ld-crsw",(long)pidClient);
  if(-1 == mkfifo(strFromMiniServerFifo,0666) && errno != EEXIST){
    fprintf(stderr, "Client-MiniServer mkfifo error. %s\n",strerror(errno));
    fprintf(fpClientLog, "Client-MiniServer mkfifo error. %s\n",strerror(errno));
    myExit(0);
  }


  if(-1 == (fdMiniServerWrite = open(strToMiniServerFifo,O_WRONLY))){
    fprintf(stderr,"Client[%ld] failed to connect MiniServerFifo : %s\n",
                                          (long)pidClient,strToMiniServerFifo);
    fprintf(fpClientLog,"Client[%ld] failed to connect MiniServerFifo : %s\n",
                                          (long)pidClient,strToMiniServerFifo);
    myExit(0);
  }


  // ################ SERVERE BILGI GONDERME CEVAP ALMA ################ //

  write(fdMiniServerWrite,&t_client,sizeof(calculate_t));
  write(fdMiniServerWrite,strFiName,t_client.iFiSize);
  write(fdMiniServerWrite,strFjName,t_client.iFjSize);
  close(fdMiniServerWrite);


  if(-1 == (fdMiniServerRead = open(strFromMiniServerFifo,O_RDWR))){
    fprintf(stderr,"Client[%ld] failed to connect MiniServerFifo : %s\n",
                                          (long)pidClient,strFromMiniServerFifo);
    fprintf(fpClientLog,"Client[%ld] failed to connect MiniServerFifo : %s\n",
                                          (long)pidClient,strFromMiniServerFifo);
    myExit(0);
  }

  alarm(ALARM_TIME);
  read(fdMiniServerRead,&pidConnectedServer,sizeof(pid_t));

  t_integralRes results;


  long double ldClientRead;
  int iStep=0;
  while(read(fdMiniServerRead,&results,sizeof(t_integralRes))){
    gettimeofday(&tClientRead,NULL);
    ldClientRead = getdifMil(&tClientConnect,&tClientRead);
    printf("%d.ms[%Lf] Lower(ms) : %f  Up(ms) : %f  -> Result = %f\n",iStep,ldClientRead,
            results.dStartTime,results.dEndTime,results.dResult);
   fprintf(fpClientLog,"%d.ms[%Lf] Lower(ms) : %f  Up(ms) : %f  -> Result = %f\n",
        iStep,ldClientRead,results.dStartTime,results.dEndTime,results.dResult);
   ++iStep;
   alarm(ALARM_TIME);
  }

  close(fdMiniServerRead);
  myExit(EXIT_SUCCESS);
}

void sigHandler(int signalNo){

  if(fpClientLog==NULL)
    fpClientLog=fopen(strClientLog,"a+");
  cpOperator=NULL;

  unlink(strFromMiniServerFifo);
  unlink(strToMiniServerFifo);
  if(signalNo == SIGINT){
    printf("Client[%ld] interrupted with CTRL+C. Server[%ld] closed.\n",
                                  (long)getpid(),(long)pidConnectedServer);
  if(pidConnectedServer!=-1)
    kill(pidConnectedServer,SIGINT);
  fprintf(fpClientLog,"Client[%ld] interrupted with CTRL+C. Server[%ld] closed.\n",
                                    (long)getpid(),(long)pidConnectedServer);
 }else if(signalNo == SIGTSTP){
   printf("Client[%ld] interrupted with CTRL+Z. Server[%ld] closed.\n",
                                 (long)getpid(),(long)pidConnectedServer);
 if(pidConnectedServer!=-1)
   kill(pidConnectedServer,SIGTSTP);
 fprintf(fpClientLog,"Client[%ld] interrupted with CTRL+Z. Server[%ld] closed.\n",
                                   (long)getpid(),(long)pidConnectedServer);
}else if(signalNo == SIGQUIT){
  printf("Client[%ld] interrupted with (ctrl+\\)SIGQUIT. Server[%ld] closed.\n",
                                (long)getpid(),(long)pidConnectedServer);
if(pidConnectedServer!=-1)
  kill(pidConnectedServer,SIGQUIT);
fprintf(fpClientLog,"Client[%ld] interrupted with (ctrl+\\)SIGQUIT. Server[%ld] closed.\n",
                                  (long)getpid(),(long)pidConnectedServer);
}else if(signalNo == SIGHUP){
  printf("Client[%ld] interrupted with SUGUP. Server[%ld] closed.\n",
                                (long)getpid(),(long)pidConnectedServer);
if(pidConnectedServer!=-1)
  kill(pidConnectedServer,SIGHUP);
fprintf(fpClientLog,"Client[%ld] interrupted with SUGUP. Server[%ld] closed.\n",
                                  (long)getpid(),(long)pidConnectedServer);
}else if(signalNo == SIGTERM){
  printf("Client[%ld] interrupted with SIGTERM. Server[%ld] closed.\n",
                                (long)getpid(),(long)pidConnectedServer);
if(pidConnectedServer!=-1)
  kill(pidConnectedServer,SIGTERM);
fprintf(fpClientLog,"Client[%ld] interrupted with SIGTERM. Server[%ld] closed.\n",
                                  (long)getpid(),(long)pidConnectedServer);
}else if(signalNo == SIGALRM){
  printf("Client[%ld] interrupted with %d seconds ALARM. Server[%ld] closed.\n",
                                (long)getpid(),ALARM_TIME,(long)pidConnectedServer);
if(pidConnectedServer!=-1)
  kill(pidConnectedServer,SIGALRM);
fprintf(fpClientLog,"Client[%ld] interrupted with %d seconds ALARM. Server[%ld] closed.\n",
                                  (long)getpid(),ALARM_TIME,(long)pidConnectedServer);
}
  if(fpClientLog!=NULL)
    fclose(fpClientLog);
  fpClientLog=NULL;
  exit(signalNo);
}

// buraya gonderirken basinda tire olmadan gelmesi lazım
// daha sonra degisebilecegi icin char * alindi
char giveOperator(const char *cpOperator){
  signal(SIGINT,sigHandler);
  if(cpOperator==NULL || strlen(cpOperator)!=1)
    return '\0';

  char op = cpOperator[0];

  if(op =='+' || op=='-' || op=='/' || op=='*')
    return op;
  else return '\0';
}


void myExit(int exitStatus){
  cpOperator=NULL;
  if(fpClientLog!=NULL)
    fclose(fpClientLog);
  fpClientLog=NULL;
  if(pidConnectedServer!=-1)
    kill(pidConnectedServer,SIGINT); // eger servere baglanmazsa
  exit(exitStatus);
}

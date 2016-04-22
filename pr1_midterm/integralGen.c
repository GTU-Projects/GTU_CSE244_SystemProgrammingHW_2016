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
#include <time.h>
#include "tinyexpr.h"
// https://github.com/codeplea/tinyexpr


#define MILLION 1000000L
#define MAX_FILE_NAME 50
#define LOG_FILE_NAME "integralGen.log"
#define MAIN_SERVER_FIFO_NAME "hmenn.ff"
#define RW_FLAG O_RDWR
#define ALARM_TIME 5

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
te_expr *expr=NULL;

char strFiName[MAX_FILE_NAME];
char strFjName[MAX_FILE_NAME];




void sigHandler(int signalNo);
void sigHandlerMini(int signalNo);
void sigDeadHandler(int signalNo);
void exitHmenn(int exitStatus);

long double getdifMic(struct timeval *start,struct timeval *end){
  return MILLION*(end->tv_sec - start->tv_sec) +end->tv_usec - start->tv_usec;
}

long double getdifMil(struct timeval *start,struct timeval *end){
  return 1000*(end->tv_sec - start->tv_sec) +(end->tv_usec - start->tv_usec)/1000.0;
}



double calculateIntegration(char *str,double up,double down,double step){

  double t=0;
  double firstValue=0;
  double lastValue=0;
  double total=0;
  /* Store variable names and pointers. */
  te_variable vars[] = {{"t", &t}};

  int err;
     /* Compile the expression with variables. */
  expr = te_compile(str, vars, 1, &err);
  double rate = (up-down)/step;
  double xi = rate; // xi = x1

  if (expr) {
         int i=1;
         total=0;
         for(i=1;i<step;++i){
           t=xi;
           total+= te_eval(expr);
           xi+=rate;
         }

         t = down;
         firstValue = te_eval(expr);
         t=up;
         lastValue = te_eval(expr);
         te_free(expr);
         expr=NULL;
       } else {
         fprintf(stderr,"Parse error. Server[%ld] closed!\n", (long)getpid());
         exitHmenn(0);
       }

  double result =  (up-down)*(firstValue+ 2*total+lastValue )/(2.0*step);
  return result;
}

int main(int argc,char *argv[]){
  int fdMainServerRead=0;
  double dResulution =0.0;
  pid_t pidChild;
  struct timeval tMainStart;
  struct timeval tMiniStart;
  struct timeval tClientReq;
  gettimeofday(&tMainStart,NULL);
  if(argc != 3 || argv[1][0]!='-' || argv[2][0]!='-'){
    fprintf(stderr,"Command-Line arguments failed.\n");
    fprintf(stderr,"USAGE: ./integralGen -resolution -max#OfClients\n");
    exit(0);
  }

  signal(SIGINT,sigHandler);
  signal(SIGTSTP,sigHandler);
  signal(SIGTERM,sigHandler);
  signal(SIGQUIT,sigHandler);
  signal(SIGHUP,sigHandler);
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
  dResulution = atof(&argv[1][1]);

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
      printf("-Client[%ld] sent request.\n",(long)pidConnectedClient);


      if(-1 == (pidChild = fork())){
        fprintf(stderr, "Failed to fork operation.\n");
        fprintf(fpLog,"Failed to fork operation.\n");
        exitHmenn(0);
      }

      if(pidChild == 0){
        gettimeofday(&tMiniStart,NULL);
        gettimeofday(&tClientReq,NULL);
        signal(SIGINT,sigHandlerMini);
        signal(SIGTSTP,sigHandlerMini);
        signal(SIGTERM,sigHandlerMini);
        signal(SIGQUIT,sigHandlerMini);
        signal(SIGHUP,sigHandlerMini);
        signal(SIGALRM,sigHandlerMini);
        pid_t pidChild;
        int fdMiniServerRead;

        int fdMiniServerWrite; // for send result to client

        time_t connected = time(NULL);
        double upper;
        double lower;
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
        alarm(15);
        read(fdMiniServerRead,&t_client,sizeof(calculate_t));
        read(fdMiniServerRead,strFiName,t_client.iFiSize);
        read(fdMiniServerRead,strFjName,t_client.iFjSize);

        close(fdMiniServerRead);
        alarm(0);

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


        #ifdef DEBUG
        fprintf(stdout,"MiniServer read Fi=%s\n",strFiName);
        fprintf(stdout,"MiniServer read Fj=%s\n",strFjName);
        fprintf(stdout,"MiniServer read iTimeInterval = %d\n",t_client.iTimeInterval);
        fprintf(stdout,"MiniServer read cOperator = %c\n",t_client.cOperator);
        #endif


        //TODO : ADD MY EXIT HERE

        long double timedif;

        timedif = getdifMil(&tMainStart,&tClientReq);
        printf("Client connected in %Lf miliseconds\n", timedif);

        lower = timedif;
        upper  = lower + dResulution;
        #ifdef DEBUG
        printf("Resolution %f\n",dResulution);
        printf("Lower %f\n",lower);
        printf("Up %f\n",upper);
        #endif
        int h=0;

        t_integralRes pIntegoutput;
        while(1){
            result = calculateIntegration("5*t",upper,lower,3);
            pIntegoutput.dStartTime = lower;
            pIntegoutput.dEndTime = upper;
            pIntegoutput.dResult = result;

            write(fdMiniServerWrite,&pIntegoutput,sizeof(t_integralRes));

            lower = upper;
            upper = upper + dResulution;
            sleep(t_client.iTimeInterval);
          }

        close(fdMiniServerWrite);
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
    kill(pPidClients[i],signalNo);
  }

  free(pPidClients);
  pPidClients=NULL;


  if(expr != NULL)
    te_free(expr);
  expr=NULL;

  unlink(MAIN_SERVER_FIFO_NAME);

  if(signalNo == SIGINT){
  printf("CTRL-C Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  fprintf(fpLog, "CTRL-C Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  }else if(signalNo == SIGHUP){
  printf("SIGUP Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  fprintf(fpLog, "SIGUP Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  }else if(signalNo == SIGTERM){
  printf("SIGTERM Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  fprintf(fpLog, "SIGTERM Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  }else if(signalNo == SIGQUIT){
  printf("SIGQUIT Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  fprintf(fpLog, "SIGQUIT Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  }else if(signalNo == SIGTSTP){
  printf("SIGTSTP Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
  fprintf(fpLog, "SIGTSTP Signal Handled. Main Server[%ld] closed.\n", (long)getpid());
}
  fclose(fpLog);
  exit(signalNo);
}

void sigHandlerMini(int signalNo)
{

    free(pPidClients);
    pPidClients=NULL;


	  unlink(strFromClientFifo);
    unlink(strToClientFifo);
    if(signalNo == SIGINT){
      kill(pidConnectedClient,SIGINT);
      printf("CTRL-C Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
      fprintf(fpLog, "CTRL-C Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
    }else if(signalNo == SIGTSTP){
      kill(pidConnectedClient,SIGTSTP);
      printf("CTRL-Z Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
      fprintf(fpLog, "CTRL-Z Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
    }else if(signalNo == SIGTERM){
      kill(pidConnectedClient,SIGTERM);
      printf("SIGTERM Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
      fprintf(fpLog, "SIGTERM Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
    }else if(signalNo == SIGQUIT){
      kill(pidConnectedClient,SIGQUIT);
      printf("SIGQUIT Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
      fprintf(fpLog, "SIGQUIT Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
    }else if(signalNo == SIGHUP){
      kill(pidConnectedClient,SIGHUP);
      printf("SIGHUP Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
      fprintf(fpLog, "SIGHUP Signal Handled. Mini Server[%ld][%ld] closed.\n",(long)getppid(),(long)getpid());
    }else if(signalNo == SIGALRM){
      kill(pidConnectedClient,SIGALRM);
      printf("SIGALRM[%d sec.] Signal Handled. Mini Server[%ld][%ld] closed.\n",ALARM_TIME,(long)getppid(),(long)getpid());
      fprintf(fpLog, "SIGALRM[%d sec.] Signal Handled. Mini Server[%ld][%ld] closed.\n",ALARM_TIME,(long)getppid(),(long)getpid());
    }
  		fclose(fpLog);
    exit(signalNo);
}

// sadece parente sinyal giderse tutulur
void sigDeadHandler(int signalNo){
  --iCurrentClientNumber;
  pid_t child=wait(NULL);
  //exitHmenn(signalNo);
}

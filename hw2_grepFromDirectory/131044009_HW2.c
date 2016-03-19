#include <stdio.h>
#include <string.h> //strerror
#include <stdlib.h> // atoi
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h> //open
#include <sys/stat.h>
#include <dirent.h> // DIR *
#include <sys/stat.h>
#include <errno.h>
#include "131044009_HW2.h"

//#define DEBUG

char *getStringOfNumber(long number){
  char *string;
  string= malloc(sizeof(long));
  sprintf(string,"%ld",number);
  return string;
}


int findOccurencesInFile(const char* fileName,const char *word){

  pid_t PID;
  char *pTempFileName;
  int fdTempFile;
  int fileToReadfd;
  char buf;
  int foundNum=0;
  int index=0;
  int equalCh=0;
  int i=0;
  char *stringOfNumbers;
  int column=0;
  int row=1;
  char *tempCoordinatText = malloc(sizeof(char)*17+sizeof(int)*3);

  if((fileToReadfd = open(fileName,READ_FLAGS)) == FAIL){
    fprintf(stderr," Failed open \"%s\" : %s ",fileName,strerror(errno));
    return FAIL;
  }

  PID = getpid();
  pTempFileName = getStringOfNumber((long)1);
  #ifdef DEBUG
  printf("PID : %ld -- ",(long)PID);
  printf("FileName(char *) :%s\n",pTempFileName);
  #endif

  fdTempFile = open(pTempFileName,WRITE_FLAGS,FD_MODE);
  write(fdTempFile,fileName,strlen(fileName));
  write(fdTempFile,"\n",1);

while(read(fileToReadfd,&buf,sizeof(char))){
    ++index;
    if(buf == '\n'){
      ++row;
      index=0;
      equalCh=0;
    }else if(buf == word[0]){
      column=index;
      ++equalCh;
      for(i=1;i<strlen(word);++i){
        read(fileToReadfd,&buf,sizeof(char));
        ++index;
        if(buf == word[i]){
          ++equalCh;
        }else if(buf == '\n'){
          equalCh=0;
          ++row;
          index=0;
          break;
        }else{
          break;
        }
      }
      if(equalCh == strlen(word)){
        #ifdef DEBUG
        printf("Index of Firt equal Characters : %d\n",column-1);
        printf("Index of end of equality : %d\n",index-1);
        #endif
        ++foundNum;
        lseek(fileToReadfd,-equalCh+1,SEEK_CUR);
        index = index -equalCh+1;
        #ifdef DEBUG
        printf("Index after lseek : %d\n",index);
        #endif
        sprintf(tempCoordinatText,"%d. Row: %d Column: %d\n",foundNum,row,column);
        write(fdTempFile,tempCoordinatText,strlen(tempCoordinatText));
        equalCh=0;
      }
    }
  }

  free(tempCoordinatText);
  close(fileToReadfd);
  close(fdTempFile);
  return foundNum;
}

bool isDirectory(const char *path){
  struct stat statbuf;

  if(stat(path,&statbuf) == -1)
    return FALSE;
  else {
    return S_ISDIR(statbuf.st_mode) ? TRUE : FALSE;
  }
}

bool isCharacterSpecialFile(const char *path){

  int i=0;
  int sizeOfFile=0;
  int sizeOfExtension=0;
  char extension[4] =".txt";
  sizeOfFile=strlen(path);
  sizeOfExtension=strlen(extension);

  for(i=0;i<4;++i){
    if(path[sizeOfFile -sizeOfExtension+i] != extension[i]){
      return FALSE;
    }
  }
  return TRUE;
}


int searchDir(const char *dirPath, const char *word){

  DIR* pDir;
  pid_t pidChild;
  struct dirent * pDirent;
  struct stat statbuf;
  char mycwd[PATH_MAX];

  if(NULL == (pDir = opendir(dirPath))){
    fprintf(stderr, "Fail -> Errno : %s",strerror(errno));
    return FAIL;
  }
  // open file and go in
  chdir(dirPath);
  getcwd(mycwd,PATH_MAX);
  //printf("%s\n",mycwd);

  while(NULL != (pDirent = readdir(pDir))){
   if(strcmp( pDirent->d_name,".")!=0 && strcmp(pDirent->d_name,"..")!=0){
     pidChild = fork();
     if(pidChild == 0){
       break;
     }
   }
 }
 if(pidChild >0){
  while(wait(NULL) != FAIL);
}else{
  #ifdef DEBUG
    printf("File Path : %s\n",mycwd);
  #endif

  if(TRUE == isDirectory(pDirent->d_name)){
    searchDir(pDirent->d_name,word);
  }else if(TRUE == isCharacterSpecialFile(pDirent->d_name)){
    #ifdef DEBUG
      printf("TXT File : %s\n",pDirent->d_name);
    #endif
      findOccurencesInFile(pDirent->d_name,word);
  }
  //printf(" %ld - %s\n",(long)getpid(),pDirent->d_name);
  exit(1);
}

  return 0;
}

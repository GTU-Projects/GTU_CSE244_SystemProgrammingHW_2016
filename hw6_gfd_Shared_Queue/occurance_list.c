#include "occurance_list.h"
#include <stdlib.h>
void addLastOccurance(occurance_t *hm,int row,int col){

	node_t *newNode; // yeni nod olustur
	newNode=(node_t*)malloc(sizeof(node_t));
	newNode->row = row;
	newNode->column=col;
	newNode->next=NULL;

	if(hm->head==NULL){ // bos ise basina ekle
		hm->total=0;
		hm->head = newNode;
		hm->last = newNode;
	}else{ // degilse sonuna ekle

		hm->last->next = newNode;
		hm->last = hm->last->next;
	}
	hm->total +=1;
}


// tum elemanlari sirayla dosyaya basicak fonksiyon
void printOccurance(occurance_t * occ){

	if(occ ==NULL)
		perror("Null parameter");
	else{
		if(occ->head==NULL){}
		else{
			node_t *ref = occ->head;
			while(ref!=NULL){
				printf("R: %d - C: %d\n",ref->row,ref->column);
				ref = ref->next;
			}
			printf("Total %d\n",occ->total);
		}
	}
}

// listi dosyaya basar
// list icinde her dosyanin adi koordinatlari vardir.
// islemin toplam zamaninida ekrana basar
void printOccurancesToLog(const char *fname,occurance_t * occ,long totalTime){

	FILE * fpLog = fopen(fname,"a");
	int i=1;

	if(occ ==NULL)
		perror("Null parameter");
	else{
		if(occ->head==NULL)
			fprintf(fpLog,"empty list\n");
		else{
			fprintf(fpLog, "\n#####################################\n");
			fprintf(fpLog, "\nFile : %s\n",occ->fileName);
			fprintf(fpLog, "Total Time : %ld(ms)\n\n",totalTime);
			node_t *ref = occ->head;
			while(ref!=NULL){
				fprintf(fpLog,"%d. row: %d - column: %d\n",i++,ref->row,ref->column);
				ref = ref->next;
			}
			fprintf(fpLog,"File Total %d\n",occ->total);
		}
	}
	fflush(fpLog); // log flusing
	fclose(fpLog);
}

// linked listi siler
void deleteOccurance(occurance_t *occ){
	if(occ!=NULL){
		if(occ->head !=NULL){
			node_t *ref;
			while(occ->head != NULL){
				ref = occ->head;
				occ->head = ref->next;
				free(ref);
			}
		}
	}
}


// link list test
int test(){

	occurance_t *hm = malloc(sizeof(occurance_t));

	hm->total=0;
	hm->head=NULL;
	hm->last=NULL;

	addLastOccurance(hm,3,4);
	addLastOccurance(hm,4,2);
	addLastOccurance(hm,5,6);
	addLastOccurance(hm,1,9);
	printOccurance(hm);
	deleteOccurance(hm);
	
	free(hm);
}
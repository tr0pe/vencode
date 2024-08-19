#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define GB 1073741824
#define MB 1048576
#define KB 1024

#ifdef __WIN32
#define FFMPEG_EXE_PATH "ffmpeg-3.4.1_LTO\\ffmpeg.exe"
#include <conio.h>
int check_ffmpeg(void){
	if(-1 == access(FFMPEG_EXE_PATH,X_OK)) return 1;
	return 0;
}
#else
#include <termios.h>
int check_ffmpeg(char *ffm_path){
	char *str = getenv("PATH");
	char *pt;
	pt = strtok(str,":");

	char *path = (char *)malloc(sizeof(char) * 64);
	if(path == NULL){
		fprintf(stderr,"Error allocating memory for FFMPEG path");
		return 1;
	}
	while (pt != NULL){
		sprintf(path,"%s/ffmpeg",pt);
		if(-1 != access(path, X_OK)){
			strcpy(ffm_path,path);
			free(path);
			return 0;
		}
		pt = strtok (NULL, ":");
	}

	free(path);
	return 1;
}
#endif

FILE *open_file(const char *filename, unsigned long long *filesize){
	FILE *fd = fopen(filename,"rb");

	if(fd == NULL){
		perror(filename);
		return NULL;
	}

	fseek(fd,0L,SEEK_END);
	*filesize = ftell(fd);
	fseek(fd,0L,SEEK_SET);

	return fd;
}

short dig(short num){
	short quan = 0;
	if(num == 0) return 1;
	while(num != 0){
		num = num/10;
		quan++;
	}
	return quan;
}

#ifndef __WIN32
char getch(void){
	static struct termios old, new;
	char ch;
	tcgetattr(0, &old);
	new = old;
	new.c_lflag &= (~ICANON & ~ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&new);
	ch = getchar();
	tcsetattr(STDIN_FILENO,TCSANOW,&old);
	return ch;
}
#endif

int compat(const void *a, const void *b){
	return strcmp(*(const char **)a,*(const char **)b);
}
int compat_reverse(const void *a, const void *b){
	return strcmp(*(const char **)b,*(const char **)a);
}
void destroy_filenames(char **filenames,unsigned int count){
	count++;
	for(int i=0;i<count;i++){
		if(filenames[i] != NULL) free(filenames[i]);
		filenames[i] = NULL;
	}
}

void print_size(unsigned long bytes){
	if(bytes >= GB) printf("%.2fGb\n",(float)bytes/GB);			//1Gb
	else if(bytes >= MB) printf("%.2fMb\n",(float)bytes/MB);	//1Mb
	else if(bytes >= KB) printf("%.2fKb\n",(float)bytes/KB);	//1Kb
	else printf("%lu bytes\n",bytes);
}

void img_index(char *ptr, unsigned int num, pid_t pid, char *ext){
	sprintf(ptr,"./%d_frames/frame_%09d.%s",pid,num,ext);
}

_Bool replace(char *filename){
	char key = 0;
	if(-1 != access(filename,R_OK)){
		printf( "Replace '%s' file? [y/n] ",filename );

		while(key != 'y' || key != 'n'){
			key = getch();
			if(key == 'n'){ printf("\n"); return 1; }
			else break;
		}
		printf("\n");
	}
	return 0;
}

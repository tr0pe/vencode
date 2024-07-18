#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define GB 1073741824
#define MB 1048576
#define KB 1024

#ifdef __WIN32
#define FFMPEG_EXE_PATH "ffmpeg-3.4.1_LTO\\ffmpeg.exe"
int check_ffmpeg(void){
	if(-1 == access(FFMPEG_EXE_PATH,X_OK)){
		return 1;
	}
	return 0;
}
#else
#include <termios.h>
int check_ffmpeg(char *ffm_path){
	char *str = getenv("PATH");
	char *pt;

	pt = strtok(str,":");

	char *path = (char *)malloc(sizeof(char) * 32);

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

FILE *open_file(const char *filename, long unsigned int *filesize){
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

int dig(int num){
	int quan = 0;

	if (num == 0){
		return 1;
	}

	while (num != 0){
		num = num/10;
		quan++;
	}

	return quan;
}

#ifndef __WIN32
char getch_(int echo){
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
void destroy_filenames(char **filenames,int count){
	count++;
	for(int i=0;i<count;i++){
		if(filenames[i] != NULL){
			free(filenames[i]);
		}
		filenames[i] = NULL;
	}
}

void print_size(long unsigned bytes){
	if(bytes >= GB){ //1Gb
		printf("%.2fGb\n",(float)bytes/GB);
	}
	else if(bytes >= MB){ //1Mb
		printf("%.2fMb\n",(float)bytes/MB);
	}
	else if(bytes >= KB){ // 1Kb
		printf("%.2fKb\n",(float)bytes/KB);
	}
	else{
		printf("%lu bytes\n",bytes);
	}
}

int img_index(char *ptr, int num, int pid, char *ext){
	if(ptr == NULL){
		return -1;
	}
	sprintf(ptr,"./%d_frames/frame_%06d.%s",pid,num,ext);

	return 0;
}

void print_num(_Bool *arr){
	for(int i=0;i<8;i++){
		printf("%d",arr[i]);
	}
	printf("\n");
}

#ifndef __util_h
#define __util_h

#ifdef __WIN32
#define FFMPEG_EXE_PATH "ffmpeg-3.4.1_LTO\\ffmpeg.exe"
int check_ffmpeg(void);
#else
int check_ffmpeg(char *ffm_path);
#endif

FILE *open_file(const char *filename, long unsigned int *filesize);
void destroy_filenames(char **filenames,int count);
char getch_(int echo);
int dig(int num);
void print_size(long unsigned bytes);
int img_index(char *ptr, int num,int pid,char *ext);
void print_num(_Bool *arr);

#ifndef __WIN32
#define getch() getch_(0)
#endif

#endif
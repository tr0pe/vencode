#ifndef __util_h
#define __util_h
#ifdef __WIN32
#define FFMPEG_EXE_PATH "ffmpeg-3.4.1_LTO\\ffmpeg.exe"
int check_ffmpeg(void);
#else
int check_ffmpeg(char *ffm_path);
char getch(void);
#endif

FILE *open_file(const char *filename, long unsigned int *filesize);
short dig(short num);
void destroy_filenames(char **filenames,unsigned int count);
void print_size(long unsigned bytes);
void img_index(char *ptr, unsigned int num, pid_t pid, char *ext);
int compat(const void *a, const void *b);
int compat_reverse(const void *a, const void *b);
_Bool replace(char *filename);

#endif

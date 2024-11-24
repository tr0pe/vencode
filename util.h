#ifndef __util_h
#define __util_h
#ifdef __WIN32
#define FFMPEG_EXE_PATH "ffmpeg-3.4.1_LTO\\ffmpeg.exe"
int check_ffmpeg(void);
#else
int check_ffmpeg(char *);
char getch(void);
#endif

FILE *open_file(const char *, unsigned long long *);
short dig(short );
void destroy_filenames(char **,unsigned int);
void print_size(unsigned long);
void img_index(char *, unsigned int, pid_t, char *);
int compat(const void *, const void *);
int compat_reverse(const void *, const void *);
_Bool replace(char *);

#endif

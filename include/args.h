#ifndef __args_h
#define __args_h

typedef struct{
	unsigned int width;
	unsigned int height;
	short threads;
	short pixel_size;
	short rmode;
	char *input_file_path;
	char *output_file_path;
	char *codec;
	char *ffmpeg_path;
	char *frame_folder;
	int framerate;
	_Bool operation;
	_Bool ultrafast;
	_Bool noconfirm;
	_Bool skip_ffmpeg;
	_Bool keep_frames;
	_Bool quiet;
	_Bool bmp;
	_Bool noprogress;
	_Bool reverse_x;
	_Bool reverse_y;
}arg_s;

int getopts(arg_s *args, int argc, char *argv[]);
void destroy_args(arg_s *args);
int compat(const void *a, const void *b);

#endif

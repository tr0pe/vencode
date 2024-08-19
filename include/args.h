#ifndef __args_h
#define __args_h

typedef struct{
	unsigned short width;
	unsigned short height;
	short threads;
	short pixel_size;
	short rmode;
	short framerate;
	short grange[2];
	char *input_file_path;
	char *output_file_path;
	char *codec;
	char *ffmpeg_path;
	char *frame_folder;
	_Bool operation;
	_Bool ultrafast;
	_Bool noconfirm;
	_Bool skip_ffmpeg;
	_Bool keep_frames;
	_Bool quiet;
	_Bool noprogress;
	_Bool reverse_x;
	_Bool reverse_y;
	_Bool invert_color;
	_Bool odd;
	_Bool invert_frames;
	_Bool invert_byte;
}arg_s;

int getopts(arg_s *args, int argc, char *argv[]);

#endif

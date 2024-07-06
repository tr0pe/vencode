#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define __VERSION "1.2.3"

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
}arg_s;

void print_help(char *argv0){
	printf("\n /$$    /$$                                              /$$ /$$$$$$$$\n");
	printf("| $$   | $$                                             | $$| $$_____/\n");
	printf("| $$   | $$ /$$$$$$  /$$$$$$$   /$$$$$$$  /$$$$$$   /$$$$$$$| $$      \n");
	printf("|  $$ / $$//$$__  $$| $$__  $$ /$$_____/ /$$__  $$ /$$__  $$| $$$$$   \n");
	printf(" \\  $$ $$/| $$$$$$$$| $$  \\ $$| $$      | $$  \\ $$| $$  | $$| $$__/   \n");
	printf("  \\  $$$/ | $$_____/| $$  | $$| $$      | $$  | $$| $$  | $$| $$      \n");
	printf("   \\  $/  |  $$$$$$$| $$  | $$|  $$$$$$$|  $$$$$$/|  $$$$$$$| $$$$$$$$\n");
	printf("    \\_/    \\_______/|__/  |__/ \\_______/ \\______/  \\_______/|________/\n");
	printf("  v%s\n\n",__VERSION);
	printf("      [ By tr0pe | get it at https://github.com/tr0pe/vencode ]\n\n");

	printf(" -e [input file name]           Convert a file to video, the file path must\n");
	printf("                                be specified.\n\n");

	printf(" -d [input video name]          Convert a video to file, the video path must\n");
	printf("                                be specified.\n\n");

	printf(" -o [output file/video name]    Set the output filename.\n\n");

	printf(" -r [WIDTHxHEIGHT]              Set the video resolution. This argument\n");
	printf("                                is optional, and only works when a file will\n");
	printf("                                be converted to video. (-e option)\n\n");

	printf(" -f [framerate]                 Set Video framerate. This argument is\n");
	printf("                                optional, and only works when a file will\n");
	printf("                                be converted to video. (-e option)\n");
	printf("                                When this argument is not used, will take\n");
	printf("                                the default value. (30)\n\n");

	printf(" -c [codec]                     Set the codec for video creation.\n");
	printf("                                This argument is optional and only works\n");
	printf("                                when a file will be converted to video\n");
	printf("                                (-e option) When this argument is not used,\n");
	printf("                                will take the default value. (libx265).\n");
	printf("                                Hint: libx264 fast, larger video size \n");
	printf("                                      libx265 slow, small video size\n\n");

	printf(" -b                             Enable BMP image support (experimental). Image\n");
	printf("                                write will be faster than PNG but requires more\n");
	printf("                                disk space.\n");
	printf("                                WARNING: ffmpeg process with BMP images\n");
	printf("                                will be slower.\n\n");

	printf(" -z                             Set ultra fast video encoding. This argument\n");
	printf("                                is only available when a file will be.\n");
	printf("                                converted to video (-e option), and it is\n");
	printf("                                optional. When this argument is used, the\n");
	printf("                                video encode will be faster, but the video\n");
	printf("                                file size will be larger.\n\n");

	printf(" -s                             Skip FFMPEG process (Video creation), and\n");
	printf("                                preserve the frames folder.\n\n");

	printf(" -t [threads number]            Number of threads for parallell frame writing.\n");
	printf("                                This argument is optional and only is available\n");
	printf("                                when a file will be converted to video.\n");
	printf("                                The default value is 1. If you set the correct\n");
	printf("                                value, the process of file to video conversion\n");
	printf("                                will be faster, but be careful.\n\n");

	printf(" -p                             Set the size of the squares that represent.\n");
	printf("                                the data bits. The default value is 4.\n");
	printf("                                IMPORTANT: If you set a custom size for file\n");
	printf("                                to video conversion, you will set the same\n");
	printf("                                size to convert that video to restore original\n");
	printf("                                file\n\n");

	printf(" -a [1,2]                       Set the pixel read mode.\n");
	printf("                                when a sqare-bit is readed, the color may be \n");
	printf("                                parsed in different ways:\n");
	printf("                                1: Get the median color of square\n");
	printf("                                     (inaccurate, fast)\n");
	printf("                                2: Get the arithmetic average of colors\n");
	printf("                                     (accurate)\n");
	printf("                                Default value: 2.\n\n");

	printf(" -n                             Disable confirm/replace prompts.\n\n");
	printf(" -q                             Disable warning messages.\n\n");
	printf(" -Q                             Disable progress messages.\n\n");
	printf(" -k                             Keep frames directory.\n\n");
	printf(" -h                             Display this message.\n\n");

	printf("Example for video encoding for YouTube:\n   %s -e files.7z -o files.7z.mp4\n\n",argv0);
	printf("Example of video convert to file:\n   %s -d files.7z.mp4 -o files.7z\n\n",argv0);
}

int getopts(arg_s *args, int argc, char *argv[]){

	char height[8];
	char  width[8];

	_Bool rarg = 0;
	_Bool targ = 0;
	_Bool parg = 0;
	_Bool earg = 0;
	_Bool darg = 0;
	_Bool oarg = 0;
	_Bool zarg = 0;
	_Bool farg = 0;
	_Bool carg = 0;
	_Bool narg = 0;
	_Bool sarg = 0;
	_Bool karg = 0;
	_Bool qarg = 0;
	_Bool barg = 0;
	_Bool Qarg = 0;
	_Bool aarg = 0;

	short rXcount = 0;

	int j;
	int k;
	int l;

	int threads = 1;
	int pixel_size = 1;

	int in_file_arg_pos = 0;
	int out_file_arg_pos = 0;

	int codec_arg_pos = 0;
	int codec_arg_len = 0;

	int framerate = 0;
	int am = 0;

	for(int i=1; i<argc; i++){
		if(!strcmp(argv[i],"-e")){ //encode argument
			if(earg){
				fprintf(stderr,"Duplicated encode (-e) argument.\n");
				return 1;
			}
			if(darg){
				fprintf(stderr,
					"-e and -d cannot be invoked at the same time.\n");
				return 1;
			}

			if(i+1 >= argc){
				fprintf(stderr,"-e requires an argument.\n");
				return 1;
			}

			in_file_arg_pos = (i+1);
			earg = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-d")){ //decode argument
			if(darg){
				fprintf(stderr,"Duplicated decode (-d) argument.\n");
				return 1;
			}
			if(earg){
				fprintf(stderr,
					"-d and -e cannot be invoked at the same time.\n");
				return 1;
			}

			if(i+1 >= argc){
				fprintf(stderr,"-d requires an argument.\n");
				return 1;
			}

			in_file_arg_pos = (i+1);

			darg = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-o")){ //output file
			if(oarg){
				fprintf(stderr,"Duplicated output file argument.\n");
				return 1;
			}

			if(i+1 >= argc){
				fprintf(stderr,"-o requires an argument.\n");
				return 1;
			}

			out_file_arg_pos = (i+1);

			oarg = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-r")){ //resolution argument
			j = 0;
			k = 0;
			l = 0;
			if(rarg){
				fprintf(stderr,"Duplicated resolution argument.\n");
				return 1;
			}
			if(i+1 >= argc){
				fprintf(stderr,"-r requires an argument.\n");
				return 1;
			}

			while(argv[i+1][j] != '\0'){

				if(rXcount > 1 ||
					argv[i+1][0] == 'x' ||
					argv[i+1][strlen(argv[i+1])-1] == 'x'){

					fprintf(stderr,"%s: Invalid resolution argument.\n",
																argv[i+1]);

					return 1;
				}

				else if(argv[i+1][j] >= 48 && argv[i+1][j] <= 57){
					if(!rXcount){
						width[k] = argv[i+1][j];
						k++;
					}
					else{
						height[l] = argv[i+1][j];
						l++;
					}
				}

				else if(argv[i+1][j] == 'x'){
					rXcount ++;
				}

				else{
					fprintf(stderr,"%s: Invalid resolution argument.\n",
																argv[i+1]);

					return 1;
				}
				j++;
			}
			if(rXcount == 0){
					fprintf(stderr,"%s: Invalid resolution argument.\n",
																argv[i+1]);
				return 1;
			}
			width[k]  = '\0';
			height[l] = '\0';

			rarg = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-t")){//threads argument
			if(targ){
				fprintf(stderr,"Duplicated threads argument.\n");
				return 1;
			}

			if(i+1 >= argc){
				fprintf(stderr,"-t requires an argument.\n");
				return 1;
			}

			j=0;
			while(argv[i+1][j] != '\0'){
				if(!(argv[i+1][j] >= 48 && argv[i+1][j] <= 57)){
					fprintf(stderr,"-t expects an integer value\n");
					return 1;
				}
				j++;
			}

			threads = atoi(argv[i+1]);

			if(threads == 0){
				fprintf(stderr,"Invalid threads number (zero)\n");
				return 1;
			}
			targ = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-p")){// pixel size
			if(parg){
				fprintf(stderr,"Duplicated pixel-size argument.\n");
				return 1;
			}

			if(i+1 >= argc){
				fprintf(stderr,"-p requires an argument.\n");
				return 1;
			}

			j=0;
			while(argv[i+1][j] != '\0'){
				if(!(argv[i+1][j] >= 48 && argv[i+1][j] <= 57)){
					fprintf(stderr,"-p expects an integer value\n");
					return 1;
				}
				j++;
			}
			pixel_size = atoi(argv[i+1]);

			if(pixel_size == 0){
				fprintf(stderr,"Invalid pixel size value (zero)\n");
				return 1;
			}
			parg = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-z")){//fast encoding == large output filesize
			if(zarg){
				fprintf(stderr,"Duplicated fast encoding argument.\n");
				return 1;
			}
			zarg = 1;
		}
		else if(!strcmp(argv[i],"-n")){//no confirm
			if(narg){
				fprintf(stderr,"Duplicated no confirm argument.\n");
				return 1;
			}
			narg = 1;
		}
		else if(!strcmp(argv[i],"-b")){//bmp support
			if(barg){
				fprintf(stderr,"Duplicated BMP image support argument.\n");
				return 1;
			}
			barg = 1;
		}
		else if(!strcmp(argv[i],"-f")){//fps
			if(farg){
				fprintf(stderr,"Duplicated framerate argument.\n");
				return 1;
			}
			if(i+1 >= argc){
				fprintf(stderr,"-f requires an argument.\n");
				return 1;
			}

			j=0;
			while(argv[i+1][j] != '\0'){
				if(!(argv[i+1][j] >= 48 && argv[i+1][j] <= 57)){
					fprintf(stderr,"-f expects an integer value\n");
					return 1;
				}
				j++;
			}

			framerate = atoi(argv[i+1]);

			if(framerate == 0){
				fprintf(stderr,"Invalid frame rate value (zero)\n");
				return 1;
			}
			farg = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-a")){//read mode
			if(aarg){
				fprintf(stderr,"Duplicated read mode argument.\n");
				return 1;
			}
			if(i+1 >= argc){
				fprintf(stderr,"-a requires an argument.\n");
				return 1;
			}

			j=0;
			while(argv[i+1][j] != '\0'){
				if(!(argv[i+1][j] >= 48 && argv[i+1][j] <= 57)){
					fprintf(stderr,"Invalid read mode value\n");
					return 1;
				}
				j++;
			}

			am = atoi(argv[i+1]);

			if(am < 1 || am > 2){
				fprintf(stderr,"Invalid read mode value\n");
				return 1;
			}
			aarg = 1;
			i++;
		}

		else if(!strcmp(argv[i],"-c")){ //codec chooser
			if(carg){
				fprintf(stderr,"Duplicated codec argument.\n");
				return 1;
			}

			if(i+1 >= argc){
				fprintf(stderr,"-c requires an argument.\n");
				return 1;
			}

			codec_arg_pos = (i+1);
			if(!(!strcmp(argv[codec_arg_pos],"libx264") ||
				!strcmp(argv[codec_arg_pos],"libx265") ||
				!strcmp(argv[codec_arg_pos],"libvpx-vp9") ||
				!strcmp(argv[codec_arg_pos],"libaom-av1") ||
				!strcmp(argv[codec_arg_pos],"mpeg2video") ||
				!strcmp(argv[codec_arg_pos],"mjpeg") ||
				!strcmp(argv[codec_arg_pos],"libtheora") ||
				!strcmp(argv[codec_arg_pos],"dnxhd") ||
				!strcmp(argv[codec_arg_pos],"prores")
				)){
					fprintf(stderr,
						"Invalid codec: %s.\n",argv[codec_arg_pos]);
					return 1;
			}

			carg = 1;
			i++;
		}
		else if(!strcmp(argv[i],"-k")){//keep frame directory
			if(karg){
				fprintf(stderr,"Duplicated keep frames argument.\n");
				return 1;
			}
			karg = 1;
		}
		else if(!strcmp(argv[i],"-s")){//skip video encode
			if(sarg){
				fprintf(stderr,"Duplicated skip video encode argument.\n");
				return 1;
			}
			sarg = 1;
		}
		else if(!strcmp(argv[i],"-q")){//quiet mode
			if(qarg){
				fprintf(stderr,"Duplicated quiet mode argument.\n");
				return 1;
			}
			qarg = 1;
		}
		else if(!strcmp(argv[i],"-Q")){//disable progress message
			if(Qarg){
				fprintf(stderr,"Duplicated no-progress mode argument.\n");
				return 1;
			}
			Qarg = 1;
		}
		else if(!strcmp(argv[i],"-h")){// help
			print_help(argv[0]);
			return -1;
		}
		else{
			fprintf(stderr,
				"Unrecognized argument: \"%s\". \nrun '%s -h' for help.\n",
				argv[i],
				argv[0]
			);

			return 1;
		}

	}
	if(qarg){
		args->quiet = 1;
	}
	else{
		args->quiet = 0;
	}

	if(!earg && !darg){
		fprintf(
			stderr,
			"Missing operation option.\nrun '%s -h' for help.\n"
			,argv[0]
		);

		return 1;
	} 
	if(!oarg && !sarg){
		fprintf(stderr,
			"Missing output file path.\nrun '%s -h' for help.\n",argv[0]);

		return 1;
	}

	if(karg){
		args->keep_frames = 1;
	}
	else{
		args->keep_frames = 0;
	}

	if(Qarg){
		args->noprogress = 1;
	}
	else{
		args->noprogress = 0;
	}

	if(rarg){
		if(strlen(width) > 6 || strlen(height) > 6){
			fprintf(stderr,"Resolution value is very big!\n");
			return 1;
		}

		args->width  = atoi(width);
		args->height = atoi(height);

		if(args->width == 0 || args->height == 0){
			fprintf(stderr,"Resolution value is 0!\n");
			return 1;
		}
	}

	else{
		if(!qarg && earg){
			fprintf(stderr,"No resolution size has been provided.");
			fprintf(stderr,"Using default value (1920x720)\n");
		}
		args->width  = 1280;
		args->height = 720;
	}

	args->noconfirm = narg;

	if(parg){
		args->pixel_size = pixel_size;
	}
	else{
		if(!qarg){
			fprintf(stderr,
				"No pixel size has been provided. Using default value (4)\n");
		}
		args->pixel_size = 4;
	}

	if(earg){
		args->operation = 0;

		if(carg){
			codec_arg_len = 1 + strlen(argv[codec_arg_pos]);
			args->codec = (char *)malloc(sizeof(char) * codec_arg_len);
			if(args->codec == NULL){
				fprintf(stderr,"Error allocating memory for codec name.\n");
				free(args->input_file_path);
				free(args->output_file_path);
				return 1;
			}
			strcpy(args->codec,argv[codec_arg_pos]);
		}
		else{
			if(!qarg){
				fprintf(stderr,
					"No codec name been provided. Using default (libx265)\n");
			}
			args->codec = (char *)malloc(sizeof(char) * 9);
			if(args->codec == NULL){
				fprintf(stderr,"Error allocating memory for codec name.\n");
				free(args->input_file_path);
				free(args->output_file_path);
				return 1;
			}
			strcpy(args->codec,"libx265");
		}

		if(zarg)
			args->ultrafast = 1;
		else
			args->ultrafast = 0;

		if(targ){
			args->threads = threads;
		}

		else{
			if(!qarg){
				fprintf(stderr,"No threads number has been provided. Using default value (1)\n");
			}
			args->threads = 1;
		}
		if(farg){
			args->framerate = framerate;
		}
		else{
			args->framerate = 30;
			if(!qarg){
				fprintf(stderr,"No frame rate has been provided. Using default value (30)\n");
			}
		}
		if(sarg){
			args->skip_ffmpeg = 1;

			if(carg && !qarg){
				fprintf(stderr,"-c will be ignored. FFMPEG process skipped.\n");
			}
			if(farg && !qarg){
				fprintf(stderr,"-f will be ignored. FFMPEG process skipped.\n");
			}
			if(zarg && !qarg){
				fprintf(stderr,"-z will be ignored. FFMPEG process skipped.\n");
			}
		}
		else{
			args->skip_ffmpeg = 0;
		}
	}

	if(darg){
		args->operation = 1;
		if(!qarg){
			if(zarg){
				fprintf(stderr,"-z argument is only available with -e (file to video). Ignoring.\n");
			}
			if(farg){
				fprintf(stderr,"-f argument is only available with -e (file to video). Ignoring.\n");
			}
			if(targ){
				fprintf(stderr,"-t argument is only available with -e (file to video). Ignoring.\n");
			}
			if(carg){
				fprintf(stderr,"-c argument is only available with -e (file to video). Ignoring.\n");
			}
			if(sarg){
				fprintf(stderr,"-s argument is only available with -e (file to video). Ignoring.\n");
			}
		}

		if(sarg){
			fprintf(stderr,"The FFMPEG process cannot be skipped in decode operation (-s argumnt)\n");
			return 1;
		}
		args->codec = NULL;
	}
	else{
		if(aarg){
			fprintf(stderr,"-a argument is only available with -d (video to file). Ignoring.\n");
		}
	}
	if(barg){
		args->bmp = 1;
	}
	else{
		args->bmp = 0;
	}

	if(earg && sarg && oarg && !qarg){
		fprintf(stderr,"-o will be ignored, FFMPEG process skipped.\n");
	}

	args->input_file_path  = argv[in_file_arg_pos];
	args->output_file_path = argv[out_file_arg_pos];

	if(pixel_size > args->width || pixel_size > args->height){
		fprintf(stderr,"Error: Pixel size is higher than width-height\n");
		return 1;
	}
	if(aarg){
		args->rmode = am;
	}
	return 0;
}

void destroy_args(arg_s *args){
	if(args->codec != NULL){
		free(args->codec);
		args->codec = NULL;
	}
	#ifndef __WIN32
	if(args->ffmpeg_path != NULL){
		free(args->ffmpeg_path);
		args->ffmpeg_path = NULL;
	}
	#endif
}
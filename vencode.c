/*
	vencode: Convert any file to video.
	Copyright (C) 2023  tr0pe <holahell90@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "args.h"
#include "util.h"

#ifndef __WIN32
#include <sys/wait.h>
#else
#include <conio.h>
#include <windows.h>
#endif

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct wr_img_s{
	char *filename;
	char *input_file_path;

	unsigned int *frame_index;
	unsigned long *buf_ind;
	unsigned long sz;
	unsigned long img_sz;

	unsigned long pos;
	FILE **input;

	short width;
	short height;
	short pixel_size;

	short thread_num;
	short return_value;

	_Bool reverse_x;
	_Bool reverse_y;
	_Bool invert_color;
	_Bool invert_byte;
};


static void 
get_bin(char c, _Bool *num, _Bool invert_color)
{
	for(short i=0;i<8;i++)
		num[i] = invert_color ? !((c >> (7 - i)) & 1) : ((c >> (7 - i)) & 1);
}

static char
set_bin(_Bool *num, _Bool invert_color)
{
	char byte = 0;
	for(short i=0;i<8;i++){
		if(i != 0) byte <<= 1;
		byte |= invert_color ? !num[i] : num[i];
	}
	return byte;
}

static void
png_set_pixel(png_bytepp rows,int y,int x,short color,short pixel_size)
{
	for(short i=0;i<pixel_size; i++)
		for(short j=0;j<pixel_size;j++)
			rows[y+i][x + j] = color;
}

static short
png_get_pixel(png_bytep *row, int x,int y,short pixel_size,short acc,
    short grange0, short grange1)
{
	png_bytep px;
	short prod = 0;

	/* Get the median color pixel from bit (may be accurate [fast]) */
	if(acc == 1 && pixel_size > 2){
		px = &(row[y+(pixel_size/2)][(x+(pixel_size/2)) * 4]);
		prod = px[0];
	}

	/* Get the arithmetic average of color from bit (accurate?) */
	else if(acc == 2 && pixel_size > 1){
		for(short i=0;i<pixel_size;i++){
			for(short j=0;j<pixel_size;j++){
				px = &(row[y+i][((x+j) * 4)]);
				prod += px[0];
			}
		}
		prod = prod/(pixel_size*pixel_size);
	}

	/* Get the first color pixel from bit (not accurate [fast]) */
	else{
		px = &(row[y][(x * 4)]);
		prod = px[1];
	}

	/* TODO: dynamic value adjustment */
 	if(prod < grange0 && prod > grange1){
		printf("\nEnd of file | Grayscale (may be ~127) = %d | ",prod);
		printf("End coord: XY(%d,%d)\n",x,y);
		return 127;
	}

	else if(prod > grange0) return 0;
	else if(prod < grange1) return 1;

	return 127;
}

static void *
write_image(void *arg)
{

	struct wr_img_s *img = (struct wr_img_s *)arg;

	_Bool *num = (_Bool *)malloc(sizeof(_Bool) * 8);
	if(num == NULL){
		fprintf(stderr,"Error allocating memory for byte array.\n");
		img->return_value = -1;
		return NULL;
	}

	char *buffer = (char *)malloc(sizeof(char) * img->img_sz);
	if(buffer == NULL){
		fprintf(stderr,"Error allocating memory for write-buffer\n");
		free(num);
		img->return_value = -1;
		return NULL;
	}

	pthread_mutex_lock(&mutex);

	if(-1 == fseek(*(img->input),img->pos,SEEK_SET)){
		perror("fseek");
		fprintf(stderr,"FSEEK ERROR! at position: %ld\n",img->pos);
		free(num);
		free(buffer);
		img->return_value = -1;
		return NULL;
	}

	if(fread(buffer,sizeof(char),img->img_sz,*(img->input)) == 0){
		if(ferror(*(img->input))){
			fprintf(stderr,"\nError reading input file stream.\n");
			free(num);
			free(buffer);
			img->return_value = -1;
			return NULL;
		}
	}

	pthread_mutex_unlock(&mutex);

	unsigned long buf_ind = img->pos;

	unsigned int buffer_index = 0;
	char c;
	short invert_byte = img->invert_byte ? 7 : 0;

	png_infop info;
	png_structp png;

	png_bytepp rows =
	(png_bytepp)malloc(img->height * sizeof(png_bytep));
	if(rows == NULL){
		fprintf(stderr,"Error allocating memory for rows pointer.\n");
		free(num);
		free(buffer);
		img->return_value = -1;
		return NULL;
	}
	for(int i=0;i < img->height;i++){
		rows[i] = (png_bytep)calloc(img->width * 4, sizeof(png_byte));
		if(rows[i] == NULL){
			fprintf(stderr,"Error allocating memory for PNG rows.\n");
			free(num);
			free(buffer);
			img->return_value = -1;
			return NULL;
		}
	}
	pthread_mutex_lock(&mutex);

	FILE *png_output;
	png_output = fopen(img->filename,"wb");

	if(png_output == NULL){
		perror(img->filename);
		free(rows);
		free(num);
		free(buffer);
		img->return_value = -1;
		return NULL;
	}

	pthread_mutex_unlock(&mutex);

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if(!png){
		fprintf(stderr,"Error creating png struct!");
		free(rows);
		free(num);
		free(buffer);
		fclose(png_output);
		img->return_value = -1;
		return NULL;
	}

	info = png_create_info_struct(png);
	if(!info){
		fprintf(stderr,"Error creating info struct!");
		free(rows);
		free(num);
		free(buffer);
		fclose(png_output);
		img->return_value = -1;
		return NULL;
	}

	pthread_mutex_lock(&mutex);
	png_init_io(png,png_output);
	pthread_mutex_unlock(&mutex);

	png_set_IHDR(
		png,
		info,
		img->width,
		img->height,
		8,
		PNG_COLOR_TYPE_GRAY,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);

	/* png_set_compression_level(png,img->png_compress_lv); */
	/* png_set_filter(png,PNG_FILTER_TYPE_BASE,PNG_ALL_FILTERS); */

	png_write_info(png,info);

	for(
		int y = img->reverse_y ? img->height - img->pixel_size : 0;
		img->reverse_y ? (y >= 0) : (y < img->height);
		img->reverse_y ? (y -= img->pixel_size) : (y += img->pixel_size)
	){
		for(
			int x = img->reverse_x ? img->width - img->pixel_size : 0;
			img->reverse_x ? (x >= 0) : (x < img->width);
			img->reverse_x ? (x -= img->pixel_size) : (x += img->pixel_size)
		){
			if(buf_ind < img->sz){
				c = buffer[buffer_index];
				buffer_index++;
				get_bin(c, num, img->invert_color);
				for(short i=0;i<8;i++){
					png_set_pixel(
						rows,
						y,
						img->reverse_x ?
						x-(img->pixel_size * i) : x+(img->pixel_size * i),
						num[abs(invert_byte - i)] ? 0 : 255,
						img->pixel_size
					);
				}
				buf_ind++;
				if(img->reverse_x) 
					x -= (img->pixel_size * 7);
				else x += (img->pixel_size * 7);
			}
			else
				png_set_pixel(rows, y,x, 127, img->pixel_size);
		}
	}

	png_write_rows(png,rows,img->height);
	png_write_end(png,NULL);

	png_destroy_write_struct(&png, &info);
	png_free_data(png,info,PNG_FREE_ALL,-1);

	for(int i=0;i<img->height;i++)
		free(rows[i]);
	free(rows);

	pthread_mutex_lock(&mutex);
	fclose(png_output);
	pthread_mutex_unlock(&mutex);

	free(info);
	free(num);
	free(buffer);

	img->return_value = 0;

	return NULL;
}

static short
encode(struct arg_s *args)
{
	if(!args->noconfirm && !args->skip_ffmpeg)
		if(replace(args->output_file_path))
			return 0;

	unsigned long long sz;

	FILE *input = open_file(args->input_file_path,&sz);
	if(input == NULL)
		return -1;

	if(sz == 0){
		fprintf(stderr,"Error. The file is empty.\n");
		fclose(input);
		return 1;
	}

	unsigned long long max_img_size;
	max_img_size = (args->width * args->height)/
	(args->pixel_size * args->pixel_size * 8);

	if(max_img_size == 0){
		fprintf(stderr,
			    "Error: At least 1 byte must be stored per frame.\n"
		);
		return 1;
	}

	unsigned long img_quant = sz/max_img_size;

	img_quant++;

	printf(" * Max image size: ");
	print_size(max_img_size);
	printf(" * Input file size: ");
	print_size(sz);
	printf(" * Frame quant: %ld\n",img_quant);

	if(!args->noconfirm){
		fprintf(stderr," > Press Enter to continue...");
		getchar();
	}

	short percent;

	pid_t pid = getpid();
	char dir[32];

	sprintf(dir,"%d_frames",pid);

#ifndef __WIN32
	mkdir(dir,0700);
#else
	mkdir(dir);
#endif

	pthread_t WRITE[args->threads];

	struct wr_img_s *img[args->threads];

	short th_ind;

	if(img_quant == 1) percent = 100;

	char ext[4];

	sprintf(dir,"%d_frames.png",pid);
	strcpy(ext,"png");

	if(args->noprogress) fprintf(stderr,"Writing images...\n");

	unsigned int frame_index = 0;
	while(frame_index < img_quant){
		th_ind = 0;
		for(short i=0;i<args->threads && frame_index < img_quant;i++){
			img[i] = (struct wr_img_s *)malloc(sizeof(struct wr_img_s));
			if(img[i] == NULL){
				fprintf(stderr,
					"Error allocating memory for thread struct %d\n",i);
				fclose(input);

				for(short i=0;i<args->threads;i++) 
					free(img[i]->filename);

				return -1;
			}
			img[i]->filename = (char *)malloc(sizeof(char)*128);
			if(img[i]->filename == NULL){
				fprintf(stderr,
					"t[%d] Error allocating memory for png path %d \n",
					i,frame_index
				);

				fclose(input);

				for(short i=0;i<args->threads;i++){
					free(img[i]->filename);
					free(img[i]);
				}
				return -1;
			}

			img_index(
				img[i]->filename,
				args->invert_frames ? (img_quant-frame_index-1) : frame_index,
				pid,ext
			);

			if(!args->noprogress){
				if(img_quant > 1) percent = (100*frame_index)/(img_quant-1);
				fprintf(
					stderr,
					"\rWriting data to: %s | %d%%",
					img[i]->filename,percent
				);
			}

			img[i]->thread_num = i;
			img[i]->sz = sz;
			img[i]->input = &input;
			img[i]->frame_index = &frame_index;
			img[i]->img_sz = max_img_size;
			img[i]->pos = max_img_size*frame_index;
			img[i]->height = args->height;
			img[i]->width = args->width;
			img[i]->pixel_size = args->pixel_size;
			img[i]->input_file_path = args->input_file_path;
			if(args->odd){
				if(!(frame_index%2)){
					img[i]->reverse_x = 1;
					img[i]->reverse_y = 1;
					img[i]->invert_color = 1;
					img[i]->invert_byte = 0;
				}
				else{
					img[i]->reverse_x = 0;
					img[i]->reverse_y = 0;
					img[i]->invert_color = 0;
					img[i]->invert_byte = 1;
				}
				if(frame_index == (img_quant - 1)){
					img[i]->reverse_x = 0;
					img[i]->reverse_y = 0;
					img[i]->invert_color = 1;
					img[i]->invert_byte = 1;
				}
			}
			else{
				img[i]->reverse_x = args->reverse_x;
				img[i]->reverse_y = args->reverse_y;
				img[i]->invert_color = args->invert_color;
				img[i]->invert_byte = args->invert_byte;
			}

			if(0 != pthread_create(&WRITE[i],NULL,write_image,img[i])){
				perror("\npthread_create");
				fclose(input);

				for(short i=0;i<args->threads;i++){
					free(img[i]->filename);
					free(img[i]);
				}

				fprintf(stderr,"thread: %d\n",i);
				fclose(input);
				return -1;
			}
			frame_index++;
			th_ind++;
		}

		for(short i=0;i<th_ind;i++){
			pthread_join(WRITE[i],NULL);
			if(img[i]->return_value != 0){

				fprintf(stderr,
					"\nAn error ocurred writing: %s.\nExiting...\n",
					img[i]->filename
				);

				for(short i=0;i<args->threads; i++){
					free(img[i]->filename);
					free(img[i]);
				}

				return -1;
			}
			free(img[i]->filename);
			if(img[i] != NULL)
				free(img[i]);
		}
	}

	fprintf(stderr,"\n");
	fclose(input);

	if(args->skip_ffmpeg){
		if(!args->quiet){
			printf("FFMPEG Video encode skipped...\n");
			printf("Frames saved in '%d_frames/' directory\n",pid);
		}
		return 0;
	}

	char *framerate = (char *)malloc(sizeof(char) * 32);
	if(framerate == NULL){
		fprintf(stderr,
			"Error allocating memory for framerate argument.");

		fclose(input);
		for(short i=0;i<args->threads; i++){
			free(img[i]->filename);
			free(img[i]);
		}
		return -1;
	}

	sprintf(framerate,"%d",args->framerate);

	char *preset = (char *)malloc(sizeof(char) * 16);
	if(preset == NULL){
		fprintf(stderr,"Error allocating memory for preset argument.");
		fclose(input);
		for(short i=0;i<args->threads; i++){
			free(img[i]->filename);
			free(img[i]);
		}
		return -1;
	}

	if(args->ultrafast)
		sprintf(preset,"ultrafast");
	else
		sprintf(preset,"medium");

#ifndef __WIN32

	char *ffm_input = (char *)malloc(sizeof(char) * 128);
	if(ffm_input == NULL){
		fprintf(stderr,
			"Error allocating memory for frames output filename\n");
		fclose(input);

		for(short i=0;i<args->threads; i++){
			free(img[i]->filename);
			free(img[i]);
		}
		return -1;
	}

	sprintf(ffm_input,"%d_frames/frame_%%09d.png",pid);

	pid_t pidC = fork();

	int status;

	if(pidC > 0) printf("Encoding video with ffmpeg...\n");
	else if(pidC == 0){
		if(args->noconfirm){
			execlp(args->ffmpeg_path,"ffmpeg",
				"-hide_banner",
				"-loglevel","error","-stats",
				"-framerate",framerate,
				"-i",ffm_input,
				"-c:v",args->codec,"-crf","25",
				"-preset",preset,"-y",
				args->output_file_path,
				NULL
			);
		}
		else{
			execlp(args->ffmpeg_path,"ffmpeg",
				"-hide_banner",
				"-loglevel","error","-stats",
				"-framerate",framerate,
				"-i",ffm_input,
				"-c:v",args->codec,"-crf","25",
				"-preset",preset,
				args->output_file_path,
				NULL
			); /* -1 is error.*/
		}
	}

	else{
		fprintf(stderr,"Error creating child process for ffmpeg.\n");
		fclose(input);
		free(ffm_input);
		free(preset);
		free(framerate);

		for(short i=0;i<args->threads; i++){
			free(img[i]->filename);
			free(img[i]);
		}
		return -1;
	}

	pidC = wait(&status);

	free(preset);
	free(framerate);
	free(ffm_input);

#else
	/*
	 * TODO
	 * I hate using the system() function, but I couldn't find
	 * an alternative for fork() function for MinGW. I'll work
	 * on it.
	*/
	printf("Encoding video with ffmpeg...\n");

	char *ffm_input = (char *)malloc(sizeof(char) * 128);
	if(ffm_input == NULL){
		fprintf(stderr,
			"Error allocating memory for ffmpeg input filename.\n");

		free(preset);
		free(framerate);

		for(short i=0;i<args->threads; i++){
			free(img[i]->filename);
			free(img[i]);
		}
		return -1;
	}

	sprintf(ffm_input,"%d_frames/frame_%%09d.png",pid);

	char *output_vid_name = (char *)malloc(sizeof(char) * 512);
	if(output_vid_name == NULL){
		fprintf(stderr,
			"Error allocating memory for ffmpeg output filename.\n");

		free(ffm_input);
		free(preset);
		free(framerate);

		for(short i=0;i<args->threads; i++){
			free(img[i]->filename);
			free(img[i]);
		}
		return -1;
	}

	strcpy(output_vid_name,args->output_file_path);

	char *ffm_args = (char *)malloc(sizeof(char) * 256);
	if(output_vid_name == NULL){
		fprintf(stderr,
			"Error allocating memory for ffmpeg arguments.\n");

		free(preset);
		free(framerate);
		free(ffm_input);
		free(output_vid_name);

		for(short i=0;i<args->threads; i++){
			free(img[i]->filename);
			free(img[i]);
		}
		return -1;
	}
	if(args->noconfirm){
		sprintf(ffm_args,"\"%s\" -framerate %s -i %s -c:v %s -crf 25 -preset "
		        "%s -y %s -hide_banner -loglevel error -stats",
			    FFMPEG_EXE_PATH,framerate,ffm_input,args->codec,preset,
			    args->output_file_path);
	}
	else{
		sprintf(ffm_args,"\"%s\" -framerate %s -i %s -c:v %s -crf 25 -preset "
		        "%s %s -hide_banner -loglevel error -stats",
			    FFMPEG_EXE_PATH,framerate,ffm_input,args->codec,preset,
			    args->output_file_path);
	}

	system(ffm_args);

	free(ffm_args);
	free(ffm_input);
	free(output_vid_name);
#endif

	if(args->keep_frames){
		printf("All done!\n");
		return 0;
	}

	unsigned long file_quant = 0;
	char path[64];

	while(1){
		sprintf(path,"%d_frames/frame_%09ld.png",pid,file_quant);

		if(access(path, R_OK) != 0)
			break;
		else
			file_quant++;

		if(0 != remove(path)){
			perror(path);
			fprintf(stderr,"Error deleting %s\n",path);
			return 1;
		}
	}

	sprintf(path,"%d_frames",pid);
	if(0 != rmdir(path)){
		perror(path);
		fprintf(stderr,"Error deleting %s directory\n",path);

		return 1;
	}

	if(!args->quiet)
		printf("Deleted %d_frames directory.\n",pid);

	printf("\nAll done!\n");
	return 0;
}

static short
decode(struct arg_s *args)
{
	if(!args->skip_ffmpeg && !args->noconfirm){
		if(replace(args->output_file_path)){
			return 0;
		}
	}

	char *input_dir;
	_Bool input_folder = 0;

	struct stat path_stat;
	stat(args->input_file_path, &path_stat);

	if(S_ISDIR(path_stat.st_mode)) {
		if(!args->quiet){
			fprintf(stderr,"'%s' is a folder. ",args->input_file_path);
			fprintf(stderr,"FFMPEG frame extraction skipped.\n");
		}
		input_dir = args->input_file_path;
		input_folder = 1;
	} else {
		pid_t pid = getpid();

		char *dir_ffm = (char *)malloc(sizeof(char) * 128);
		if(dir_ffm == NULL){
			fprintf(stderr,
				"Error allocating memory for output frame extract.\n");

			return -1;
		}

		sprintf(dir_ffm,"%d_frames/frame_%%09d.png",pid);

		input_dir = (char *)malloc(sizeof(char) * 128);
		sprintf(input_dir,"%d_frames",pid);

		if(input_dir == NULL) {
			fprintf(stderr,
				"Error allocating memory for output frame extract.\n");
			return -1;
		}

#ifndef __WIN32
		mkdir(input_dir,0700);
		pid_t pidC;
		int status;

		pidC = fork();

		if(pidC > 0)
			printf("Extracting frames of %s\n",args->input_file_path);

		else if(pidC == 0) {
			execlp(args->ffmpeg_path,"ffmpeg",
			"-i",args->input_file_path,dir_ffm,
			"-hide_banner","-loglevel","error","-stats",NULL);
		} else {
			fprintf(stderr,"Error creating ffmpeg child process.\n");
			free(input_dir);
			free(dir_ffm);
			return -1;
		}

		pidC = wait(&status);
		free(dir_ffm);

#else 
		/*
		 * TODO
		 * I hate using the system() function, but I couldn't find
		 * an alternative for fork() function for MinGW. I'll work
		 * on it.
		*/
		mkdir(input_dir);
		printf("Extracting frames of %s\n",args->input_file_path);

		char *ffm_args = (char *)malloc(sizeof(char) * 256);
		sprintf(ffm_args,
			"\"%s\" -i %s %s -hide_banner -loglevel error -stats",
			FFMPEG_EXE_PATH,args->input_file_path,dir_ffm);

		printf("%s\n",ffm_args);
		system(ffm_args);

#endif
	}

	DIR *dir = opendir(input_dir);
	if(dir == NULL){
		fprintf(stderr,"Error opening directory: %s\n",input_dir);
		if(!input_folder)
			free(input_dir);
		return -1;
	}

	FILE * out = fopen(args->output_file_path,"wb");
	if(!out){
		perror(args->output_file_path);
		if(!input_folder)
			free(input_dir);
		return -1;
	}

	char framename[7]; /* frame_ */
	char **filenames;

	filenames = (char **)malloc(sizeof(char *));
	if(filenames == NULL){
		fprintf(stderr,"Error allocating memory for frames path\n");
		closedir(dir);
		fclose(out);
		if(!input_folder)
			free(input_dir);
		return -1;
	}

	filenames[0] = (char *)malloc(sizeof(char)*64);
	if(filenames[0] == NULL){
		fprintf(stderr,"Error allocating memory for frames path\n");
		closedir(dir);
		fclose(out);
		if(!input_folder)
			free(input_dir);
		free(filenames);
		return -1;
	}

	struct dirent *input;
	int frame_count = 0;

	while((input = readdir(dir)) != NULL){
		/* 
		 * TODO:
		 * errno needed 
		 */
		/*
		if(input == -1){
			perror("readdir");
			fprintf(stderr,"Error reading frames directory");
			fclose(out);
			closedir(dir);
			free(filenames[0]);
			free(filenames);
			if(!input_folder) free(input_dir);
			return -1;
		}
		*/
		for(short i=0;i<5;i++)
			framename[i] = input->d_name[i];

		framename[5] = '\0';

		if(!strcmp(framename, "frame")){
			char **filenames_p;

			sprintf(filenames[frame_count],
				"%s/%s",
				input_dir,
				input->d_name);

			frame_count++;

			filenames_p = realloc(filenames,
				          (frame_count + 1) * sizeof(char *));

			if(filenames_p == NULL){
				fprintf(stderr,
					"Error reallocating memory for frames path\n");

				fclose(out);
				destroy_filenames(filenames,frame_count);
				free(filenames);
				if(!input_folder)
					free(input_dir);
				return -1;
			}
			filenames = filenames_p;

			filenames[frame_count] = (char *)malloc(sizeof(char) * 64);
			if(filenames[frame_count] == NULL){
				fprintf(stderr,
					"Error allocating memory for frames path\n");

				fclose(out);
				destroy_filenames(filenames,frame_count);
				free(filenames);
				if(!input_folder)
					free(input_dir);

				return -1;
			}
		}
	}

	if(frame_count == 0){
		fprintf(stderr,"No frame files found.\n");
		fclose(out);
		free(filenames[0]);
		free(filenames);
		return -1;
	}

	if(args->invert_frames)
		qsort(filenames,frame_count,sizeof(char *),compat_reverse);
	else
		qsort(filenames,frame_count,sizeof(char *),compat);

	unsigned int aprox_size;
	aprox_size = (args->width*args->height * frame_count) /
	             (args->pixel_size * args->pixel_size * 8);

	printf(" * Frames: %d\n",frame_count);
	printf(" * Approximate output size: ");
	print_size(aprox_size);

	if(!args->noconfirm){
		fprintf(stderr," > Press Enter to continue... ");
		getchar();
	}

	_Bool *num = (_Bool *)malloc(sizeof(_Bool) * 8);
	if(num == NULL){
		fprintf(stderr,"Error allocating memory for byte buffer.\n");
		fclose(out);
		destroy_filenames(filenames,frame_count);
		free(filenames);
		return -1;
	}

	if(args->noprogress)
		fprintf(stderr,"Decoding data from images...\n");

	short percent;

	_Bool reverse_x = 0;
	_Bool reverse_y = 0;
	_Bool invert_color = 0;
	_Bool eend = 0;
	short invert_byte = args->invert_byte ? 7 : 0;

	png_structp png;
	png_infop info;
	FILE *frame;

	for(int i=0;i<frame_count && !eend;i++){
		if(args->odd) {
			if(!(i%2)) {
				reverse_x = 1;
				reverse_y = 1;
				invert_color = 1;
				invert_byte = 0;
			} else {
				reverse_x = 0;
				reverse_y = 0;
				invert_color = 0;
				invert_byte = 7;
			}
			if(i == (frame_count - 1)){
				reverse_x = 0;
				reverse_y = 0;
				invert_color = 1;
				invert_byte = 7;
			}
		} else {
			if(args->invert_color) invert_color = 1;
			if(args->reverse_x) reverse_x = 1;
			if(args->reverse_y) reverse_y = 1;
		}

		if(!args->noprogress){
			if(frame_count > 1) percent = (100*i)/(frame_count-1);
			else percent = 100;
			fprintf(stderr,"\rDecoding %s | %d%%",filenames[i],percent);
		}

		frame = fopen(filenames[i],"rb");
		if(!frame){
			perror(filenames[i]);
			fprintf(stderr,"\nError opening file: %s",filenames[i]);
			fclose(out);
			destroy_filenames(filenames,frame_count);
			free(filenames);
			return -1;
		}
		png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

		if(!png){
			fprintf(stderr,"\nError creating PNG read struct\n");
			fclose(out);
			fclose(frame);
			destroy_filenames(filenames,frame_count);
			free(filenames);
			return 1;
		}

		info = png_create_info_struct(png);

		if(!info){
			fprintf(stderr,"\nError creating PNG info struct\n");
			fclose(out);
			fclose(frame);
			destroy_filenames(filenames,frame_count);
			png_destroy_read_struct(&png,NULL,NULL);
			free(filenames);
			return 1;
		}

		png_init_io(png,frame);
		png_read_info(png,info);

		int width = png_get_image_width(png,info);
		int height = png_get_image_height(png,info);

		png_byte color_type = png_get_color_type(png, info);
		png_byte bit_depth = png_get_bit_depth(png, info);

		if(color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_expand_gray_1_2_4_to_8(png);

		if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
			png_set_expand_gray_1_2_4_to_8(png);

		if(png_get_valid(png, info, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(png);

		if(color_type == PNG_COLOR_TYPE_RGB ||
		   color_type == PNG_COLOR_TYPE_GRAY ||
		   color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

		if(color_type == PNG_COLOR_TYPE_GRAY ||
		   color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(png);

		png_read_update_info(png, info);

		png_bytep *row_pointers = 
		(png_bytep *)malloc(sizeof(png_bytep) * height);
		if(row_pointers == NULL){
			fprintf(stderr, "\nError allocating memory for PNG row vector.\n");
			fclose(out);
			fclose(frame);
			destroy_filenames(filenames,frame_count);
			free(filenames);
			return -1;
		}

		for(int i=0;i<height;i++){
			row_pointers[i] =
			(png_byte *)malloc(png_get_rowbytes(png, info));

			if(row_pointers[i] == NULL){
				fprintf(stderr,
					"\nError allocating memory for PNG row pointers.\n");

				fclose(out);
				fclose(frame);
				destroy_filenames(filenames,frame_count);
				free(filenames);
				return -1;
			}
		}

		png_read_image(png, row_pointers);

		short k = 0;
		short res;
		char lett;

		for(
			int y = reverse_y ? height - args->pixel_size : 0;
			reverse_y ? (y >= 0  && !eend) : (y < height && !eend);
			reverse_y ? (y-=args->pixel_size) : (y+=args->pixel_size)
		){
			for(
				int x = reverse_x ? width - args->pixel_size : 0;
				reverse_x ? (x >= 0  && !eend) : (x < width && !eend);
				reverse_x ? (x-=args->pixel_size) : (x+=args->pixel_size)
			){
				res = png_get_pixel(row_pointers, x,y, args->pixel_size,
					                args->rmode,
					                args->grange[0] ? args->grange[0] : 180,
					                args->grange[1] ? args->grange[1] : 80);

				if(res == 127){
					eend = 1;
					break;
				}

				num[abs(invert_byte - k)] = res;
				k++;

				if(k>7){
					lett = set_bin(num,invert_color);
					if(0 == fwrite(&lett,1,sizeof(char),out)){
						fprintf(stderr,
							"\nError writing byte to output file.\n");

						fclose(out);
						fclose(frame);
						destroy_filenames(filenames,frame_count);
						free(filenames);
						free(num);
						return -1;
					}
					k=0;
				}
			}
		}

		png_destroy_read_struct(&png, &info,NULL);
		png_free_data(png,info,PNG_FREE_ALL,-1);

		fclose(frame);

		for(int i=0;i<height;i++)
			free(row_pointers[i]);

		free(row_pointers);
	}

	free(num);

	if(!args->keep_frames){
		if(!args->quiet) fprintf(stderr,"Deleting frame files...\n");
		for(int i=0;i<frame_count;i++){
			if(0 != remove(filenames[i])){
				perror("remove");
				fprintf(stderr, "Error deleting file: %s...", filenames[i]);
			}
		}

		if(-1 == rmdir(input_dir)){
			perror("rmdir");
			fprintf(stderr,"Error deleting folder: %s...",input_dir);
		} else if(!args->quiet)
			printf("Removed directory: %s/\n",input_dir);
	}

	closedir(dir);

	if(!input_folder)
		free(input_dir);

	destroy_filenames(filenames,frame_count);
	free(filenames);
	printf("\nAll done! Final output size: ");
	print_size(ftell(out));
	fclose(out);

	return 0;
}

int
main(int argc, char *argv[])
{
	struct arg_s args;

	if(getopts(&args,argc,argv) != 0)
		return 1;

#ifndef __WIN32
	if(!args.skip_ffmpeg){
		args.ffmpeg_path = (char *)malloc(sizeof(char) * 64);

		if(args.ffmpeg_path == NULL) {
			fprintf(stderr,"Error allocating memory for FFMPEG file path.\n");
			return 1;
		}

		if(0 == check_ffmpeg(args.ffmpeg_path)) {
			if(!args.quiet) printf("Found FFMPEG at: %s\n",args.ffmpeg_path);
		} else {
			fprintf(stderr,"\nFFMPEG not found on your system.\n");
			fprintf(stderr,"Please install it and retry.\n");
			return 1;
		}
	}
#else
	if(!args.skip_ffmpeg) {
		if(0 == check_ffmpeg()) {
			fprintf(stderr,"Error. '%s' not found.\n",FFMPEG_EXE_PATH);
			return 1;
		}
	}
#endif

	args.operation ? decode(&args) : encode(&args);

#ifndef __WIN32
	free(args.ffmpeg_path);
#endif
	return 0;
}

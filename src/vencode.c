/*
	vencode: Convert any file to video.
	Copyright (C) 2023  tr0pe

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <png.h>

#include <args.h>
#include <util.h>
#include <bmp.h>

#ifndef __WIN32
#include <sys/wait.h>
#else
#include <conio.h>
#include <windows.h>
#endif

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void get_bin(char c, _Bool *num, _Bool invert_color){
	for(int i = 0; i < 8; i++){
		num[i] = (c >> (7 - i)) & 1;
		if(invert_color){
			if(num[i]) num[i] = 0;
			else num[i] = 1;
		}
	}
}

char set_bin(_Bool *num, _Bool invert_color){
	char byte = 0;
	for(int i=0;i<8;i++){
		if(i != 0) byte <<= 1;
		if(invert_color){
			if(num[i]) num[i] = 0;
			else num[i] = 1;
		}
		byte |= num[i];
	}
	return byte;
}

void png_set_pixel(png_bytepp rows,int y, int x, int color, int pixel_size){
	for(int i=0;i<pixel_size; i++){
		for(int j=0;j<pixel_size;j++){
			rows[y+i][x + j] = color;
		}
	}
}

void bmp_set_pixel(
	char *bitmap,
	int x,
	int y,
	int color,
	int padded_width,
	int pixel_size){

	for(int i = y ; i < (y+pixel_size) ; i++){
		for(int j = x; j< (x+pixel_size); j++){
			for (int color_ind = 0; color_ind < 3; color_ind++){
				int index = i * padded_width + j * 3 + color_ind;
				bitmap[index] = color;
			}
		}
	}
}

int png_get_pixel(png_bytep *row, int x,int y,int pixel_size,int acc){
	png_bytep px;
	int prod = 0;

	//get the median color pixel from bit (may be accurate [fast])
	if(acc == 1 && pixel_size > 2){
		px = &(row[y+(pixel_size/2)][(x+(pixel_size/2)) * 4]);
		prod = px[0];
	}

	//get the arithmetic average of color from bit (accurate?)
	else if(acc == 2 && pixel_size > 1){
		for(int i=0;i<pixel_size;i++){
			for(int j=0;j<pixel_size;j++){
				px = &(row[y+i][((x+j) * 4)]);
				prod += px[0];
			}
		}
		prod = prod/(pixel_size*pixel_size);
	}

	//get the first color pixel from bit (not accurate [fast])
	else{
		px = &(row[y][(x * 4)]);
		prod = px[1];
	}

	//todo: dynamic value adjustment
	if(prod < 180 && prod > 80){
		printf("\nEnd of file | Grayscale (may be ~127) = %d | ",prod);
		printf("End coord: XY(%d,%d)\n",x,y);
		return 127;
	}

	else if(prod > 180){
		return 0;
	}

	else if(prod < 80){
		return 1;
	}

	return 127;
}

int bmp_get_pixel(unsigned char *data, int padded_width,
					int y, int x, int width, int pixel_size,int bpp,int acc){

	int prod = 0;
	int index;

	//get the median color pixel from bit (may be accurate [fast])
	if(acc == 1 && pixel_size > 2){
		if(bpp > 24)
			index = (y+pixel_size)/2 * padded_width * 4 + (x+pixel_size)/2 * 4;
		else
			index = (y+pixel_size)/2 * padded_width * 3 + (x+pixel_size)/2 * 3;

		prod = data[index];
	}

	//get the arithmetic average of color from bit (accurate?)
	else if(acc == 2 && pixel_size > 1){
		for(int i=0;i<pixel_size;i++){
			for(int j=0;j<pixel_size;j++){
				if(bpp > 24){
					index = (y+i) * padded_width * 4 + (x+j) * 4;
				}
				else{
					index = (y+i) * padded_width * 3 + (x+j) * 3;
				}
				prod += data[index];
			}
		}
		prod = prod/(pixel_size*pixel_size);
	}

	//get the first color pixel from bit (not accurate [fast])
	else{
		if(bpp > 24)
			index = y * padded_width * 4 + x * 4;
		else
			index = y * padded_width * 3 + x * 3;

		prod = data[index];
	}

	//todo: dynamic value adjustment
	if(prod < 180 && prod > 80){
		printf("\nEnd of file | Grayscale (may be ~127) = %d | ",prod);
		printf("End coord: XY(%d,%d)\n",x,y);
		return 127;
	}

	else if(prod > 180){
		return 0;
	}

	else if(prod < 80){
		return 1;
	}

	return 127;
}

typedef struct{
	char *filename;
	char *input_file_path;

	unsigned int *frame_index;
	long unsigned int *buf_ind;
	long unsigned int sz;
	size_t img_sz;

	long int pos;
	FILE **input;

	int width;
	int height;
	int pixel_size;

	int thread_num;
	int return_value;

	int bmp;
	_Bool reverse_x;
	_Bool reverse_y;
	_Bool invert_color;
	_Bool invert_byte;
}wr_img_s;

void destroy_wr_img(wr_img_s *img){
	if(img->filename != NULL){
		free(img->filename);
		img->filename = NULL;
	}
}

void *write_image(void *arg){

	wr_img_s *img = (wr_img_s *)arg;

	if(img == NULL){
		fprintf(stderr,"NULL argument.\nExiting...\n");
		return NULL;
	}

	if(img->input == NULL){
		fprintf(stderr,"Input stream pointer is NULL\n");
		img->return_value = -1;
		return NULL;
	}

	if(img->filename == NULL){
		fprintf(stderr,"The image filename is NULL\n");
		img->return_value = -1;
		return NULL;
	}

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

	int num_it;

	num_it = fread(buffer,sizeof(char),img->img_sz,*(img->input));

	if(num_it == 0){ //fread error
		if(ferror(*(img->input))){
			fprintf(stderr,"\nError reading input file stream.\n");
			free(num);
			free(buffer);
			img->return_value = -1;
			return NULL;
		}
	}
	else{ //end of file?
//		fprintf(stderr,"\nEOF\n");
	}

	pthread_mutex_unlock(&mutex);

	int buf_ind = img->pos;

	int buffer_index = 0;
	char c;

	short invert_byte;
	if(img->invert_byte) invert_byte = 7;
	else invert_byte = 0;

	if(img->bmp){
		//thanks to https://lmcnulty.me/words/bmp-output/
		pthread_mutex_lock(&mutex);
		FILE *BMP_OUTPUT = fopen(img->filename,"w");

		if(BMP_OUTPUT == NULL){
			perror(img->filename);
			fprintf(stderr,"Error creating BMP output file.\n");
			img->return_value = -1;
			return NULL;
		}

		pthread_mutex_unlock(&mutex);

		int bitmap_size = 3 * img->height * (img->width + 1);
		int padded_width = bmp_round4(img->width * 3);

		char bmp_tag[] = {'B','M'};

		int header[] = {
			0x0,				// File Size (update at the end)
			0x00,				// Unused
			0x36,				// Byte offset of pixel data
			0x28,				// Header Size
			img->width,			// image width in pixels
			img->height,		// Image height in pixels
			0x180001,			// 24 bits/pixel, 1 color pane
			0,					// BI_RGB no ompression
			0,					// Pixel data size in bytes
			0x002e23, 0x002e23,	// Print resolution
			0,0					// No color palette
		};

		header[0] = sizeof(header) + sizeof(bmp_tag) + bitmap_size;

		char *bitmap = (char *)calloc(bitmap_size,sizeof(char));
		if(bitmap == NULL){
			fprintf(
				stderr,
				"Error allocating memory for bitmap image.\n"
			);
			pthread_mutex_lock(&mutex);
			fclose(BMP_OUTPUT);
			pthread_mutex_lock(&mutex);
			free(num);
			free(buffer);
			img->return_value = -1;
			return NULL;
		}

		if(img->reverse_y){
			for(
				int row = 0;
				row < img->height;
				row += img->pixel_size
			){
				if(img->reverse_x){
					for(
						int col = img->width - img->pixel_size;
						col >= 0;
						col -= img->pixel_size
					){
						if(buf_ind < img->sz){
							c = buffer[buffer_index];
							buffer_index++;
							get_bin(c, num, img->invert_color);

							for(int i=0;i<8;i++){
								if(num[abs(invert_byte - i)]){
									bmp_set_pixel(
										bitmap,
										col-(img->pixel_size * i),
										row,
										0,
										padded_width,
										img->pixel_size
									);
								}
								else{
									bmp_set_pixel(
										bitmap,
										col-(img->pixel_size * i),
										row,
										255,
										padded_width,
										img->pixel_size
									);
								}
							}
							col-=(img->pixel_size * 7);
							buf_ind++;
						}
						else{
							bmp_set_pixel(
								bitmap,
								col,
								row,
								127,
								padded_width,
								img->pixel_size
							);
						}
					}
				}
				else{
					for(
						int col = 0;
						col < img->width;
						col += img->pixel_size
					){
						if(buf_ind < img->sz){
							c = buffer[buffer_index];
							buffer_index++;
							get_bin(c, num, img->invert_color);

							for(int i=0;i<8;i++){
								if(num[abs(invert_byte - i)]){
									bmp_set_pixel(
										bitmap,
										col+(img->pixel_size * i),
										row,
										0,
										padded_width,
										img->pixel_size
									);
								}
								else{
									bmp_set_pixel(
										bitmap,
										col+(img->pixel_size * i),
										row,
										255,
										padded_width,
										img->pixel_size
									);
								}
							}
							col+=(img->pixel_size * 7);
							buf_ind++;
						}
						else{
							bmp_set_pixel(
								bitmap,
								col,
								row,
								127,
								padded_width,
								img->pixel_size
							);
						}
					}
				}
			}
		}
		else{
			for(
				int row = img->height-img->pixel_size;
				row >= 0;
				row-=img->pixel_size
			){
				if(img->reverse_x){
					for(
						int col = img->width - img->pixel_size;
						col >= 0;
						col -= img->pixel_size
					){
						if(buf_ind < img->sz){
							c = buffer[buffer_index];
							buffer_index++;
							get_bin(c, num, img->invert_color);

							for(int i=0;i<8;i++){
								if(num[abs(invert_byte - i)]){
									bmp_set_pixel(
										bitmap,
										col-(img->pixel_size * i),
										row,
										0,
										padded_width,
										img->pixel_size
									);
								}
								else{
									bmp_set_pixel(
										bitmap,
										col-(img->pixel_size * i),
										row,
										255,
										padded_width,
										img->pixel_size
									);
								}
							}
							col-=(img->pixel_size * 7);
							buf_ind++;
						}
						else{
							bmp_set_pixel(
								bitmap,
								col,
								row,
								127,
								padded_width,
								img->pixel_size
							);
						}
					}
				}
				else{
					for(
						int col = 0;
						col < img->width;
						col += img->pixel_size
					){
						if(buf_ind < img->sz){
							c = buffer[buffer_index];
							buffer_index++;
							get_bin(c, num, img->invert_color);

							for(int i=0;i<8;i++){
								if(num[abs(invert_byte - i)]){
									bmp_set_pixel(
										bitmap,
										col+(img->pixel_size * i),
										row,
										0,
										padded_width,
										img->pixel_size
									);
								}
								else{
									bmp_set_pixel(
										bitmap,
										col+(img->pixel_size * i),
										row,
										255,
										padded_width,
										img->pixel_size
									);
								}
							}
							col+=(img->pixel_size * 7);
							buf_ind++;
						}
						else{
							bmp_set_pixel(
								bitmap,
								col,
								row,
								127,
								padded_width,
								img->pixel_size
							);
						}
					}
				}
			}
		}

		pthread_mutex_lock(&mutex);
		//error managment

		fwrite(&bmp_tag,sizeof(bmp_tag),1, BMP_OUTPUT);
		fwrite(&header, sizeof(header), 1, BMP_OUTPUT);
		fwrite(bitmap, bitmap_size, sizeof(char), BMP_OUTPUT);

		fclose(BMP_OUTPUT);
		pthread_mutex_unlock(&mutex);
		free(buffer);
		free(num);
		free(bitmap);
		img->return_value = 0;

		return NULL;
	}

	png_infop info;
	png_structp png;

	png_bytepp rows =
	(png_bytepp)malloc(img->height * sizeof(png_bytep));
	if(rows == NULL){
		fprintf(stderr,"Error allocating memory for row pointer.\n");
		free(num);
		free(buffer);
		img->return_value = -1;
		return NULL;
	}
	for(int i=0;i < img->height;i++){
		rows[i] = (png_bytep)calloc(img->width * 4, sizeof(png_byte));
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

//	png_set_compression_level(png,img->png_compress_lv);
//	png_set_filter(png,PNG_FILTER_TYPE_BASE,PNG_ALL_FILTERS);

	png_write_info(png,info);
	if(img->reverse_y){
		for(
			int y = img->height - img->pixel_size;
			y >= 0;
			y -= img->pixel_size 
		){
			if(img->reverse_x){
				for(
					int x = img->width - img->pixel_size;
					x >= 0;
					x -= img->pixel_size
				){
					if(buf_ind < img->sz){
						c = buffer[buffer_index];
						buffer_index++;
						get_bin(c, num, img->invert_color);
						for(int i=0;i<8;i++){
							if(num[abs(invert_byte - i)])
								png_set_pixel(
									rows,
									y,
									x-(img->pixel_size * i),
									0,
									img->pixel_size
								);
							else
								png_set_pixel(
									rows,
									y,
									x-(img->pixel_size * i),
									255,
									img->pixel_size
								);
						}
						buf_ind++;
						x -= (img->pixel_size * 7);
					}
					else{
						png_set_pixel(rows, y,x, 127, img->pixel_size);
					}
				}
			}
			else{
				for(
					int x = 0;
					x < img->width;
					x += img->pixel_size
				){
					if(buf_ind < img->sz){
						c = buffer[buffer_index];
						buffer_index++;
						get_bin(c, num, img->invert_color);
						for(int i=0;i<8;i++){
							if(num[abs(invert_byte - i)])
								png_set_pixel(
									rows,
									y,
									x+(img->pixel_size * i),
									0,
									img->pixel_size
								);
							else
								png_set_pixel(
									rows,
									y,
									x+(img->pixel_size * i),
									255,
									img->pixel_size
								);
						}
						buf_ind++;
						x += (img->pixel_size * 7);
					}
					else{
						png_set_pixel(rows, y,x, 127, img->pixel_size);
					}
				}
			}
		}	
	}
	else{
		for(
			int y = 0;
			y < img->height;
			y += img->pixel_size
		){
			if(img->reverse_x){
				for(
					int x = img->width - img->pixel_size;
					x >= 0;
					x -= img->pixel_size
				){
					if(buf_ind < img->sz){
						c = buffer[buffer_index];
						buffer_index++;
						get_bin(c, num, img->invert_color);
						for(int i=0;i<8;i++){
							if(num[abs(invert_byte - i)])
								png_set_pixel(
									rows,
									y,
									x-(img->pixel_size * i),
									0,
									img->pixel_size
								);
							else
								png_set_pixel(
									rows,
									y,
									x-(img->pixel_size * i),
									255,
									img->pixel_size
								);
						}
						buf_ind++;
						x -= (img->pixel_size * 7);
					}
					else{
						png_set_pixel(rows, y,x, 127, img->pixel_size);
					}
				}
			}
			else{
				for(
					int x = 0;
					x < img->width;
					x += img->pixel_size
				){
					if(buf_ind < img->sz){
						c = buffer[buffer_index];
						buffer_index++;
						get_bin(c, num, img->invert_color);
						for(int i=0;i<8;i++){
							if(num[abs(invert_byte - i)])
								png_set_pixel(
									rows,
									y,
									x+(img->pixel_size * i),
									0,
									img->pixel_size
								);
							else
								png_set_pixel(
									rows,
									y,
									x+(img->pixel_size * i),
									255,
									img->pixel_size
								);
						}
						buf_ind++;
						x += (img->pixel_size * 7);
					}
					else{
						png_set_pixel(rows, y,x, 127, img->pixel_size);
					}
				}
			}
		}
	}
	png_write_rows(png,rows,img->height);
	png_write_end(png,NULL);

	png_destroy_write_struct(&png, &info);
	png_free_data(png,info,PNG_FREE_ALL,-1);

	for(int i=0;i<img->height;i++){
		free(rows[i]);
	}
	free(rows);

	pthread_mutex_lock(&mutex);
	fflush(png_output);
	fclose(png_output);
	pthread_mutex_unlock(&mutex);

	free(info);
	free(num);
	free(buffer);

	img->return_value = 0;

	return NULL;
}

int encode(arg_s *args){

	long unsigned int sz;

	FILE *input = open_file(args->input_file_path,&sz);
	if(input == NULL){
		return -1;
	}

	char key = 0;
	if(!args->skip_ffmpeg){
		if(-1 != access(args->output_file_path,R_OK)){
			if(!args->noconfirm){

				printf(
					"Replace '%s' file? [y/n] ",
					args->output_file_path
				);

				while(key != 'y' || key != 'n'){
					key = getch();
					if(key == 'n'){
						printf("\n");
						return -1;
					}
					else{
						break;
					}
				}
				printf("\n");
			}
		}
	}

	if(sz == 0){
		fprintf(stderr,"Error. The file is empty.\n");
		fclose(input);
		return(1);
	}

	long unsigned int max_img_size;
	max_img_size = (args->width * args->height)/
	(args->pixel_size * args->pixel_size * 8);

	if(max_img_size == 0){
		fprintf(
			stderr,
			"!! Error (divide by 0) this may be fixed on later versions...\n"
		);
		return 1;
	}

	unsigned int img_quant = sz/max_img_size;

	img_quant++;

	printf(" * Max image size: ");
	print_size(max_img_size);
	printf(" * Input file size: ");
	print_size(sz);
	printf(" * Frame quant: %d\n",img_quant);

	if(!args->noconfirm){
		fprintf(stderr," > Press Enter to continue...");
		getchar();
	}

	int percent;

	int pid = getpid();
	char dir[32];

	sprintf(dir,"%d_frames",pid);

	#ifndef __WIN32
	mkdir(dir,0700);
	#else
	mkdir(dir);
	#endif

	pthread_t WRITE[args->threads];

	wr_img_s *img[args->threads];

	int th_ind;

	if(img_quant == 1){
		percent = 100;
	}

	char ext[4];

	if(args->bmp){
		sprintf(dir,"%d_frames.bmp",pid);
		strcpy(ext,"bmp");
	}
	else{
		sprintf(dir,"%d_frames.png",pid);
		strcpy(ext,"png");
	}

	if(args->noprogress){
		fprintf(stderr,"Writing images...\n");
	}

	unsigned int frame_index = 0;
	while(frame_index < img_quant){
		th_ind = 0;
		for(int i=0;i<args->threads && frame_index < img_quant;i++){

			img[i] = (wr_img_s *)malloc(sizeof(wr_img_s));
			if(img[i] == NULL){
				fprintf(stderr,
					"Error allocating memory for thread struct %d\n",i);
				fclose(input);

				for(int i=0;i<args->threads;i++){
					destroy_wr_img(img[i]);
				}

				return -1;
			}

			img[i]->filename = (char *)malloc(sizeof(char)*128);
			if(img[i]->filename == NULL){

				fprintf(
					stderr,
					"t[%d] Error allocating memory for png path %d \n",
					i,frame_index
				);

				fclose(input);

				for(int i=0;i<args->threads;i++){
					destroy_wr_img(img[i]);
					free(img[i]);
				}

				return -1;
			}

			if(args->invert_frames){
				if( -1 == img_index(img[i]->filename,img_quant - frame_index - 1,pid,ext)){
					fprintf(stderr,"error\n");
					fclose(input);

					for(int i=0;i<args->threads;i++){
						destroy_wr_img(img[i]);
						if(img[i] != NULL){
							free(img[i]);
						}
					}
					return -1;
				}
			}
			else{
				if( -1 == img_index(img[i]->filename,frame_index,pid,ext)){
					fprintf(stderr,"error\n");
					fclose(input);

					for(int i=0;i<args->threads;i++){
						destroy_wr_img(img[i]);
						if(img[i] != NULL){
							free(img[i]);
						}
					}
					return -1;
				}
			}
			if(!args->noprogress){
				if(img_quant > 1){
					percent = (100*frame_index)/(img_quant-1);
				}

				fprintf(stderr,"\rWriting data to: %s | %d%%",
					img[i]->filename,percent);
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
			if(args->odd_mode){
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
			}
			else{
				img[i]->reverse_x = args->reverse_x;
				img[i]->reverse_y = args->reverse_y;
				img[i]->invert_color = args->invert_color;
				img[i]->invert_byte = args->invert_byte;
			}

			if(args->bmp){
				img[i]->bmp = 1;
			}
			else{
				img[i]->bmp = 0;
			}

			if(0 != pthread_create(&WRITE[i],NULL,write_image,img[i])){
				perror("\npthread_create");
				fclose(input);

				for(int i=0;i<args->threads;i++){
					destroy_wr_img(img[i]);
					free(img[i]);
				}

				fprintf(stderr,"thread: %d\n",i);
				fclose(input);
				return -1;
			}
			frame_index++;
			th_ind++;
		}

		for(int i=0;i<th_ind;i++){
			pthread_join(WRITE[i],NULL);
			if(img[i]->return_value != 0){

				fprintf(
					stderr,
					"\nAn error ocurred writing: %s.\nExiting...\n",
					img[i]->filename
				);

				for(int i=0;i<args->threads; i++){
					destroy_wr_img(img[i]); 
					free(img[i]);
				}

				return -1;
			}
			destroy_wr_img(img[i]);
			if(img[i] != NULL){
				free(img[i]);
			}
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
		fprintf(
			stderr,
			"Error allocating memory for framerate argument."
		);
		fclose(input);

		for(int i=0;i<args->threads; i++){
			destroy_wr_img(img[i]);
			free(img[i]);
		}

		return -1;
	}

	sprintf(framerate,"%d",args->framerate);

	char *preset = (char *)malloc(sizeof(char) * 16);
	if(preset == NULL){
		fprintf(stderr,"Error allocating memory for preset argument.");
		fclose(input);

		for(int i=0;i<args->threads; i++){
			destroy_wr_img(img[i]);
			free(img[i]);
		}

		return -1;
	}

	if(args->ultrafast){
		sprintf(preset,"ultrafast");
	}
	else{
		sprintf(preset,"medium");
	}

#ifndef __WIN32

	char *ffm_input = (char *)malloc(sizeof(char) * 128);
	if(ffm_input == NULL){

		fprintf(stderr,
			"Error allocating memory for frames output filename\n");

		fclose(input);

		for(int i=0;i<args->threads; i++){
			destroy_wr_img(img[i]);
			free(img[i]);
		}

		return -1;
	}

	if(args->bmp){
		sprintf(ffm_input,"%d_frames/frame_%%06d.bmp",pid);
	}
	else{
		sprintf(ffm_input,"%d_frames/frame_%%06d.png",pid);
	}

	pid_t pidC = fork();

	int status;
	if(pidC > 0){
		printf("Encoding video with ffmpeg...\n");
	}
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
			); //-1 is error.
		}
	}

	else{
		fprintf(stderr,"Error creating child process for ffmpeg.\n");
		fclose(input);
		free(ffm_input);
		free(preset);
		free(framerate);

		for(int i=0;i<args->threads; i++){
			destroy_wr_img(img[i]);
			free(img[i]);
		}

		return -1;
	}

	pidC = wait(&status);

	free(preset);
	free(framerate);
	free(ffm_input);

#else /*TODO!!!!*/
/*
	I hate using the system() function, but I couldn't find
	an alternative for fork() function for MinGW. I'll work
	on it.
*/
	printf("Encoding video with ffmpeg...\n");

	char *ffm_input = (char *)malloc(sizeof(char) * 128);
	if(ffm_input == NULL){

		fprintf(stderr,
			"Error allocating memory for ffmpeg input filename.\n");

		free(preset);
		free(framerate);

		for(int i=0;i<args->threads; i++){
			destroy_wr_img(img[i]);
			free(img[i]);
		}

		return -1;
	}

	if(args->bmp){
		sprintf(ffm_input,"%d_frames/frame_%%06d.bmp",pid);
	}
	else{
		sprintf(ffm_input,"%d_frames/frame_%%06d.png",pid);
	}

	char *output_vid_name = (char *)malloc(sizeof(char) * 512);
	if(output_vid_name == NULL){

		fprintf(stderr,
			"Error allocating memory for ffmpeg output filename.\n");

		free(ffm_input);
		free(preset);
		free(framerate);

		for(int i=0;i<args->threads; i++){
			destroy_wr_img(img[i]);
			free(img[i]);
		}

		return -1;
	}

	strcpy(output_vid_name,args->output_file_path);

	char *ffm_args = (char *)malloc(sizeof(char) * 256);
	if(output_vid_name == NULL){
		fprintf(
			stderr,
			"Error allocating memory for ffmpeg arguments.\n"
		);
		free(preset);
		free(framerate);
		free(ffm_input);
		free(output_vid_name);

		for(int i=0;i<args->threads; i++){
			destroy_wr_img(img[i]);
			free(img[i]);
		}

		return -1;
	}
	if(args->noconfirm){
		sprintf(ffm_args,"\"%s\" -framerate %s -i %s \
		-c:v %s -crf 25 -preset %s -y %s -hide_banner -loglevel error \
		-stats",FFMPEG_EXE_PATH,framerate,ffm_input,args->codec,preset,
		args->output_file_path);
	}
	else{
		sprintf(ffm_args,"\"%s\" -framerate %s -i %s \
		-c:v %s -crf 25 -preset %s %s -hide_banner -loglevel error \
		-stats",FFMPEG_EXE_PATH,framerate,ffm_input,args->codec,preset,
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

	unsigned long int file_quant = 0;
	char path[64];

	while(1){

		if(args->bmp){
			sprintf(path,"%d_frames/frame_%06ld.bmp",pid,file_quant);
		}
		else{
			sprintf(path,"%d_frames/frame_%06ld.png",pid,file_quant);
		}

		if(access(path, R_OK) != 0){
			break;
		}
		else{
			file_quant++;
		}

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

	if(!args->quiet) printf("Deleted %d_frames directory.\n",pid);

	printf("\nAll done!\n");
	return 0;
}

int decode(arg_s *args){
	char key = 0;
	if(-1 != access(args->output_file_path,R_OK)){
		if(!args->noconfirm){
			printf("Replace '%s' file? [y/n] ",args->output_file_path);
			while(key != 'y' || key != 'n'){
				key = getch();
				if(key == 'n'){
					printf("\n");
					return -1;
				}
				else if(key == 'y'){
					break;
				}
			}
			printf("\n");
		}
	}

	int input_folder= 0;
	char *input_dir;

	struct stat path_stat;
	stat(args->input_file_path, &path_stat);

	if(S_ISDIR(path_stat.st_mode)){
		input_folder = 1;
	}

	if(input_folder){
		if(!args->quiet){
			fprintf(stderr,"'%s' is a folder. ",args->input_file_path);
			fprintf(stderr,"FFMPEG frame extraction skipped.\n");
		}
		input_dir = args->input_file_path;
	}

	else{
		int pid = getpid();

		char *dir_ffm = (char *)malloc(sizeof(char) * 128);
		if(dir_ffm == NULL){

			fprintf(
				stderr,
				"Error allocating memory for output frame extract.\n"
			);

			return -1;
		}

		if(args->bmp){
			sprintf(dir_ffm,"%d_frames/frame_%%06d.bmp",pid);
		}
		else{
			sprintf(dir_ffm,"%d_frames/frame_%%06d.png",pid);
		}

		input_dir = (char *)malloc(sizeof(char) * 128);
		sprintf(input_dir,"%d_frames",pid);
		if(input_dir == NULL){

			fprintf(
				stderr,
				"Error allocating memory for output frame extract.\n"
			);

			return -1;
		}

		#ifndef __WIN32
		mkdir(input_dir,0700);
		#else
		mkdir(input_dir);
		#endif

		#ifndef __WIN32

		pid_t pidC;
		int status;

		pidC = fork();

		if(pidC > 0){
			printf("Extracting frames of %s\n",args->input_file_path);
		}
		else if(pidC == 0){
			execlp(args->ffmpeg_path,"ffmpeg",
			"-i",args->input_file_path,dir_ffm,
			"-hide_banner","-loglevel","error","-stats",NULL);
		}

		else{
			fprintf(stderr,"Error creating ffmpeg child process.\n");
			free(input_dir);
			free(dir_ffm);
			return -1;
		}

		pidC = wait(&status);
		free(dir_ffm);

		#else //TODO!!!!
		/*
			I hate using the system() function, but I couldn't find
			an alternative for fork() function for MinGW. I'll work
			on it.
		*/
		printf("Extracting frames of %s\n",args->input_file_path);

		char *ffm_args = (char *)malloc(sizeof(char) * 256);
		sprintf(
			ffm_args,
			"\"%s\" -i %s %s -hide_banner -loglevel error -stats",
			FFMPEG_EXE_PATH,args->input_file_path,dir_ffm
		);
		printf("%s\n",ffm_args);
		system(ffm_args);

		#endif
	}

	DIR *dir = opendir(input_dir);
	if(dir == NULL){
		fprintf(stderr,"Error opening directory: %s\n",input_dir);
		if(!input_folder) free(input_dir);
		return -1;
	}

	FILE * out = fopen(args->output_file_path,"wb");
	if(!out){
		perror(args->output_file_path);
		if(!input_folder) free(input_dir);
		return -1;
	}

	char framename[7]; /*frame_*/

	char **filenames;

	filenames = (char **)malloc(sizeof(char *));
	if(filenames == NULL){
		fprintf(stderr,"Error allocating memory for frames path\n");
		closedir(dir);
		fclose(out);
		if(!input_folder) free(input_dir);
		return -1;
	}

	filenames[0] = (char *)malloc(sizeof(char)*64);
	if(filenames[0] == NULL){
		fprintf(stderr,"Error allocating memory for frames path\n");
		closedir(dir);
		fclose(out);
		if(!input_folder) free(input_dir);
		free(filenames);
		return -1;
	}

	struct dirent *input;
	int frame_count = 0;

	while((input = readdir(dir)) != NULL){
/*
		//errno needed.
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
		for(int i=0;i<5;i++){
			framename[i] = input->d_name[i];
		}

		framename[5] = '\0';

		if(!strcmp(framename, "frame")){

			char **filenames_p;

			sprintf(
				filenames[frame_count],
				"%s/%s",
				input_dir,
				input->d_name
			);
			frame_count++;

			filenames_p = realloc(filenames,
							(frame_count + 1) * sizeof(char *));

			if(filenames_p == NULL){
				fprintf(
					stderr,
					"Error reallocating memory for frames path\n"
				);
				fclose(out);
				destroy_filenames(filenames,frame_count);
				free(filenames);
				if(!input_folder) free(input_dir);
				return -1;
			}
			filenames = filenames_p;

			filenames[frame_count] = (char *)malloc(sizeof(char) * 64);
			if(filenames[frame_count] == NULL){
				fprintf(
					stderr,
					"Error allocating memory for frames path\n"
				);
				fclose(out);
				destroy_filenames(filenames,frame_count);
				free(filenames);
				if(!input_folder) free(input_dir);
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

	int aprox_size;
	aprox_size = (args->width*args->height*frame_count)/
				 (args->pixel_size*args->pixel_size*8);

//	aprox_size = aprox_size*(frame_count/8);

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

	if(args->noprogress){
		fprintf(stderr,"Decoding data from images...\n");
	}

	short percent;

	_Bool reverse_x = 0;
	_Bool reverse_y = 0;
	_Bool invert_color = 0;

	short invert_byte;
	if(args->invert_byte)
		invert_byte = 7;
	else
		invert_byte = 0;

	if(args->bmp){
		bmp_t *bmp;
		unsigned char lett;
		for(int i=0;i<frame_count;i++){
			if(args->odd_mode){
				if(!(i%2)){
					reverse_x = 1;
					reverse_y = 1;
					invert_color = 1;
					invert_byte = 0;
				}
				else{
					reverse_x = 0;
					reverse_y = 0;
					invert_color = 0;
					invert_byte = 7;
				}
			}
			else{
				if(args->invert_color) invert_color = 1;
				if(args->reverse_x) reverse_x = 1;
				if(args->reverse_y) reverse_y = 1;
			}
			bmp = bmp_read(filenames[i]);
			int padded_width = bmp_round4(bmp->info_header->width);
			int k = 0;
			int res;
			int eend = 0;

			if(!args->noprogress){
				if(frame_count > 1){
					percent = (100*i)/(frame_count-1);
				}
				else{
					percent = 100;
				}
				fprintf(stderr,"\rDecoding %s | %d%%",filenames[i],percent);
			}

			if(reverse_y){
				for(
					int i = 0;
					i >= bmp->info_header->height && !eend;
					i += args->pixel_size
				){
					if(reverse_x){
						for(
							int j = bmp->info_header->width - args->pixel_size;
							j >= 0 ;
							j -= args->pixel_size
						){
							res = bmp_get_pixel(
										bmp->data,
										padded_width,
										i,j,
										bmp->info_header->width,
										args->pixel_size,
										bmp->info_header->bits,
										args->rmode
									);
							if(res == 127){
								eend = 1;
								break;
							}

							num[abs(invert_byte - k)] = res;
							k++;

							if(k>7){
								lett = set_bin(num,invert_color);

								if(0 == fwrite(&lett,1,sizeof(unsigned char),out)){

									fprintf(stderr,
										"Error writing byte to output file.\n");

									fclose(out);
									destroy_bmp(bmp);
									destroy_filenames(filenames,frame_count);
									free(filenames);
									free(num);
									return -1;
								}
								k=0;
							}
						}
					}
					else{
						for(
							int j = 0;
							j < bmp->info_header->width;
							j += args->pixel_size
						){
							res = bmp_get_pixel(
										bmp->data,
										padded_width,
										i,j,
										bmp->info_header->width,
										args->pixel_size,
										bmp->info_header->bits,
										args->rmode
									);
							if(res == 127){
								eend = 1;
								break;
							}

							num[abs(invert_byte - k)] = res;
							k++;

							if(k>7){
								lett = set_bin(num,invert_color);

								if(0 == fwrite(&lett,1,sizeof(unsigned char),out)){

									fprintf(stderr,
										"Error writing byte to output file.\n");

									fclose(out);
									destroy_bmp(bmp);
									destroy_filenames(filenames,frame_count);
									free(filenames);
									free(num);
									return -1;
								}
								k=0;
							}
						}
					}
				}
			}
			else{
				for(
					int i = bmp->info_header->height - args->pixel_size;
					i >= 0 && !eend;
					i-=args->pixel_size
				){
					if(reverse_x){
						for(
							int j = bmp->info_header->width - args->pixel_size;
							j >= 0 ;
							j -= args->pixel_size
						){
							res = bmp_get_pixel(
										bmp->data,
										padded_width,
										i,j,
										bmp->info_header->width,
										args->pixel_size,
										bmp->info_header->bits,
										args->rmode
									);
							if(res == 127){
								eend = 1;
								break;
							}

							num[abs(invert_byte - k)] = res;
							k++;

							if(k>7){
								lett = set_bin(num,invert_color);

								if(0 == fwrite(&lett,1,sizeof(unsigned char),out)){

									fprintf(stderr,
										"Error writing byte to output file.\n");

									fclose(out);
									destroy_bmp(bmp);
									destroy_filenames(filenames,frame_count);
									free(filenames);
									free(num);
									return -1;
								}
								k=0;
							}
						}
					}
					else{
						for(
							int j = 0;
							j < bmp->info_header->width;
							j += args->pixel_size
						){
							res = bmp_get_pixel(
										bmp->data,
										padded_width,
										i,j,
										bmp->info_header->width,
										args->pixel_size,
										bmp->info_header->bits,
										args->rmode
									);
							if(res == 127){
								eend = 1;
								break;
							}

							num[abs(invert_byte - k)] = res;
							k++;

							if(k>7){
								lett = set_bin(num,invert_color);

								if(0 == fwrite(&lett,1,sizeof(unsigned char),out)){

									fprintf(stderr,
										"Error writing byte to output file.\n");

									fclose(out);
									destroy_bmp(bmp);
									destroy_filenames(filenames,frame_count);
									free(filenames);
									free(num);
									return -1;
								}
								k=0;
							}
						}
					}
				}
			}
			destroy_bmp(bmp);
		}
	}

	else{
		png_structp png;
		png_infop info;
		FILE *frame;
		int eend = 0;

		for(int i=0;i<frame_count && !eend;i++){
			if(args->odd_mode){
				if(!(i%2)){
					reverse_x = 1;
					reverse_y = 1;
					invert_color = 1;
					invert_byte = 0;
				}
				else{
					reverse_x = 0;
					reverse_y = 0;
					invert_color = 0;
					invert_byte = 7;
				}
			}
			else{
				if(args->invert_color) invert_color = 1;
				if(args->reverse_x) reverse_x = 1;
				if(args->reverse_y) reverse_y = 1;
			}

			if(!args->noprogress){
				if(frame_count > 1){
					percent = (100*i)/(frame_count-1);
				}
				else{
					percent = 100;
				}
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
			png = png_create_read_struct(
					PNG_LIBPNG_VER_STRING,
					NULL,NULL,NULL);

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


			if (color_type == PNG_COLOR_TYPE_PALETTE){
				png_set_expand_gray_1_2_4_to_8(png);
			}

			if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8){
				png_set_expand_gray_1_2_4_to_8(png);
			}

			if (png_get_valid(png, info, PNG_INFO_tRNS)){
				png_set_tRNS_to_alpha(png);
			}

			if (color_type == PNG_COLOR_TYPE_RGB ||
				color_type == PNG_COLOR_TYPE_GRAY ||
				color_type == PNG_COLOR_TYPE_PALETTE){
				png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
			}

			if (color_type == PNG_COLOR_TYPE_GRAY ||
				color_type == PNG_COLOR_TYPE_GRAY_ALPHA){
				png_set_gray_to_rgb(png);
			}

			png_read_update_info(png, info);

			png_bytep *row_pointers;
			row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
			if(row_pointers == NULL){
				fprintf(
					stderr,
					"\nError allocating memory for PNG row vector.\n"
				);
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

					fprintf(
						stderr,
						"\nError allocating memory for PNG row pointers.\n"
					);

					fclose(out);
					fclose(frame);
					destroy_filenames(filenames,frame_count);
					free(filenames);
					return -1;
				}
			}

			png_read_image(png, row_pointers);

			int k = 0;
			int res;
			char lett;

			if(reverse_y){
				for(
					int y = height - args->pixel_size;
					y >= 0  && !eend;
					y -= args->pixel_size
				){
					if(reverse_x){
						for(
							int x = width - args->pixel_size;
							x >= 0  && !eend;
							x -= args->pixel_size
						){
							res = png_get_pixel(
									row_pointers,
									x,y,
									args->pixel_size,
									args->rmode
								);

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
					else{
						for(
							int x = 0;
							x < width  && !eend;
							x += args->pixel_size
						){
							res = png_get_pixel(
									row_pointers,
									x,y,
									args->pixel_size,
									args->rmode
								);

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
				}
			}
			else{
				for(
					int y = 0;
					y < height  && !eend;
					y += args->pixel_size
				){
					if(reverse_x){
						for(
							int x = width - args->pixel_size;
							x >= 0  && !eend;
							x -= args->pixel_size
						){
							res = png_get_pixel(
									row_pointers,
									x,y,
									args->pixel_size,
									args->rmode
								);

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
					else{
						for(
							int x = 0;
							x < width  && !eend;
							x += args->pixel_size
						){
							res = png_get_pixel(
									row_pointers,
									x,y,
									args->pixel_size,
									args->rmode
								);

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
				}
			}

			png_destroy_read_struct(&png, &info,NULL);
			png_free_data(png,info,PNG_FREE_ALL,-1);

			fclose(frame);

			for(int i=0;i<args->height;i++){
				free(row_pointers[i]);
			}
			free(row_pointers);
		}
	}

	free(num);

	if(!args->keep_frames){
		if(!args->quiet)
			fprintf(stderr,"Deleting frame files...\n");

		for(int i=0;i<frame_count;i++){
			if(0 != remove(filenames[i])){
				perror("remove");
				fprintf(
					stderr,
					"Error deleting file: %s...",
					filenames[i]
				);
			}
		}

		if(-1 == rmdir(input_dir)){
			perror("rmdir");
			fprintf(stderr,"Error deleting folder: %s...",input_dir);
		}
		else{
			if(!args->quiet)
				printf("Removed directory: %s/\n",input_dir);
		}
	}

	closedir(dir);

	if(!input_folder) free(input_dir);
	destroy_filenames(filenames,frame_count);
	free(filenames);
	printf("\nAll done! Final output size: ");
	print_size(ftell(out));
	fclose(out);

	return 0;
}

int main(int argc, char *argv[]){
	arg_s args;

	args.ffmpeg_path = NULL;
	args.codec = NULL;
	args.skip_ffmpeg = 0;
	args.rmode = 2;

	int return_value;
	return_value = getopts(&args,argc,argv);
	if(return_value != 0)
		return 1;

	if(!args.skip_ffmpeg){
		args.ffmpeg_path = (char *)malloc(sizeof(char) * 32);
		if(args.ffmpeg_path == NULL){

			fprintf(stderr,
				"Error allocating memory for FFMPEG file path.\n");

			destroy_args(&args);
			return 1;
		}
#ifdef __WIN32
		if(0 == check_ffmpeg()){
			fprintf(stderr,"Error. '%s' not found.\n",FFMPEG_EXE_PATH);
		}
#else
		if(0 == check_ffmpeg(args.ffmpeg_path)){
			if(!args.quiet)
				printf(
					"Found FFMPEG binary at: %s\n",
					args.ffmpeg_path
				);
		}
		else{
			fprintf(stderr,"\nFFMPEG not found on your system.\n");
			fprintf(stderr,"Please install it and retry.\n");
		}
#endif
	}

	if(args.operation) decode(&args);
	else encode(&args);
	destroy_args(&args);

	return 0;
}

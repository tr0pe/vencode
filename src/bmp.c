#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct{
	uint16_t type;      // 'BM' tag
	uint32_t size;      // BMP size
	uint16_t notused1;  // reserved
	uint16_t notused2;  // reserved
	uint32_t offset;    // Byte offset of pixel data
}bmp_header;

typedef struct{
	uint32_t size;              // Header size in bytes
	int32_t  width,height;      // Width and height of image
	uint16_t planes;            // Number of colour planes
	uint16_t bits;              // Bits per pixel
	uint32_t compression;       // Compression type
	uint32_t imagesize;         // Image size in bytes
	int32_t  xresolution;       // Pixels per meter
	int32_t  yresolution;       // Pixels per meter
	uint32_t ncolours;          // Number of colours
	uint32_t importantcolours;  // Important colours
}bmp_info;

typedef struct{
	bmp_info *info_header;
	bmp_header *header;
	unsigned char *data;
}bmp_t;

void destroy_bmp(bmp_t *bmp){
	if(bmp->info_header != NULL) free(bmp->info_header);
	if(bmp->header != NULL) free(bmp->header);
	if(bmp->data != NULL) free(bmp->data);
	if(bmp != NULL) free(bmp);
}

int bmp_round4(int x){
	return x % 4 == 0 ? x : x - x % 4 + 4;
}

bmp_t *bmp_read(const char *filename){
	bmp_header *header;
	header = (bmp_header *)malloc(sizeof(bmp_header));

	bmp_info *info_header;
	info_header = (bmp_info *)malloc(sizeof(bmp_info));

	FILE *bmp_input = fopen(filename,"rb");
	if(bmp_input == NULL) {
		perror(filename);
		return NULL;
	}

	fread(&header->type,    1,sizeof(uint16_t),bmp_input);
	fread(&header->size,    1,sizeof(uint32_t),bmp_input);
	fread(&header->notused1,1,sizeof(uint16_t),bmp_input);
	fread(&header->notused2,1,sizeof(uint16_t),bmp_input);
	fread(&header->offset,  1,sizeof(uint32_t),bmp_input);

	if(fread(info_header,sizeof(bmp_info),1,bmp_input) != 1) {
		fprintf(stderr,"Failed to read BMP info header\n");
		return NULL;
	}

	bmp_t *bmp = (bmp_t *)malloc(sizeof(unsigned char) * header->size);
	bmp->header = header;
	bmp->info_header = info_header;

	int psize = bmp->header->size - bmp->info_header->size;
	bmp->data = (unsigned char *)malloc(bmp->header->size);

	fseek(bmp_input,header->offset,SEEK_SET);
	fread(bmp->data,psize,sizeof(unsigned char),bmp_input);

	fclose(bmp_input);
	return bmp;
}

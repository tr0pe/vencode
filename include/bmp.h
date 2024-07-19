#ifndef __bmp_h
#define __bmp_h
#include <stdint.h>
typedef struct {
	uint16_t type;      // 'BM' tag
	uint32_t size;      // BMP size
	uint16_t notused1;  // reserved
	uint16_t notused2;  // reserved
	uint32_t offset;    // Byte offset of pixel data
}bmp_header;

typedef struct {
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

void destroy_bmp(bmp_t *bmp);
void bmp_check(bmp_t *bmp);
bmp_t *bmp_read(const char *filename);
int bmp_round4(int x);

#endif

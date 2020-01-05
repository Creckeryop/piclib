//code is written by creckeryop (some things are taken from xerpi's vita2d thanks to him (https://github.com/xerpi))
#include "piclib.h"
#include <stdio.h>
#include <vitasdk.h>
#include <jpeglib.h>
#include <png.h>
#include <stdlib.h>
#define MAX_TEXTURE_SIZE 4096 //GXM Limit

#define PNG_SIGSIZE (8)

static void read_png_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	sceIoRead(*(SceUID *)png_get_io_ptr(png_ptr), data, length);
}

vita2d_texture *load_PNG_file_part(const char *filename, int x, int y, int width, int height)
{
	if (x < 0 || y < 0 || width < 0 || height < 0)
	{
		return NULL;
	}
	png_byte pngsig[PNG_SIGSIZE];
	SceUID fd;

	if ((fd = sceIoOpen(filename, SCE_O_RDONLY, 0777)) < 0)
	{
		return NULL;
	}

	if (sceIoRead(fd, pngsig, PNG_SIGSIZE) != PNG_SIGSIZE)
	{
		sceIoClose(fd);
		return NULL;
	}

	if (png_sig_cmp(pngsig, 0, PNG_SIGSIZE) != 0)
	{
		sceIoClose(fd);
		return NULL;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
	{
		return NULL;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	png_set_read_fn(png_ptr, (png_voidp)&fd, read_png_fn);
	png_set_sig_bytes(png_ptr, PNG_SIGSIZE);
	png_read_info(png_ptr, info_ptr);

	unsigned int width_info, height_info;
	int bit_depth, color_type;

	png_get_IHDR(png_ptr, info_ptr, &width_info, &height_info, &bit_depth, &color_type, NULL, NULL, NULL);
	if (x > width_info || y > height_info || x + width > width_info || y + height > height_info)
	{
		return NULL;
	}
	if ((color_type == PNG_COLOR_TYPE_PALETTE && bit_depth <= 8) || (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) || png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) || (bit_depth == 16))
	{
		png_set_expand(png_ptr);
	}

	if (bit_depth == 16)
		png_set_scale_16(png_ptr);
	if (bit_depth == 8 && color_type == PNG_COLOR_TYPE_RGB)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	switch (color_type)
	{
	case PNG_COLOR_TYPE_RGB:
		break;
	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		png_set_gray_to_rgb(png_ptr);
		break;
	case PNG_COLOR_TYPE_PALETTE:
		png_set_palette_to_rgb(png_ptr);
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
		break;
	}

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (bit_depth < 8)
		png_set_packing(png_ptr);

	png_read_update_info(png_ptr, info_ptr);
	float scaleWidth = (float)width / MAX_TEXTURE_SIZE;
	float scaleHeight = (float)height / MAX_TEXTURE_SIZE;
	float scale = scaleWidth > scaleHeight ? scaleWidth : scaleHeight;

	if (scale <= 1.f)
	{
		scale = 1;
	}
	else if (scale <= 2.f)
	{
		scale = 2;
	}
	else if (scale <= 4.f)
	{
		scale = 4;
	}
	else if (scale <= 8.f)
	{
		scale = 8;
	}
	else
	{
		return NULL;
	}
	vita2d_texture *texture = vita2d_create_empty_texture(width / scale, height / scale);

	if (!texture)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	unsigned int *texture_data = vita2d_texture_get_datap(texture);
	unsigned int *trash = malloc(width_info << 2);
	unsigned long skip = (vita2d_texture_get_stride(texture) >> 2) - width / scale;
	for (int i = 0; i < y; ++i)
	{
		png_read_row(png_ptr, (png_bytep)trash, NULL);
	}

	for (int i = 0; i < (int)(height / scale); ++i)
	{
		for (int k = 0; k < scale; ++k)
		{
			png_read_row(png_ptr, (png_bytep)trash, NULL);
		}
		unsigned int *row_ptr = trash + x;
		for (int j = 0; j < width / scale; ++j)
		{
			*texture_data++ = *row_ptr;
			row_ptr += (int)scale;
		}
		texture_data += skip;
	}

	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
	free(trash);
	sceIoClose(fd);

	return texture;
}

vita2d_texture *load_JPEG_file_part(const char *filename, int x, int y, int width, int height)
{
	if (x < 0 || y < 0 || width < 0 || height < 0)
	{
		return NULL;
	}
	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	if (fd <= 0)
	{
		return NULL;
	}
	uint32_t size = sceIoLseek(fd, 0, SEEK_END);
	sceIoLseek(fd, 0, SEEK_SET);
	void *buffer = malloc(sizeof(unsigned int) * size);
	sceIoRead(fd, buffer, size);
	sceIoClose(fd);

	unsigned int magic = *(unsigned int *)buffer;
	if (magic != 0xE0FFD8FF && magic != 0xE1FFD8FF)
	{
		return NULL;
	}
	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;
	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_mem_src(&jinfo, buffer, size);
	jpeg_read_header(&jinfo, 1);
	if (x > jinfo.image_width || y > jinfo.image_height || x + width > jinfo.image_width || y + height > jinfo.image_height)
	{
		return NULL;
	}

	float scaleWidth = (float)width / MAX_TEXTURE_SIZE;
	float scaleHeight = (float)height / MAX_TEXTURE_SIZE;
	float scale = scaleWidth > scaleHeight ? scaleWidth : scaleHeight;

	if (scale <= 1.f)
	{
		jinfo.scale_denom = 1;
	}
	else if (scale <= 2.f)
	{
		jinfo.scale_denom = 2;
	}
	else if (scale <= 4.f)
	{
		jinfo.scale_denom = 4;
	}
	else if (scale <= 8.f)
	{
		jinfo.scale_denom = 8;
	}
	else
	{
		return NULL;
	}

	width /= jinfo.scale_denom;
	height /= jinfo.scale_denom;
	x /= jinfo.scale_denom;
	y /= jinfo.scale_denom;

	jpeg_start_decompress(&jinfo);

	vita2d_texture *texture = vita2d_create_empty_texture_format(width, height, jinfo.out_color_space == JCS_GRAYSCALE ? SCE_GXM_TEXTURE_FORMAT_U8_R111 : SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR);
	if (!texture)
	{
		jpeg_abort_decompress(&jinfo);
		return NULL;
	}

	JSAMPROW row_pointer[1];
	*row_pointer = (unsigned char *)malloc(jinfo.output_width * jinfo.num_components);
	unsigned char *pointer = (unsigned char *)vita2d_texture_get_datap(texture);
	jpeg_skip_scanlines(&jinfo, y);
	unsigned int stride = width * jinfo.num_components;
	unsigned int skip = vita2d_texture_get_stride(texture) - stride;
	unsigned int x_start = x * jinfo.num_components;
	unsigned int x_end = x_start + stride;
	for (int l = 0; l < height; ++l)
	{
		jpeg_read_scanlines(&jinfo, row_pointer, 1);
		for (int i = x_start; i < x_end;)
			*pointer++ = (*row_pointer)[i++];
		pointer += skip;
	}
	jpeg_skip_scanlines(&jinfo, jinfo.output_height - y - height);
	free(*row_pointer);
	jpeg_finish_decompress(&jinfo);
	jpeg_destroy_decompress(&jinfo);
	return texture;
}

vita2d_texture *load_BMP_file_part(const char *filename, int x, int y, int width, int height)
{
	if (x < 0 || y < 0 || width < 0 || height < 0)
	{
		return NULL;
	}
	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	if (fd <= 0)
	{
		return NULL;
	}
	short magic = 0;
	sceIoRead(fd, (void *)&magic, 2);
	if (magic != 0x4D42)
	{
		sceIoClose(fd);
		return NULL;
	}
	unsigned int fSize, offset, dib_size, info_width, info_height;
	short bits;
	sceIoRead(fd, (void *)&fSize, 4);
	sceIoLseek(fd, 4, SEEK_CUR);
	sceIoRead(fd, (void *)&offset, 4);
	sceIoRead(fd, (void *)&dib_size, 4);
	sceIoRead(fd, (void *)&info_width, 4);
	sceIoRead(fd, (void *)&info_height, 4);
	if (x > info_width || y > info_height || x + width > info_width || y + height > info_height)
	{
		sceIoClose(fd);
		return NULL;
	}
	sceIoLseek(fd, 2, SEEK_CUR);
	sceIoRead(fd, (void *)&bits, 2);
	if (bits < 16 && bits > 32)
	{
		sceIoClose(fd);
		return NULL;
	}
	sceIoLseek(fd, dib_size - 16, SEEK_CUR);
	float scaleWidth = (float)width / MAX_TEXTURE_SIZE;
	float scaleHeight = (float)height / MAX_TEXTURE_SIZE;
	float scale = scaleWidth > scaleHeight ? scaleWidth : scaleHeight;
	int shift;
	if (scale <= 1.f)
	{
		shift = 0;
	}
	else if (scale <= 2.f)
	{
		shift = 1;
	}
	else if (scale <= 4.f)
	{
		shift = 2;
	}
	else if (scale <= 8.f)
	{
		shift = 3;
	}
	else
	{
		return NULL;
	}
	width >>= shift;
	height >>= shift;
	x >>= shift;
	y >>= shift;
	vita2d_texture *texture = vita2d_create_empty_texture(width, height);

	if (!texture)
	{
		sceIoClose(fd);
		return NULL;
	}
	short bytes = bits >> 3;
	unsigned int row_stride = (bytes * info_width + 3) & ~3;
	unsigned char *row = malloc(row_stride), *row_ptr;
	unsigned int stride = vita2d_texture_get_stride(texture) >> 2;
	unsigned int *texture_data = (unsigned int *)vita2d_texture_get_datap(texture) + stride * (height - 1);
	unsigned int x_start = x * bytes << shift;
	unsigned int skip_rows = row_stride * ((1 << shift) - 1);
	sceIoLseek(fd, (info_height - ((height + y) << shift)) * row_stride, SEEK_CUR);
	for (int i = 0; i < height; ++i)
	{
		sceIoLseek(fd, skip_rows, SEEK_CUR);
		sceIoRead(fd, row, row_stride);
		row_ptr = row + x_start;
		for (int j = 0; j < width; j++)
		{
			unsigned int color;
			memcpy(&color, row_ptr += bytes << shift, bytes);
			if (bytes == 2)
				*texture_data++ = (int)((color & 0x1F) * 255.f / 31) << 16 | (int)((color >> 5 & 0x3F) * 255.f / 63) << 8 | (int)((color >> 11 & 0x1F) * 255.f / 31) | 0xFF000000;
			else if (bytes == 3)
				*texture_data++ = color | 0xFF000000;
			else if (bytes == 4)
				*texture_data++ = (color & 0xFF00FF00) | ((color & 0xFF) << 16) | (color >> 16 & 0xFF);
		}
		texture_data -= width + stride;
	}
	sceIoClose(fd);
	free(row);
	return texture;
}

void get_PNG_resolution(const char *filename, int *dest_width, int *dest_height)
{
	SceUID fd;

	if ((fd = sceIoOpen(filename, SCE_O_RDONLY, 0777)) < 0)
	{
		return;
	}
	png_byte pngsig[PNG_SIGSIZE];
	if (sceIoRead(fd, pngsig, PNG_SIGSIZE) != PNG_SIGSIZE)
	{
		sceIoClose(fd);
		return;
	}

	if (png_sig_cmp(pngsig, 0, PNG_SIGSIZE) != 0)
	{
		sceIoClose(fd);
		return;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
	{
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return;
	}

	png_set_read_fn(png_ptr, (png_voidp)&fd, read_png_fn);
	png_set_sig_bytes(png_ptr, PNG_SIGSIZE);
	png_read_info(png_ptr, info_ptr);

	int bit_depth, color_type;
	unsigned int dw, dh;
	png_get_IHDR(png_ptr, info_ptr, &dw, &dh, &bit_depth, &color_type, NULL, NULL, NULL);
	*dest_width = dw;
	*dest_height = dh;
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
}
void get_JPEG_resolution(const char *filename, int *dest_width, int *dest_height)
{
	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	if (fd <= 0)
	{
		return;
	}
	uint32_t size = sceIoLseek(fd, 0, SEEK_END);
	sceIoLseek(fd, 0, SEEK_SET);
	void *buffer = malloc(sizeof(unsigned int) * size);
	sceIoRead(fd, buffer, size);
	sceIoClose(fd);

	unsigned int magic = *(unsigned int *)buffer;
	if (magic != 0xE0FFD8FF && magic != 0xE1FFD8FF)
	{
		return;
	}
	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;
	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_mem_src(&jinfo, buffer, size);
	jpeg_read_header(&jinfo, 1);
	*dest_width = jinfo.image_width;
	*dest_height = jinfo.image_height;
	jpeg_destroy_decompress(&jinfo);
}
void get_BMP_resolution(const char *filename, int *dest_width, int *dest_height)
{
	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	if (fd <= 0)
	{
		return;
	}
	short magic = 0;
	sceIoRead(fd, (void *)&magic, 2);
	if (magic != 0x4D42)
	{
		sceIoClose(fd);
		return;
	}
	sceIoLseek(fd, 16, SEEK_CUR);
	sceIoRead(fd, (void *)dest_width, 4);
	sceIoRead(fd, (void *)dest_height, 4);
	sceIoClose(fd);
}

void get_PIC_resolution(const char *filename, int *dest_width, int *dest_height)
{
	SceUID file = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	uint16_t magic;
	sceIoRead(file, &magic, 2);
	sceIoClose(file);
	if (magic == 0x4D42)
		get_BMP_resolution(filename, dest_width, dest_height);
	else if (magic == 0xD8FF)
		get_JPEG_resolution(filename, dest_width, dest_height);
	else if (magic == 0x5089)
		get_PNG_resolution(filename, dest_width, dest_height);
}

vita2d_texture *load_PIC_file(const char *filename, int x, int y, int width, int height)
{
	SceUID file = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	uint16_t magic;
	sceIoRead(file, &magic, 2);
	sceIoClose(file);
	if (magic == 0x4D42)
		return load_BMP_file_part(filename, x, y, width, height);
	else if (magic == 0xD8FF)
		return load_JPEG_file_part(filename, x, y, width, height);
	else if (magic == 0x5089)
		return load_PNG_file_part(filename, x, y, width, height);
	return NULL;
}
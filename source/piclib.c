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
		sceIoClose(fd);
		return NULL;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		sceIoClose(fd);
		png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		sceIoClose(fd);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	png_set_read_fn(png_ptr, (png_voidp)&fd, read_png_fn);
	png_set_sig_bytes(png_ptr, PNG_SIGSIZE);
	png_read_info(png_ptr, info_ptr);

	unsigned int width_info, height_info;
	int bit_depth, color_type;
	int interlace;

	png_get_IHDR(png_ptr, info_ptr, &width_info, &height_info, &bit_depth,
				 &color_type, &interlace, NULL, NULL);

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (color_type == PNG_COLOR_TYPE_RGB ||
		color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

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
		sceIoClose(fd);
		return NULL;
	}
	vita2d_texture *texture = vita2d_create_empty_texture(width / scale, height / scale);

	if (!texture)
	{
		sceIoClose(fd);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	unsigned int *texture_data = vita2d_texture_get_datap(texture);

	unsigned int *trash = malloc(width_info << 2);
	int stride = vita2d_texture_get_stride(texture) >> 2;

	if (interlace == PNG_INTERLACE_ADAM7)
	{
		int s = scale;
		int output_height = height / s;
		int output_width = width / s;
		for (int pass = 0; pass < 7; ++pass)
		{
			int x_start = PNG_PASS_START_COL(pass);
			int y_start = PNG_PASS_START_ROW(pass);
			int x_step = 1 << PNG_PASS_COL_SHIFT(pass);
			int y_step = 1 << PNG_PASS_ROW_SHIFT(pass);
			int y_in = 0;

			for (int y_out = y_start; y_out < height_info; y_out += y_step)
			{
				png_read_row(png_ptr, (png_bytep)trash, NULL);
				if (y_out % s == 0 && y_out >= y && y_out - y < output_height)
				{
					int x_in = 0;
					for (int x_out = x_start; x_out < width_info; x_out += x_step, x_in++)
					{
						if (x_out % s == 0 && x_out / s >= x && x_out / s - x < output_width)
						{
							texture_data[(x_out / s - x) + (y_out / s - y) * stride] = trash[x_in];
						}
					}
				}
			}
		}
	}
	else
	{
		unsigned long skip = stride - width / scale;
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
	}
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
	free(trash);
	sceIoClose(fd);

	return texture;
}

vita2d_texture *load_PNG_file_downscaled(const char *filename, int level)
{
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
		sceIoClose(fd);
		return NULL;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		sceIoClose(fd);
		png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		sceIoClose(fd);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	png_set_read_fn(png_ptr, (png_voidp)&fd, read_png_fn);
	png_set_sig_bytes(png_ptr, PNG_SIGSIZE);
	png_read_info(png_ptr, info_ptr);

	unsigned int width_info, height_info;
	int bit_depth, color_type;
	int interlace;

	png_get_IHDR(png_ptr, info_ptr, &width_info, &height_info, &bit_depth,
				 &color_type, &interlace, NULL, NULL);

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (color_type == PNG_COLOR_TYPE_RGB ||
		color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);
	float scaleWidth = (float)width_info / MAX_TEXTURE_SIZE;
	float scaleHeight = (float)height_info / MAX_TEXTURE_SIZE;
	float scale = scaleWidth > scaleHeight ? scaleWidth : scaleHeight;
	scale = scale > level ? scale : level;

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
		sceIoClose(fd);
		return NULL;
	}
	vita2d_texture *texture = vita2d_create_empty_texture(width_info / scale, height_info / scale);

	if (!texture)
	{
		sceIoClose(fd);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	unsigned int *texture_data = vita2d_texture_get_datap(texture);
	unsigned int *trash = malloc(width_info << 2);
	int stride = vita2d_texture_get_stride(texture) >> 2;

	if (interlace == PNG_INTERLACE_ADAM7)
	{
		int s = scale;
		int output_height = height_info / s;
		int output_width = width_info / s;
		for (int pass = 0; pass < 7; ++pass)
		{
			int x_start = PNG_PASS_START_COL(pass);
			int y_start = PNG_PASS_START_ROW(pass);
			int x_step = 1 << PNG_PASS_COL_SHIFT(pass);
			int y_step = 1 << PNG_PASS_ROW_SHIFT(pass);
			int y_in = 0;

			for (int y_out = y_start; y_out < height_info; y_out += y_step)
			{
				png_read_row(png_ptr, (png_bytep)trash, NULL);
				if (y_out % s == 0 && y_out < output_height)
				{
					int x_in = 0;
					for (int x_out = x_start; x_out < width_info; x_out += x_step, x_in++)
					{
						if (x_out % s == 0 && x_out / s < output_width)
						{
							texture_data[x_out / s + y_out / s * stride] = trash[x_in];
						}
					}
				}
			}
		}
	}
	else
	{
		unsigned long skip = stride - width_info / scale;
		for (int i = 0; i < (int)(height_info / scale); ++i)
		{
			for (int k = 0; k < scale; ++k)
			{
				png_read_row(png_ptr, (png_bytep)trash, NULL);
			}
			unsigned int *row_ptr = trash;
			for (int j = 0; j < width_info / scale; ++j)
			{
				*texture_data++ = *row_ptr;
				row_ptr += (int)scale;
			}
			texture_data += skip;
		}
	}
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
	free(trash);
	sceIoClose(fd);

	return texture;
}

typedef struct
{
	struct jpeg_source_mgr pub;
	SceUID fd;
	JOCTET *buffer;
	boolean start_of_file;
} sce_io_src_mgr;

typedef sce_io_src_mgr *sce_io_src_ptr;

static void init_source(j_decompress_ptr cinfo)
{
	sce_io_src_ptr src = (sce_io_src_ptr)cinfo->src;
	src->start_of_file = TRUE;
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
	sce_io_src_ptr src = (sce_io_src_ptr)cinfo->src;

	size_t nbytes;
	src->start_of_file = FALSE;
	nbytes = (size_t)sceIoRead(src->fd, src->buffer, 4096);
	if (nbytes <= 0)
	{
		src->buffer[0] = (JOCTET)0xFF;
		src->buffer[1] = (JOCTET)JPEG_EOI;
		nbytes = 2;
	}
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	return TRUE;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	struct jpeg_source_mgr *src = cinfo->src;
	if (num_bytes > 0)
	{
		while (num_bytes > (long)src->bytes_in_buffer)
		{
			num_bytes -= (long)src->bytes_in_buffer;
			(void)(*src->fill_input_buffer)(cinfo);
		}
		src->next_input_byte += (size_t)num_bytes;
		src->bytes_in_buffer -= (size_t)num_bytes;
	}
}

static void term_source(j_decompress_ptr cinfo)
{
}

static void jpeg_vita_src(j_decompress_ptr cinfo, SceUID infile)
{
	sce_io_src_ptr src;
	if (cinfo->src == NULL)
	{
		cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
																		  (size_t)sizeof(sce_io_src_mgr));
		src = (sce_io_src_ptr)cinfo->src;
		src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
														   4096 * ((size_t)sizeof(JOCTET)));
	}

	src = (sce_io_src_ptr)cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->pub.term_source = term_source;
	src->fd = infile;
	src->pub.bytes_in_buffer = 0;	 /* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL; /* until buffer loaded */
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
	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;
	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_vita_src(&jinfo, fd);
	jpeg_read_header(&jinfo, 1);
	if (x > jinfo.image_width || y > jinfo.image_height || x + width > jinfo.image_width || y + height > jinfo.image_height)
	{
		sceIoClose(fd);
		jpeg_destroy_decompress(&jinfo);
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
		sceIoClose(fd);
		jpeg_destroy_decompress(&jinfo);
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
		jpeg_destroy_decompress(&jinfo);
		sceIoClose(fd);
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
	jpeg_finish_decompress(&jinfo);
	jpeg_destroy_decompress(&jinfo);
	free(*row_pointer);
	sceIoClose(fd);
	return texture;
}

vita2d_texture *load_JPEG_file_downscaled(const char *filename, int level)
{
	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	if (fd <= 0)
	{
		return NULL;
	}
	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;
	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_vita_src(&jinfo, fd);
	jpeg_read_header(&jinfo, 1);

	float scaleWidth = (float)jinfo.image_width / MAX_TEXTURE_SIZE;
	float scaleHeight = (float)jinfo.image_height / MAX_TEXTURE_SIZE;
	float scale = scaleWidth > scaleHeight ? scaleWidth : scaleHeight;
	scale = scale > level ? scale : level;

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
		sceIoClose(fd);
		jpeg_destroy_decompress(&jinfo);
		return NULL;
	}

	jpeg_start_decompress(&jinfo);

	vita2d_texture *texture = vita2d_create_empty_texture_format(jinfo.output_width, jinfo.output_height, jinfo.out_color_space == JCS_GRAYSCALE ? SCE_GXM_TEXTURE_FORMAT_U8_R111 : SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR);
	if (!texture)
	{
		jpeg_destroy_decompress(&jinfo);
		sceIoClose(fd);
		return NULL;
	}

	JSAMPROW row_pointer[1];
	*row_pointer = (unsigned char *)malloc(jinfo.output_width * jinfo.num_components);
	unsigned char *pointer = (unsigned char *)vita2d_texture_get_datap(texture);
	unsigned int stride = jinfo.output_width * jinfo.num_components;
	unsigned int skip = vita2d_texture_get_stride(texture) - stride;
	for (int l = 0; l < jinfo.output_height; ++l)
	{
		jpeg_read_scanlines(&jinfo, row_pointer, 1);
		for (int i = 0; i < stride;)
			*pointer++ = (*row_pointer)[i++];
		pointer += skip;
	}
	jpeg_finish_decompress(&jinfo);
	jpeg_destroy_decompress(&jinfo);
	free(*row_pointer);
	sceIoClose(fd);
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

vita2d_texture *load_BMP_file_downscaled(const char *filename, int level)
{
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
	sceIoLseek(fd, 2, SEEK_CUR);
	sceIoRead(fd, (void *)&bits, 2);
	if (bits < 16 && bits > 32)
	{
		sceIoClose(fd);
		return NULL;
	}
	sceIoLseek(fd, dib_size - 16, SEEK_CUR);
	float scaleWidth = (float)info_width / MAX_TEXTURE_SIZE;
	float scaleHeight = (float)info_height / MAX_TEXTURE_SIZE;
	float scale = scaleWidth > scaleHeight ? scaleWidth : scaleHeight;
	scale = scale > level ? scale : level;
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
	info_width >>= shift;
	info_height >>= shift;
	vita2d_texture *texture = vita2d_create_empty_texture(info_width, info_height);

	if (!texture)
	{
		sceIoClose(fd);
		return NULL;
	}
	short bytes = bits >> 3;
	unsigned int row_stride = (bytes * info_width + 3) & ~3;
	unsigned char *row = malloc(row_stride), *row_ptr;
	unsigned int stride = vita2d_texture_get_stride(texture) >> 2;
	unsigned int *texture_data = (unsigned int *)vita2d_texture_get_datap(texture) + stride * (info_height - 1);
	unsigned int x_start = 0 * bytes << shift;
	unsigned int skip_rows = row_stride * ((1 << shift) - 1);
	sceIoLseek(fd, (info_height - (info_height << shift)) * row_stride, SEEK_CUR);
	for (int i = 0; i < info_height; ++i)
	{
		sceIoLseek(fd, skip_rows, SEEK_CUR);
		sceIoRead(fd, row, row_stride);
		row_ptr = row + x_start;
		for (int j = 0; j < info_width; j++)
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
		texture_data -= info_width + stride;
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
		sceIoClose(fd);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		sceIoClose(fd);
		png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		sceIoClose(fd);
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
	sceIoClose(fd);
}

void get_JPEG_resolution(const char *filename, int *dest_width, int *dest_height)
{
	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	if (fd <= 0)
	{
		return;
	}
	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;
	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_vita_src(&jinfo, fd);
	jpeg_read_header(&jinfo, 1);
	*dest_width = jinfo.image_width;
	*dest_height = jinfo.image_height;
	jpeg_destroy_decompress(&jinfo);
	sceIoClose(fd);
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

vita2d_texture *load_PIC_file_downscaled(const char *filename, int level)
{
	SceUID file = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	uint16_t magic;
	sceIoRead(file, &magic, 2);
	sceIoClose(file);
	if (magic == 0x4D42)
		return load_BMP_file_downscaled(filename, level);
	else if (magic == 0xD8FF)
		return load_JPEG_file_downscaled(filename, level);
	else if (magic == 0x5089)
		return load_PNG_file_downscaled(filename, level);
	return NULL;
}
//code is written by creckeryop
#ifndef PICLIB_H
#define PICLIB_H

#include <vita2d.h>

vita2d_texture *load_PIC_file(const char *filename, int x, int y, int width, int height);
vita2d_texture *load_PIC_file_downscaled(const char *filename, int level);
void get_PIC_resolution(const char *filename, int *dest_width, int *dest_height);

#endif
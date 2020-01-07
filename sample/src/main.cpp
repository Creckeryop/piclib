#include <vitasdk.h>
#include <iostream>
#include <vita2d.h>
extern "C"
{
    #include "piclib/piclib.h"
}
int main()
{
    while (true)
    {
        vita2d_init_advanced(0x800000);
        int width = 0, height = 0;
        get_PIC_resolution("app0:image.jpg", &width, &height);
        vita2d_texture *textureModed = load_PIC_file("app0:image.jpg", 0, 0, 4096, 4096);
        vita2d_texture *textureStandart = vita2d_load_JPEG_file("app0:image.jpg");
        for (;;)
        {
            vita2d_start_drawing();
            vita2d_clear_screen();
            /*
            So why piclib is needed to me?.
            I do use it in lpp-vita because i tried to load manga page for example 500x32000 (Web-manga)
            and standart load_JPEG_file function divided it in 8 times so i saw 63x4000 picture, yes
            it's smaller than limit 4096x4096, but 63 pixels? you won't see any text in here. So 
            i decided to create piclib function to have an ability to load part of image.
            After i created it, i just croped 500x32000 on 8 parts 500x4000 and rendered sequential
            and then text was visible in normal resolution.
            In this sample i divide all stats because standart function downscales picture 
            in 2 times, so you can see here if you have an ability to load custom part of image
            you can see picture more detalised. 
            */
            vita2d_draw_texture_part(textureStandart, 0, 0, 2800/2, 2750/2, 480/2,544/2);
            vita2d_draw_texture_part(textureModed, 480, 0, 2800, 2750,480,544);
            vita2d_end_drawing();
            vita2d_swap_buffers();
        }
        vita2d_fini();
        sceKernelExitProcess(0);
        return 0;
    }
}
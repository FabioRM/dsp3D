/* SPDX-License-Identifier: BSD-3-Clause
 * Native reference renderer and benchmark, independent of a windowing library. */
#include "dsp3D.h"
#include "metamask.h"
#include "suzanne.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static void le32(FILE *f,unsigned x){for(int i=0;i<4;i++)fputc((int)((x>>(i*8))&255),f);}
static int bmp(const char *path,const uint16_t *pixels,unsigned w,unsigned h){
    FILE *f=fopen(path,"wb");if(!f)return 0;
    unsigned row=(w*3+3)&~3u;fputs("BM",f);le32(f,54+row*h);le32(f,0);le32(f,54);le32(f,40);le32(f,w);le32(f,h);
    fputc(1,f);fputc(0,f);fputc(24,f);fputc(0,f);le32(f,0);le32(f,row*h);le32(f,2835);le32(f,2835);le32(f,0);le32(f,0);
    for(unsigned y=h;y>0;y--){
        for(unsigned x=0;x<w;x++){uint16_t c=pixels[(y-1)*w+x];fputc((c&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc(((c>>11)&31)*255/31,f);}
        for(unsigned p=w*3;p<row;p++)fputc(0,f);
    }return fclose(f)==0;
}
int main(int argc,char **argv){
    if(argc>1&&!strcmp(argv[1],"--benchmark")){
        puts("backend,width,height,model,mode,frames,mean_render_ms,mean_clear_ms");
        for(unsigned res=0;res<2;res++){
            unsigned w=res?800:480,h=res?480:272;size_t count=(size_t)w*h;
            uint16_t *pixels=malloc(count*2);float *depth=malloc(count*4);dsp3d_cached_vertex *scratch=malloc(4096*sizeof(*scratch));dsp3d_context c;
            if(dsp3d_init(&c,w,h,pixels,depth,scratch,4096)!=DSP3D_OK)return 1;
            for(unsigned mesh=0;mesh<2;mesh++)for(int mode=0;mode<4;mode++){
                clock_t render=0,clear=0;
                for(unsigned frame=0;frame<120;frame++){
                    dsp3d_transform t={{0.2f,frame*0.025f,0},{0,0,0},1.3f};clock_t a=clock();dsp3d_begin_frame(&c,0);clock_t b=clock();
                    if(dsp3d_draw(&c,mesh?&suzanne:&metamask,&t,(dsp3d_mode)mode)!=DSP3D_OK)return 2;
                    clock_t d=clock();clear+=b-a;render+=d-b;
                }
                printf("host,%u,%u,%s,%d,120,%.4f,%.4f\n",w,h,mesh?"suzanne":"metamask",mode,1000.0*render/CLOCKS_PER_SEC/120,1000.0*clear/CLOCKS_PER_SEC/120);
            }free(pixels);free(depth);free(scratch);
        }return 0;
    }
    if(argc<3){fprintf(stderr,"Usage: dsp3d_preview metamask|suzanne output.bmp [mode 0..3]\n       dsp3d_preview --benchmark\n");return 1;}
    const dsp3d_mesh *mesh=!strcmp(argv[1],"metamask")?&metamask:&suzanne;
    unsigned w=800,h=480;size_t count=(size_t)w*h;
    uint16_t *pixels=malloc(count*2);float *depth=malloc(count*4);dsp3d_cached_vertex *scratch=malloc(4096*sizeof(*scratch));dsp3d_context c;
    if(dsp3d_init(&c,w,h,pixels,depth,scratch,4096)!=DSP3D_OK)return 2;
    dsp3d_transform t={{0.1f,0.3f,0},{0,0,0},1.3f};dsp3d_begin_frame(&c,dsp3d_rgb565(0x0b1423));
    if(dsp3d_draw(&c,mesh,&t,argc>3?(dsp3d_mode)atoi(argv[3]):DSP3D_GOURAUD)!=DSP3D_OK)return 3;
    if(!bmp(argv[2],pixels,w,h))return 4;
    printf("Rendered %u vertices, %u triangles, %u pixels\n",c.stats.transformed_vertices,c.stats.rasterized_triangles,c.stats.shaded_pixels);
    free(pixels);free(depth);free(scratch);return 0;
}

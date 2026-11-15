/* SPDX-License-Identifier: BSD-3-Clause */
#include "gallery.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static uint16_t pixels[400*240];
static float depth[400*240];
static dsp3d_cached_vertex scratch[2048];
static dsp3d_context c;
int main(void) {
    gallery *g=malloc(sizeof(*g));
    assert(g && gallery_init(g)==DSP3D_OK);
    assert(dsp3d_init(&c,400,240,pixels,depth,scratch,2048)==DSP3D_OK);
    puts("demo,pose,pixels,triangles,rejected,crc32");
    for(unsigned i=0;i<GALLERY_COUNT;i++) {
        gallery_select(g,i);
        for(unsigned t=0;t<3;t++) {
            gallery_step(g,i,.75f);
            dsp3d_begin_frame(&c,0);
            dsp3d_transform pose={{.2f,.4f,0},{0,0,0},1};
            assert(gallery_draw(g,&c,i,&pose,i==2 || i==4 ? DSP3D_TEXTURED : DSP3D_GOURAUD)==DSP3D_OK);
            assert(c.stats.shaded_pixels>100);
            printf("%u,%u,%lu,%lu,%lu,%08lx\n",i,t,(unsigned long)c.stats.shaded_pixels,
                   (unsigned long)c.stats.rasterized_triangles,(unsigned long)c.stats.rejected_objects,
                   (unsigned long)dsp3d_crc32(pixels,sizeof(pixels)));
            if(i==9) assert(c.stats.rejected_objects>0);
            char path[80];snprintf(path,sizeof(path),"build/native/gallery-%u-%u.ppm",i,t);
            FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n400 240\n255\n");
            for(unsigned p=0;p<400*240;p++) {
                unsigned rgb=pixels[p];
                fputc((rgb>>11)*255/31,f);fputc(((rgb>>5)&63)*255/63,f);fputc((rgb&31)*255/31,f);
            }
            assert(!fclose(f));
            if(i==5) for(unsigned n=0;n<g->scene.count;n++) if(g->nodes[n].geometry) {
                dsp3d_begin_frame(&c,0);
                assert(dsp3d_draw_matrix(&c,g->nodes[n].geometry,&g->nodes[n].world,DSP3D_GOURAUD)==DSP3D_OK);
                fprintf(stderr,"hierarchy node %u position %.3f %.3f %.3f pixels %lu\n",n,
                        (double)g->nodes[n].world.m[3],(double)g->nodes[n].world.m[7],
                        (double)g->nodes[n].world.m[11],(unsigned long)c.stats.shaded_pixels);
                assert(c.stats.shaded_pixels>0);
            }
        }
    }
    free(g);
}

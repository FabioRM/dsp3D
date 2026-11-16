/* SPDX-License-Identifier: BSD-3-Clause
 * Actual viewer code, using host files in place of the card driver. */
#define DIR HOST_DIR
#include <dirent.h>
#undef DIR
#include <sys/stat.h>
#include <assert.h>
#include <stdlib.h>
#define main firmware_main
#include "../examples/viewer/main.c"
#undef main
uint32_t SystemCoreClock=200000000;
volatile uint32_t board_frame_count,board_render_cycles;
static unsigned fake_media=1,fail_read;
static uint16_t framebuffer[800*480],renderbuffer[800*480];
static float zbuffer[800*480];
static dsp3d_cached_vertex scratch[16384];
static uint8_t model_slots[2][BOARD_MODEL_BYTES],arenas[2][BOARD_WORKSPACE_BYTES];
static uint8_t app_bytes[BOARD_APP_BYTES];
static struct {FIL *owner;FILE *file;} handles[4];
static HOST_DIR *host_directory;
static char host_folder[512];
static void host_path(char out[512],const char *path) {
    assert(!strncmp(path,"0:",2));
    snprintf(out,512,"build/native/sd-fixture%s",path+2);
}
FRESULT f_mount(FATFS *fs,const TCHAR *path,BYTE option) {
    (void)fs;(void)path;(void)option;return fake_media ? FR_OK : FR_NOT_READY;
}
FRESULT f_open(FIL *f,const TCHAR *path,BYTE mode_) {
    (void)mode_;if(!fake_media) return FR_NOT_READY;
    char full[512];host_path(full,path);
    for(unsigned i=0;i<4;i++) if(!handles[i].owner) {
        FILE *fp=fopen(full,"rb");if(!fp) return FR_NO_FILE;
        assert(!fseek(fp,0,SEEK_END));f->obj.objsize=(FSIZE_t)ftell(fp);rewind(fp);
        handles[i].owner=f;handles[i].file=fp;return FR_OK;
    }
    return FR_TOO_MANY_OPEN_FILES;
}
FRESULT f_close(FIL *f) {
    for(unsigned i=0;i<4;i++) if(handles[i].owner==f) {
        fclose(handles[i].file);handles[i].owner=NULL;return FR_OK;
    }
    return FR_INVALID_OBJECT;
}
FRESULT f_read(FIL *f,void *data,UINT size,UINT *got) {
    *got=0;
    if(!fake_media || (fail_read && !--fail_read)) return FR_DISK_ERR;
    for(unsigned i=0;i<4;i++) if(handles[i].owner==f) {
        *got=(UINT)fread(data,1,size,handles[i].file);return ferror(handles[i].file) ? FR_DISK_ERR : FR_OK;
    }
    return FR_INVALID_OBJECT;
}
FRESULT f_lseek(FIL *f,FSIZE_t offset) {
    for(unsigned i=0;i<4;i++) if(handles[i].owner==f)
        return fseek(handles[i].file,(long)offset,SEEK_SET) ? FR_DISK_ERR : FR_OK;
    return FR_INVALID_OBJECT;
}
FRESULT f_opendir(DIR *dir,const TCHAR *path) {
    (void)dir;host_path(host_folder,path);host_directory=opendir(host_folder);
    return host_directory ? FR_OK : FR_NO_PATH;
}
FRESULT f_closedir(DIR *dir) {(void)dir;closedir(host_directory);host_directory=NULL;return FR_OK;}
FRESULT f_readdir(DIR *dir,FILINFO *info) {
    (void)dir;memset(info,0,sizeof(*info));
    struct dirent *entry=readdir(host_directory);if(!entry) return FR_OK;
    snprintf(info->fname,sizeof(info->fname),"%.127s",entry->d_name);
    char full[1024];snprintf(full,sizeof(full),"%s/%s",host_folder,entry->d_name);
    struct stat st;assert(!stat(full,&st));info->fsize=(FSIZE_t)st.st_size;
    info->fattrib=S_ISDIR(st.st_mode) ? AM_DIR : 0;return FR_OK;
}
void board_init(void) {}
void board_renderer_init(dsp3d_context *c) {
    assert(dsp3d_init(c,800,480,framebuffer,zbuffer,scratch,16384)==DSP3D_OK);
}
void board_frame_begin(dsp3d_context *c,uint16_t background) {
    c->color=framebuffer;for(unsigned i=0;i<800*480;i++) framebuffer[i]=background;
}
void board_present(dsp3d_context *c) {(void)c;board_frame_count++;}
void *board_model_buffer(unsigned slot) {return model_slots[slot&1];}
void *board_asset_workspace(unsigned slot) {return arenas[slot&1];}
void *board_app_buffer(void) {return app_bytes;}
uint16_t *board_render_buffer(void) {return renderbuffer;}
void board_log(const char *value) {(void)value;}
int board_read_char(void) {return -1;}
uint32_t board_cycles(void) {static uint32_t n;return ++n;}
void board_panic(uint32_t code) {fprintf(stderr,"panic %lu\n",(unsigned long)code);abort();}
uint32_t HAL_GetTick(void) {return 0;}
int BSP_PB_GetState(unsigned b) {(void)b;return 0;}
int BSP_TS_GetState(TS_StateTypeDef *s) {(void)s;return 1;}
unsigned BSP_SD_IsDetected(void) {return fake_media;}
int board_preferences_save(const void *d,size_t b) {(void)d;(void)b;return 1;}
int board_preferences_load(void *d,size_t b) {(void)d;(void)b;return 0;}
static unsigned find(const char *name) {
    for(unsigned i=0;i<file_count;i++) if(!strcmp(app->files[i].name,name)) return i;
    fprintf(stderr,"missing %s in %s\n",name,directory);abort();
}
int main(void) {
    assert(sizeof(application)<BOARD_APP_BYTES);
    app=board_app_buffer();memset(app,0,sizeof(*app));
    board_renderer_init(&renderer);board_renderer_init(&scene_renderer);renderer.depth=NULL;
    assert(gallery_init(&app->demos)==DSP3D_OK);embedded(0);bind_scene(1);
    scan();assert(browser && file_count>5 && app->files[0].directory);
    for(unsigned i=1;i<file_count;i++) assert(compare_entry(&app->files[i-1],&app->files[i])<=0);
    load(find("suzanne-v2.d3m"));assert(from_sd && loaded.geometry.mesh.vertex_count==505);
    const dsp3d_vertex *previous=loaded.geometry.mesh.vertices;
    scan();load(find("bad.d3m"));assert(loaded.geometry.mesh.vertices==previous);
    scan();fail_read=2;load(find("metamask-v2.d3m"));assert(loaded.geometry.mesh.vertices==previous);fail_read=0;
    scan();load(find("examples"));assert(strstr(directory,"examples"));
    load(find("ribbon.d3s"));assert(scene_active && from_sd);
    scene_player *player=&app->players[loaded_slot];assert(player->scene.count==3 && player->count==1);
    assert(animate_scene(player,1)==DSP3D_OK);
    assert(fabsf(player->deformed[0][17].position.x-player->rest[0][17].position.x)>.05f);
    dsp3d_begin_frame(&scene_renderer,0);
    assert(dsp3d_scene_render(&scene_renderer,&player->scene,DSP3D_TEXTURED)==DSP3D_OK);
    assert(scene_renderer.stats.shaded_pixels>0);
    scan();load(find("badscene.d3s"));assert(scene_active && &app->players[loaded_slot]==player);
    back_folder();assert(!strcmp(directory,"0:/models"));
    fake_media=0;scan();assert(!file_count && strstr(status,"unavailable"));
    fake_media=1;scan();assert(file_count>0);
    embedded(2);ui(300);command('g');ui(300);command('o');ui(300);
    puts("PASS: viewer sorting, folders, extended models, interrupted load rollback, scene catalog, skinned playback and media recovery");
}

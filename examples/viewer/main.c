/* SPDX-License-Identifier: BSD-3-Clause
 * Touch model gallery: embedded assets and read-only microSD models. */
#include "board.h"
#include "metamask.h"
#include "suzanne.h"
#include "gallery.h"
#include "ff.h"
#include "fonts.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef BOARD_F769
#define TOP 64
#define CONTROLS 64
#define STATUS_HEIGHT 40
#define INFO_HEIGHT 32
#define LIST_HEADER 40
#define LIST_ROW 56
#define TAB_FONT Font24
#define CONTROL_FONT Font20
#define BODY_FONT Font16
#define FILE_FONT Font20
#else
#define TOP 30
#define CONTROLS 29
#define STATUS_HEIGHT 17
#define INFO_HEIGHT 17
#define LIST_HEADER 25
#define LIST_ROW 28
#define TAB_FONT Font12
#define CONTROL_FONT Font12
#define BODY_FONT Font12
#define FILE_FONT Font12
#endif
#define BOTTOM (CONTROLS + STATUS_HEIGHT)
#define FILE_LIMIT 128
#define SCENE_MODELS 16
#define SCENE_KEYS 2048
static dsp3d_context renderer, scene_renderer;
typedef struct { char name[128]; uint32_t size; uint8_t directory; } file_entry;
typedef struct {
    dsp3d_asset assets[SCENE_MODELS];
    const dsp3d_vertex *rest[SCENE_MODELS];
    dsp3d_vertex *deformed[SCENE_MODELS];
    const dsp3d_geometry *catalog[SCENE_MODELS];
    dsp3d_node nodes[128];
    dsp3d_track tracks[128];
    dsp3d_keyframe keys[SCENE_KEYS];
    dsp3d_joint_palette palette[64];
    dsp3d_scene scene;
    dsp3d_animation animation;
    float time;
    unsigned count;
} scene_player;
typedef struct {
    gallery demos;
    file_entry files[FILE_LIMIT];
    uint16_t thumbnails[6][32*32];
    uint8_t thumbnail_valid[6];
    scene_player players[2];
} application;
static application *app;
static dsp3d_asset loaded;
static dsp3d_transform transform = {{0, 0, 0}, {0, 0, 0}, 1.05f};
static dsp3d_mode mode = DSP3D_FLAT;
static FATFS filesystem;
static unsigned file_count, page, loaded_slot, browser, autorotate = 1, demo, from_sd;
static unsigned menu, resolution, light_preset, material_preset, pan_gesture, media_present;
static unsigned scene_active;
static char directory[256]="0:/models";
static char model_name[128] = "MetaMask Fox", status[160] = "Embedded gallery ready.";
static const char *const modes[] = {"Flat", "Gouraud", "Wireframe", "Points", "Textured"};
static dsp3d_orbit orbit={{0,0,0},0,.08f,4};
static dsp3d_quality quality;
static unsigned divisor=1, render_width, render_height;
static uint32_t metrics[5];
typedef struct { uint32_t version, demo, mode, resolution, light, material, pan, rotate; } preferences;
volatile uint32_t viewer_command;
static void rect(int x, int y, int w, int h, uint32_t rgb) {
    uint16_t color = dsp3d_rgb565(rgb);
    for (int j = y; j < y + h && j < (int)BOARD_HEIGHT; j++)
        for (int i = x; i < x + w && i < (int)BOARD_WIDTH; i++)
            if (i >= 0 && j >= 0)
                renderer.color[j * BOARD_WIDTH + i] = color;
}
static void text_font(int x, int y, const char *s, uint32_t rgb, const sFONT *font, int right) {
    uint16_t color = dsp3d_rgb565(rgb);
    if (right > (int)BOARD_WIDTH)
        right = BOARD_WIDTH;
    while (*s && x + (int)font->Width <= right) {
        unsigned ch = (unsigned char)*s++;
        if (ch < 32 || ch > 126)
            ch = '?';
        const uint8_t *glyph = font->table + (ch - 32) * font->Height * ((font->Width + 7) / 8);
        unsigned bytes = (font->Width + 7) / 8;
        for (unsigned row = 0; row < font->Height; row++)
            for (unsigned col = 0; col < font->Width; col++) {
                if ((glyph[row * bytes + col / 8] & (0x80u >> (col & 7))) && x + (int)col >= 0 &&
                    y + (int)row >= 0 && y + row < BOARD_HEIGHT)
                    renderer.color[(y + row) * BOARD_WIDTH + x + col] = color;
            }
        x += font->Width;
    }
}
static void text(int x, int y, const char *s, uint32_t rgb) {
    text_font(x, y, s, rgb, &BODY_FONT, BOARD_WIDTH);
}
static void button(unsigned column, unsigned columns, int y, int height, const char *label,
                   uint32_t accent, int active, const sFONT *font) {
    int left = (int)(column * BOARD_WIDTH / columns);
    int right = (int)((column + 1) * BOARD_WIDTH / columns);
#ifdef BOARD_F769
    int x = left + 5, width = right - left - 10;
    rect(x, y + 5, width, height - 10, active ? accent : 0x36516c);
    rect(x + 2, y + 7, width - 4, height - 14, active ? 0x274868 : 0x1c334b);
    unsigned length = (unsigned)strlen(label);
    int text_x = x + (width - (int)(length * font->Width)) / 2;
    text_font(text_x, y + (height - font->Height) / 2, label, accent, font, right - 7);
#else
    (void)active;
    text_font(left + 8, y + (height - font->Height) / 2, label, accent, font, right - 2);
#endif
}
static void status_text(void) {
#ifdef BOARD_F769
    /* Wrap status messages rather than shrinking the font or hiding their ending. */
    const unsigned capacity = (BOARD_WIDTH - 24) / BODY_FONT.Width;
    const char *remaining = status;
    for (unsigned row = 0; row < 2 && *remaining; row++) {
        unsigned count = (unsigned)strlen(remaining);
        if (count > capacity) {
            count = capacity;
            while (count && remaining[count] != ' ')
                count--;
            if (!count)
                count = capacity;
        }
        char line[96];
        memcpy(line, remaining, count);
        line[count] = 0;
        text(12, BOARD_HEIGHT - STATUS_HEIGHT + 3 + (int)row * 18, line, 0xb7cce0);
        remaining += count;
        while (*remaining == ' ')
            remaining++;
    }
#else
    text(6, BOARD_HEIGHT - STATUS_HEIGHT, status, 0xaabdd1);
#endif
}
static unsigned rows(void) {
    unsigned n=(BOARD_HEIGHT-TOP-BOTTOM-LIST_HEADER)/LIST_ROW;
    return n>6 ? 6 : n;
}
static void reset_view(void) {
    orbit=(dsp3d_orbit){{0,0,0},0,.08f,4};
    transform=(dsp3d_transform){{0,0,0},{0,0,0},1.05f};
}
static void embedded(unsigned index) {
    if(index>=GALLERY_COUNT) return;
    demo=index;from_sd=browser=menu=page=scene_active=0;
    gallery_select(&app->demos,index);
    mode=index==1 || index==6 || index==7 || index==8 ? DSP3D_GOURAUD :
         index==2 || index==4 ? DSP3D_TEXTURED : DSP3D_FLAT;
    snprintf(model_name,sizeof(model_name),"%s",gallery_names[index]);
    snprintf(status,sizeof(status),"%s",gallery_descriptions[index]);
    reset_view();
    if(index==4) orbit.pitch=.35f;
    char log[200];snprintf(log,sizeof(log),"DEMO,%u,%s\r\n",index,gallery_names[index]);board_log(log);
}
static int compare_entry(const file_entry *a,const file_entry *b) {
    if(a->directory!=b->directory) return a->directory ? -1 : 1;
    const unsigned char *x=(const unsigned char *)a->name,*y=(const unsigned char *)b->name;
    while(*x && tolower(*x)==tolower(*y)) {x++;y++;}
    return tolower(*x)-tolower(*y);
}
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void thumbnails(void) {
    memset(app->thumbnail_valid,0,sizeof(app->thumbnail_valid));
    for(unsigned row=0;row<rows() && page*rows()+row<file_count;row++) {
        file_entry *e=&app->files[page*rows()+row];
        if(e->directory) continue;
        char path[400];snprintf(path,sizeof(path),"%s/%s",directory,e->name);
        FIL f;UINT got;
        if(f_open(&f,path,FA_READ)!=FR_OK) continue;
        uint8_t header[64],entry[16];
        if(f_read(&f,header,64,&got)==FR_OK && got==64 && !memcmp(header,"DSP3",4) &&
           read32(header+4)==2 && read32(header+56)==32 && read32(header+60)==32 &&
           read32(header+24)<=32 && read32(header+8)==f_size(&f)) {
            unsigned chunks=read32(header+24);
            for(unsigned i=0;i<chunks;i++) {
                if(f_lseek(&f,64+i*16)!=FR_OK || f_read(&f,entry,16,&got)!=FR_OK || got!=16) break;
                uint32_t offset=read32(entry+4);
                if(!memcmp(entry,"THMB",4) && read32(entry+8)==1024 && read32(entry+12)==2 &&
                   offset<=f_size(&f) && f_size(&f)-offset>=2048 && f_lseek(&f,offset)==FR_OK &&
                   f_read(&f,app->thumbnails[row],2048,&got)==FR_OK && got==2048) {
                    app->thumbnail_valid[row]=1;break;
                }
            }
        }
        f_close(&f);
    }
}
static void scan(void) {
    file_count=page=0;browser=1;menu=0;
    memset(app->thumbnail_valid,0,sizeof(app->thumbnail_valid));
    f_mount(NULL,"0:",0);
    FRESULT result=f_mount(&filesystem,"0:",1);
    if(result!=FR_OK) {
        snprintf(status,sizeof(status),"SD unavailable (%u). Insert FAT32 media and tap Refresh.",(unsigned)result);return;
    }
    DIR dir;result=f_opendir(&dir,directory);
    if(result!=FR_OK) {
        snprintf(status,sizeof(status),"Cannot open folder (%u). Back returns to its parent.",(unsigned)result);return;
    }
    FILINFO info;
    while(file_count<FILE_LIMIT && (result=f_readdir(&dir,&info))==FR_OK && info.fname[0]) {
        size_t n=strlen(info.fname);
        if(info.fname[0]=='.' || n>=sizeof(app->files[0].name)) continue;
        unsigned is_dir=(info.fattrib&AM_DIR)!=0;
        if(!is_dir && (n<4 || info.fname[n-4]!='.' || tolower((unsigned char)info.fname[n-3])!='d' ||
           info.fname[n-2]!='3' || (tolower((unsigned char)info.fname[n-1])!='m' &&
                                  tolower((unsigned char)info.fname[n-1])!='s'))) continue;
        file_entry entry={0};memcpy(entry.name,info.fname,n+1);
        entry.directory=(uint8_t)is_dir;entry.size=(uint32_t)info.fsize;
        unsigned i=file_count++;
        while(i && compare_entry(&app->files[i-1],&entry)>0) {app->files[i]=app->files[i-1];i--;}
        app->files[i]=entry;
    }
    f_closedir(&dir);
    snprintf(status,sizeof(status),"%u entries%s | Folders first, sorted A-Z | Tap to open",file_count,
             file_count==FILE_LIMIT ? " (limit reached)" : "");
    thumbnails();
}
static void loading_screen(unsigned percent) {
    board_frame_begin(&renderer,dsp3d_rgb565(0x0b1423));
    char label[60];snprintf(label,sizeof(label),"Loading model... %u%%",percent);
    text(20,BOARD_HEIGHT/2-30,label,0xffffff);
    rect(20,BOARD_HEIGHT/2,BOARD_WIDTH-40,20,0x243f58);
    rect(20,BOARD_HEIGHT/2,(int)((BOARD_WIDTH-40)*percent/100),20,0x58ceaf);
    board_present(&renderer);
}
static FRESULT read_into(const char *path,uint8_t *data,size_t capacity,size_t *bytes) {
    FIL f;FRESULT result=f_open(&f,path,FA_READ);
    *bytes=0;
    if(result!=FR_OK) return result;
    size_t size=f_size(&f);
    if(!size || size>capacity) {f_close(&f);return FR_INVALID_PARAMETER;}
    while(*bytes<size) {
        UINT got,n=size-*bytes>4096 ? 4096 : (UINT)(size-*bytes);
        result=f_read(&f,data+*bytes,n,&got);
        if(result!=FR_OK || got!=n) {result=FR_DISK_ERR;break;}
        *bytes+=got;
    }
    f_close(&f);return result;
}
static void load_scene(const char *path,const char *name) {
    unsigned slot=loaded_slot^1u;
    uint8_t *storage=board_model_buffer(slot);
    size_t scene_size;
    FRESULT fr=read_into(path,storage,BOARD_MODEL_BYTES,&scene_size);
    if(fr!=FR_OK || scene_size<40) {
        snprintf(status,sizeof(status),"Scene read failed (%u). Previous scene retained.",(unsigned)fr);return;
    }
    char manifest_path[400];snprintf(manifest_path,sizeof(manifest_path),"%s",path);
    char *dot=strrchr(manifest_path,'.');if(!dot) return;
    strcpy(dot,".catalog");
    char list[SCENE_MODELS*129+1];size_t list_size;
    fr=read_into(manifest_path,(uint8_t *)list,sizeof(list)-1,&list_size);
    if(fr!=FR_OK) {snprintf(status,sizeof(status),"Scene requires its adjacent .catalog model list.");return;}
    list[list_size]=0;
    char *names[SCENE_MODELS],*cursor=list;unsigned count=0;
    while(*cursor) {
        while(*cursor=='\r' || *cursor=='\n') cursor++;
        if(!*cursor) break;
        if(count==SCENE_MODELS) {snprintf(status,sizeof(status),"Scene exceeds 16 model assets.");return;}
        names[count++]=cursor;
        while(*cursor && *cursor!='\r' && *cursor!='\n') cursor++;
        if(*cursor) *cursor++=0;
    }
    if(!count) {snprintf(status,sizeof(status),"Empty scene catalog.");return;}
    scene_player *player=&app->players[slot];memset(player,0,sizeof(*player));
    size_t offset=(scene_size+31)&~(size_t)31;
    size_t arena_bytes=(BOARD_WORKSPACE_BYTES/count)&~(size_t)31;
    dsp3d_result result=DSP3D_OK;
    for(unsigned i=0;i<count;i++) {
        if(!names[i][0] || strlen(names[i])>127 || strstr(names[i],"..") || strpbrk(names[i],"/\\:")) {
            snprintf(status,sizeof(status),"Invalid scene catalog filename.");return;
        }
        char model_path[400];snprintf(model_path,sizeof(model_path),"%s/%s",directory,names[i]);
        size_t bytes;
        if(offset>=BOARD_MODEL_BYTES || read_into(model_path,storage+offset,BOARD_MODEL_BYTES-offset,&bytes)!=FR_OK) {
            snprintf(status,sizeof(status),"Scene model read failed; pack must fit 2 MiB.");return;
        }
        uint8_t *arena=(uint8_t *)board_asset_workspace(slot)+i*arena_bytes;
        result=dsp3d_asset_open(storage+offset,bytes,arena,arena_bytes/2,player->assets+i);
        if(result!=DSP3D_OK) break;
        dsp3d_asset *asset=player->assets+i;
        if(asset->geometry.mesh.vertex_count>BOARD_MAX_VERTICES ||
           (asset->skin && (size_t)asset->geometry.mesh.vertex_count*sizeof(dsp3d_vertex)>arena_bytes/2)) {
            result=DSP3D_INSUFFICIENT_MEMORY;break;
        }
        player->rest[i]=asset->geometry.mesh.vertices;
        if(asset->skin) {
            player->deformed[i]=(dsp3d_vertex *)(arena+arena_bytes/2);
            memcpy(player->deformed[i],player->rest[i],asset->geometry.mesh.vertex_count*sizeof(dsp3d_vertex));
            asset->geometry.mesh.vertices=player->deformed[i];
            asset->geometry.bounds_valid=0;asset->geometry.face_normals=NULL;
        }
        player->catalog[i]=&asset->geometry;
        offset=(offset+bytes+31)&~(size_t)31;
        loading_screen((i+1)*100/count);
    }
    if(result==DSP3D_OK) {
        player->scene=(dsp3d_scene){player->nodes,0};
        player->animation=(dsp3d_animation){player->tracks,player->keys,0,0,0,0};
        result=dsp3d_scene_read(storage,scene_size,&player->scene,128,&player->animation,128,SCENE_KEYS,player->catalog,count);
    }
    if(result==DSP3D_OK) for(unsigned i=0;i<count;i++) if(player->assets[i].skin) {
        unsigned instances=0;
        for(unsigned n=0;n<player->scene.count;n++) instances+=player->nodes[n].mesh_id==i;
        if(instances>1) {result=DSP3D_INVALID_MODEL;break;}
        for(unsigned j=0;j<player->assets[i].joint_count;j++) if(player->assets[i].joint_nodes[j]>=player->scene.count)
            result=DSP3D_INVALID_MODEL;
    }
    if(result!=DSP3D_OK) {snprintf(status,sizeof(status),"Scene load failed: %s",dsp3d_result_string(result));return;}
    player->count=count;loaded_slot=slot;scene_active=from_sd=1;browser=menu=0;
    mode=DSP3D_TEXTURED;reset_view();
    snprintf(model_name,sizeof(model_name),"%s",name);
    snprintf(status,sizeof(status),"Scene: %lu nodes | %u meshes | %lu tracks | %.2f seconds",
        (unsigned long)player->scene.count,count,(unsigned long)player->animation.track_count,(double)player->animation.duration);
    board_log(status);board_log("\r\n");
}
static dsp3d_result animate_scene(scene_player *player,float dt) {
    player->time+=dt;
    dsp3d_result r=player->animation.track_count ? dsp3d_scene_animate(&player->scene,&player->animation,player->time) :
        dsp3d_scene_update(&player->scene);
    if(r!=DSP3D_OK) return r;
    for(unsigned n=0;n<player->scene.count;n++) {
        uint32_t id=player->nodes[n].mesh_id;
        if(id>=player->count || !player->assets[id].skin) continue;
        dsp3d_asset *asset=player->assets+id;
        dsp3d_mat4 inverse;
        r=dsp3d_matrix_inverse(&player->nodes[n].world,&inverse);
        if(r==DSP3D_OK) r=dsp3d_skin_palette(&player->scene,asset->joint_nodes,asset->inverse_bind,
            asset->joint_count,&inverse,player->palette);
        if(r==DSP3D_OK) r=dsp3d_skin_vertices(player->rest[id],asset->skin,asset->geometry.mesh.vertex_count,
            player->palette,asset->joint_count,player->deformed[id],asset->geometry.mesh.vertex_count);
        if(r!=DSP3D_OK) return r;
    }
    return DSP3D_OK;
}
static void load(unsigned index) {
    if(index>=file_count) return;
    file_entry entry=app->files[index];
    if(entry.directory) {
        if(strlen(directory)+strlen(entry.name)+2>=sizeof(directory)) {
            snprintf(status,sizeof(status),"Folder path exceeds 255 characters.");return;
        }
        strcat(directory,"/");strcat(directory,entry.name);scan();return;
    }
    char path[400];snprintf(path,sizeof(path),"%s/%s",directory,entry.name);
    if(tolower((unsigned char)entry.name[strlen(entry.name)-1])=='s') {load_scene(path,entry.name);return;}
    FIL f;FRESULT result=f_open(&f,path,FA_READ);
    if(result!=FR_OK) {snprintf(status,sizeof(status),"Open failed (%u).",(unsigned)result);return;}
    FSIZE_t size=f_size(&f);
    if(size<32 || size>BOARD_MODEL_BYTES) {
        f_close(&f);snprintf(status,sizeof(status),"Model must fit the 2 MiB asset slot.");return;
    }
    unsigned slot=loaded_slot^1u;
    uint8_t *buffer=board_model_buffer(slot);
    uint32_t offset=0,last_percent=101;
    while(offset<size) {
        UINT got,bytes=size-offset>4096 ? 4096 : (UINT)(size-offset);
        result=f_read(&f,buffer+offset,bytes,&got);
        if(result!=FR_OK || got!=bytes) break;
        offset+=got;
        unsigned percent=(unsigned)((uint64_t)offset*100/size);
        if(last_percent==101 || percent>=last_percent+10 || percent==100) {
            loading_screen(percent);last_percent=percent;
        }
    }
    f_close(&f);
    if(result!=FR_OK || offset!=size) {
        snprintf(status,sizeof(status),"SD read failed. Previous asset retained in RAM.");return;
    }
    dsp3d_asset candidate;
    dsp3d_result r=dsp3d_asset_open(buffer,size,board_asset_workspace(slot),BOARD_WORKSPACE_BYTES,&candidate);
    if(r==DSP3D_OK && candidate.geometry.mesh.vertex_count>BOARD_MAX_VERTICES) r=DSP3D_INSUFFICIENT_MEMORY;
    if(r!=DSP3D_OK) {snprintf(status,sizeof(status),"Load failed: %s",dsp3d_result_string(r));return;}
    loaded=candidate;loaded_slot=slot;from_sd=1;browser=menu=scene_active=0;
    mode=loaded.texture_count ? DSP3D_TEXTURED : DSP3D_GOURAUD;
    reset_view();snprintf(model_name,sizeof(model_name),"%s",entry.name);
    snprintf(status,sizeof(status),"%lu vertices | %lu triangles | %lu materials | %lu KiB",
        (unsigned long)loaded.geometry.mesh.vertex_count,(unsigned long)loaded.geometry.mesh.triangle_count,
        (unsigned long)loaded.geometry.material_count,(unsigned long)(size/1024));
    board_log("SD asset validated and loaded transactionally.\r\n");
}
static void back_folder(void) {
    char *slash=strrchr(directory,'/');
    if(slash && slash!=directory+2) *slash=0;
    else strcpy(directory,"0:");
    scan();
}
static void bind_scene(unsigned d) {
    unsigned height=BOARD_HEIGHT-TOP-INFO_HEIGHT-BOTTOM;
    unsigned width=BOARD_WIDTH/d,h=height/d;
    if(render_width!=width || render_height!=h) {
        dsp3d_bind_target(&scene_renderer,width,h,board_render_buffer(),width,scene_renderer.depth,width);
        render_width=width;render_height=h;
    }
}
static void apply_presets(void) {
    const dsp3d_vec3 lights[]={{-.4f,.7f,1},{1,1,.2f},{-.8f,.3f,-1}};
    scene_renderer.light_direction=lights[light_preset%3];
    scene_renderer.ambient=light_preset==2 ? .4f : .2f;
    scene_renderer.specular=material_preset==2 ? .8f : 0;
    scene_renderer.tint_enabled=material_preset!=0;
    scene_renderer.tint=material_preset==1 ? 0x6de2b0 : 0xf7c267;
}
static void fit_view(void) {
    dsp3d_bounds bounds=from_sd ? loaded.geometry.bounds : demo==0 ? app->demos.fox.bounds :
                        demo==2 ? app->demos.cube.bounds : app->demos.monkey.bounds;
    if(scene_active || (!from_sd && demo>=3 && demo!=8)) {
        bounds.center=(dsp3d_vec3){0,0,0};bounds.radius=2.2f;
        bounds.min=(dsp3d_vec3){-2,-1.5f,-2};bounds.max=(dsp3d_vec3){2,1.5f,2};
    }
    dsp3d_camera_fit(&scene_renderer.camera,&bounds,(float)render_width/render_height,1.2f);
    orbit.target=scene_renderer.camera.target;
    orbit.distance=sqrtf(dsp3d_vdot(dsp3d_vsub(scene_renderer.camera.eye,orbit.target),
                                   dsp3d_vsub(scene_renderer.camera.eye,orbit.target)));
    transform.scale=1;transform.rotation=(dsp3d_vec3){0,0,0};
}
static void benchmark(unsigned equal) {
    char log[240];
    board_log("BENCH,backend,board,width,height,clock_hz,model,mode,frames,mean_render_us,min_render_us,max_render_us,mean_clear_us\r\n");
    unsigned w=equal ? 400 : BOARD_WIDTH,h=equal ? 240 : BOARD_HEIGHT;
    dsp3d_bind_target(&scene_renderer,w,h,board_render_buffer(),w,scene_renderer.depth,w);
    dsp3d_camera saved=scene_renderer.camera;
    scene_renderer.camera=(dsp3d_camera){{0,0,4},{0,0,0},{0,1,0},.85f,.05f,100};
    for(unsigned m=0;m<2;m++) for(unsigned rendering=0;rendering<4;rendering++) {
        uint64_t sum=0,clear=0;uint32_t minimum=UINT32_MAX,maximum=0;
        for(unsigned frame=0;frame<120;frame++) {
            dsp3d_transform pose={{.2f,(float)frame*.025f,0},{0,0,0},1.05f};
            uint32_t a=board_cycles();dsp3d_begin_frame(&scene_renderer,0);uint32_t b=board_cycles();
            dsp3d_draw(&scene_renderer,m ? &suzanne : &metamask,&pose,(dsp3d_mode)rendering);
            uint32_t elapsed=board_cycles()-b;sum+=elapsed;clear+=(uint32_t)(b-a);
            if(elapsed<minimum) minimum=elapsed;
            if(elapsed>maximum) maximum=elapsed;
        }
#ifdef DSP3D_CMSIS
        const char *backend="cmsis-dsp";
#else
        const char *backend="scalar";
#endif
        unsigned mhz=SystemCoreClock/1000000;
        snprintf(log,sizeof(log),"BENCH,%s,%s,%u,%u,%lu,%s,%s,120,%lu,%lu,%lu,%lu\r\n",backend,
            BOARD_NAME,w,h,(unsigned long)SystemCoreClock,m ? "suzanne" : "metamask",modes[rendering],
            (unsigned long)(sum/120/mhz),(unsigned long)(minimum/mhz),(unsigned long)(maximum/mhz),
            (unsigned long)(clear/120/mhz));board_log(log);
    }
    scene_renderer.camera=saved;render_width=render_height=0;
    board_log("BENCH complete\r\n");
}
static void save_preferences(void) {
    preferences p={3,demo,(uint32_t)mode,resolution,light_preset,material_preset,pan_gesture,autorotate};
    int ok=board_preferences_save(&p,sizeof(p));
    snprintf(status,sizeof(status),"%s",ok ? "Settings saved to reserved internal flash." : "Settings save failed.");
    board_log(status);board_log("\r\n");
}
static void command(int ch) {
    if(ch>='0' && ch<='9') {embedded((unsigned)(ch-'0'));return;}
    switch(ch) {
    case 'm': mode=(dsp3d_mode)((mode+1)%5);break;
    case 'r': autorotate^=1;break;
    case '+': orbit.distance=fmaxf(.3f,orbit.distance/1.15f);break;
    case '-': orbit.distance=fminf(30,orbit.distance*1.15f);break;
    case 'c': scene_renderer.backface_culling^=1;break;
    case 's': scan();board_log(status);board_log("\r\n");break;
    case 'g': menu=1;browser=page=0;break;
    case 'o': menu=2;browser=page=0;break;
    case 'f': fit_view();break;
    case 'x': reset_view();break;
    case 'p': pan_gesture^=1;break;
    case 'q': resolution=(resolution+1)%4;quality.stable_frames=0;break;
    case 'l': light_preset=(light_preset+1)%3;apply_presets();break;
    case 'v': material_preset=(material_preset+1)%3;apply_presets();break;
    case 'w': save_preferences();break;
    case 'b': benchmark(0);break;
    case 't': benchmark(1);break;
    case ']': if(browser && (page+1)*rows()<file_count) {page++;thumbnails();}break;
    case '[': if(browser && page) {page--;thumbnails();}break;
    default: break;
    }
}
static const char *const setting_names[]={"Fit camera","Reset view","Gesture: orbit / pan",
    "Resolution: full / half / quarter / auto","Light preset","Material preset","Save preferences","Benchmark 400 x 240","Return to viewer"};
static void setting(unsigned i) {
    const char actions[]={'f','x','p','q','l','v','w','t',0};
    if(i>=sizeof(actions)) return;
    if(actions[i]) command(actions[i]);else menu=0;
}
static void tap(unsigned x,unsigned y) {
    if(x>=BOARD_WIDTH || y>=BOARD_HEIGHT) return;
    if(y<TOP) {
        unsigned cell=x*4/BOARD_WIDTH;
        if(cell<2) embedded(cell);
        else if(cell==2) command('g');else scan();
        return;
    }
    if(browser || menu) {
        unsigned count=browser ? file_count : menu==1 ? GALLERY_COUNT : 9;
        if(y>=BOARD_HEIGHT-BOTTOM && y<BOARD_HEIGHT-STATUS_HEIGHT) {
            unsigned cell=x*4/BOARD_WIDTH;
            if(cell==0 && page) page--;
            if(cell==1 && (page+1)*rows()<count) page++;
            if(cell==2) {if(browser) back_folder();else {menu=menu==1 ? 2 : 1;page=0;}}
            if(cell==3) {if(browser) scan();else menu=0;}
            if(browser) thumbnails();
        } else if(y>=TOP+LIST_HEADER && y<BOARD_HEIGHT-BOTTOM) {
            unsigned row=(y-TOP-LIST_HEADER)/LIST_ROW;
            if(row>=rows()) return;
            unsigned index=page*rows()+row;
            if(browser) load(index);
            else if(menu==1 && index<GALLERY_COUNT) embedded(index);
            else if(menu==2) setting(index);
        }
        return;
    }
    if(y>=BOARD_HEIGHT-BOTTOM && y<BOARD_HEIGHT-STATUS_HEIGHT) {
        const char actions[]={'m','r','-','+','o'};command(actions[x*5/BOARD_WIDTH]);
    }
}
static void label(int x,int y,const char *value,int right) {
    char clipped[160];snprintf(clipped,sizeof(clipped),"%s",value);
    unsigned capacity=(unsigned)(right-x)/FILE_FONT.Width;
    if(capacity<sizeof(clipped) && strlen(clipped)>capacity && capacity>3) memcpy(clipped+capacity-3,"...",4);
    text_font(x,y,clipped,0xe4edf8,&FILE_FONT,right);
}
static void ui(unsigned fps10) {
    rect(0,0,BOARD_WIDTH,TOP,0x14253b);
    const char *tabs[]={"Fox","Suzanne","Gallery","SD Models"};
    for(unsigned i=0;i<4;i++) button(i,4,0,TOP,tabs[i],i<2 ? 0xffbb64 : 0x91d1ff,
        i==2 ? menu==1 : i==3 ? browser : !browser && !menu && !from_sd && demo==i,&TAB_FONT);
    rect(0,BOARD_HEIGHT-BOTTOM,BOARD_WIDTH,BOTTOM,0x14253b);
    char line[160];
    if(browser || menu) {
        rect(0,TOP,BOARD_WIDTH,BOARD_HEIGHT-TOP-BOTTOM,0x0b1423);
        unsigned count=browser ? file_count : menu==1 ? GALLERY_COUNT : 9;
        for(unsigned row=0;row<rows() && page*rows()+row<count;row++) {
            unsigned i=page*rows()+row;int y=TOP+LIST_HEADER+(int)row*LIST_ROW;
            rect(8,y+2,BOARD_WIDTH-16,LIST_ROW-4,0x1d334b);
            int left=20;
            if(browser) {
                file_entry *e=&app->files[i];
                if(app->thumbnail_valid[row] && LIST_ROW>=32) {
                    for(unsigned yy=0;yy<32;yy++) memcpy(renderer.color+(y+4+yy)*BOARD_WIDTH+16,
                        app->thumbnails[row]+yy*32,64);
                    left=60;
                }
                snprintf(line,sizeof(line),"%s%s",e->directory ? "[+] " : "",e->name);
            } else snprintf(line,sizeof(line),"%s",menu==1 ? gallery_names[i] : setting_names[i]);
            label(left,y+(LIST_ROW-FILE_FONT.Height)/2,line,BOARD_WIDTH-20);
        }
        if(browser) snprintf(line,sizeof(line),"SD %.60s | Page %u",directory+2,page+1);
        else if(menu==1) snprintf(line,sizeof(line),"EXAMPLE GALLERY | Page %u",page+1);
        else snprintf(line,sizeof(line),"SETTINGS | %s | %s | Light %u | Material %u",
            pan_gesture ? "Pan" : "Orbit",resolution==3 ? "Auto" : resolution==2 ? "1/4" : resolution==1 ? "1/2" : "Full",
            light_preset+1,material_preset+1);
        text(12,TOP+(LIST_HEADER-BODY_FONT.Height)/2,line,0xffffff);
        const char *buttons[]={"Previous","Next",browser ? "Parent" : menu==1 ? "Settings" : "Gallery",browser ? "Refresh" : "Close"};
        for(unsigned i=0;i<4;i++) button(i,4,BOARD_HEIGHT-BOTTOM,CONTROLS,buttons[i],0xffffff,0,&CONTROL_FONT);
    } else {
        const char *buttons[]={modes[mode],autorotate ? "Pause" : "Play","Zoom -","Zoom +","Settings"};
        for(unsigned i=0;i<5;i++) button(i,5,BOARD_HEIGHT-BOTTOM,CONTROLS,buttons[i],0xffffff,0,&CONTROL_FONT);
        snprintf(line,sizeof(line),"%.30s | %u x %u | %u.%u FPS | draw %lu us",model_name,render_width,render_height,
            fps10/10,fps10%10,(unsigned long)metrics[1]);
        rect(0,TOP,BOARD_WIDTH,INFO_HEIGHT,0x0b1423);
        text(6,TOP+(INFO_HEIGHT-BODY_FONT.Height)/2,line,0xc6d8eb);
    }
    status_text();
}
static void pick(unsigned x,unsigned y) {
    if(browser || menu || y<TOP+INFO_HEIGHT || y>=BOARD_HEIGHT-BOTTOM) return;
    float px=(float)x*render_width/BOARD_WIDTH;
    float py=(float)(y-TOP-INFO_HEIGHT)*render_height/(BOARD_HEIGHT-TOP-INFO_HEIGHT-BOTTOM);
    dsp3d_hit hit;uint32_t node_id=UINT32_MAX;dsp3d_result r;
    if(scene_active) r=dsp3d_scene_pick(&scene_renderer,&app->players[loaded_slot].scene,px,py,&node_id,&hit);
    else if(from_sd) {
        dsp3d_mat4 world=dsp3d_matrix_trs(transform.position,dsp3d_quat_euler(transform.rotation),
            (dsp3d_vec3){transform.scale,transform.scale,transform.scale});
        r=dsp3d_pick_mesh(&scene_renderer.camera,scene_renderer.viewport,px,py,&loaded.geometry.mesh,&world,&hit);
        if(isfinite(hit.distance)) node_id=0;
    } else r=gallery_pick(&app->demos,&scene_renderer,px,py,&node_id,&hit);
    if(r==DSP3D_OK && node_id!=UINT32_MAX) {
        snprintf(status,sizeof(status),"Picked node %lu | Triangle %lu | Distance %.2f",
            (unsigned long)node_id,(unsigned long)hit.triangle,(double)hit.distance);
        board_log(status);board_log("\r\n");
    } else snprintf(status,sizeof(status),"No surface at this position.");
}
int main(void) {
    board_init();board_renderer_init(&renderer);board_renderer_init(&scene_renderer);
    renderer.depth=NULL;
    if(sizeof(application)>BOARD_APP_BYTES) board_panic(20);
    app=board_app_buffer();memset(app,0,sizeof(*app));
    if(gallery_init(&app->demos)!=DSP3D_OK) board_panic(21);
    dsp3d_quality_init(&quality,25000);
    embedded(0);
    preferences pref;
    if(board_preferences_load(&pref,sizeof(pref)) && pref.version==3 && pref.demo<GALLERY_COUNT &&
       pref.mode<=DSP3D_TEXTURED && pref.resolution<4 && pref.light<3 && pref.material<3 && pref.pan<2 && pref.rotate<2) {
        embedded(pref.demo);mode=(dsp3d_mode)pref.mode;resolution=pref.resolution;light_preset=pref.light;
        material_preset=pref.material;pan_gesture=pref.pan;autorotate=pref.rotate;
        board_log("Saved preferences restored.\r\n");
    }
    apply_presets();bind_scene(1);
    uint32_t last=HAL_GetTick(),last_log=last,last_frame=last,down_time=0;
    unsigned fps10=0,previous_touch=0,previous_button=0,gesture=0,moved=0;
    int previous_x=0,previous_y=0,down_x=0,down_y=0;float previous_span=0;
    TS_StateTypeDef touch={0};
    board_log("Controls: 0-9 examples, g gallery, o settings, m mode, r play, +/- zoom, p pan, f fit, x reset, q resolution, l light, v material, w save, t equal benchmark, b full benchmark, s SD.\r\n");
    for(;;) {
        uint32_t now=HAL_GetTick();float dt=fminf(.1f,(float)(now-last)*.001f);last=now;
        unsigned button=BSP_PB_GetState(BOARD_BUTTON)!=RESET;
        if(button && !previous_button) embedded((demo+1)%GALLERY_COUNT);
        previous_button=button;
        int ch;while((ch=board_read_char())>=0) command(ch);
        if(viewer_command) {command((int)viewer_command);viewer_command=0;}
        if(BSP_TS_GetState(&touch)==TS_OK) {
            unsigned down=touch.touchDetected;
            if(down) {
                int x=touch.touchX[0],y=touch.touchY[0];
                if(!previous_touch) {
                    gesture=!browser && !menu && x<(int)BOARD_WIDTH && y>=TOP+INFO_HEIGHT && y<(int)BOARD_HEIGHT-BOTTOM;
                    down_x=x;down_y=y;down_time=now;moved=0;tap((unsigned)x,(unsigned)y);
                } else if(gesture && x>=0 && x<(int)BOARD_WIDTH && y>=TOP+INFO_HEIGHT && y<(int)BOARD_HEIGHT-BOTTOM) {
                    int dx=x-previous_x,dy=y-previous_y;
                    if(abs(x-down_x)+abs(y-down_y)>8) moved=1;
                    if(down==1 && moved) {
                        if(pan_gesture) {
                            dsp3d_orbit_apply(&orbit,&scene_renderer.camera);
                            dsp3d_camera_pan(&scene_renderer.camera,-dx*orbit.distance*.0015f,dy*orbit.distance*.0015f);
                            orbit.target=scene_renderer.camera.target;
                        } else {orbit.yaw-=dx*.008f;orbit.pitch=fmaxf(-1.4f,fminf(1.4f,orbit.pitch+dy*.008f));}
                        autorotate=0;
                    } else if(down>1) {
                        float dx2=(float)x-touch.touchX[1],dy2=(float)y-touch.touchY[1],span=sqrtf(dx2*dx2+dy2*dy2);
                        if(previous_touch>1 && previous_span>10) orbit.distance=fmaxf(.3f,fminf(30,orbit.distance*previous_span/span));
                        previous_span=span;moved=1;
                    }
                }
                previous_x=x;previous_y=y;
            } else if(previous_touch && gesture && !moved && now-down_time<500) pick((unsigned)down_x,(unsigned)down_y);
            previous_touch=down;
        }
        if(autorotate && !browser && !menu && !previous_touch) {
            if(!from_sd) gallery_step(&app->demos,demo,dt);
            if((from_sd && !scene_active) || (!from_sd && (demo<4 || demo==8)))
                transform.rotation.y=remainderf(transform.rotation.y+dt*.45f,6.2831853f);
        }
        divisor=resolution==3 ? quality.divisor : 1u<<resolution;
        bind_scene(divisor);dsp3d_orbit_apply(&orbit,&scene_renderer.camera);
        uint32_t start=board_cycles();board_frame_begin(&renderer,dsp3d_rgb565(0x0b1423));
        dsp3d_begin_frame(&scene_renderer,dsp3d_rgb565(0x0b1423));
        uint32_t stamp=board_cycles(),mhz=SystemCoreClock/1000000;metrics[0]=(stamp-start)/mhz;
        if(!browser && !menu) {
            dsp3d_result r;
            if(scene_active) {
                scene_player *player=&app->players[loaded_slot];
                r=animate_scene(player,autorotate ? dt : 0);
                if(r==DSP3D_OK) r=dsp3d_scene_render(&scene_renderer,&player->scene,mode);
            } else r=from_sd ? dsp3d_draw_geometry(&scene_renderer,&loaded.geometry,&transform,mode) :
                gallery_draw(&app->demos,&scene_renderer,demo,&transform,mode);
            if(r!=DSP3D_OK) snprintf(status,sizeof(status),"Render failed: %s",dsp3d_result_string(r));
        }
        board_render_cycles=board_cycles()-stamp;metrics[1]=board_render_cycles/mhz;
        stamp=board_cycles();
        if(!browser && !menu) dsp3d_scale_rgb565(scene_renderer.color,render_width,render_height,render_width,
            renderer.color+(TOP+INFO_HEIGHT)*BOARD_WIDTH,BOARD_WIDTH,BOARD_HEIGHT-TOP-INFO_HEIGHT-BOTTOM,BOARD_WIDTH);
        metrics[2]=(board_cycles()-stamp)/mhz;stamp=board_cycles();ui(fps10);
        metrics[3]=(board_cycles()-stamp)/mhz;stamp=board_cycles();board_present(&renderer);
        metrics[4]=(board_cycles()-stamp)/mhz;
        if(resolution==3 && !browser && !menu) dsp3d_quality_update(&quality,metrics[0]+metrics[1]+metrics[2]+metrics[3]);
        now=HAL_GetTick();uint32_t elapsed=now-last_frame;last_frame=now;if(elapsed) fps10=10000/elapsed;
        if(now-last_log>=1000) {
            char log[340];snprintf(log,sizeof(log),
                "frame=%lu demo=%u model=%.40s mode=%s width=%u height=%u clear_us=%lu render_us=%lu scale_us=%lu ui_us=%lu present_us=%lu fps10=%u tris=%lu pixels=%lu lod=%lu rejected=%lu\r\n",
                (unsigned long)board_frame_count,demo,model_name,modes[mode],render_width,render_height,
                (unsigned long)metrics[0],(unsigned long)metrics[1],(unsigned long)metrics[2],(unsigned long)metrics[3],
                (unsigned long)metrics[4],fps10,(unsigned long)scene_renderer.stats.rasterized_triangles,
                (unsigned long)scene_renderer.stats.shaded_pixels,(unsigned long)app->demos.active_lod,
                (unsigned long)scene_renderer.stats.rejected_objects);board_log(log);last_log=now;
            unsigned detected=BSP_SD_IsDetected()==SD_PRESENT;
            if(detected!=media_present) {
                media_present=detected;
                if(!detected) {
                    f_mount(NULL,"0:",0);
                    snprintf(status,sizeof(status),"SD removed. Loaded model remains in RAM.");
                } else if(browser) scan();
                else snprintf(status,sizeof(status),"SD inserted. Open SD Models to browse.");
            }
        }
    }
}

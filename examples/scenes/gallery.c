/* SPDX-License-Identifier: BSD-3-Clause */
#include "gallery.h"
#include "scenes.h"
#include "metamask.h"
#include "suzanne.h"
#include "suzanne_compact.h"
#include "suzanne_lod1.h"
#include "suzanne_lod2.h"
#include <math.h>
#include <string.h>
const char *const gallery_names[GALLERY_COUNT] = {
    "MetaMask Fox", "Suzanne", "Perspective texture", "Alpha materials", "Light and shadow",
    "Scene hierarchy", "Keyframe animation", "Skeletal ribbon", "Compact mesh + LOD", "Clip and pick"};
const char *const gallery_descriptions[GALLERY_COUNT] = {
    "Prepared face normals | Flat / Gouraud / wire / points",
    "Smooth normals | Orbit, pan, fit and triangle picking",
    "Perspective-correct UV | Repeating RGB565 checker",
    "Opaque pass + sorted translucent objects | Depth blending",
    "Specular highlight | Directional planar shadow on floor",
    "Parent transforms | Orbiting children | Tap to pick nodes",
    "TRS keyframes | Quaternion interpolation | Loop playback",
    "Two joints | Four-weight skin API | Animated normals",
    "12-byte packed vertices | Three distance-based LOD levels",
    "Near/side clipping | Off-screen bounds rejection | Picking"};
static void node(gallery *g, unsigned i, const dsp3d_geometry *geometry, int parent,
                 dsp3d_vec3 p, float scale) {
    dsp3d_node *n = g->nodes+i;
    memset(n, 0, sizeof(*n));
    n->parent = parent;
    n->mesh_id = geometry ? i : UINT32_MAX;
    n->geometry = geometry;
    n->translation = p;
    n->rotation.w = 1;
    n->scale = (dsp3d_vec3){scale, scale, scale};
    n->visible = 1;
}
dsp3d_result gallery_init(gallery *g) {
    if (!g) return DSP3D_INVALID_ARGUMENT;
    memset(g, 0, sizeof(*g));
    if (dsp3d_geometry_init(&g->fox, &metamask, g->fox_normals) != DSP3D_OK ||
        dsp3d_geometry_init(&g->monkey, &suzanne, g->monkey_normals) != DSP3D_OK)
        return DSP3D_INVALID_MODEL;
    dsp3d_geometry_init(&g->cube, &dsp3d_example_cube, NULL);
    const uint32_t *blobs[] = {suzanne_compact_data, suzanne_lod1_data, suzanne_lod2_data};
    const size_t sizes[] = {suzanne_compact_size, suzanne_lod1_size, suzanne_lod2_size};
    for (unsigned i=0; i<3; i++) {
        dsp3d_result r = dsp3d_asset_open(blobs[i], sizes[i], g->workspace+i*20000, 20000, g->compact+i);
        if (r != DSP3D_OK) return r;
    }
    for (unsigned i=0; i<24; i++)
        g->uv[i] = (dsp3d_uv){(i%4==1 || i%4==2) ? 2 : 0, (i%4>=2) ? 2 : 0};
    for (unsigned y=0; y<16; y++) for (unsigned x=0; x<16; x++)
        g->checker[y*16+x] = dsp3d_rgb565(((x/4+y/4)&1) ? 0xf9b34b : 0x246bb8);
    g->texture = (dsp3d_texture){g->checker, NULL, 16, 16, 16, 1};
    g->paint = (dsp3d_material){0xffffff, &g->texture, .25f, 0, 255, 0};
    g->transparent = (dsp3d_material){0x4cdbfa, NULL, .3f, .1f, 120, DSP3D_MATERIAL_DOUBLE_SIDED};
    g->polished = (dsp3d_material){0xd87532, NULL, .9f, 0, 255, 0};
    g->dark = (dsp3d_material){0x080e18, NULL, 0, .8f, 165, DSP3D_MATERIAL_DOUBLE_SIDED};
    g->ground = (dsp3d_material){0x48647c, NULL, 0, 0, 255, DSP3D_MATERIAL_DOUBLE_SIDED};
    g->textured=g->cube;
    g->textured.uv=g->uv;
    g->textured.materials=&g->paint;
    g->textured.material_count=1;
    g->glass=g->cube;
    g->glass.materials=&g->transparent;
    g->glass.material_count=1;
    g->shiny=g->monkey;
    g->shiny.materials=&g->polished;
    g->shiny.material_count=1;
    for (unsigned y=0; y<9; y++) for (unsigned x=0; x<2; x++) {
        unsigned i=y*2+x;
        g->rest[i]=(dsp3d_vertex){{x ? .3f : -.3f, (float)y*.25f-1, 0}, {0,0,1}};
        float weight = fmaxf(0, fminf(1, ((float)y-2)/4));
        g->weights[i]=(dsp3d_skin_vertex){{0,1,0,0},{1-weight,weight,0,0}};
        if (y<8 && !x) {
            g->ribbon_faces[y*2]=(dsp3d_triangle){{i,i+1,i+3},0x6fcf9f};
            g->ribbon_faces[y*2+1]=(dsp3d_triangle){{i,i+3,i+2},0xf0b854};
        }
    }
    memcpy(g->deformed,g->rest,sizeof(g->rest));
    dsp3d_mesh ribbon={g->deformed,g->ribbon_faces,18,16};
    dsp3d_geometry_init(&g->ribbon,&ribbon,NULL);
    g->ribbon.bounds_valid=0;
    g->joints[0]=1;g->joints[1]=2;
    g->inverse_bind[0]=g->inverse_bind[1]=dsp3d_identity();
    dsp3d_mesh shadow={g->projected,dsp3d_example_cube.triangles,24,12};
    g->shadow.mesh=shadow;
    g->shadow.materials=&g->dark;g->shadow.material_count=1;
    const dsp3d_vertex floor[] = {{{-3,-1.1f,3},{0,1,0}},{{3,-1.1f,3},{0,1,0}},
                                  {{3,-1.1f,-3},{0,1,0}},{{-3,-1.1f,-3},{0,1,0}}};
    memcpy(g->floor_vertices,floor,sizeof(floor));
    g->floor_faces[0]=(dsp3d_triangle){{0,1,2},0x48647c};
    g->floor_faces[1]=(dsp3d_triangle){{0,2,3},0x48647c};
    g->floor.mesh=(dsp3d_mesh){g->floor_vertices,g->floor_faces,4,2};
    g->floor.materials=&g->ground;g->floor.material_count=1;
    g->scene=(dsp3d_scene){g->nodes,1};
    g->selected=UINT32_MAX;
    return DSP3D_OK;
}
void gallery_select(gallery *g,unsigned index) {
    g->time=0;g->selected=UINT32_MAX;g->active_lod=0;
    g->scene.count=1;
    node(g,0,index==0 ? &g->fox : &g->monkey,-1,(dsp3d_vec3){0,0,0},1);
    if(index==2) g->nodes[0].geometry=&g->textured;
    if(index==3) {
        g->scene.count=3;
        node(g,0,&g->cube,-1,(dsp3d_vec3){0,0,-.6f},.65f);
        node(g,1,&g->glass,-1,(dsp3d_vec3){-.5f,0,.5f},.7f);
        node(g,2,&g->glass,-1,(dsp3d_vec3){.65f,.15f,.1f},.55f);
    }
    if(index==4) {
        g->scene.count=3;
        node(g,0,&g->floor,-1,(dsp3d_vec3){0,0,0},1);
        node(g,1,&g->shadow,-1,(dsp3d_vec3){0,0,0},1);
        node(g,2,&g->textured,-1,(dsp3d_vec3){0,0,0},.65f);
    }
    if(index==5) {
        g->scene.count=4;
        node(g,0,&g->shiny,-1,(dsp3d_vec3){0,0,0},.6f);
        node(g,1,NULL,-1,(dsp3d_vec3){0,0,0},1);
        node(g,2,&g->textured,1,(dsp3d_vec3){1.5f,0,0},.25f);
        node(g,3,&g->cube,2,(dsp3d_vec3){2.1f,0,0},.35f);
    }
    if(index==6) {
        g->nodes[0].geometry=&g->shiny;
        g->keys[0]=(dsp3d_keyframe){0,{-1,0,0},{0,0,0,1},{.6f,.6f,.6f}};
        g->keys[1]=(dsp3d_keyframe){2,{1,.35f,0},{0,1,0,0},{.8f,.8f,.8f}};
        g->keys[2]=(dsp3d_keyframe){4,{-1,0,0},{0,0,0,-1},{.6f,.6f,.6f}};
        g->track=(dsp3d_track){0,0,3,0};
        g->animation=(dsp3d_animation){&g->track,g->keys,1,3,4,1};
    }
    if(index==7) {
        g->scene.count=3;
        g->nodes[0].geometry=&g->ribbon;
        node(g,1,NULL,-1,(dsp3d_vec3){0,0,0},1);
        node(g,2,NULL,1,(dsp3d_vec3){0,0,0},1);
    }
    if(index==9) {
        g->scene.count=3;
        node(g,0,&g->textured,-1,(dsp3d_vec3){-1.3f,0,0},1.1f);
        node(g,1,&g->monkey,-1,(dsp3d_vec3){1.4f,0,1.6f},.8f);
        node(g,2,&g->fox,-1,(dsp3d_vec3){100,0,0},1);
    }
}
void gallery_step(gallery *g,unsigned index,float dt) {
    g->time=fmodf(g->time+dt,1000);
    float t=g->time;
    if(index==5) {
        g->nodes[1].rotation=dsp3d_quat_euler((dsp3d_vec3){.2f,t,0});
        g->nodes[2].rotation=dsp3d_quat_euler((dsp3d_vec3){t*.4f,t*1.5f,0});
    }
    if(index==6) (void)dsp3d_scene_animate(&g->scene,&g->animation,t);
    if(index==7) {
        g->nodes[2].rotation=dsp3d_quat_euler((dsp3d_vec3){sinf(t)*.35f,0,sinf(t*1.5f)*.8f});
        (void)dsp3d_scene_update(&g->scene);
        dsp3d_mat4 identity=dsp3d_identity();
        (void)dsp3d_skin_palette(&g->scene,g->joints,g->inverse_bind,2,&identity,g->palette);
        (void)dsp3d_skin_vertices(g->rest,g->weights,18,g->palette,2,g->deformed,18);
    }
}
dsp3d_result gallery_draw(gallery *g,dsp3d_context *c,unsigned index,
                         const dsp3d_transform *pose,dsp3d_mode mode) {
    if(index>=GALLERY_COUNT) return DSP3D_INVALID_ARGUMENT;
    if(index==0 || index==1 || index==2 || index==8) {
        const dsp3d_geometry *geometry=index==0 ? &g->fox : index==2 ? &g->textured : &g->monkey;
        if(index==8) {
            float distance=sqrtf(dsp3d_vdot(dsp3d_vsub(c->camera.eye,c->camera.target),
                                           dsp3d_vsub(c->camera.eye,c->camera.target)))/pose->scale;
            dsp3d_lod levels[]={{&g->compact[0].geometry.mesh,0},{&g->compact[1].geometry.mesh,4.5f},
                                {&g->compact[2].geometry.mesh,7}};
            const dsp3d_mesh *chosen=dsp3d_select_lod(levels,3,distance);
            g->active_lod=chosen==levels[0].mesh ? 0 : chosen==levels[1].mesh ? 1 : 2;
            geometry=&g->compact[g->active_lod].geometry;
        }
        g->nodes[0].geometry=geometry;
        g->nodes[0].world=dsp3d_matrix_trs(pose->position,dsp3d_quat_euler(pose->rotation),
                                         (dsp3d_vec3){pose->scale,pose->scale,pose->scale});
        return dsp3d_draw_geometry(c,geometry,pose,mode);
    }
    if(index==4) {
        dsp3d_mat4 world=dsp3d_matrix_trs((dsp3d_vec3){0,0,0},
            dsp3d_quat_euler((dsp3d_vec3){.2f,g->time*.5f,.1f}),(dsp3d_vec3){.65f,.65f,.65f});
        g->nodes[2].rotation=dsp3d_quat_euler((dsp3d_vec3){.2f,g->time*.5f,.1f});
        dsp3d_result r=dsp3d_planar_shadow(&dsp3d_example_cube,&world,c->light_direction,-1.1f,g->projected,24);
        if(r!=DSP3D_OK) return r;
    }
    return dsp3d_scene_render(c,&g->scene,mode);
}
dsp3d_result gallery_pick(gallery *g,const dsp3d_context *c,float x,float y,
                         uint32_t *node_id,dsp3d_hit *hit) {
    dsp3d_result r=dsp3d_scene_pick(c,&g->scene,x,y,node_id,hit);
    if(r==DSP3D_OK) g->selected=*node_id;
    return r;
}

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "../common/binout.h"

#include "ini.h"
#include "../../include/GL/gl_enums.h"
#include "material64.h"
#include "../../src/material64_internal.h"

int flag_verbose = 0;

material64_t defaultmaterial;

void init_defaultmaterial(){
    defaultmaterial = (material64_t){
        .texture = NULL,
        .alphamode = 0,
        .opengl.lighting = true,
        .opengl.shading = true,
        .opengl.culling = GL_BACK,
        .opengl.texgen = GL_NONE,
        .opengl.m.shininess = 0,
        .opengl.m.ambient = MATERIAL64_GL_COLOR(0.2, 0.2, 0.2, 1.0),
        .opengl.m.diffuse = MATERIAL64_GL_COLOR(0.8, 0.8, 0.8, 1.0),
        .opengl.m.specular = MATERIAL64_GL_COLOR(0, 0, 0, 1),
        .opengl.m.emissive = MATERIAL64_GL_COLOR(0, 0, 0, 1),
        .rdpq.multitexture = NULL,
        .rdpq.combiner = MATERIAL64_INVALID_COMBINER,
        .rdpq.blender = MATERIAL64_INVALID_BLENDER,
        .rdpq.regs.primlod = MATERIAL64_INVALID_PRIMLOD,
        .rdpq.regs.primitive = MATERIAL64_INVALID_COLOR,
        .rdpq.regs.environment = MATERIAL64_INVALID_COLOR,
        .rdpq.regs.blend = MATERIAL64_INVALID_COLOR,
        .rdpq.regs.fog = MATERIAL64_INVALID_COLOR,
        .extension.size = 0,
        .extension.buffer = NULL,
    };
}

uint32_t get_type_size(uint32_t type)
{
    switch (type) {
    case GL_BYTE:
        return sizeof(int8_t);
    case GL_UNSIGNED_BYTE:
        return sizeof(uint8_t);
    case GL_SHORT:
        return sizeof(int16_t);
    case GL_UNSIGNED_SHORT:
        return sizeof(uint16_t);
    case GL_INT:
        return sizeof(int32_t);
    case GL_UNSIGNED_INT:
        return sizeof(uint32_t);
    case GL_FLOAT:
        return sizeof(float);
    case GL_DOUBLE:
        return sizeof(double);
    case GL_HALF_FIXED_N64:
        return sizeof(int16_t);
    default:
        return 0;
    }
}

void print_args( char * name )
{
    fprintf(stderr, "mkmaterial -- Convert INI-styled text files into the mat64 format for libdragon\n\n");
    fprintf(stderr, "Usage: %s [flags] <input files...>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command-line flags:\n");
    fprintf(stderr, "   -o/--output <dir>         Specify output directory (default: .)\n");
    fprintf(stderr, "   -v/--verbose              Verbose output\n");
    fprintf(stderr, "\n");
}

mat64file_t* material64_alloc()
{
    mat64file_t *matfile = calloc(1, sizeof(mat64file_t));
    matfile->magic = MAT64_MAGIC;
    matfile->version = MAT64_VERSION;
    matfile->header_size = sizeof(mat64file_t);
    matfile->material = defaultmaterial;
    matfile->extensiondata_size = 0;
    return matfile;
}

void gl_color_write(FILE *out, mat64_gl_color_t* color){
    for(int i = 0; i < 4; i++){
        w32(out, color->c[i]);
    }
}

void color_write(FILE *out, color_t* color){
    w8(out, color->r);
    w8(out, color->g);
    w8(out, color->b);
    w8(out, color->a);
}

void material64_write(FILE *out, material64_t *material){
    w32(out, material->texture);
    w32(out, material->alphamode);
    w8(out,  material->opengl.lighting);
    w8(out,  material->opengl.shading);
    gl_color_write(out, &material->opengl.m.ambient);
    gl_color_write(out, &material->opengl.m.diffuse);
    gl_color_write(out, &material->opengl.m.specular);
    gl_color_write(out, &material->opengl.m.emissive);
    w32(out, material->opengl.m.shininess);
    w32(out, material->opengl.culling);
    w32(out, material->opengl.texgen);
    w32(out, material->rdpq.multitexture);
    w64(out, material->rdpq.combiner); // TODO w64
    w32(out, material->rdpq.blender);
    color_write(out, &material->rdpq.regs.primitive);
    color_write(out, &material->rdpq.regs.environment);
    color_write(out, &material->rdpq.regs.blend);
    color_write(out, &material->rdpq.regs.fog);
    w32(out, material->rdpq.regs.primlod);
    w32(out, material->extension.size);
    w32(out, material->extension.buffer);
}

void matfile_write(mat64file_t *matfile, FILE *out)
{
    // Write header
    //int header_start = ftell(out);
    
    w32(out, matfile->magic);
    w32(out, matfile->version);
    w32(out, matfile->header_size);
    material64_write(out, &matfile->material);
    // TODO write encoded pointers texture, multitexture, extensiondata_size, extensiondata
}

static int handler(void* fptr, const char* section, const char* name, const char* value)
{
    mat64file_t* file = (mat64file_t*)fptr;
    #define SECTION(s) strcmp(section, s) == 0

    if(SECTION("OpenGL")){
		MATERIAL64_DECODE_BOOL(		"lighting", 	value, name, file->material.opengl.lighting);
		MATERIAL64_DECODE_BOOL(		"shading", 		value, name, file->material.opengl.shading);
		MATERIAL64_DECODE_FLOAT(	"shininess", 	value, name, file->material.opengl.m.shininess);
		MATERIAL64_DECODE_GL_COLOR(	"ambient", 		value, name, file->material.opengl.m.ambient);
		MATERIAL64_DECODE_GL_COLOR(	"diffuse", 		value, name, file->material.opengl.m.diffuse);
		MATERIAL64_DECODE_GL_COLOR(	"specular", 	value, name, file->material.opengl.m.specular);
		MATERIAL64_DECODE_GL_COLOR(	"emissive", 	value, name, file->material.opengl.m.emissive);
		// TODO culling, texgen
	}
	else if(SECTION("RDPQ")){
		if(!strcmp("multitexture", name)){
            size_t namesize = strlen(name+sizeof('\0'));
            file->multitexture = (char*)malloc(namesize);
            memcpy(file->multitexture, name, namesize);
            return 1;
        }
		MATERIAL64_DECODE_COLOR(	"primitive", 	value, name, file->material.rdpq.regs.primitive);
		MATERIAL64_DECODE_COLOR(	"environment", 	value, name, file->material.rdpq.regs.environment);
		MATERIAL64_DECODE_COLOR(	"blend", 		value, name, file->material.rdpq.regs.blend);
		MATERIAL64_DECODE_COLOR(	"fog", 			value, name, file->material.rdpq.regs.fog);
		MATERIAL64_DECODE_INTEGER(	"primlod", 		value, name, file->material.rdpq.regs.primlod);
		// TODO combiner, blender
	}
	else if(SECTION("Extension")){
        size_t namesize, valuesize;
        namesize = strlen(name)+sizeof('\0');
        valuesize = strlen(value)+sizeof('\0');
		if(!file->extensiondata_size) file->extensiondata = (char*)malloc(namesize + valuesize);
        else file->extensiondata = (char*)realloc(file->extensiondata, file->extensiondata_size + namesize + valuesize);
        memcpy(file->extensiondata, name, namesize);
        memcpy(file->extensiondata + namesize, value, valuesize);
        file->extensiondata_size += namesize + valuesize;
        return 1;
	}
	else{
        if(!strcmp("texture", name)){
            size_t namesize = strlen(name+sizeof('\0'));
            file->texture = (char*)malloc(namesize);
            memcpy(file->texture, name, namesize);
            return 1;
        }
		// TODO alphamode
	}
	return 0;  // unknown section/name, error 
}

int convert(const char *infn, const char *outfn)
{
    mat64file_t* matfile = material64_alloc();
    int err_no = 0;
    if ((err_no = ini_parse(infn, handler, matfile)) < 0) {
        printf("Can't load '%s', error %i\n", infn, err_no);
        goto error;
    }
    free(matfile);
    return 0;

error:
    free(matfile);
    return 1;
}

int main(int argc, char *argv[])
{
    char *infn = NULL, *outdir = ".", *outfn = NULL;
    bool error = false;

    init_defaultmaterial();

    if (argc < 2) {
        print_args(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                print_args(argv[0]);
                return 0;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                flag_verbose++;
            } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if (++i == argc) {
                    fprintf(stderr, "missing argument for %s\n", argv[i-1]);
                    return 1;
                }
                outdir = argv[i];
            } else {
                fprintf(stderr, "invalid flag: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        infn = argv[i];
        char *basename = strrchr(infn, '/');
        if (!basename) basename = infn; else basename += 1;
        char* basename_noext = strdup(basename);
        char* ext = strrchr(basename_noext, '.');
        if (ext) *ext = '\0';

        asprintf(&outfn, "%s/%s.mat64", outdir, basename_noext);
        if (flag_verbose)
            printf("Converting: %s -> %s\n",
                infn, outfn);
        if (convert(infn, outfn) != 0) {
            error = true;
        }
        free(outfn);
    }

    return error ? 1 : 0;
}
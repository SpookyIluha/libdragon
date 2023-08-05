#ifndef __LIBDRAGON_MATERIAL64_INTERNAL_H
#define __LIBDRAGON_MATERIAL64_INTERNAL_H

#include <stdint.h>
#include "GL/gl.h"
#include "graphics.h"
#include "sprite.h"
#include "rdpq.h"
#include "material64.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief material64 file magic header */
#define MAT64_MAGIC           0x4d544c48 // "MTLH"
/** @brief material64 loaded model buffer magic */
#define MAT64_MAGIC_LOADED    0x4d544c4c // "MTLL"
/** @brief material64 owned model buffer magic */
#define MAT64_MAGIC_OWNED     0x4d544c4f // "MTLO"
/** @brief Current version of material64 */
#define MAT64_VERSION         1

typedef struct mat64file_s {
	uint32_t 		magic;                 	///< Magic header (MAT64_MAGIC)
    uint32_t 		version;               	///< Version of this file
    uint32_t 		header_size;           	///< Size of the header in bytes
    material64_t 	material;          		///< Standard material data
    char*           texture;
    char*           multitexture;           ///< Pointers to the texture and multitexture file names
	size_t	 		extensiondata_size;    	///< Size of the full extension string
	char*	 		extensiondata;    		///< File encoded pointer to the extension string
} mat64file_t;

#define MATERIAL64_INVALID_COMBINER 0xDEADBEEFDEADBEEF
#define MATERIAL64_INVALID_BLENDER  0xDEADBEEF
#define MATERIAL64_INVALID_COLOR    (color_t){.r=0xDE, .g=0xAD, .b=0xBE, .a=0xEF} //0xDEADBEEF
#define MATERIAL64_INVALID_PRIMLOD  -1

#define MATERIAL64_GL_COLOR(r,g,b,a) (mat64_gl_color_t){.c[0] = r, .c[1] = g, .c[2] = b, .c[3] = a}

//#define MATERIAL64_DECODE_BOOL(key, value, name, variable) 			if(strcmp(key, name) == 0) {variable = strcmp(value, "true")? false : true; printf("found bool %s with value %i\n", name, variable); return 1;}
//#define MATERIAL64_DECODE_INTEGER(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = atoi(value); printf("found integer %s with value %i\n", name, variable); return 1;}
//#define MATERIAL64_DECODE_FLOAT(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = (float)atof(value); printf("found float %s with value %f\n", name, variable); return 1;}
//#define MATERIAL64_DECODE_STRING(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = value; printf("found string %s with value %s\n", name, variable); return 1;}
//#define MATERIAL64_DECODE_TEXTURE(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = sprite_load(value); printf("found texture %s with value %s\n", name, variable); return 1;}
//#define MATERIAL64_DECODE_COLOR(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = strclr(value); printf("found color %s with value %i\n", name, variable); return 1;}
//#define MATERIAL64_DECODE_GL_COLOR(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = strclr_gl(value); printf("found gl color %s with value %i\n", name, variable); return 1;}

#ifdef __cplusplus
}
#endif

#endif

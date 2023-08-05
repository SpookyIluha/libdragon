#ifndef __LIBDRAGON_MATERIAL64_H
#define __LIBDRAGON_MATERIAL64_H

#include <stdint.h>
#include "GL/gl.h"
#include "graphics.h"
#include "sprite.h"
#include "rdpq.h"

#ifdef __cplusplus
extern "C" {
#endif

enum mat64_alphamode_t {
    MAT_OPAQUE = 0,
    MAT_CUTOUT = 1,
    MAT_ALPHABLEND  = 2,
};

typedef struct mat64_gl_color_s {
    float c[4];
} mat64_gl_color_t;

/**
 * @brief Material extension for #material64_get_extension.
 * 
 * This structure contains a pointer to the custom material data
 * a user might need to properly configure said material.
 * 
 */
typedef struct material64_ext_s{
    size_t  size;              ///< Size of the structure
    void*   buffer;            ///< Pointer to the extension structure
} material64_ext_t;

/**
 * @brief Material parameters for #material64_upload.
 * 
 * This structure contains all possible parameters that a standard material 
 * would need. It contains 4 important sections: rdpq parameters, opengl 
 * parameters, global, and custom parameters (a material extension).
 * 
 */
typedef struct material64_s {
	//char* name;                         ///< Name of the material
	sprite_t*           texture;    ///< Main sprite texture associated with this material
	int   				alphamode;  ///< Material's alpha mode when rendering (opaque, alpha cutout or blend)
    //bool                zbuffer;    ///< True if material should read and write to zbuffer
	struct {
		bool            lighting;	///< True if lighting calculations need to be active
		bool            shading;	///< True if vertex colors need to be enabled
        //bool            fog;        ///< True if fog calculations need to be active for this material
		struct {
			mat64_gl_color_t       ambient, diffuse, specular, emissive;	///< OpenGL material parameters
			float       shininess;
		} m;				        ///< OpenGL material lighting parameters
		GLenum          culling;	///< OpenGL culling mode for triangles (front, back, none, or both)
		GLenum          texgen;		///< OpenGL texture generation mode (environment mapping)
	} opengl;				        ///< Section of OpenGL parameters of a material
	struct {
		sprite_t* 	    multitexture;	///< Additional sprite texture to be used for multitexturing
		rdpq_combiner_t combiner;	    ///< RDP Color Combiner mode
		rdpq_blender_t	blender;	    ///< RDP Color Blender mode
		struct {
			color_t     primitive, environment, blend, fog;	///< RDP color register that can be used in Color Combiner/Color Blender
			int     	primlod;				
		} regs;				        ///< Different RDP color/value registers that can be used in Color Combiner/Color Blender
	} rdpq;					        ///< Section of RDPQ parameters of a material
	material64_ext_t        extension;	///< Pointer to a user-defined structure to be used as an extension to the material data
} material64_t;

typedef void (*material64_ext_load)(void *ctx, const char *key, const char *value);
typedef void (*material64_ext_upload)(material64_t *material);

material64_t *material64_load(const char *fn);
material64_t *material64_load_buf(void *buf, int sz);
void material64_free(material64_t *material);

/**
 * @brief Define an extension that can be used with custom material parameters
 */
void material64_define_extension(size_t struct_size, material64_ext_load load_handler, material64_ext_upload upload_handler);

/**
 * @brief Remove a defined extension to the material structure
 */
void material64_remove_extension();

/**
 * @brief Get the extension data that contains custom material parameters
 */
material64_ext_t* material64_get_extension(material64_t* material);

/**
 * @brief Upload the current material with its settings (with extension invoked beforehand) as rdpq/opengl commands
 */
void material64_upload(material64_t* material);


/// Decoder helpers to be used in loading a material extension from text.

// TODO decode str to color_t
inline color_t strclr(const char* str){
    int r = 0, g = 0, b = 0, a = 255;
    float rf = 0.0, gf = 0.0, bf = 0.0, af = 255.0;
    if(sscanf(str, "%d, %d, %d, %d", &r, &g, &b, &a) >= 3)
        return RGBA32(r,g,b,a);
    if(sscanf(str, "%d %d %d %d", &r, &g, &b, &a) >= 3)
        return RGBA32(r,g,b,a);
    if(sscanf(str, "%f, %f, %f, %f", &rf, &gf, &bf, &af) >= 3)
        return RGBA32(rf*255,gf*255,bf*255,af*255);
    if(sscanf(str, "%f %f %f %f", &rf, &gf, &bf, &af) >= 3)
        return RGBA32(rf*255,gf*255,bf*255,af*255);
    assertf(0, "Color string is invalid: %s", str);
}

// TODO decode str to gl_color_t
inline mat64_gl_color_t strclr_gl(const char* str){
	mat64_gl_color_t clr = {0};
    if(sscanf(str, "%f, %f, %f, %f", &(clr.c[0]), &(clr.c[1]), &(clr.c[2]), &(clr.c[3])) >= 3)
        return clr;
    if(sscanf(str, "%f %f %f %f", &(clr.c[0]), &(clr.c[1]), &(clr.c[2]), &(clr.c[3])) >= 3)
        return clr;
    assertf(0, "GL Color string is invalid: %s", str);
	return clr;
}

#define MATERIAL64_DECODE_BOOL(key, value, name, variable) 			if(strcmp(key, name) == 0) {variable = strcmp(value, "true")? false : true; return 1;}
#define MATERIAL64_DECODE_INTEGER(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = atoi(value); return 1;}
#define MATERIAL64_DECODE_FLOAT(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = (float)atof(value); return 1;}
#define MATERIAL64_DECODE_STRING(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = value; return 1;}
#define MATERIAL64_DECODE_TEXTURE(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = sprite_load(value); return 1;}
#define MATERIAL64_DECODE_COLOR(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = strclr(value); return 1;}
#define MATERIAL64_DECODE_GL_COLOR(key, value, name, variable) 		if(strcmp(key, name) == 0) {variable = strclr_gl(value); return 1;}

#ifdef __cplusplus
}
#endif

#endif

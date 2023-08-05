#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "GL/gl.h"
#include "graphics.h"
#include "sprite.h"
#include "rdpq.h"
#include "rdpq_mode.h"
#include "rdpq_sprite.h"
#include "asset.h"
#include "material64.h"
#include "material64_internal.h"

typedef struct{
	bool   defined;
	size_t struct_size;
	material64_ext_load load_handler;
	material64_ext_upload upload_handler;
} material64_ext_state_t;
material64_ext_state_t extensionstate = (material64_ext_state_t){0};

#define PTR_DECODE(material, ptr)    ((void*)(((uint8_t*)(material)) + (uint32_t)(ptr)))
#define PTR_ENCODE(material, ptr)    ((void*)(((uint8_t*)(ptr)) - (uint32_t)(material)))

void material64_init_ext(material64_t* material){
	if(extensionstate.defined && extensionstate.struct_size > 0){
		material->extension.buffer = malloc(extensionstate.struct_size);
		material->extension.size = extensionstate.struct_size;
	}
}

void material64_parse_extension(material64_t *mat, material64_ext_load parser, const char *str) {
    void *extstruct = mat->extension.buffer;
    if (!extstruct || !str) return;
    while (*str) {
        const char *key = str;
        const char *value = key + strlen(key) + 1;
        parser(extstruct, key, value);
        str = value + strlen(value) + 1;
    }
}

material64_t *material64_load_buf(void *buf, int sz){
	mat64file_t *matfile = buf;
    assertf(matfile->magic != MAT64_MAGIC_LOADED, 	"Trying to load already loaded matetial data (buf=%p, sz=%08x)", buf, sz);
    assertf(matfile->magic == MAT64_MAGIC, 			"invalid matetial data (magic: %08lx)", matfile->magic);
	assertf(matfile->version == MAT64_VERSION, 		"invalid matetial file version (%lu), please regenerate your assets", matfile->version);

	material64_t *material = (material64_t*)malloc(sizeof(material64_t));
	memcpy(material, &matfile->material, sizeof(material64_t));

	if(material->texture){
		const char* texture = (const char*)PTR_DECODE(material, material->texture);
		material->texture = sprite_load(texture);
	}
	if(material->rdpq.multitexture){
		const char* multitexture = (const char*)PTR_DECODE(material, material->rdpq.multitexture);
		material->rdpq.multitexture = sprite_load(multitexture);
	}

	if(extensionstate.defined && extensionstate.load_handler){
    	const char* extensiondata = (const char*)PTR_DECODE(matfile, matfile->extensiondata);
		material64_init_ext(material);
		material64_parse_extension(material, extensionstate.load_handler, extensiondata);
	}

	matfile->magic = MAT64_MAGIC_LOADED;
	return material;
}

material64_t *material64_load(const char *fn){
	int sz;
    void *buf = asset_load(fn, &sz);
    material64_t *material = material64_load_buf(buf, sz);
	free(buf);
    return material;
}

void material64_free(material64_t *material){
	if(material){
		if(material->texture) sprite_free(material->texture);
		if(material->rdpq.multitexture) sprite_free(material->rdpq.multitexture);
		if(material->extension.buffer) free(material->extension.buffer);
		free(material);
	}
}

void material64_define_extension(size_t struct_size, material64_ext_load load_handler, material64_ext_upload upload_handler){
	extensionstate.struct_size = struct_size;
	extensionstate.load_handler = load_handler;
	extensionstate.upload_handler = upload_handler;
	extensionstate.defined = true;
}

/**
 * @brief Remove a defined extension to the material structure
 */
void material64_remove_extension(){
	extensionstate.struct_size = 0;
	extensionstate.load_handler = NULL;
	extensionstate.upload_handler = NULL;
	extensionstate.defined = false;
}

/**
 * @brief Get the extension data that contains custom material parameters
 */
material64_ext_t* material64_get_extension(material64_t* material){
	return &material->extension;
}

/**
 * @brief Upload the current material with its settings (with extension invoked beforehand) as rdpq/opengl commands
 */
void material64_upload(material64_t* material){
	if(extensionstate.defined && extensionstate.upload_handler) 
		extensionstate.upload_handler(material);

	bool opengl_context = ...;// TODO get context
	bool opengl_rdpq_material = false;
	bool opengl_rdpq_texturing = false;

	if(opengl_context){
		(material->opengl.lighting? glEnable : glDisable)(GL_LIGHTING);
		(material->opengl.shading? glEnable : glDisable)(GL_COLOR_MATERIAL);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, material->opengl.m.shininess);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, &material->opengl.m.diffuse);
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, &material->opengl.m.ambient);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, &material->opengl.m.specular);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, &material->opengl.m.emissive);
		glCullFace(material->opengl.culling);
		// TODO texgen
		(material->rdpq.combiner != MATERIAL64_INVALID_COMBINER || material->rdpq.blender != MATERIAL64_INVALID_BLENDER? glEnable : glDisable)(GL_RDPQ_MATERIAL_N64);
		if(material->rdpq.multitexture){
			glEnable(GL_RDPQ_TEXTURING_N64);
			opengl_rdpq_material = true;
		} else glDisable(GL_RDPQ_TEXTURING_N64);
	}
	else{
		if(material->texture && !material->multitexture) 
			rdpq_sprite_upload(TILE0, material->texture, NULL);
		if(material->rdpq.combiner != MATERIAL64_INVALID_COMBINER)
			rdpq_mode_combiner(material->rdpq.combiner);
		if(material->rdpq.blender != MATERIAL64_INVALID_BLENDER)
			rdpq_mode_blender(material->rdpq.blender);
	}
}


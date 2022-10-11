#ifndef NK_MRE_H_
#define NK_MRE_H_ 

#include "nuklear.h"
#include "mre_menu.h"

enum component_type {
	NK_MRE_COMPONENT_BUTTON,
	NK_MRE_COMPONENT_INPUT_TEXT,
	NK_MRE_COMPONENT_TEXT,
	NK_MRE_COMPONENT_IMAGE,
	NK_MRE_COMPONENT_VIDEO,
	NK_MRE_COMPONENT_CHECKBOX,
	NK_MRE_COMPONENT_SEPARATOR,
};

struct MreFont{
	struct nk_user_font nk;
	int size;
};

struct FocusableObjects{
	int id;
	struct nk_rect area;
};

// new

#define NK_MRE_COLOR_WHITE VM_RGB565_ARGB(255, 255, 255, 255)
#define NK_MRE_COLOR_BLACK VM_RGB565_ARGB(255, 0, 0, 0)
#define NK_MRE_COLOR_BITCOIN_ORANGE VM_RGB565_ARGB(255, 242, 169, 0)
#define NK_MRE_COLOR_BITCOIN_ORANGE_LIGHT VM_RGB565_ARGB(255, 242, 200, 0)
#define NK_MRE_COLOR_BITCOIN_ORANGE_DARK VM_RGB565_ARGB(255, 242, 140, 0)

struct mre_nk_component {
	int type;	// component_type
	char *text;
	int is_hoverable;
	int hovering;
	int checked;
	int color; // VM_RGB565_ARGB
	int hovering_color;
	int text_color;
	int (*action)(int component_index);
};

struct mre_nk_view {
	int id;
	int previous_id;
	int background_color;
	char *title;
	struct mre_nk_component **components;
};

/////
struct mre_nk_c {
	int id;
	int type;	//type of component
	char *title;
	int len;
	//Placeholder. Not using style at the moment.
	//struct nk_style style; 
	int hovering;
	int is_hoverable;
	/* For Media assets */ 
	char *url;
	char *thumbnail;
};


/* 
MRE View Properties. The list of components and hoverable counter. 
The view controller will process through this object.
*/	
struct mre_view_p{
	int hoverable_counter;
	int hoverable_items;
	struct mre_nk_c **components;
	//struct mre_nk_c * components;
	int components_count;
};


/* Create the mre display object */
struct mre_t {
	//some sort of window object - most similar in mre is layers?
	int window;
	//int memory_mre;
	unsigned int width;
	unsigned int height;
	struct nk_context ctx;

	//Focusable Object Areas:
	struct nk_rect fObjs[20]; 
} ;

void initiate_nk_gui(void);
void update_gui();
void set_test2_view();

/* Font */
typedef struct MreFont MreFont; //Gotta do this for it to know it exists
MreFont* nk_mrefont_create(const char *name, int size);
void nk_mre_set_font();
void nk_mrefont_del();

/* Declare the implementation:*/
struct nk_context* nk_mre_init(MreFont *font,unsigned int width, unsigned int height);
void nk_mre_handle_sys_event(VMINT message, VMINT param);
void nk_mre_handle_key_event(VMINT message, VMINT keycode);
void nk_mre_handle_penevt (VMINT event, VMINT x, VMINT y);
void nk_mre_render(struct nk_color clear);
void nk_mre_shutdown(void);

/*Platform specific*/
int nk_init_mre(struct nk_context *ctx, const struct nk_user_font *font);
void* mre_malloc(nk_handle unused, void *old, nk_size size); //nk_handle: A pointer, an int id; nk_size: unsigned long
void mre_free(nk_handle unused, void* old); //nk_handle: A pointer, an int id

struct mre_nk_c * mre_nk_c_create(int type, char *title, int len, int hovering, char *url, char *thumb);


#endif
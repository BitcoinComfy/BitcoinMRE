#include "share.h"
#include "nuklear_mre.h"
#include "rand.h"
#include "vmche.h"
#include "vmcamera.h"
#include "sha2.h"
#define CBC 1
#define AES256 1
#include "aes.h"
#include "pkcs7_padding.h"
#include "reedsolomon.h"
#include "bitcoin_mre.h"
#include "bip39.h"
#include "zbar.h"

int g_wallet_type;

#define MAX_WORD_LEN_240x320 23
#define GRAPHIC_ELEMENT_HEIGHT 31


#ifdef TRY_TESTNET
// change entropy gen from 255 to 5
// just for testing, disable in production
#define QUICK_ENTROPY_TEST 1
#endif

#ifdef QUICK_ENTROPY_TEST
#define TEXT_ENTROPY_LEN 5
#else
#define TEXT_ENTROPY_LEN 255
#endif

#define FONT_SIZE 9

	struct mre_nk_view g_view[NK_MRE_VIEW_MAX_ID];
	struct mre_nk_view* get_mre_nk_view();
	//void set_view(int view);
	int input_text(int component_index);
	int enter_entropy(int component_index);
	int flip_checkbox(int component_index);
	void get_hoovers(int *curr_hoover, int *prev_hoover, int *nex_hoover);
	int next_view_gen_entropy(int component_index);

	void create_view_create_3();
	void replace_mnemo_view_create_3(char* mnemonics);

	void create_encrypted_wallet(char* wallet_name, uint32_t type, char* wallet_data, uint32_t wallet_data_len, VMWSTR password);
	bool load_encrypted_wallet(VMWSTR w_wallet_path, uint32_t *type, char** wallet_data, uint32_t* wallet_data_len, VMWSTR password);

	int select_wallet(int component_index);
	int recover_wallet(int component_index);
	int create_wallet(int component_index);

	VMINT	layer_handle[1];//layer handle array.
	VMINT	layer_hdl[1];	//layer handle array.
	VMINT	g_mre_drv;

	int g_callback_none = false;
	int g_camera_preview = false;
	VM_CAMERA_HANDLE g_camera_handle = -1;
	
#define QR_ARR_MAX_SIZE 1000 // 1 + 999 // part1of2, starts from 1, let's not mess with indexes and keep 0 empty
	char* g_last_decoded_qr_arr[QR_ARR_MAX_SIZE] = {0};
	char* g_last_decoded_qr = NULL;
	//int g_after_camera_next_view = NK_MRE_VIEW_NONE;
	void (*g_after_camera_action)(void) = NULL;


#ifdef TRY_TESTNET
int dbg_init = 0;
VMWCHAR w_dbg_path[260];
char dbg_path[260];

void dbg(char* format, ...)
{
	VMFILE handle = -1;
	VMUINT written = 0;

	char buffer[1024];
	va_list aptr;
	va_start(aptr, format);
	buffer[0] = 0;
	vm_vsprintf(buffer, format, aptr);

	if (dbg_init == 0)
	{
		sprintf(dbg_path, "%c:\\btc_mre_wallet\\dbg.txt", mre_get_drv());
		vm_ascii_to_ucs2(w_dbg_path, sizeof(w_dbg_path), dbg_path);
		handle = vm_file_open(w_dbg_path, MODE_CREATE_ALWAYS_WRITE, FALSE);
		dbg_init = 1;
	} else {
		handle = vm_file_open(w_dbg_path, MODE_APPEND, FALSE);
	}
	if (handle > 0)
	{
		vm_file_write(handle, buffer, strlen(buffer), &written);
		vm_file_close(handle);
	} 

	va_end(aptr);
}
#else
void dbg(char* format, ...) {do{}while(0);};
#endif

char* my_strdup(char* in)
{
	char* new_alloc = NULL;
	if (in == NULL)
	{
		new_alloc = vm_calloc(4);
	} else {
		new_alloc = vm_malloc(strlen(in)+1);
		strcpy(new_alloc, in);
	}
	return new_alloc;
}

void clean_last_decoded_qr()
{
	int i=0;
	dbg("[+] clean_last_decoded_qr now\n");
	if (g_last_decoded_qr)
	{
		dbg("[+] g_last_decoded_qr: %p\n", g_last_decoded_qr);
		dbg("[+] g_last_decoded_qr: %s\n", g_last_decoded_qr);
		memset(g_last_decoded_qr, 0, strlen(g_last_decoded_qr));
		vm_free(g_last_decoded_qr);
		g_last_decoded_qr = NULL;
	}
	dbg("[+] g_last_decoded_qr cleaned\n");
	for (i=0; i<QR_ARR_MAX_SIZE;i++)
	{
		char* tmp = g_last_decoded_qr_arr[i];
		if (tmp)
		{
			dbg("[+] g_last_decoded_qr_arr[%d]=%s\n", i, tmp);
			memset(tmp, 0, strlen(tmp));
			vm_free(tmp);
			g_last_decoded_qr_arr[i] = NULL;
			dbg("[+] g_last_decoded_qr_arr[%d] done\n", i);
		}
	}
	dbg("[+] g_last_decoded_qr done\n");
};

void stop_preview()
{
	int res = vm_camera_preview_stop(g_camera_handle);
}

void stop_camera()
{
	int i = 0;
	int res = 0;
	int layer_count = 0;

	res = vm_release_camera_instance(g_camera_handle);

	layer_count = vm_graphic_get_layer_count();
	//dbg("[+] vm_graphic_get_layer_count: %d\n", layer_count);

	for (i=0; i<layer_count; i++)
	{
		res = vm_graphic_delete_layer(i); 
		//dbg("[+] vm_graphic_delete_layer[%d]: %d\n", i, res);
	}

	g_camera_preview = false;
	g_camera_handle = -1;
	g_callback_none = false;
	vm_reg_keyboard_callback (nk_mre_handle_key_event);
	update_gui();
}

void
nk_mre_handle_key_event_none(VMINT event, VMINT keycode)
{
	if (event == VM_KEY_EVENT_UP)
	{
		if (g_camera_preview && g_camera_handle >= 0)
		{
			stop_preview();
		}
	}
}


	/* 
		MRE View Properties. The list of components and hoverable counter. 
		The view controller will process through this object.
	*/	
	struct mre_view_p mre_view;

	/* Create the mre display object */
	struct mre_t mre;

	void* mre_malloc(nk_handle unused, void *old, nk_size size){
		NK_UNUSED(unused);
		NK_UNUSED(old);
		return (void *)vm_monitored_malloc(size); 
	}

	void mre_free(nk_handle unused, void *ptr){
		NK_UNUSED(unused);
		vm_free(ptr);
	}

	/*
		Initialises the context - MRE way
		We have to call this instead of nk_init_default (which is part of nuklear..h)
		because that method uses STL and we have to make our own implementation of it	
	*/
	int
	nk_init_mre(struct nk_context *ctx, const struct nk_user_font *font) //similar to nk_init_default
	{
		/*
			nk_allocator:
				a. nk_alloc Obj: userdata
					i. A pointer
					ii. An id
				b. alloc ()
				c. free ()

			Steps:
			1. Set the pointer to a value
			2. Set alloc method
			3. Set Free Method
			4. Call nk_init(ctx, font) to begin process

		*/

		struct nk_allocator alloc;
		alloc.userdata.ptr = 0;
		/*	Assign implementation's alloc and malloc
			alloc(nk_handle, void *old, nk_size)
			free(nk_handle, void*) 
			Make sure they work OK
		*/ 
		alloc.alloc = mre_malloc;
		alloc.free = mre_free;

		//NOT ALLOWING FONT TO BE SET HERE. 
		//return nk_init(ctx, &alloc, font);


		return nk_init(ctx, &alloc, font);

		/*Origi code: 
		struct nk_allocator alloc;
		alloc.userdata.ptr = 0;
		alloc.alloc = nk_malloc; //(nk_handle, void *old, nk_size);
		alloc.free = nk_mfree;   //(nk_handle, void*)
		return nk_init(ctx, &alloc, font);*/
	}

	/*
		Convert nuklear's color into implementation color!
		 Color being used for all that drawing we gotta do
		 Used internally, not an implementation 
		 Can be called anything..
	*/
	static VMINT 
	convert_color(struct nk_color c){
		//..convert nk_color c to MRE color..
		//returning red for now..
		//return VM_RGB32_ARGB(c.a, c.r, c.g, c.b);
		return VM_RGB565_ARGB(c.a, c.r, c.g, c.b);
		//return VM_COLOR_RED;
	}


	/***********************************************
		IMPL FUNCTIONS

		Stroke: is line drawing
		Fill: is line drawing + the area under the 
				outline (usually color)
	***********************************************/

	/*
		Scissor
		Something to do with scissor ? 
		Called by this command: NK_COMMAND_SCISSOR
	*/
	static void 
	nk_mre_scissor(void){
		vm_log_debug("Scissor-ing..");
	}

	/*
		Stroke line
		Draw a line as stroke (outline)
		Called by command: NK_COMMAND_LINE
	*/ 
	static void
	nk_mre_stroke_line(short x0, short y0, short x1, short y1, 
						unsigned int line_thickness, struct nk_color col)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		VMINT LINE_X1, LINE_X2, LINE_Y1, LINE_Y2;
		//Have the layer handle ready.. layer_hdl
		//Maybe as global or passed here..

		/* 
			get the target buffer from the layer
		*/
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//No need to fill a rectangle here..
		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//					 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_GREEN);

		color = VM_COLOR_RED;
		color = convert_color(col);
		LINE_X1 = x0; 
		LINE_X2 = x1;
		LINE_Y1 = y0;
		LINE_Y2 = y1;
		//line_thickness not implemented..

		vm_graphic_line(buffer, LINE_X1, LINE_Y1, LINE_X2, LINE_Y2, color);
		/* 
			Gotta flush 
			Flush updates the LCD
		*/
		//vm_graphic_flush_layer(layer_hdl, 1);
		vm_log_debug("draw line fun ends");
	}

	/*
		Stroke Rectangle
		Draw a rectangle as a stroke (outline)
		Called by: NK_COMMAND_RECT
	*/
	static void
	nk_mre_stroke_rect(short x, short y, unsigned short w,
		unsigned short h, unsigned short r, unsigned short line_thickness, struct nk_color col)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		VMINT REC_X, REC_Y, REC_W, REC_L;

		color = convert_color(col);
		REC_X =	x;
		REC_Y = y;
		REC_W = w;
		REC_L = h;

		/*Get the target buffer */
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//                 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_BLUE);

		vm_graphic_rect(buffer, REC_X, REC_Y, REC_W, REC_L, color);
		/* 
			Gotta flush:
			Flush updates the LCD 
		*/
		//vm_graphic_flush_layer(layer_hdl, 1); 

	}

	/*
		Fill Rectangle
		Draws a rectangle outline and color fills it
		Called by command: NK_COMMAND_RECT_FILLED
	*/
	static void
	nk_mre_fill_rect(short x, short y, unsigned short w,
		unsigned short h, unsigned short r, struct nk_color col)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		VMINT REC_X, REC_Y, REC_W, REC_L;

		color = convert_color(col);
		REC_X =	x;
		REC_Y = y;
		REC_W = w;
		REC_L = h;

		/*Get the target buffer */
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//                 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_BLUE);

		//vm_graphic_rect(buffer, REC_X, REC_Y, REC_W, REC_L, color);
		vm_graphic_fill_rect(buffer, REC_X, REC_Y, REC_W, REC_L, color, color);

		//Ignoring filled one first
		//vm_graphic_fill_rect(buffer, REC_X, REC_Y, REC_W, REC_L, color, color);
		/* 
			Gotta flush:
			Flush updates the LCD 
		*/
		//vm_graphic_flush_layer(layer_hdl, 1); 

	}

	/*
		Fill Triangle
		Draws a triangle and fills area in between with color
		Called by command: NK_COMMAND_TRIANGLE_FILLED
	*/
	static void
	nk_mre_fill_triangle(short x0, short y0, short x1, short y1,
		short x2, short y2, struct nk_color colr)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		VMINT TRN_X1, TRN_X2, TRN_X3, TRN_Y1, TRN_Y2, TRN_Y3;
		vm_graphic_color col;

		//vm_graphic_color col;
		vm_graphic_point points[3];

		color = convert_color(colr);
		TRN_X1 = x0;
		TRN_X2 = x1;
		TRN_X3 = x2;
		TRN_Y1 = y0;
		TRN_Y2 = y1;
		TRN_Y3 = y2;

		/* function body */
		vm_log_debug("draw triangle starts");
		points[0].x = TRN_X1;
		points[0].y = TRN_Y1;
		points[1].x = TRN_X2;
		points[1].y = TRN_Y2;
		points[2].x = TRN_X3;
		points[2].y = TRN_Y3;
	   
		col.vm_color_565 = VM_COLOR_RED;
		col.vm_color_888 = VM_COLOR_565_TO_888(VM_COLOR_RED);
		/* sets global color */
		vm_graphic_setcolor(&col);

		/* get the target buffer*/
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//					 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_GREEN);

		/* Draw 3 points from points on the layer */
		vm_graphic_fill_polygon(layer_hdl[0], points, 3);

		/* Gotta flush */
		//vm_graphic_flush_layer(layer_hdl, 1);

	}

	/*	
		Stroke Triangle
		Draws the outline of the triangle
		Called by command: NK_COMMAND_TRIANGLE
	*/
	static void
	nk_mre_stroke_triangle(short x0, short y0, short x1, short y1,
		short x2, short y2, unsigned short line_thickness, struct nk_color colr)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		VMINT TRN_X1, TRN_X2, TRN_X3, TRN_Y1, TRN_Y2, TRN_Y3;

		vm_graphic_color col;
		vm_graphic_point points[3];

		color = convert_color(colr);
		TRN_X1 = x0;
		TRN_X2 = x1;
		TRN_X3 = x2;
		TRN_Y1 = y0;
		TRN_Y2 = y1;
		TRN_Y3 = y2;

		/* function body */
		vm_log_debug("draw triangle starts");
		points[0].x = TRN_X1;
		points[0].y = TRN_Y1;
		points[1].x = TRN_X2;
		points[1].y = TRN_Y2;
		points[2].x = TRN_X3;
		points[2].y = TRN_Y3;
	   
		col.vm_color_565 = VM_COLOR_RED;
		col.vm_color_888 = VM_COLOR_565_TO_888(VM_COLOR_RED);

		/* sets global color */
		//vm_graphic_setcolor(&col);

		/* get the target buffer*/
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//					 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_GREEN);

		/* Draw 3 points from points on the layer */
		vm_graphic_fill_polygon(layer_hdl[0], points, 3);

		/* Gotta flush */
		//vm_graphic_flush_layer(layer_hdl, 1);

	}

	/*
		Fill Polygon
		Draws polygon with count number from points and fills color in 
		Called by command: NK_COMMAND_POLYGON_FILLED
	*/
	static void
	nk_mre_fill_polygon(const struct nk_vec2i *pnts, int count, struct nk_color colr)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		
		/*int a=42;
		const int b=a;
		int ipoints[b]; //Wont work because C89
		*/

		//int *a = new int[count]; //Not C syntax. More like c++
		//vm_graphic_point points[10]; //Works because number is known at the time of compilation

		//The way dynamically loading works:
		//char* utf8 = malloc(utf8size);
		int pointsize = count * sizeof(vm_graphic_point);
		vm_graphic_point* points = vm_monitored_malloc(pointsize);

		//VMINT TRN_X1, TRN_X2, TRN_X3, TRN_Y1, TRN_Y2, TRN_Y3;

		vm_graphic_color col;
		
		color = convert_color(colr);
		/*TRN_X1 = x0;
		TRN_X2 = x1;
		TRN_X3 = x2;
		TRN_Y1 = y0;
		TRN_Y2 = y1;
		TRN_Y3 = y2;*/

		/* function body */
		vm_log_debug("draw polygon starts");

		/*for every point in nk_vec2i pnts .. put value in points..*/

		/*points[0].x = TRN_X1;
		points[0].y = TRN_Y1;
		points[1].x = TRN_X2;
		points[1].y = TRN_Y2;
		points[2].x = TRN_X3;
		points[2].y = TRN_Y3;*/
	   
		col.vm_color_565 = VM_COLOR_RED;
		col.vm_color_888 = VM_COLOR_565_TO_888(VM_COLOR_RED);

		/* sets global color */
		vm_graphic_setcolor(&col);

		/* get the target buffer*/
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//					 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_GREEN);

		/* Draw 3 points from points on the layer */
		vm_graphic_fill_polygon(layer_hdl[0], points, 3);

		/* Gotta flush */
		//vm_graphic_flush_layer(layer_hdl, 1);
	}

	/*
		Stroke Polygon
		Draws polygon of count points from points as outline
		Called by command: NK_COMMAND_POLYGON
	*/
	static void
	nk_mre_stroke_polygon(const struct nk_vec2i *pnts, int count, 
		unsigned short line_thickness, struct nk_color colr)
	{
			
		VMUINT8 *buffer;
		VMUINT16 color;
		//VMINT TRN_X1, TRN_X2, TRN_X3, TRN_Y1, TRN_Y2, TRN_Y3;

		vm_graphic_color col;
		vm_graphic_point points[3];

		color = convert_color(colr);

		/*TRN_X1 = x0;
		TRN_X2 = x1;
		TRN_X3 = x2;
		TRN_Y1 = y0;
		TRN_Y2 = y1;
		TRN_Y3 = y2;*/

		/* function body */
		vm_log_debug("draw triangle starts");

		/* for every points in vec2i pts put value in points..*/

		/*points[0].x = TRN_X1;
		points[0].y = TRN_Y1;
		points[1].x = TRN_X2;
		points[1].y = TRN_Y2;
		points[2].x = TRN_X3;
		points[2].y = TRN_Y3;*/
	   
		col.vm_color_565 = VM_COLOR_RED;
		col.vm_color_888 = VM_COLOR_565_TO_888(VM_COLOR_RED);

		/* sets global color */
		//vm_graphic_setcolor(&col);

		/* get the target buffer*/
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//					 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_GREEN);

		/* Draw 3 points from points on the layer */
		vm_graphic_fill_polygon(layer_hdl[0], points, 3);

		/* Gotta flush */
		//vm_graphic_flush_layer(layer_hdl, 1);

	}

	/*
		Stroke PolyLine
		Draws a polyline as outline
		Called by command: NK_COMMAND_POLYLINE
	*/
	static void
	nk_mre_stroke_polyline(const struct nk_vec2i *pnts, int count,
		unsigned short line_thickness, struct nk_color col)
	{
		//VMUINT8 *buffer;
		VMUINT16 color;

		color = convert_color(col);

		if (count > 0) {
			int i;
			//MoveToEx(dc, pnts[0].x, pnts[0].y, NULL);
			for (i = 1; i < count; ++i)
			{
				//LineTo(dc, pnts[i].x, pnts[i].y);
				//Replace this with draw line
			}
		}

	}

	/*	
		Fill Circle
		Draws a circle and fills the area inside of it
		Called by command: NK_COMMAND_CIRCLE_FILLED
	*/
	static void
	nk_mre_fill_circle(short x, short y, unsigned short w,
		unsigned short h, struct nk_color col)
	{
		//VMUINT8 *buffer;
		VMUINT16 color;
		
		color = convert_color(col);
		

	}

	/*
		Stroke Circle
		Draws a circle's outline
		Called by command: NK_COMMAND_CIRCLE
	*/
	static void
	nk_mre_stroke_circle(short x, short y, unsigned short w,
		unsigned short h, unsigned short line_thickness, struct nk_color col)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		VMINT REC_X, REC_Y, REC_W, REC_L;

		color = convert_color(col);
		REC_X =	x;
		REC_Y = y;
		REC_W = w;
		REC_L = h;

		/*Get the target buffer */
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//                 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_BLUE);

		//void vm_graphic_ellipse (VMUINT8 * buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 color);
		/*TODO: Figure this out */
		//vm_graphic_ellipse(buffer,...);

		/* 
			Gotta flush:
			Flush updates the LCD 
		*/
		//vm_graphic_flush_layer(layer_hdl, 1); 

	}

	/*
		Stroke Curve
		Draws a curve outline
		Called by command: NK_COMMAND_CURVE
	*/
	static void
	nk_mre_stroke_curve(struct nk_vec2i p1, struct nk_vec2i p2, 
		struct nk_vec2i p3, struct nk_vec2i p4, unsigned short line_thickness,
			struct nk_color col)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		//VMINT REC_X, REC_Y, REC_W, REC_L;

		color = convert_color(col);
		
		/*Get the target buffer */
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		//vm_graphic_fill_rect(buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width(), 
		//                 vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_BLUE);

		//void vm_graphic_ellipse (VMUINT8 * buf, VMINT x, VMINT y, VMINT width, VMINT height, VMUINT16 color);
		/*TODO: Figure this out*/
		//vm_graphic_fill_ellipse(buffer,...);

		/* 
			Gotta flush:
			Flush updates the LCD 
		*/
		//vm_graphic_flush_layer(layer_hdl, 1); 

	}

	/*
		Draw Text
		Draws text
		Called by command: NK_COMMAND_TEXT
	*/
	static void
	nk_mre_draw_text(short x, short y, unsigned short w, unsigned short h, 
		const char *text, int len, struct nk_color cbg, struct nk_color col)
	{
		VMUINT8 *buffer;
		VMUINT16 color;
		VMWCHAR display_string[MRE_STR_SIZE_MAX];
		color = convert_color(col);
		
		/*Get the target buffer */
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		/*
			Text it out
			We need to convert strings to usc2 format that 
			MRE only suppports 
			Then we draw the text on the screen
		*/
		/* converts string into usc2 format to display on the screen */
		vm_gb2312_to_ucs2(display_string, MRE_STR_SIZE_MAX, (VMSTR)text);

		/* 
			Draw Text 
			*  disp_buf : [IN] display buffer pointer 
			* x : [IN] x offset of start position
			* y : [IN] y offset of start position
			* s : [IN] string pointer
			* length : [IN] string length
			* color : [IN] string color
		*/
		vm_graphic_textout(buffer, x, 
			y, display_string, 
			wstrlen(display_string), VM_COLOR_WHITE);

		/* 
			Gotta flush:
			Flush updates the LCD 
		*/
		//vm_graphic_flush_layer(layer_hdl, 1);


	}

	/*
		Draw Default Image
		Draws defaut image. Used in testing
		Called by command: NK_COMMAND_IMAGE
	*/
	static void
	nk_mre_draw_image_default(short x, short y, short w, short h)
	{
		//struct nk_image img, int x, int y, const char *text
		VMUINT8 *buffer;
		//VMUINT16 color;
		//VMWSTR filename;
		//VMWCHAR display_string[MRE_STR_SIZE_MAX];
		//VMINT res;
		int ret;
		/* File name related variables */
		VMWSTR wfilename;
		VMINT wfilename_size;
		VMSTR filename;
		VMCHAR f_name[MRE_STR_SIZE_MAX + 1];	//Old usage for video filename string
		VMWCHAR f_wname[MRE_STR_SIZE_MAX + 1]; //Old usage for video filename
		VMSTR file_name = "tips.gif";

		
		/*Get the target buffer */
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		/* 
			Gotta flush:
			Flush updates the LCD 
		*/
		//vm_graphic_flush_layer(layer_hdl, 1);

		/* Draw the PIKCHAAA */ 
		/* Notes:
			VM_GDI_HANDlE is basically VMINT

		*/
		//VMINT vm_graphic_draw_image_from_file(VM_GDI_HANDLE dest_layer_handle, 
		//	VMINT x, VMINT y, const VMWSTR filename);
		//Testing:

		/* Getting em strings ready */
			//Gotta allocate VMSTR before we do anything to it!
			filename = vm_monitored_malloc(MRE_STR_SIZE_MAX);
			sprintf (filename, "%c:\\%s", vm_get_removable_driver(), file_name);
			//Gotta allocate VMWSTR before we do anything to it!
			wfilename_size = (strlen(filename) + 1) * 2;
			wfilename = vm_monitored_malloc(wfilename_size);
			sprintf(f_name, "%c:\\%s", vm_get_removable_driver(), file_name);

		/* String format conversion */
			vm_ascii_to_ucs2 (wfilename, MRE_STR_SIZE_MAX, filename);
			vm_ascii_to_ucs2(f_wname, MRE_STR_SIZE_MAX, f_name);

		ret = vm_graphic_draw_image_from_file(layer_handle[0],
			//80, 80, "E:\\tips.gif");
			80, 80, f_wname);
			//80, 80, "E:\umicon.jpg");
		if (ret == VM_GDI_SUCCEED){
			printf("Drew image?"); //Not sure what happens in there.
			mre_show_image(MRE_GRAPHIC_IMAGE_CURRENT, f_wname, file_name, layer_hdl, x, y);
		}else{
			printf("Couldn't draw image..");
		}

	}

	/*
		Draw Image
		Draws image set in image object
		Called by command: NK_COMMAND_IMAGE
	*/
	static void
	nk_mre_draw_image(short x, short y, short w, short h, struct nk_image img)
	{
		//struct nk_image img, int x, int y, const char *text
		VMUINT8 *buffer;
		//VMUINT16 color;
		//VMWSTR filename;
		//VMWCHAR display_string[MRE_STR_SIZE_MAX];
		//VMINT res;
		int ret;
		/* File name related variables */
		//VMWSTR wfilename;
		//VMINT wfilename_size;
		//VMSTR filename;
		//VMCHAR f_name[MRE_STR_SIZE_MAX + 1];	//Old usage for video filename string
		//VMWCHAR f_wname[MRE_STR_SIZE_MAX + 1]; //Old usage for video filename
		VMWCHAR image_wpath[MRE_STR_SIZE_MAX +1];
		//VMSTR file_name = "tips.gif";
		VMSTR file_name;

		/* Play with nk_image object */
		
		
		/*Get the target buffer */
		buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		/* 
			Gotta flush:
			Flush updates the LCD 
		*/
		//vm_graphic_flush_layer(layer_hdl, 1);

		/* Draw the PIKCHAAA */ 
		/* Notes:
			VM_GDI_HANDlE is basically VMINT

		*/
		//VMINT vm_graphic_draw_image_from_file(VM_GDI_HANDLE dest_layer_handle, 
		//	VMINT x, VMINT y, const VMWSTR filename);

		//Testing:
		/* Getting em strings ready */
			/* 
			//Gotta allocate VMSTR before we do anything to it!
			filename = vm_monitored_malloc(MRE_STR_SIZE_MAX);
			sprintf (filename, "%c:\\%s", vm_get_removable_driver(), file_name);
			//Gotta allocate VMWSTR before we do anything to it!
			wfilename_size = (strlen(filename) + 1) * 2;
			wfilename = vm_monitored_malloc(wfilename_size);
			sprintf(f_name, "%c:\\%s", vm_get_removable_driver(), file_name);
			*/

		/* String format conversion */
			//vm_ascii_to_ucs2 (wfilename, MRE_STR_SIZE_MAX, filename);
			//vm_ascii_to_ucs2(f_wname, MRE_STR_SIZE_MAX, f_name);
			vm_ascii_to_ucs2(image_wpath, MRE_STR_SIZE_MAX, img.path);

		/* Get filename from the path 
			TODO: This
		*/
		file_name = "tips.gif";

		ret = vm_graphic_draw_image_from_file(layer_handle[0],
			//80, 80, "E:\umicon.jpg");
			//80, 80, "E:\\tips.gif");
			//80, 80, f_wname);
			10, 10, image_wpath); //Ideally it should be x,y //Update: doesnt do anything
			
		if (ret == VM_GDI_SUCCEED){
			printf("Drew image?"); //Not sure what happens in there.
			mre_show_image(MRE_GRAPHIC_IMAGE_CURRENT, image_wpath, file_name, layer_hdl, x, y);
		}else{
			printf("Couldn't draw image..");
		}

	}


	/*
		Clear It all
		No idea what this is doing
	*/
	static void
	nk_mre_clear(struct nk_color col)
	{
		VMUINT16 color;
		color = convert_color(col);

	}

	/*
		Whats a blit?
		No idea what this is doing
		I have a feeling this copies something from memory to memory
		In this case its copying the worked on device context 
			to the gdi's memory device contesxt
		(Probably not needed in MRE. Most similar is: flushing)
	*/
	static void
	nk_mre_blit(void)
	{
		//Do nothing	
	}

	/********************************************************
						FONT STUFF
	********************************************************/

	/*Create the Font*/
	MreFont*
	nk_mrefont_create(const char *name, int size)
	{
		MreFont *font;
		int obj_size = 1;
		//MreFont *font = (MreFont*)vm_monitored_calloc(1, sizeof(MreFont));
		obj_size = sizeof(MreFont);
		font = (MreFont*)vm_monitored_calloc(obj_size);
		if (!font){
			return NULL;
		}
		/*Create font params*/
		font->size = size;
		return font;
	}

	/*Get Text Width*/
	static float
	nk_mrefont_get_text_width()
	{
		//Do nothing
		return (float)1;
	}

	/*
		Set Font
		Sets the font required in the platform
		Called at the start of the implementation
	*/
	void
	nk_mre_set_font(void)
	{
		vm_log_debug("Not setting font - MRE will default it");
	}

	/*
		Delete the font
		Deletes the font set
	*/
	void 
	nk_mrefont_del()
	{
		//Do nothing
	}

	/* Get font float 
		Needed in nk_mre_init method for initialising nk's global font object:
		nk_user_font. We need to give it some value. Since MRE's font isn't
		really complete, we just give it some random float value (1) for now
	*/
	static float
	get_font_width(nk_handle handle, float height, const char *text, int len)
	{
		//Get font width in float
		float size;
		float letter_size = FONT_SIZE;
		size = letter_size * len;
		return (float)size;

	}



	/********************************************************
						CLIPBOARD STUFF
	********************************************************/

	/*Paste from clipbard?*/
	/*
	static void
	nk_gdi_clipbard_paste(nk_handle usr, struct nk_text_edit *edit)
	{
		(void)usr;
		if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(NULL))
		{
			HGLOBAL mem = GetClipboardData(CF_UNICODETEXT); 
			if (mem)
			{
				SIZE_T size = GlobalSize(mem) - 1;
				if (size)
				{
					LPCWSTR wstr = (LPCWSTR)GlobalLock(mem); 
					if (wstr) 
					{
						int utf8size = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)(size / sizeof(wchar_t)), NULL, 0, NULL, NULL);
						if (utf8size)
						{
							char* utf8 = malloc(utf8size);
							if (utf8)
							{
								WideCharToMultiByte(CP_UTF8, 0, wstr, (int)(size / sizeof(wchar_t)), utf8, utf8size, NULL, NULL);
								nk_textedit_paste(edit, utf8, utf8size);
								free(utf8);
							}
						}
						GlobalUnlock(mem); 
					}
				}
			}
			CloseClipboard();
		}
	}
	*/

	/*Copy from clipboard*/
	/*
	static void
	nk_gdi_clipbard_copy(nk_handle usr, const char *text, int len)
	{
		if (OpenClipboard(NULL))
		{
			int wsize = MultiByteToWideChar(CP_UTF8, 0, text, len, NULL, 0);
			if (wsize)
			{
				HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (wsize + 1) * sizeof(wchar_t));
				if (mem)
				{
					wchar_t* wstr = GlobalLock(mem);
					if (wstr)
					{
						MultiByteToWideChar(CP_UTF8, 0, text, len, wstr, wsize);
						wstr[wsize] = 0;
						GlobalUnlock(mem); 

						SetClipboardData(CF_UNICODETEXT, mem); 
					}
				}
			}
			CloseClipboard();
		}
	}
	*/

	/********************************************************
					INITIALISE DECLARATIONS
	********************************************************/

	/*
		Initialise the Display stuff - fonts, window, etc
	*/
	struct nk_context*
	nk_mre_init(MreFont *mrefont, unsigned int width, unsigned int height){
		//Maybe keep some of this outside ? : Not sure. Needs to be thought after its working ok
		
		/* Define window/layer */
		//Layer already initiated in mre_io_sample.h:
		//VMINT		layer_hdl[1];

		/*Declaring the buffer for drawing*/
		VMUINT8 *layer_buf;

		/*Declare the nk_user_font*/
		struct nk_user_font *font = &mrefont->nk;

		/*Initialising the layer handle*/
		layer_hdl[0] = -1;

		/*Create nk_user_font object from implementations' font object*/
		//struct nk_user_font *font = &mrefont->nk;
		font->userdata = nk_handle_ptr(mrefont); //Creates an nk_hanlde off that ptr (nk_handle has id and ptr)
		font->height = (float)mrefont->size;
		font->width = get_font_width; //Set it to whatever
		//TODO: Check..

		/*Setting Global data for Graphics(set width, height, font size*/
		vm_font_set_font_size(VM_SMALL_FONT);

		/*Set language, system, key event callbacks*/
		vm_mul_lang_set_lang (VM_LANG_ENG);	//Sets Language

		/*
		vm_mul_lang_set_lang (VM_LANG_ENG);
		vm_reg_sysevt_callback (handle_sysevt);
		vm_reg_keyboard_callback (handle_keyevt);
		vm_reg_pen_callback (handle_penevt);
		*/
		
		/*
		vm_mul_lang_set_lang (VM_LANG_ENG);
		vm_reg_sysevt_callback (nk_mre_handle_sys_event); //looks at implementation event handler (does thesame thing)
		vm_reg_keyboard_callback (nk_mre_handle_key_event);	//Keyboard callback
		vm_reg_pen_callback (handle_penevt);	//Pen callback - not really needed
		vm_log_file ("declared buffer, initialised layer handle,set global data done.\n");
		*/
		
		/*Set the mre struct object*/
		mre.width = width;
		mre.height = height;

		/*PAINTING AND PREPARING THE LAYER HANDLER*/ 

		/* create base layer that has same size as the screen*/
		layer_hdl[0] = vm_graphic_create_layer(MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, 
			vm_graphic_get_screen_width(), 
			vm_graphic_get_screen_height(), VM_NO_TRANSPARENT_COLOR);

		/*
			set clip area
			Remember to reset clip region after drawing
		*/
		vm_graphic_set_clip (0, 0, 
				vm_graphic_get_screen_width (), 
				vm_graphic_get_screen_height ());
		//vm_graphic_set_clip(CLIP_LT_X, CLIP_LT_Y, CLIP_RT_X,  CLIP_RT_Y);

		/* get the layer handler'target buffer*/
		layer_buf = vm_graphic_get_layer_buffer (layer_hdl[0]);
		
		/* fill the whole screen with BLACK with WHITE as line color*/
		vm_graphic_fill_rect (layer_buf, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width (), 
							 vm_graphic_get_screen_height (), VM_COLOR_WHITE, VM_COLOR_BLACK);
		
		/* Flushing - this will actually flush and display on the screen */
		vm_graphic_flush_layer (layer_hdl, 1);
		vm_log_debug ("Initialised ALL OK.");
	
		/*Init Memory Allocations for context*/
		//nk_init_default(&mre.ctx, 0); //Wont work uses StandardLib
		//nk_init_mre(&mre.ctx, 0); //Old before font bug was discovered.
		nk_init_mre(&mre.ctx, font);

		return &mre.ctx;
	}

	void nk_mre_handle_penevt (VMINT event, VMINT x, VMINT y)
	{
		vm_log_debug ("touch event handle function starts with event : %d and x,y :%d, %d", event, x, y);
		vm_log_debug ("touch event handle function exits");

	}

void get_hoovers(int *curr_hoover, int *prev_hoover, int *nex_hoover)
{
	struct mre_nk_view* view;
	int current_hoover = -1;
	int previous_hoover = -1;
	int next_hoover = -1;
	int i = 0;

	view = get_mre_nk_view();

    if (view->components && view->components[0])
    {
        for(i=0; view->components[i] != NULL; i++){
          struct mre_nk_component * comp = view->components[i];
          if (comp->is_hoverable)
          {

			if (current_hoover != -1)
			{
				next_hoover = i;
				break;
			}

            if (comp->hovering)
			{
				if (next_hoover == -1) {
					next_hoover = i;
				}
				current_hoover = i;
				if (previous_hoover == -1) {
					previous_hoover = i;
				}
			}

			if (current_hoover == -1)
			{
				previous_hoover = i;
			}

          }
        }
    }
	*curr_hoover = current_hoover;
	*prev_hoover = previous_hoover;
	*nex_hoover = next_hoover;
}

void hoover_action(VMINT keycode)
{
	struct mre_nk_view* view;
	int current_hoover = -1;
	int previous_hoover = -1;
	int next_hoover = -1;

	view = get_mre_nk_view();

	get_hoovers(&current_hoover, &previous_hoover, &next_hoover);

	if (keycode == VM_KEY_OK || keycode == VM_KEY_QWERTY_ENTER || keycode == VM_KEY_RIGHT)
	{
		if (current_hoover != -1)
		{
			if (view->components[current_hoover]->action)
			{
				view->components[current_hoover]->action(current_hoover);
			}
		}
	} else if (keycode == VM_KEY_DOWN)
	{
		if (current_hoover != -1 && next_hoover != -1)
		{
			view->components[current_hoover]->hovering = 0;
			view->components[next_hoover]->hovering = 1;
		}
	} else if (keycode == VM_KEY_UP)
	{
		if (current_hoover != -1 && previous_hoover != -1)
		{
			view->components[current_hoover]->hovering = 0;
			view->components[previous_hoover]->hovering = 1;
		}
	}
	/*
	else if (keycode == VM_KEY_UP)
	{

		cleanup_sensitive_data();	
		vm_exit_app();
		while(1){};
	}
	*/
	if (g_callback_none == false) {
		update_gui();
	}
}

	void
	nk_mre_handle_key_event(VMINT event, VMINT keycode){

		bool b_update_gui = true;
		int j = 0;
	    struct mre_nk_view* view = get_mre_nk_view();

		if (event == VM_KEY_EVENT_UP)
		{
 			int down = VM_KEY_EVENT_UP;

			switch (keycode) 
			{
				case VM_KEY_OK:
				case VM_KEY_QWERTY_ENTER:
				case VM_KEY_RIGHT:
					nk_input_key(&mre.ctx, NK_KEY_ENTER, down);
					hoover_action(VM_KEY_OK);
					break;

				case VM_KEY_LEFT:
					if (view->previous_id > NK_MRE_VIEW_NONE && view->previous_id < NK_MRE_VIEW_MAX_ID)
					{
						set_view(view->previous_id);
						if (b_update_gui)
						{
							update_gui();
						}
					}
					break;

				case VM_KEY_DOWN:
					nk_input_key(&mre.ctx, NK_KEY_SCROLL_DOWN, down);
					hoover_action(VM_KEY_DOWN);
					break;

				case VM_KEY_UP:
					nk_input_key(&mre.ctx, NK_KEY_SCROLL_UP, down);
					hoover_action(VM_KEY_UP);
					break;
			}

		}

	}

	/*
		Handles Key Press events
		Declared in header file
		Initialised at init 
	*/
	static int nk_gui_init_done = 0;

	void
	nk_mre_handle_sys_event	(VMINT message, VMINT param){
		//nk_mre_handle_event(Event crumb(s)){
		//crumb being values we can judge what event happened..

		//Do stuff with the events. System Event
		switch (message) 
		{
			case VM_MSG_CREATE:
				/* the GDI operation is not recommended as the response of the message*/
				break;
			case VM_MSG_ACTIVE:
				break;
			case VM_MSG_PAINT:
				if (nk_gui_init_done == 0)
				{
					vm_log_debug ("paint message, support bg");
					if ( layer_hdl[0] != -1 )
					{
						vm_graphic_delete_layer (layer_hdl[0]);
						layer_hdl[0] = -1;
					}
					layer_hdl[0] = vm_graphic_create_layer (0, 0, vm_graphic_get_screen_width (), 
													vm_graphic_get_screen_height (), -1);
					vm_graphic_active_layer (layer_hdl[0]);
					/* set clip area */
					vm_graphic_set_clip (0, 0, 
						vm_graphic_get_screen_width (), 
						vm_graphic_get_screen_height ());
		           
					nk_gui_init_done++;
					initiate_nk_gui();
				} else {
					update_gui();
				}
				break;

			case VM_MSG_HIDE:
			case VM_MSG_INACTIVE:
				vm_log_debug ("VM_MSG_HIDE message, support bg");
				if ( layer_hdl[0] != -1 )
				{
					vm_graphic_delete_layer (layer_hdl[0]);
					layer_hdl[0] = -1;
				}
				if (g_camera_preview)
				{
					stop_preview();
				}
				break;

			case VM_MSG_QUIT:
				vm_log_debug ("VM_MSG_QUIT message, support bg");
				if ( layer_hdl[0] != -1 )
				{
					vm_graphic_delete_layer (layer_hdl[0]);
					layer_hdl[0] = -1;
				}
				clean_and_exit(0);
				break;
		}

		//This is similar to MRE's handle_sysevt(VMINT message, VMINT param).
		// We could call that method from here..
		//handle_keyevt(message, param);
	}

	/*
		Shutdown the MRE display stuff
		Delete from mem and display/bitmap
		Declared during initialisation
	*/
	void
	nk_mre_shutdown(void){

		/*Flush layer - Like dry the layers together / Refresh*/ 
		//vm_graphic_flush_layer(layer_hdl, 2); 

		/* Delete top layer */ 
		//vm_graphic_delete_layer(layer_hdl[1]); 
		//layer_hdl[1] = -1; 
		//layer_buf[1] = NULL; 
		//Resets clip region to full size.
		//vm_graphic_reset_clip();	
	}

	/*
		Render Event handler - Actually draws the UI
		This function loops through all the primitive
		draw calls and calls MRE implementation equivalents.
		
		This function is triggered whenever the GUI needs 
		updating. This Event handler calls the implementation bits
		Declared at the begenning
	*/
	void
	nk_mre_render(struct nk_color clear){

		const struct nk_command *cmd;
		struct nk_context *ctx = &mre.ctx;
		
		//VMUINT8 *buffer;

		//Apart from command, there is some form
		//of context that xlib uses

		/* 
			get the target buffer from the layer
		*/
		//buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

		/* fill the screen*/
		//vm_graphic_fill_rect (buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, vm_graphic_get_screen_width (), 
		//					 vm_graphic_get_screen_height (), VM_COLOR_WHITE, VM_COLOR_BLACK);

		/*Test: Clearing*/
		//nk_clear(&mre.ctx); //Clears everything here..


		/* Something with memory */

		/*Actual Render bits*/
		const struct nk_command *first_cmd = NULL;
		bool bug_found = false;
		nk_foreach(cmd, &mre.ctx)
		{
			if (first_cmd == NULL)
			{
				first_cmd = cmd;
			} else if (first_cmd == cmd) {
				bug_found = true;
				break;
			}
			switch (cmd->type) {
			case NK_COMMAND_NOP: break;
			case NK_COMMAND_SCISSOR: {
				const struct nk_command_scissor *s =(const struct nk_command_scissor*)cmd;
				//nk_gdi_scissor(memory_dc, s->x, s->y, s->w, s->h);
				//nk_mre_scissor(s->x, s->y, s->w, s->h);
				nk_mre_scissor();
			} break;
			case NK_COMMAND_LINE: {
				const struct nk_command_line *l = (const struct nk_command_line *)cmd;
				//nk_mre_stroke_line(memory_dc, l->begin.x, l->begin.y, l->end.x,
				//	l->end.y, l->line_thickness, l->color);
				nk_mre_stroke_line(l->begin.x, l->begin.y, l->end.x,
					l->end.y, l->line_thickness, l->color);
			} break;
			case NK_COMMAND_RECT: {
				const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
				//nk_mre_stroke_rect(memory_dc, r->x, r->y, r->w, r->h,
				//	(unsigned short)r->rounding, r->line_thickness, r->color);
				nk_mre_stroke_rect(r->x, r->y, r->w, r->h,
					(unsigned short)r->rounding, r->line_thickness, r->color);
			} break;
			case NK_COMMAND_RECT_FILLED: {
				const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
				//nk_mre_fill_rect(memory_dc, r->x, r->y, r->w, r->h,
				//	(unsigned short)r->rounding, r->color);
				nk_mre_fill_rect(r->x, r->y, r->w, r->h,
					(unsigned short)r->rounding, r->color);
			} break;
			case NK_COMMAND_CIRCLE: {
				const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
				//nk_mre_stroke_circle(memory_dc, c->x, c->y, c->w, c->h, c->line_thickness, c->color);
				nk_mre_stroke_circle(c->x, c->y, c->w, c->h, c->line_thickness, c->color);
			} break;
			case NK_COMMAND_CIRCLE_FILLED: {
				const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
				//nk_mre_fill_circle(memory_dc, c->x, c->y, c->w, c->h, c->color);
				nk_mre_fill_circle(c->x, c->y, c->w, c->h, c->color);
			} break;
			case NK_COMMAND_TRIANGLE: {
				const struct nk_command_triangle*t = (const struct nk_command_triangle*)cmd;
				//nk_mre_stroke_triangle(memory_dc, t->a.x, t->a.y, t->b.x, t->b.y,
				//	t->c.x, t->c.y, t->line_thickness, t->color);
				nk_mre_stroke_triangle( t->a.x, t->a.y, t->b.x, t->b.y,
					t->c.x, t->c.y, t->line_thickness, t->color);
			} break;
			case NK_COMMAND_TRIANGLE_FILLED: {
				const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
				//nk_mre_fill_triangle(memory_dc, t->a.x, t->a.y, t->b.x, t->b.y,
				//	t->c.x, t->c.y, t->color);
				nk_mre_fill_triangle(t->a.x, t->a.y, t->b.x, t->b.y,
					t->c.x, t->c.y, t->color);
			} break;
			case NK_COMMAND_POLYGON: {
				const struct nk_command_polygon *p =(const struct nk_command_polygon*)cmd;
				//nk_mre_stroke_polygon(memory_dc, p->points, p->point_count, p->line_thickness,p->color);
				nk_mre_stroke_polygon(p->points, p->point_count, p->line_thickness,p->color);
			} break;
			case NK_COMMAND_POLYGON_FILLED: {
				const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled *)cmd;
				//nk_mre_fill_polygon(memory_dc, p->points, p->point_count, p->color);
				nk_mre_fill_polygon(p->points, p->point_count, p->color);
			} break;
			case NK_COMMAND_POLYLINE: {
				const struct nk_command_polyline *p = (const struct nk_command_polyline *)cmd;
				//nk_mre_stroke_polyline(memory_dc, p->points, p->point_count, p->line_thickness, p->color);
				nk_mre_stroke_polyline( p->points, p->point_count, p->line_thickness, p->color);
			} break;
			case NK_COMMAND_TEXT: {
				const struct nk_command_text *t = (const struct nk_command_text*)cmd;
				/*nk_mre_draw_text(memory_dc, t->x, t->y, t->w, t->h,
					(const char*)t->string, t->length,
					(GdiFont*)t->font->userdata.ptr,
					t->background, t->foreground);*/
				printf("Text starts at (%d,%d) height: %d, width:%d. \"%s\"  \n",
					t->x,t->y,t->h,t->w,t->string);
				nk_mre_draw_text(t->x, t->y, t->w, t->h,
					(const char*)t->string, t->length,
					t->background, t->foreground);
			} break;
			case NK_COMMAND_CURVE: {
				const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
				//nk_mre_stroke_curve(memory_dc, q->begin, q->ctrl[0], q->ctrl[1],
				//	q->end, q->line_thickness, q->color);
				nk_mre_stroke_curve(q->begin, q->ctrl[0], q->ctrl[1],
					q->end, q->line_thickness, q->color);
			} break;
			case NK_COMMAND_RECT_MULTI_COLOR:
				break;
#if 0
			case NK_COMMAND_IMAGE: {
				/* Get the image from the command and image object
					nk_mre_draw_image(nk_image_img, pos_x, pos_y, image_file_path);
				So we can have something like this: 
					nk_mre_draw_image(i->x, i->y, i->w, i->h, i->img.thumb .. );
				*/ 
				const struct nk_command_image *i = (const struct nk_command_image *)cmd;
				printf("Finally, lets draw some pictures!");
				
				//nk_mre_draw_image_default(i->x, i->y, i->w, i->h); //default testing 

				/* 
				nk_image: 
					struct nk_image {nk_handle handle;unsigned short w,h;unsigned short region[4];};
				nk_handle:
					typedef union {void *ptr; int id;} nk_handle;
				*/ 
				nk_mre_draw_image(i->x, i->y, i->w, i->h, i->img);
				printf("Drew the image. Flushing..");
			} break;
#endif
			case NK_COMMAND_ARC:
			case NK_COMMAND_ARC_FILLED:
			default: break;
			}
			//Testing: Check as it draws..
			vm_graphic_flush_layer(layer_hdl, 1);
		}

		///*One last flush */
		vm_graphic_flush_layer(layer_hdl, 1); 

		/* Clear and Blit called */
		//nk_gdi_blit(gdi.window_dc);
		nk_clear(&mre.ctx);

		if (bug_found)
		{
			update_gui();
		}
	}

struct mre_nk_component * mre_nk_component_create(int type, char *text, int is_hoverable, int hovering, int (*action)(int component_index)) { //, int color, int hovering_color, int text_color){
	int text_alloc_len;

	// alloc mre_nk_component
	struct mre_nk_component *cmpnt = (struct mre_nk_component *)vm_monitored_calloc(sizeof(struct mre_nk_component));
	
	// type
	cmpnt->type = type;

	// text
	text_alloc_len = 1;
	if (text != NULL)
	{
		text_alloc_len = strlen(text) + 1;
	}
	cmpnt->text = vm_monitored_calloc(text_alloc_len);
	if (text != NULL)
	{
		strcpy(cmpnt->text, text);
	}
	
	// hovering
	cmpnt->is_hoverable = is_hoverable;
	cmpnt->hovering = hovering;
	if (type != NK_MRE_COMPONENT_BUTTON && type != NK_MRE_COMPONENT_INPUT_TEXT && type != NK_MRE_COMPONENT_CHECKBOX){
		cmpnt->is_hoverable = 0;
		cmpnt->hovering = 0;
	}

	// color
	//cmpnt->color = color;
	//cmpnt->hovering_color = hovering_color;
	//cmpnt->text_color = text_color;

	// action
	cmpnt->action = action;

	return cmpnt;
}

VMWCHAR w_text_entropy[256];

static int entropy_char_left = TEXT_ENTROPY_LEN;

void replace_ascii_text_alloc(char** old_alloc, char* new_str)
{
	int str_len = strlen(new_str);
	char* old_str = (*old_alloc);
	char* new_alloc = vm_calloc(str_len + 1);
	strcpy(new_alloc, new_str);
	(*old_alloc) = new_alloc;
	vm_free(old_str);
}

void update_char_left(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		struct mre_nk_view* view;
		int current_hoover = -1;
		int previous_hoover = -1;
		int next_hoover = -1;
		int len_text = wstrlen(text);
		char rnd_char_str[64];
		vm_wstrcpy(w_text_entropy, text);
		entropy_char_left = TEXT_ENTROPY_LEN - len_text;

		view = get_mre_nk_view();
		
		get_hoovers(&current_hoover, &previous_hoover, &next_hoover);

		vm_sprintf(rnd_char_str, "Random chars left: %d",  entropy_char_left);
		replace_ascii_text_alloc(&view->components[current_hoover-1]->text, rnd_char_str);
		
		if (entropy_char_left == 0)
		{
			// first random_buffer/random32 call
			// it does all the initialization
			uint8_t buff[512];

			// init csprng algo
			random_buffer(buff, sizeof(buff));

			// init reed solomon
			_fec_init();

			set_view(NK_MRE_VIEW_WALLET_ACTION);
		}

		update_gui();
	}
}

int enter_entropy(int component_index)
{
	printf("ENTER ENTROPY NOW\n");

	vm_input_text3 (w_text_entropy, TEXT_ENTROPY_LEN, VM_INPUT_METHOD_ALPHABETIC_EXT,
                                     update_char_left);

	return NULL;
}

int flip_checkbox(int component_index)
{
	struct mre_nk_view* view;
	printf("flip_checkbox\n");
	view = get_mre_nk_view();
	if (view->components[component_index]->checked == 0)
	{
		view->components[component_index]->checked = 1;
	} else {
		view->components[component_index]->checked = 0;
	}
	update_gui(); // gui is updated in hoover_action
	return 0;
}

int next_view_gen_entropy(int component_index)
{
	struct mre_nk_view* view;
	printf("next_view_gen_entropy\n");
	view = get_mre_nk_view();
	// is T&C are accepted perform action
	if (view->components[component_index-1]->checked != 0)
	{
		set_view(NK_MRE_VIEW_ENTROPY_GEN);
		update_gui(); // since next screen has a button that supports hoover, updating the gui is unnecessary
	}
	return 0;
}

VMWCHAR w_text_input[256];
int last_component_index = 0;

void update_text_input(VMINT state, VMWSTR text)
{
	struct mre_nk_view* view;

	if (text != NULL && state == VM_INPUT_OK)
	{
		int text_len = 0;
		char* new_text = NULL;
		void* old_text_alloc = NULL;

		vm_wstrcpy(w_text_input, text);
			
		view = get_mre_nk_view();
		
		text_len = vm_wstrlen(w_text_input);
		new_text = vm_monitored_calloc(text_len+1);

		vm_ucs2_to_ascii(new_text, text_len+1, w_text_input);//view->components[component_index]->text);

		old_text_alloc = view->components[last_component_index]->text;

		view->components[last_component_index]->text = new_text;

		vm_free(old_text_alloc);

		update_gui();
	}
}

int input_text(int component_index)
{

	struct mre_nk_view* view;
	
	printf("INPUT TEXT NOW\n");

	view = get_mre_nk_view();
	last_component_index = component_index;

	vm_ascii_to_ucs2(w_text_input, sizeof(w_text_input), view->components[component_index]->text);

	vm_input_text3 (w_text_input, TEXT_ENTROPY_LEN, VM_INPUT_METHOD_ALPHABETIC,
                                     update_text_input);

	return NULL;
}

void create_view_welcome_screen(){
	struct mre_nk_component *comp_label;
	struct mre_nk_component *comp_checkbox;
	struct mre_nk_component *comp_button;

	struct mre_nk_view* view;
	char str_title[] = "Bitcoin MRE Wallet";

	view = &g_view[NK_MRE_VIEW_WELCOME];
	view->id = NK_MRE_VIEW_WELCOME;

	view->previous_id = NK_MRE_VIEW_NONE;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "T&C: USE AT YOUR OWN RISK, NO GUARANTEE IS PROVIDED AND YOU MIGHT LOSE MONEY. PLEASE USE A BETTER HARDWARE WALLET IF YOU CAN AFFORD IT!", 0, 0, NULL);//, NK_MRE_COLOR_BITCOIN_ORANGE, NK_MRE_COLOR_BITCOIN_ORANGE_LIGHT, NK_MRE_COLOR_BLACK);
	comp_checkbox = mre_nk_component_create(NK_MRE_COMPONENT_CHECKBOX, "Accept T&C", 1, 0, flip_checkbox);//, NK_MRE_COLOR_BITCOIN_ORANGE, NK_MRE_COLOR_BITCOIN_ORANGE_LIGHT, NK_MRE_COLOR_BLACK);
	comp_button = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Next", 1, 0, next_view_gen_entropy);//, NK_MRE_COLOR_BITCOIN_ORANGE, NK_MRE_COLOR_BITCOIN_ORANGE_LIGHT, NK_MRE_COLOR_BLACK);

	view->components = vm_monitored_calloc(4*sizeof(struct mre_nk_component *)); // 2 components + NULL
	view->components[0] = comp_label;
	view->components[1] = comp_checkbox;
	view->components[2] = comp_button;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_entropy_gen(){
	struct mre_nk_component *comp_label;
	struct mre_nk_component *comp_sep;
	struct mre_nk_component *comp_label2;
	struct mre_nk_component *comp_label3;
	struct mre_nk_component *comp_button;
	struct mre_nk_component *comp_button2;

	struct mre_nk_view* view;
	char str_gen_entropy[] = "Extra entropy generation";

	view = &g_view[NK_MRE_VIEW_ENTROPY_GEN];
	view->id = NK_MRE_VIEW_ENTROPY_GEN;
	view->previous_id = NK_MRE_VIEW_NONE;
	
	view->title = vm_monitored_calloc(strlen(str_gen_entropy)+1);
	strcpy(view->title, str_gen_entropy);

	comp_label = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "!!! WARNING !!! Scroll down to fully read me before proceeding. This device does not have any TRNG. Please insert 255 totally random characters. You do NOT have to remember/save/store them. Please be sure to include Upper Case, Lower Case, Digits, Special Chars and if possible Unicode Chars (e.g. Chinese, Arabic, Russian...). Please avoid using patterns. The random the better for entropy generation. Weak entropy will cause loss of funds. If your device supports camera, please close the app and take a bunch of random pictures before continuing.", 0, 0, NULL);
	comp_label2 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "Random chars left: 255", 0, 0, NULL);
	//comp_button = mre_nk_component_create(NK_MRE_COMPONENT_CHECKBOX, "I understood", 1, 0, flip_checkbox);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Enter random", 1, 0, enter_entropy);
	 
	view->components = vm_monitored_calloc(5*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label;
	view->components[1] = comp_label2;
	view->components[2] = comp_button2;
	//view->components[3] = comp_button2;
	//view->components[4] = NULL; // calloc does the job
}

//
#define RS_BLOCK_SIZE 4 // use dynamic block size
#define PARITY_SHARDS 127
#define DATA_SHARDS 128
#define BTC_WALLET_MAGIC 0xB15B00BA

struct btc_wallet {
	uint32_t magic;
	uint32_t version;
	uint32_t len;
	uint32_t type;
	unsigned char salt[16];
	unsigned char iv[16];
	// priv key / wif / seed / entropy
};

void create_encrypted_wallet(char* wallet_name, uint32_t type, char* wallet_data, uint32_t wallet_data_len, VMWSTR password)
{
	VMFILE handle = -1;

	// create folder
	VMWCHAR w_path[MAX_APP_NAME_LEN];
	char path[MAX_APP_NAME_LEN];
	sprintf (path, "%c:\\btc_mre_wallet", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_file_mkdir(w_path);

	sprintf(path, "%c:\\btc_mre_wallet\\%s.wallet", mre_get_drv(), wallet_name);
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	handle = vm_file_open(w_path, MODE_CREATE_ALWAYS_WRITE, TRUE);
	if (handle > 0)
	{
		VMUINT written = 0;
		VMINT pass_len = 0;
		uint8_t pass_hass[SHA256_DIGEST_LENGTH] = {0};
		uint8_t wallet_data_hash[SHA256_DIGEST_LENGTH] = {0};
		struct AES_ctx ctx;
		int wallet_data_len_aligned = 0;
		int dec_len = 0;
		uint8_t* dec_buff = NULL;
		int wallet_data_pad = 0;
		int pad_valid = 0;
		reed_solomon* rs = NULL;
		int parity_shards = PARITY_SHARDS;
		int data_shards = DATA_SHARDS;
		int nr_shards = 0;
		int rs_block_size = RS_BLOCK_SIZE;
		unsigned char **rs_data_blocks = NULL;
		unsigned char* rs_data = NULL;
		int i = 0;
		int j = 0;
		int n = 0;
		int nr_blocks = 0;
		int nr_fec_blocks = 0;
		struct btc_wallet btcw;
		SHA256_CTX	context;

		btcw.magic = BTC_WALLET_MAGIC;
		btcw.version = 1;
		btcw.type = type;
		//btcw.len = 0; // do it below
		random_buffer(btcw.salt, sizeof(btcw.salt));
		random_buffer(btcw.iv, sizeof(btcw.iv));

		pass_len = (vm_wstrlen(password)+1) * 2;

		// rudimental PBKDF2-style derivation

		// first pass
		sha256_Init(&context);
		sha256_Update(&context, (const uint8_t *)password, pass_len);
		sha256_Update(&context, btcw.salt, sizeof(btcw.salt));
		sha256_Final(&context, pass_hass);

		// 2048 more pass
		for (i=0; i<2048; i++)
		{
			sha256_Raw(pass_hass, sizeof(pass_hass), pass_hass);
		}

		//Proper Length of report
		wallet_data_len_aligned = wallet_data_len;
		if (wallet_data_len % 16) {
			wallet_data_len_aligned += 16 - (wallet_data_len % 16);
		}
		
		btcw.len = sizeof(struct btc_wallet) + wallet_data_len_aligned + SHA256_DIGEST_LENGTH;

		dec_len = btcw.len;

		dec_buff = vm_calloc(dec_len);
		memcpy(dec_buff, &btcw, sizeof(struct btc_wallet));
		memcpy(dec_buff+sizeof(struct btc_wallet), wallet_data, wallet_data_len);

		wallet_data_pad = pkcs7_padding_pad_buffer(dec_buff+sizeof(struct btc_wallet), wallet_data_len, wallet_data_len_aligned, AES_BLOCKLEN);
#if 0
		pad_valid = pkcs7_padding_valid(dec_buff+sizeof(struct btc_wallet), wallet_data_len, wallet_data_len_aligned, AES_BLOCKLEN );
		if (pad_valid == 0)
		{
			vm_exit_app();
		}
#endif
		// | magic | version | len | type | salt | iv | wallet_data | PAD | now append --> SHA256(metadata+decrypted_wallet_data+PAD) |
		sha256_Raw(dec_buff, dec_len-SHA256_DIGEST_LENGTH, &dec_buff[sizeof(struct btc_wallet)+wallet_data_len_aligned]);
		    
		// encrypt all but metadata
		AES_init_ctx_iv(&ctx, pass_hass, btcw.iv); // pass_hass is the encryption key
		AES_CBC_encrypt_buffer(&ctx, dec_buff+sizeof(struct btc_wallet), dec_len-sizeof(struct btc_wallet));

		// add error correcting code
#if 0
		rs_block_size = (dec_len+data_shards-1) / data_shards;
		nr_shards = data_shards + parity_shards;
#else
		nr_blocks = (dec_len+rs_block_size-1)/rs_block_size;
		nr_blocks = ((nr_blocks+data_shards-1)/data_shards) * data_shards;
		n = nr_blocks / data_shards;
		nr_fec_blocks = n * parity_shards;
		nr_shards = nr_blocks + nr_fec_blocks;
#endif

		rs_data = vm_calloc(nr_shards*rs_block_size);
		memcpy(rs_data, dec_buff, dec_len);

		//wallet_data_pad = pkcs7_padding_pad_buffer(rs_data, dec_len, nr_blocks*rs_block_size, nr_blocks*rs_block_size);

		rs_data_blocks = (unsigned char**)vm_calloc(nr_shards * sizeof(unsigned char*));
		for(i = 0; i < nr_shards; i++) {
			rs_data_blocks[i] = rs_data + i*rs_block_size;
		}

		rs = _reed_solomon_new(data_shards, parity_shards);
		_reed_solomon_encode2(rs, rs_data_blocks, nr_shards, rs_block_size);

		for(i = 0; i < nr_shards; i++) {
			written = 0;
			vm_file_write(handle, rs_data_blocks[i], rs_block_size, &written);
		}
		// write also bitflipped values so we can detect error
		// very rudimental but whatever, replace with a crc32 maybe for each block
		for(i = 0; i < nr_shards; i++) {
			unsigned int* block_ptr = (unsigned int*)rs_data_blocks[i];
			for (j = 0; j<rs_block_size/sizeof(unsigned int); j++)
			{
				// bit flip
				block_ptr[j] = ~(block_ptr[j]);
			}
		}
		for(i = 0; i < nr_shards; i++) {
			written = 0;
			vm_file_write(handle, rs_data_blocks[i], rs_block_size, &written);
		}
		//vm_file_write(handle, dec_buff, dec_len, &written);
		vm_file_close(handle);

		// reset mem
		pass_len = 0;
		memset(&btcw, 0, sizeof(struct btc_wallet));
		memset(pass_hass, 0, sizeof(pass_hass));
		memset(wallet_data_hash, 0, sizeof(wallet_data_hash));
		memset(&ctx, 0, sizeof(ctx));
		memset(dec_buff, 0, dec_len);
		memset(rs_data, 0, nr_shards*rs_block_size);
		memset(rs_data_blocks, 0, nr_shards * sizeof(unsigned char*));

		vm_free(rs_data);
		vm_free(rs_data_blocks);
		vm_free(dec_buff);

		_reed_solomon_release(rs);
	}
	//vm_file_mkdir
}

bool load_encrypted_wallet(VMWSTR w_wallet_path, uint32_t* type, char** wallet_data, uint32_t* wallet_data_len, VMWSTR password)
{
	VMFILE handle = -1;
	bool res = false;

	handle = vm_file_open(w_wallet_path, MODE_READ, TRUE);
	if (handle > 0)
	{
		VMUINT nread = 0;
		VMINT pass_len = 0;
		uint8_t pass_hass[SHA256_DIGEST_LENGTH] = {0};
		uint8_t wallet_data_hash[SHA256_DIGEST_LENGTH] = {0};
		struct AES_ctx ctx;
		int dec_len = 0;
		uint8_t* dec_buff = NULL;
		int wallet_data_pad = 0;
		int pad_valid = 0;
		reed_solomon* rs = NULL;
		int parity_shards = PARITY_SHARDS;
		int data_shards = DATA_SHARDS;
		int nr_shards = 0;
		int rs_block_size = RS_BLOCK_SIZE;
		unsigned char **rs_data_blocks = NULL;
		unsigned char* rs_data = NULL;
		int i = 0;
		int j = 0;
		int n = 0;
		unsigned char *rs_errors = NULL;
		int nr_blocks = 0;
		int nr_fec_blocks = 0;
		int rs_res = 0;

		VMUINT rs_file_size = 0;
		uint8_t* rs_file_data = NULL;
		
		struct btc_wallet* btcw;
		SHA256_CTX	context;

		if (type == NULL || wallet_data_len == NULL || wallet_data == NULL)
		{
			return false;
		}
		(*type) = 0;
		(*wallet_data_len) = 0;
		(*wallet_data) = NULL;

		vm_file_getfilesize(handle, &rs_file_size);
		rs_file_data = vm_malloc(rs_file_size);
		if (rs_file_data == NULL)
		{
			vm_file_close(handle);
			return false;
		}

		// read buff
		vm_file_read(handle, rs_file_data, rs_file_size, &nread);
		vm_file_close(handle);

#if 0 // test, add some random errors (128 corrupted bytes)
		for(i = 0; i < 128; i++) {
			uint32_t x = random_range_max_excluded(0, rs_file_size);
			memset(rs_file_data + x, 0x41, 1);
		}
#endif

#if 0
		nr_shards = data_shards + parity_shards;
#else
		nr_shards = (rs_file_size/2) / rs_block_size;

		//nr_blocks = (data_size+block_size-1)/block_size;
		//nr_blocks = ((nr_blocks+data_shards-1)/data_shards) * data_shards;
		//n = nr_blocks / data_shards;
		//nr_fec_blocks = n * parity_shards;
		//nr_shards = nr_blocks + nr_fec_blocks;
#endif
		rs = _reed_solomon_new(data_shards, parity_shards);

        rs_errors = (unsigned char*)vm_calloc(nr_shards);
		rs_data_blocks = (unsigned char**)vm_calloc(nr_shards * sizeof(unsigned char*));
		for(i = 0; i < nr_shards; i++) {
			unsigned int* block_ptr = (unsigned int*) (rs_file_data + i*rs_block_size);
			unsigned int* flipped_block_ptr = (unsigned int*) (rs_file_data + (rs_file_size/2) + i*rs_block_size);
			for (j = 0; j<rs_block_size/sizeof(unsigned int); j++)
			{
				if (block_ptr[j] != ~(flipped_block_ptr[j]))
				{
					rs_errors[i] = 1;
				}
			}
			rs_data_blocks[i] = rs_file_data + i*rs_block_size;
		}

		rs_res = _reed_solomon_reconstruct(rs, rs_data_blocks, rs_errors, nr_shards, rs_block_size);
		if (rs_res != 0)
		{
			// try the bitflipped ones
			for(i = 0; i < nr_shards; i++) {
				unsigned int* block_ptr = (unsigned int*) (rs_file_data + i*rs_block_size);
				unsigned int* flipped_block_ptr = (unsigned int*) (rs_file_data + (rs_file_size/2) + i*rs_block_size);
				for (j = 0; j<rs_block_size/sizeof(unsigned int); j++)
				{
					block_ptr[j] = ~(flipped_block_ptr[j]);
					// rs_errors will be the same so don't update
				}
				// rs_data_blocks will be the same probably but let's assign it to be sure
				rs_data_blocks[i] = rs_file_data + i*rs_block_size;
			}

			// try reed solomon again
			rs_res = _reed_solomon_reconstruct(rs, rs_data_blocks, rs_errors, nr_shards, rs_block_size);
			if (rs_res != 0)
			{
				//too corrupt to recover but try anyway, worst case we get a wrong pass
			}
		}

		btcw = (struct btc_wallet*)rs_file_data;
		if (btcw->magic != BTC_WALLET_MAGIC)
		{
			res = false;
			goto cleanup;
		}
		if (btcw->type >= TYPE_MAX_ENUM)
		{
			res = false;
			goto cleanup;
		}
		if (btcw->version > 1)
		{
			res = false;
			goto cleanup;
		}
		if (btcw->len > rs_file_size/2)
		{
			res = false;
			goto cleanup;
		}

		pass_len = (vm_wstrlen(password)+1) * 2;

		// first pass
		sha256_Init(&context);
		sha256_Update(&context, (const uint8_t *)password, pass_len);
		sha256_Update(&context, btcw->salt, sizeof(btcw->salt));
		sha256_Final(&context, pass_hass);

		// 2048 more pass
		for (i=0; i<2048; i++)
		{
			sha256_Raw(pass_hass, sizeof(pass_hass), pass_hass);
		}

		AES_init_ctx_iv(&ctx, pass_hass, btcw->iv);
		AES_CBC_decrypt_buffer(&ctx, rs_file_data+sizeof(struct btc_wallet), btcw->len-sizeof(struct btc_wallet));

		// | magic | version | len | type | salt | iv | wallet_data | PAD | now append --> SHA256(metadata+decrypted_wallet_data+PAD) |
		sha256_Raw(rs_file_data, btcw->len-SHA256_DIGEST_LENGTH, wallet_data_hash);
		 
		if (memcmp(wallet_data_hash, &rs_file_data[btcw->len-SHA256_DIGEST_LENGTH], sizeof(wallet_data_hash)) != 0)
		{
			// wrong pass
			res = false;
			goto cleanup;
		}

		wallet_data_pad = pkcs7_padding_data_length(rs_file_data+sizeof(struct btc_wallet), btcw->len-sizeof(struct btc_wallet)-SHA256_DIGEST_LENGTH, AES_BLOCKLEN);

		(*type) = btcw->type;
		(*wallet_data_len) = wallet_data_pad;
		(*wallet_data) = vm_malloc(wallet_data_pad);
		memcpy(*wallet_data, &rs_file_data[sizeof(struct btc_wallet)], wallet_data_pad);
		
		res = true;
		// cleanup
cleanup:
		memset(rs_file_data, 0, rs_file_size);
		memset(rs_errors, 0, nr_shards);
		memset(rs_data_blocks, 0, nr_shards * sizeof(unsigned char*));

		vm_free(rs_file_data);
		vm_free(rs_errors);
		vm_free(rs_data_blocks);

		return res;
#if 0











		////////////




		pass_len = (vm_wstrlen(password)+1) * 2;
		sha256_Raw(password, pass_len, pass_hass);

		random_buffer(iv, sizeof(iv));

		wallet_data_len = strlen(wallet_data) + 1;

		//Proper Length of report
		wallet_data_len_aligned = wallet_data_len;
		if (wallet_data_len % 16) {
			wallet_data_len_aligned += 16 - (wallet_data_len % 16);
		}

		dec_len = wallet_data_len_aligned + SHA256_DIGEST_LENGTH;

		dec_buff = vm_calloc(dec_len);
		memcpy(dec_buff, wallet_data, wallet_data_len);
		wallet_data_pad = pkcs7_padding_pad_buffer(dec_buff, wallet_data_len, wallet_data_len_aligned, 16 );
		pad_valid = pkcs7_padding_valid(dec_buff, wallet_data_len, wallet_data_len_aligned, 16 );
		if (pad_valid == 0)
		{
			vm_exit_app();
		}

		// | wallet_data | PAD | SHA256(wallet_data+PAD) |
		sha256_Raw(dec_buff, wallet_data_len_aligned, &dec_buff[wallet_data_len_aligned]);
		    
		// encrypt
		AES_init_ctx_iv(&ctx, pass_hass, iv); // pass_hass is the encryption key
		AES_CBC_encrypt_buffer(&ctx, dec_buff, dec_len);

		// add error correcting code
		rs_block_size = (dec_len+data_shards-1) / data_shards;
		nr_shards = data_shards + parity_shards;

		rs_data = vm_calloc(nr_shards*rs_block_size);
		memcpy(rs_data, dec_buff, dec_len);

		rs_data_blocks = (unsigned char**)vm_calloc(nr_shards * sizeof(unsigned char*));
		for(i = 0; i < nr_shards; i++) {
			rs_data_blocks[i] = rs_data + i*rs_block_size;
		}

		rs = _reed_solomon_new(data_shards, parity_shards);
		_reed_solomon_encode2(rs, rs_data_blocks, nr_shards, rs_block_size);

		for(i = 0; i < nr_shards; i++) {
			written = 0;
			vm_file_write(handle, rs_data_blocks[i], rs_block_size, &written);
		}

		//vm_file_write(handle, dec_buff, dec_len, &written);
		vm_file_close(handle);

		// reset mem
		pass_len = 0;
		memset(iv, 0, sizeof(iv));
		memset(pass_hass, 0, sizeof(pass_hass));
		memset(wallet_data_hash, 0, sizeof(wallet_data_hash));
		memset(&ctx, 0, sizeof(ctx));
		memset(dec_buff, 0, dec_len);
		memset(rs_data, 0, nr_shards*rs_block_size);
		memset(rs_data_blocks, 0, nr_shards * sizeof(unsigned char*));

		vm_free(rs_data);
		vm_free(rs_data_blocks);
		vm_free(dec_buff);
#endif
		_reed_solomon_release(rs);
	}
	//vm_file_mkdir
}

VMWCHAR w_old_wallet_pass[32];
VMWCHAR w_old_wallet_path[MAX_APP_NAME_LEN];

void update_old_wallet_pass(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		int pass_len = vm_wstrlen(text);
		if (pass_len >= 16)
		{
			int loaded_wallet_len = 0;
			char* loaded_wallet = NULL;
			int type = 0;
			bool loaded = 0;

			vm_wstrcpy(w_old_wallet_pass, text);

			loaded = load_encrypted_wallet(w_old_wallet_path, &type, &loaded_wallet, &loaded_wallet_len, w_old_wallet_pass);
			if (loaded && loaded_wallet)
			{
				if (type == TYPE_MNEMONIC)
				{
					set_mnemo(loaded_wallet);
				} else if (type == TYPE_WIF) {
					set_wif(loaded_wallet);
				}
				g_wallet_type = type;
				set_type(type);
				
				memset(loaded_wallet, 0, loaded_wallet_len);
				memset(w_old_wallet_pass, 0, sizeof(w_old_wallet_pass));
				memset(w_old_wallet_path, 0, sizeof(w_old_wallet_path));

				vm_free(loaded_wallet);
				if (g_wallet_type == TYPE_MNEMONIC) {
					set_view(NK_MRE_VIEW_WALLET_MAIN_1);
				} else if (g_wallet_type == TYPE_WIF) {
					//create_priv_key();
					set_view(NK_MRE_VIEW_WALLET_WIF_1);
				}
			}
		}
		update_gui();
	}
}

int input_old_wallet_password(int component_index)
{
	w_old_wallet_pass[0] = 0;
	vm_input_text3(w_old_wallet_pass, 31, VM_INPUT_METHOD_PASSWORD,
                                     update_old_wallet_pass);
	return 0;
}

VMINT wallet_selection_cb(VMWCHAR * file_path, VMINT wlen)
{
	if (wlen > 0)
	{
		vm_wstrcpy(w_old_wallet_path, file_path);

		set_view(NK_MRE_VIEW_WALLET_UNLOCK_PIN);
		update_gui();
		//input_old_wallet_password(0);
	}
	return 0;
}

int select_wallet(int component_index)
{
	VMWCHAR w_path[MAX_APP_NAME_LEN];
	char path[MAX_APP_NAME_LEN];
	int res = -1;
	sprintf (path, "%c:\\btc_mre_wallet\\", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	res = vm_selector_run(VM_SELECTOR_TYPE_ALL, w_path, wallet_selection_cb);

	if (res == VM_SELECTOR_ERR_SUCCESS)
	{
		
	}


	return 0;
}

int recover_wallet(int component_index)
{
	set_view(NK_MRE_VIEW_WALLET_RECOVERY);
	update_gui(); // next view has hoover that updates the gui

	return 0;
}

int create_wallet(int component_index)
{
	set_view(NK_MRE_VIEW_WALLET_CREATION_1);
	update_gui(); // next view has hoover that updates the gui

	return 0;
}

void vm_rec_cb(VMINT result)
{
	if (result == 666) {
		result = 0;
		//printf(result);
	}
}

int testtest(int component_index)
{

	int rec_res = VM_RECORD_FAILED;
	VMWCHAR w_full_path[MAX_APP_NAME_LEN];
	char drive[16];
	memset(w_full_path, 0, sizeof(w_full_path));
	sprintf(drive, "%c", mre_get_drv());
	rec_res = vm_record_start(drive, "\\", "TEST", VM_FORMAT_AMR, w_full_path, vm_rec_cb);
	update_gui(); // use it like a sleep function
	rec_res = vm_record_stop();

	set_view(NK_MRE_VIEW_TESTTEST);
	update_gui();
	return 0;
}

int open_create_2(int component_index)
{
	struct mre_nk_view* view;
	view = get_mre_nk_view();
	//view->components[component_index]

	
	set_view(NK_MRE_VIEW_WALLET_CREATION_2);
	update_gui();

	return 0;
}

void replace_mnemo_view_create_3(char* mnemo)
{
	struct mre_nk_view* view = &g_view[NK_MRE_VIEW_WALLET_CREATION_3];
	char* oldtext = view->components[0]->text;
	char* mnemo_rows = vm_calloc(33* MAX_WORD_LEN_240x320 + 1);
	char* str = NULL;
	//MAX_WORD_LEN_240x320
	int i = 0;
	int j = 0;
	char *p;

	int str_alloc_len = strlen(mnemo)+1;
	str = vm_malloc(str_alloc_len);
	strcpy(str, mnemo);

    p = strtok (str," ");
    while (p != NULL)
    {
        //printf ("%s\n", p);
		int word_len = 0;
		char* curr_row = mnemo_rows + (i * MAX_WORD_LEN_240x320);
		sprintf(curr_row, "%02d. %s", (i+1), p);
		word_len = strlen(curr_row);
		for (j = word_len; j < MAX_WORD_LEN_240x320; j++)
		{
			strcat(curr_row, " ");
		}
		i++;
        p = strtok (NULL, " ");
    }

	view->components[0]->text = mnemo_rows;
	if (oldtext)
	{
		memset(oldtext, 0, strlen(oldtext));
		vm_free(oldtext);
	}

	memset(str, 0, str_alloc_len);
	vm_free(str);
}

int open_create_3(int component_index)
{
	const char* mnemonics;
	struct mre_nk_view* view;
	view = get_mre_nk_view();
	//view->components[component_index]
	
	//component_index
	if (component_index == 0)
	{
		g_wallet_type = TYPE_MNEMONIC;
		mnemonics = (const char*)mnemonic_generate(256);
	} else if (component_index == 1) {
		g_wallet_type = TYPE_MNEMONIC;
		mnemonics = (const char*)mnemonic_generate(192);
	} else if (component_index == 2) {
		g_wallet_type = TYPE_MNEMONIC;
		mnemonics = (const char*)mnemonic_generate(128);
	} else {
		return 0;
	}
	
	set_mnemo((char*)mnemonics);
	replace_mnemo_view_create_3((char*)mnemonics);

	mnemonic_clear();
	
	set_view(NK_MRE_VIEW_WALLET_CREATION_3);
	update_gui();

	return 0;
}


int open_create_pin(int component_index)
{
	struct mre_nk_view* view;

	set_view(NK_MRE_VIEW_WALLET_CREATION_4);
	view = &g_view[NK_MRE_VIEW_WALLET_CREATION_4];
	view->previous_id = NK_MRE_VIEW_WALLET_CREATION_3;
	
	update_gui();
	return 0;
}



// input_new_wallet_password
// input_new_wallet_name

VMWCHAR w_wallet_pass[32];
VMWCHAR w_wallet_name[32];
char wallet_name[32];

void update_wallet_name(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		int name_len = vm_wstrlen(text);
		if (name_len > 0)
		{
			vm_wstrcpy(w_wallet_name, text);

			vm_ucs2_to_ascii(wallet_name, 32, w_wallet_name);
			set_type(g_wallet_type);

			if (g_wallet_type == TYPE_MNEMONIC) {
				create_encrypted_wallet(wallet_name, g_wallet_type, get_mnemo(), strlen(get_mnemo())+1, w_wallet_pass);
			} else if (g_wallet_type == TYPE_WIF) {
				create_encrypted_wallet(wallet_name, g_wallet_type, get_wif(), strlen(get_wif())+1, w_wallet_pass);
			}

			memset(w_wallet_pass, 0, sizeof(w_wallet_pass));
			memset(w_wallet_name, 0, sizeof(w_wallet_name));
			memset(wallet_name, 0, sizeof(wallet_name));

			if (g_wallet_type == TYPE_MNEMONIC) {
				set_view(NK_MRE_VIEW_WALLET_MAIN_1);
			} else if (g_wallet_type == TYPE_WIF) {
				//create_priv_key();
				set_view(NK_MRE_VIEW_WALLET_WIF_1);
			}
			
		}
		update_gui();
	}
}

void update_wallet_pass(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		int pass_len = vm_wstrlen(text);
		if (pass_len >= 16)
		{
			vm_wstrcpy(w_wallet_pass, text);
			set_view(NK_MRE_VIEW_WALLET_CREATION_5);
		}
		update_gui();
	}
}

int input_new_wallet_name(int component_index)
{
	w_wallet_name[0] = 0;
	vm_input_text3(w_wallet_name, 31, VM_INPUT_METHOD_TEXT,
                                     update_wallet_name);
	return 0;
}

int input_new_wallet_password(int component_index)
{
	w_wallet_pass[0] = 0;
	vm_input_text3(w_wallet_pass, 31, VM_INPUT_METHOD_PASSWORD,
                                     update_wallet_pass);
	return 0;
}

VMWCHAR w_optional_passphrase[257];

void update_wallet_passphrase(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		char optional_passphrase[257];

		vm_wstrcpy(w_optional_passphrase, text);
		vm_ucs2_to_ascii(optional_passphrase, 257, w_optional_passphrase);

		set_passphrase(optional_passphrase);

		memset(optional_passphrase, 0, sizeof(optional_passphrase));
		memset(w_optional_passphrase, 0, sizeof(w_optional_passphrase));

		set_view(NK_MRE_VIEW_WALLET_MAIN_2);
		update_gui();
	}
}

int open_wallet_with_passphrase(int x)
{
	w_optional_passphrase[0] = 0;
	vm_input_text3(w_optional_passphrase, 256, VM_INPUT_METHOD_ALPHABETIC,
                                     update_wallet_passphrase);
	return 0;
}

VMWCHAR w_custom_deriv_path[64];

void update_custom_deriv_path(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		char custom_deriv_path[64];

		vm_wstrcpy(w_custom_deriv_path, text);
		vm_ucs2_to_ascii(custom_deriv_path, 64, w_custom_deriv_path);

		set_derivationpath(custom_deriv_path, "NONE");
		derive_xpriv();

		memset(custom_deriv_path, 0, sizeof(custom_deriv_path));
		memset(w_custom_deriv_path, 0, sizeof(w_custom_deriv_path));

		set_view(NK_MRE_VIEW_WALLET_MAIN_3);
		update_gui();
	}
}

int open_wallet_without_passphrase(int x)
{
	set_passphrase("");
	set_view(NK_MRE_VIEW_WALLET_MAIN_2);
	update_gui();
	return 0;
}

int open_bip84(int x)
{
#ifndef TRY_TESTNET
	set_derivationpath("m/84'/0'/0'/0", "m/84'/0'/0'");
#else
	set_derivationpath("m/84'/1'/0'/0", "m/84'/1'/0'");
#endif
	set_hardened(false);
	derive_xpriv();
	set_view(NK_MRE_VIEW_WALLET_MAIN_3);
	update_gui();
	return 0;
}

int open_bip44(int x)
{
#ifndef TRY_TESTNET
	set_derivationpath("m/44'/0'/0'/0", "m/44'/0'/0'");
#else
	set_derivationpath("m/44'/1'/0'/0", "m/44'/1'/0'");
#endif
	set_hardened(false);
	derive_xpriv();
	set_view(NK_MRE_VIEW_WALLET_MAIN_3);
	update_gui();
	return 0;
}

int open_bip49(int x)
{
#ifndef TRY_TESTNET
	set_derivationpath("m/49'/0'/0'/0", "m/49'/0'/0'");
#else
	set_derivationpath("m/49'/1'/0'/0", "m/49'/1'/0'");
#endif
	set_hardened(false);
	derive_xpriv();
	set_view(NK_MRE_VIEW_WALLET_MAIN_3);
	update_gui();
	return 0;
}

int open_btc_core(int x)
{
	set_derivationpath("m/0'/0'", "NONE");
	set_hardened(true);
	derive_xpriv();
	set_view(NK_MRE_VIEW_WALLET_MAIN_3);
	update_gui();
	return 0;
}

int open_root_xpub(int x)
{
	set_derivationpath("NONE", "NONE");
	set_hardened(false);
	derive_xpriv();
	set_view(NK_MRE_VIEW_WALLET_MAIN_3);
	update_gui();
	return 0;
}

int open_custom_dp(int x)
{
	char bip32[64] = "m/0";
	//set_derivationpath("m/0");
	set_hardened(false);

	vm_ascii_to_ucs2(w_custom_deriv_path, sizeof(w_custom_deriv_path), bip32);
	vm_input_text3(w_custom_deriv_path, 63, VM_INPUT_METHOD_ALPHABETIC,
                                     update_custom_deriv_path);
	//derive_xpriv();
	//set_view(NK_MRE_VIEW_WALLET_MAIN_3);
	//update_gui();
	return 0;
}

int open_custom_dp_hardened(int x)
{
	char bip32[64] = "m/0";
	//set_derivationpath("m/0");
	set_hardened(true);

	vm_ascii_to_ucs2(w_custom_deriv_path, sizeof(w_custom_deriv_path), bip32);
	vm_input_text3(w_custom_deriv_path, 63, VM_INPUT_METHOD_ALPHABETIC,
                                     update_custom_deriv_path);
	//derive_xpriv();
	//set_view(NK_MRE_VIEW_WALLET_MAIN_3);
	//update_gui();
	return 0;
}

int set_addr_segwit(int x)
{
	set_pub_type(TYPE_SEGWIT);
	create_priv_key();
	set_view(NK_MRE_VIEW_WALLET_WIF_2);
	update_gui();

	return 0;
}


int set_addr_legacy(int x)
{
	set_pub_type(TYPE_LEGACY);
	create_priv_key();
	set_view(NK_MRE_VIEW_WALLET_WIF_2);
	update_gui();

	return 0;
}


int set_addr_nested_segwit(int x)
{
	set_pub_type(TYPE_NESTED_SEGWIT);
	create_priv_key();
	set_view(NK_MRE_VIEW_WALLET_WIF_2);
	update_gui();

	return 0;
}

enum display_what {
	SENSITIVE_XPRIV,
	SENSITIVE_XPUB,
	SENSITIVE_MNEMO,
	SENSITIVE_WIF,
	SENSITIVE_PUB
};

int g_display_what = 0;

void update_sensitive_data()
{
	struct mre_nk_view* view = &g_view[NK_MRE_VIEW_DISPLAY_SENSITIVE];
	char* oldtext1 = view->components[0]->text;
	char* oldtext2 = view->components[1]->text;

	char* type = (char*)vm_calloc(64);
	char* sensitive = NULL;

	if (g_display_what == SENSITIVE_XPRIV)
	{
		strcpy(type, "Private info");
		sensitive = (char*)vm_calloc(strlen(get_xpriv())+1);
		strcpy(sensitive, get_xpriv());
	} else if  (g_display_what == SENSITIVE_MNEMO)
	{
		strcpy(type, "Mnemonics");
		sensitive = (char*)vm_calloc(strlen(get_mnemo())+1);
		strcpy(sensitive, get_mnemo());
	} else if (g_display_what == SENSITIVE_WIF)
	{
		strcpy(type, "Private key");
		sensitive = (char*)vm_calloc(strlen(get_priv())+1);
		strcpy(sensitive, get_priv());
	} else if (g_display_what == SENSITIVE_XPUB)
	{
		strcpy(type, "Public info");
		sensitive = (char*)vm_calloc(strlen(get_xpub())+1);
		strcpy(sensitive, get_xpub());
	} else if (g_display_what == SENSITIVE_PUB)
	{
		strcpy(type, "Public key");
		sensitive = (char*)vm_calloc(strlen(get_pub())+1);
		strcpy(sensitive, get_pub());
	} else {
		sensitive = (char*)vm_calloc(16);
	}
	
	view->components[0]->text = type;
	view->components[1]->text = sensitive;
	//view->components[2] = NULL;

	if (oldtext1)
	{
		memset(oldtext1, 0, strlen(oldtext1));
		vm_free(oldtext1);
	}
	if (oldtext2)
	{
		memset(oldtext2, 0, strlen(oldtext2));
		vm_free(oldtext2);
	}
}

int display_sensitive(int a)
{
	update_sensitive_data();
	set_view(NK_MRE_VIEW_DISPLAY_SENSITIVE);
	update_gui();
	return 0;
}


int display_xpub(int a)
{
	struct mre_nk_view* view;
	g_display_what = SENSITIVE_XPUB;
	view = &g_view[NK_MRE_VIEW_DISPLAY_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	view = &g_view[NK_MRE_VIEW_WARNING_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	display_sensitive(0);
	//update_gui();
	return 0;
}

int display_xpriv(int a)
{
	struct mre_nk_view* view;
	g_display_what = SENSITIVE_XPRIV;
	view = &g_view[NK_MRE_VIEW_DISPLAY_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	view = &g_view[NK_MRE_VIEW_WARNING_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	set_view(NK_MRE_VIEW_WARNING_SENSITIVE);
	update_gui();
	return 0;
}

int display_mnemo(int a)
{
	struct mre_nk_view* view;
	g_display_what = SENSITIVE_MNEMO;
	view = &g_view[NK_MRE_VIEW_DISPLAY_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	view = &g_view[NK_MRE_VIEW_WARNING_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	set_view(NK_MRE_VIEW_WARNING_SENSITIVE);
	update_gui();
	return 0;
}

int display_pub(int a)
{
	struct mre_nk_view* view;
	g_display_what = SENSITIVE_PUB;
	view = &g_view[NK_MRE_VIEW_DISPLAY_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	view = &g_view[NK_MRE_VIEW_WARNING_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	display_sensitive(0);
	//update_gui();
	return 0;
}

int display_priv(int a)
{
	struct mre_nk_view* view;
	g_display_what = SENSITIVE_WIF;
	view = &g_view[NK_MRE_VIEW_DISPLAY_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	view = &g_view[NK_MRE_VIEW_WARNING_SENSITIVE];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	set_view(NK_MRE_VIEW_WARNING_SENSITIVE);
	update_gui();
	return 0;
}

enum save_what {
	SAVE_XPUB,
	SAVE_XPRIV,
	SAVE_PUB,
	SAVE_PRIV
};

int g_save_what = 0;

VMWCHAR w_save_name[MAX_APP_NAME_LEN];

void update_save_name(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		int name_len = vm_wstrlen(text);
		if (name_len > 0)
		{
			int written = 0;
			VMFILE handle = -1;

			vm_wstrcpy(w_save_name, text); // max 255
			vm_wstrcat(w_save_name, (VMWSTR)(L".txt")); //max 4, still 1 byte for null terminator

			handle = vm_file_open(w_save_name, MODE_CREATE_ALWAYS_WRITE, TRUE);
			if (handle > 0)
			{
				if (g_save_what == SAVE_XPUB)
				{
					char * xpub = get_xpub();
					if (xpub)
					{
						vm_file_write(handle, xpub, strlen(xpub), &written);
					}
				} else if (g_save_what == SAVE_XPRIV)
				{
					char * xpriv = get_xpriv();
					if (xpriv)
					{
						vm_file_write(handle, xpriv, strlen(xpriv), &written);
					}
				} else if (g_save_what == SAVE_PUB)
				{
					char * pub = get_pub();
					if (pub)
					{
						vm_file_write(handle, pub, strlen(pub), &written);
					}
				} else if (g_save_what == SAVE_PRIV)
				{
					char * priv = get_priv();
					if (priv)
					{
						vm_file_write(handle, priv, strlen(priv), &written);
					}
				}
				vm_file_close(handle);
			}

			memset(w_save_name, 0, sizeof(w_save_name));
			
		}
		update_gui();
	}
}

int input_save_name(int component_index)
{
	vm_input_text3(w_save_name, 255, VM_INPUT_METHOD_TEXT,
                                     update_save_name);
	return 0;
}

void save_info(int type, char* ext)
{
	VMWCHAR w_path[MAX_APP_NAME_LEN];
	char path[MAX_APP_NAME_LEN];
	sprintf (path, "%c:\\btc_mre_wallet", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_file_mkdir(w_path);
	sprintf (path, "%c:\\btc_mre_wallet\\%s", mre_get_drv(), ext);
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_file_mkdir(w_path);

	sprintf (path, "%c:\\btc_mre_wallet\\%s\\", mre_get_drv(), ext);
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_wstrcpy(w_save_name, w_path);
	g_save_what = type;
	input_save_name(0);
}

int save_sensitive(int a)
{
	struct mre_nk_view* view;
	view = &g_view[NK_MRE_VIEW_SENSITIVE_SAVED];
	if (g_save_what == SAVE_XPRIV)
	{
		save_info(SAVE_XPRIV, "xpriv");
		view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	} else if (g_save_what == SAVE_PRIV)
	{
		save_info(SAVE_PRIV, "priv");
		view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	} else if (g_save_what == SAVE_XPUB)
	{
		save_info(SAVE_XPUB, "xpub");
		view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	} else if (g_save_what == SAVE_PUB)
	{
		save_info(SAVE_PUB, "pub");
		view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	}
	set_view(NK_MRE_VIEW_SENSITIVE_SAVED);
	update_gui();
	return 0;
}

int save_xpub(int a)
{
	//save_info(SAVE_XPUB, "xpub");
	g_save_what = SAVE_XPUB;
	save_sensitive(0);
	return 0;
}

int save_xpriv(int a)
{
	//save_info(SAVE_XPRIV, "xpriv");
	struct mre_nk_view* view;
	g_save_what = SAVE_XPRIV;
	view = &g_view[NK_MRE_VIEW_SENSITIVE_SAVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	view = &g_view[NK_MRE_VIEW_SENSITIVE_SAVE];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	set_view(NK_MRE_VIEW_SENSITIVE_SAVE);
	update_gui();
	return 0;
}

int save_pub(int a)
{
	//save_info(SAVE_PUB, "pub");
	g_save_what = SAVE_PUB;
	save_sensitive(0);
	return 0;
}

int save_priv(int a)
{
	//save_info(SAVE_PRIV, "priv");
	struct mre_nk_view* view;
	g_save_what = SAVE_PRIV;
	view = &g_view[NK_MRE_VIEW_SENSITIVE_SAVE];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	view = &g_view[NK_MRE_VIEW_SENSITIVE_SAVE];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	set_view(NK_MRE_VIEW_SENSITIVE_SAVE);
	update_gui();
	return 0;
}

#if 0
unsigned char wallet_3_a8431ce5_psbt[] = {
  0x70, 0x73, 0x62, 0x74, 0xff, 0x01, 0x00, 0x71, 0x02, 0x00, 0x00, 0x00,
  0x01, 0x66, 0xc7, 0xc9, 0x87, 0x34, 0xc8, 0xf9, 0x21, 0x00, 0xa0, 0x9c,
  0xdc, 0x6f, 0x57, 0x69, 0xf6, 0x25, 0x8e, 0xa2, 0x4f, 0x6b, 0x59, 0x5e,
  0x77, 0x49, 0x9c, 0x95, 0x1a, 0x0d, 0xf2, 0xed, 0x62, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfd, 0xff, 0xff, 0xff, 0x02, 0x10, 0x27, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x16, 0x00, 0x14, 0x33, 0x32, 0x21, 0x5b, 0xfe, 0xb9,
  0xc6, 0xc2, 0xa9, 0xfa, 0xcb, 0x1d, 0x7d, 0x74, 0xb2, 0x39, 0x53, 0xca,
  0xb6, 0x69, 0x78, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00,
  0x14, 0x33, 0x32, 0x21, 0x5b, 0xfe, 0xb9, 0xc6, 0xc2, 0xa9, 0xfa, 0xcb,
  0x1d, 0x7d, 0x74, 0xb2, 0x39, 0x53, 0xca, 0xb6, 0x69, 0x16, 0xdb, 0x23,
  0x00, 0x00, 0x01, 0x00, 0xbf, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
  0xb4, 0xf2, 0x0d, 0x3e, 0xee, 0x7a, 0x94, 0x21, 0xe4, 0x63, 0xd0, 0x04,
  0xe7, 0x0a, 0xca, 0x4e, 0xf9, 0xa6, 0x70, 0xcd, 0x32, 0xf8, 0xa3, 0x66,
  0xac, 0x57, 0xf8, 0x29, 0xa5, 0x95, 0xa5, 0x7d, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xfd, 0xff, 0xff, 0xff, 0x01, 0x52, 0xdc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x16, 0x00, 0x14, 0x33, 0x32, 0x21, 0x5b, 0xfe, 0xb9, 0xc6,
  0xc2, 0xa9, 0xfa, 0xcb, 0x1d, 0x7d, 0x74, 0xb2, 0x39, 0x53, 0xca, 0xb6,
  0x69, 0x02, 0x47, 0x30, 0x44, 0x02, 0x20, 0x18, 0xeb, 0x88, 0x5d, 0x02,
  0x27, 0xe8, 0x87, 0xb0, 0x6f, 0x03, 0x97, 0x58, 0x5e, 0x9b, 0xe8, 0xbc,
  0x6a, 0x2e, 0x7a, 0xb7, 0xd6, 0x25, 0xbb, 0x09, 0x39, 0xcc, 0xd1, 0x73,
  0x98, 0x66, 0x50, 0x02, 0x20, 0x52, 0xc7, 0x3d, 0xbe, 0x2c, 0x74, 0x2d,
  0x54, 0x60, 0x04, 0x99, 0xdc, 0x52, 0xf6, 0xf3, 0x96, 0x0c, 0x47, 0x5b,
  0x57, 0xee, 0x45, 0xae, 0x35, 0x0d, 0xe4, 0x3b, 0x52, 0x29, 0x62, 0xda,
  0x1c, 0x01, 0x21, 0x02, 0x47, 0x75, 0x40, 0xb5, 0xc5, 0x0a, 0x6f, 0xbf,
  0x8e, 0x24, 0x58, 0x2a, 0x8c, 0x0d, 0xe8, 0xa2, 0xd7, 0x4a, 0x70, 0x58,
  0xa9, 0x31, 0x7d, 0xee, 0x88, 0x52, 0xb4, 0xda, 0x6a, 0xe9, 0xe2, 0xfb,
  0xfa, 0xda, 0x23, 0x00, 0x00, 0x00, 0x00
};
unsigned int wallet_3_a8431ce5_psbt_len = 319;

void save_test_psbt()
{
	VMWCHAR w_path[MAX_APP_NAME_LEN];
	char path[MAX_APP_NAME_LEN];
	VMUINT written = 0;
	VMFILE handle = -1;
	
	sprintf(path, "%c:\\btc_mre_wallet\\psbt\\testnet_wif.psbt", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);

	handle = vm_file_open(w_path, MODE_CREATE_ALWAYS_WRITE, FALSE);
	if (handle > 0)
	{
		vm_file_write(handle, wallet_3_a8431ce5_psbt, wallet_3_a8431ce5_psbt_len, &written);
		vm_file_close(handle);
	}
}
#endif

#if 0
void save_test_psbt_mnemo()
{
	char psbt[] = "cHNidP8BAJoCAAAAAkzxWp0FJBpa/6aEqajG7t5H3T6ZIlm1jnJHZ0TkW7s/AAAAAAD9////TPFanQUkGlr/poSpqMbu3kfdPpkiWbWOckdnRORbuz8BAAAAAP3///8C8a08AAAAAAAWABQEk2IHzvbP1TUu+9x+LA6/R1B2nUBCDwAAAAAAFgAUieM46pZMD41V7zLOKTomEbsMfgsm3SMAAAEBH4CEHgAAAAAAFgAUMzIhW/65xsKp+ssdfXSyOVPKtmkiBgL3BLA31U+ZFD/OeYhirRjyOlZF3gTML8v59nZ7p0jT/wzm33BXAAAAAAAAAAAAAQEfhGwtAAAAAAAWABQzMiFb/rnGwqn6yx19dLI5U8q2aSIGAvcEsDfVT5kUP855iGKtGPI6VkXeBMwvy/n2dnunSNP/DObfcFcAAAAAAAAAAAAiAgJiFCBDt79rUci8yT5pRnkfMfQItgQVf5AyV2X8FgvuDgzm33BXAQAAAAAAAAAAIgICp2/LooSPfTg334nbxDpXfMjjV5Y2rN0M/d+TH9op0CcM5t9wVwAAAAABAAAAAA==";
	VMWCHAR w_path[MAX_APP_NAME_LEN];
	char path[MAX_APP_NAME_LEN];
	VMUINT written = 0;
	VMFILE handle = -1;
	
	sprintf(path, "%c:\\btc_mre_wallet\\psbt\\testnet_mnemo.psbt", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);

	handle = vm_file_open(w_path, MODE_CREATE_ALWAYS_WRITE, FALSE);
	if (handle > 0)
	{
		vm_file_write(handle, psbt, strlen(psbt), &written);
		vm_file_close(handle);
	}
}
#endif

VMWCHAR w_psbt_path[MAX_APP_NAME_LEN];

bool sign_now(bool is_hd)
{
	bool res = false;
	void* file_data = NULL;
	VMUINT nread = 0;
	VMUINT file_size = 0;
	VMFILE handle = vm_file_open(w_psbt_path, MODE_READ, TRUE);
	if (handle > 0)
	{
		uint32_t psbt_header_1 = 0;
		uint32_t psbt_header_2 = 0;
		void* signed_pstb_data = NULL;
		size_t signed_pstb_len = 0;
		bool is_signed = false;
		VMFILE wr_handle = -1;
		VMUINT nwrite = 0;
		bool b_skip_b64_conv = false;
		size_t dec_data_len = 0;
		void* dec_data = NULL;
		
		vm_file_getfilesize(handle, &file_size);
		file_data = vm_malloc(file_size);
		if (file_data == NULL)
		{
			vm_file_close(handle);
			return false;
		}

		// read buff
		vm_file_read(handle, file_data, file_size, &nread);
		vm_file_close(handle);

		if (file_size < 8)
		{
			return false;
		}
		
		// wow, impressively ugly
		psbt_header_1 = (uint32_t)*(uint32_t**)file_data;
		psbt_header_2 = (uint32_t)*(uint32_t**)((uint8_t*)file_data+4);

		// psbt
		if (psbt_header_1 == 0x74627370)
		{
			b_skip_b64_conv = true;

		} else if (psbt_header_1 == 0x33373037 && psbt_header_1 == 0x34373236) // 37 30 37 33 36 32 37 34
		{
			b_skip_b64_conv = true;
		} 

		if (b_skip_b64_conv == false)
		{
			dec_data_len = c_fromBase64Length((const uint8_t *)file_data, file_size, 0);
			dec_data = vm_calloc(dec_data_len);
			c_fromBase64((const char *)file_data, file_size, dec_data, dec_data_len, 0);

			vm_free(file_data);

			file_data = dec_data;
			file_size = dec_data_len;
		}

		if (is_hd)
		{
			is_signed = sign_psbt_from_hd((uint8_t*)file_data, file_size, &signed_pstb_data, &signed_pstb_len);
		} else {
			is_signed = sign_psbt_from_pk((uint8_t*)file_data, file_size, &signed_pstb_data, &signed_pstb_len);
		}
		if (is_signed && signed_pstb_data && signed_pstb_len > 0)
		{
			unsigned short* ext = L".signed.psbt";
			vm_wstrcat(w_psbt_path, (VMWSTR)ext);
			wr_handle = vm_file_open(w_psbt_path, MODE_CREATE_ALWAYS_WRITE, TRUE);

			vm_file_write(wr_handle, signed_pstb_data, signed_pstb_len, &nwrite);
			vm_file_close(wr_handle);

			memset(signed_pstb_data, 0, signed_pstb_len);
			vm_free(signed_pstb_data);

			res = true;
		}
		vm_free(file_data);
	}
	return res;
}


bool sign_now_qr(bool is_hd)
{
	bool res = false;		
	void* file_data = my_strdup(g_last_decoded_qr);
	VMUINT file_size = strlen(g_last_decoded_qr);

		uint32_t psbt_header_1 = 0;
		uint32_t psbt_header_2 = 0;
		void* signed_pstb_data = NULL;
		size_t signed_pstb_len = 0;
		bool is_signed = false;
		VMFILE wr_handle = -1;
		VMUINT nwrite = 0;
		bool b_skip_b64_conv = false;
		size_t dec_data_len = 0;
		void* dec_data = NULL;

		vm_time_t curr_time = {0};

#if 0
typedef struct vm_time_t {
	VMINT year;		
	VMINT mon;		/* month, begin from 1	*/
	VMINT day;		/* day,begin from  1 */
	VMINT hour;		/* house, 24-hour	*/
	VMINT min;		/* minute	*/
	VMINT sec;		/* second	*/
} vm_time_t;
#endif
	char path[MAX_APP_NAME_LEN];

	vm_get_time(&curr_time);

	sprintf (path, "%c:\\btc_mre_wallet", mre_get_drv());
	vm_ascii_to_ucs2(w_psbt_path, sizeof(w_psbt_path), path);
	vm_file_mkdir(w_psbt_path);
	sprintf (path, "%c:\\btc_mre_wallet\\psbt", mre_get_drv());
	vm_ascii_to_ucs2(w_psbt_path, sizeof(w_psbt_path), path);
	vm_file_mkdir(w_psbt_path);
	sprintf (path, "%c:\\btc_mre_wallet\\psbt\\%d_%d_%d-%d_%d_%d", mre_get_drv(), curr_time.day, curr_time.mon, curr_time.year, curr_time.hour, curr_time.min, curr_time.sec);
	vm_ascii_to_ucs2(w_psbt_path, sizeof(w_psbt_path), path);

		if (file_size < 8)
		{
			return false;
		}
		
		// wow, impressively ugly
		psbt_header_1 = (uint32_t)*(uint32_t**)file_data;
		psbt_header_2 = (uint32_t)*(uint32_t**)((uint8_t*)file_data+4);

		// psbt
		if (psbt_header_1 == 0x74627370)
		{
			b_skip_b64_conv = true;

		} else if (psbt_header_1 == 0x33373037 && psbt_header_1 == 0x34373236) // 37 30 37 33 36 32 37 34
		{
			b_skip_b64_conv = true;
		} 

		if (b_skip_b64_conv == false)
		{
			dec_data_len = c_fromBase64Length((const uint8_t *)file_data, file_size, 0);
			dec_data = vm_calloc(dec_data_len);
			c_fromBase64((const char *)file_data, file_size, dec_data, dec_data_len, 0);

			vm_free(file_data);

			file_data = dec_data;
			file_size = dec_data_len;
		}

		if (is_hd)
		{
			is_signed = sign_psbt_from_hd((uint8_t*)file_data, file_size, &signed_pstb_data, &signed_pstb_len);
		} else {
			is_signed = sign_psbt_from_pk((uint8_t*)file_data, file_size, &signed_pstb_data, &signed_pstb_len);
		}
		if (is_signed && signed_pstb_data && signed_pstb_len > 0)
		{
			unsigned short* ext = L".signed.psbt";
			vm_wstrcat(w_psbt_path, (VMWSTR)ext);
			wr_handle = vm_file_open(w_psbt_path, MODE_CREATE_ALWAYS_WRITE, TRUE);

			vm_file_write(wr_handle, signed_pstb_data, signed_pstb_len, &nwrite);
			vm_file_close(wr_handle);

			memset(signed_pstb_data, 0, signed_pstb_len);
			vm_free(signed_pstb_data);

			res = true;
		}

	return res;
}

void sign_now_non_hd(bool (*sign_now_fptr)(bool is_hd))
{
		struct mre_nk_view* view;

		bool is_signed = false;
		
		is_signed = sign_now_fptr(false);
		if (is_signed)
		{
			view = &g_view[NK_MRE_VIEW_WALLET_PSBT_SIGNED];
			view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
			set_view(NK_MRE_VIEW_WALLET_PSBT_SIGNED);
		} else {
			view = &g_view[NK_MRE_VIEW_WALLET_PSBT_ERROR];
			view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
			set_view(NK_MRE_VIEW_WALLET_PSBT_ERROR);
		}
		update_gui();
}

void sign_now_hd(bool (*sign_now_fptr)(bool is_hd))
{
		struct mre_nk_view* view;

		bool is_signed = false;
		
		is_signed = sign_now_fptr(true);
		if (is_signed)
		{
			view = &g_view[NK_MRE_VIEW_WALLET_PSBT_SIGNED];
			view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
			set_view(NK_MRE_VIEW_WALLET_PSBT_SIGNED);
		} else {
			view = &g_view[NK_MRE_VIEW_WALLET_PSBT_ERROR];
			view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
			set_view(NK_MRE_VIEW_WALLET_PSBT_ERROR);
		}
		update_gui();
}


void sign_psbt_mnemo_qr_after()
{
		sign_now_hd(sign_now_qr);
}
void sign_psbt_wif_qr_after()
{
		sign_now_non_hd(sign_now_qr);
}

VMINT psbt_selection_wif_cb(VMWCHAR * file_path, VMINT wlen)
{
	if (wlen > 0)
	{
		vm_wstrcpy(w_psbt_path, file_path);
		sign_now_non_hd(sign_now);
	}
	return 0;
}

VMINT psbt_selection_mnemo_cb(VMWCHAR * file_path, VMINT wlen)
{
	if (wlen > 0)
	{
		vm_wstrcpy(w_psbt_path, file_path);
		sign_now_hd(sign_now);
	}
	return 0;
}

int sign_psbt_mnemo(int a)
{
	struct mre_nk_view* view;
	VMWCHAR w_path[MAX_APP_NAME_LEN];
	char path[MAX_APP_NAME_LEN];
	sprintf (path, "%c:\\btc_mre_wallet", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_file_mkdir(w_path);
	sprintf (path, "%c:\\btc_mre_wallet\\psbt", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_file_mkdir(w_path);
	sprintf (path, "%c:\\btc_mre_wallet\\psbt\\", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);

	view = &g_view[NK_MRE_VIEW_WALLET_PSBT_SIGNED];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	view = &g_view[NK_MRE_VIEW_WALLET_PSBT_ERROR];
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;

	//save_test_psbt_mnemo();

	vm_selector_run(VM_SELECTOR_TYPE_ALL, w_path, psbt_selection_mnemo_cb);
	
	return 0;
}

int sign_psbt_wif(int a)
{
	struct mre_nk_view* view;
	VMWCHAR w_path[MAX_APP_NAME_LEN];
	char path[MAX_APP_NAME_LEN];
	sprintf (path, "%c:\\btc_mre_wallet", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_file_mkdir(w_path);
	sprintf (path, "%c:\\btc_mre_wallet\\psbt", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);
	vm_file_mkdir(w_path);
	sprintf (path, "%c:\\btc_mre_wallet\\psbt\\", mre_get_drv());
	vm_ascii_to_ucs2(w_path, sizeof(w_path), path);

	view = &g_view[NK_MRE_VIEW_WALLET_PSBT_SIGNED];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	view = &g_view[NK_MRE_VIEW_WALLET_PSBT_ERROR];
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;

	//save_test_psbt();

	vm_selector_run(VM_SELECTOR_TYPE_ALL, w_path, psbt_selection_wif_cb);
	
	return 0;
}

VMWCHAR w_mnemo_rec[512];
VMWCHAR w_wif_rec[64];

void update_mnemo_rec(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		struct mre_nk_view* view;

		char mnemo_rec[512];
		char mnemo_rec_tmp[512];

		vm_wstrcpy(w_mnemo_rec, text);
		vm_ucs2_to_ascii(mnemo_rec_tmp, 512, w_mnemo_rec);

		vm_lower_case(mnemo_rec, mnemo_rec_tmp);

		if (mnemonic_check(mnemo_rec) != 0)
		{
			set_mnemo((char*)mnemo_rec);
			g_wallet_type = TYPE_MNEMONIC;
		
			set_view(NK_MRE_VIEW_WALLET_CREATION_4);
			view = &g_view[NK_MRE_VIEW_WALLET_CREATION_4];
			view->previous_id = NK_MRE_VIEW_WALLET_RECOVERY;
			update_gui();
		}

		memset(mnemo_rec, 0, sizeof(mnemo_rec));
		memset(mnemo_rec_tmp, 0, sizeof(mnemo_rec_tmp));
		memset(w_mnemo_rec, 0, sizeof(w_mnemo_rec));
		mnemonic_clear();
	}
}

void update_wif_rec(VMINT state, VMWSTR text)
{
	if (text != NULL && state == VM_INPUT_OK)
	{
		struct mre_nk_view* view;

		char wif_rec[64];

		vm_wstrcpy(w_wif_rec, text);
		vm_ucs2_to_ascii(wif_rec, 64, w_wif_rec);

		if (is_wif_valid(wif_rec))
		{
			set_wif((char*)wif_rec);
			g_wallet_type = TYPE_WIF;
		
			set_view(NK_MRE_VIEW_WALLET_CREATION_4);
			view = &g_view[NK_MRE_VIEW_WALLET_CREATION_4];
			view->previous_id = NK_MRE_VIEW_WALLET_RECOVERY;
			update_gui();
		}

		memset(wif_rec, 0, sizeof(wif_rec));
		memset(w_wif_rec, 0, sizeof(w_wif_rec));
		mnemonic_clear();
	}
}

int recover_mnemo(int idx)
{
	//char mnemo_test_vector[512] = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
#ifdef TRY_TESTNET
	char mnemo_test_vector[512] = "stadium crowd forward sniff armed vacant walk unit just eagle castle amused";
	vm_ascii_to_ucs2(w_mnemo_rec, sizeof(w_mnemo_rec), mnemo_test_vector);
#else
	w_mnemo_rec[0] = 0;
#endif
	vm_input_text3(w_mnemo_rec, 511, VM_INPUT_METHOD_ALPHABETIC,
                                     update_mnemo_rec);
	return 0;
}

int recover_wif(int idx)
{
	//char wiftest[64] = "L22hMkYd8zbknEkKVAzbzxjXx6igV6teQmQtCQAALrqi5BFrSNPV";
#ifdef TRY_TESTNET
	char wiftest[64] = "cQE6SdYC9oP5ghRdFuyKkBDKR5NvPpPkHLLqKWzfjF7zAF73FZE6";
	vm_ascii_to_ucs2(w_wif_rec, sizeof(w_wif_rec), wiftest);
#else
	w_wif_rec[0] = 0;
#endif
	vm_input_text3(w_wif_rec, 52, VM_INPUT_METHOD_ALPHABETIC,
                                     update_wif_rec);
	return 0;
}

void scan_qr_mnemo_after()
{
	vm_ascii_to_ucs2(w_mnemo_rec, sizeof(w_mnemo_rec), g_last_decoded_qr);
	vm_input_text3(w_mnemo_rec, 511, VM_INPUT_METHOD_ALPHABETIC,
                                     update_mnemo_rec);
}

void scan_qr_wif_after()
{
	vm_ascii_to_ucs2(w_wif_rec, sizeof(w_wif_rec), g_last_decoded_qr);
	vm_input_text3(w_wif_rec, 52, VM_INPUT_METHOD_ALPHABETIC,
                                     update_wif_rec);
}

int recover_pk(int idx)
{

	return 0;
}

int recover_xpriv(int idx)
{

	return 0;
}

char* decode_qr(void *raw, size_t raw_sz, int width, int height)
{
	char* decoded_str = NULL;
	zbar_image_scanner_t *scanner = NULL;
	
	zbar_image_t *image = NULL;
	const zbar_symbol_t *symbol = NULL;
	int n = 0;
    /* create a reader */
    scanner = zbar_image_scanner_create();

    /* configure the reader */
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    /* wrap image data */
    image = zbar_image_create();
    zbar_image_set_format(image, *(int*)"GREY");
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, raw, width * height, zbar_image_free_data);

    /* scan the image for barcodes */
    n = zbar_scan_image(scanner, image);
	dbg("[+] zbar_scan_image: %d\n", n);

    /* extract results */
    symbol = zbar_image_first_symbol(image);
    for(; symbol; symbol = zbar_symbol_next(symbol)) {
        /* do something useful with results */
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
		//unsigned int data_len = zbar_symbol_get_data_length(symbol);
		//unsigned int decoded_str_len = strlen(data);
        dbg("[+] decoded %s symbol \"%s\"\n",
               zbar_get_symbol_name(typ), data);

		decoded_str = vm_malloc(strlen(data)+1);
		strcpy(decoded_str, data);
		break;
    }

    /* clean up */
	//memset(raw, 0, raw_sz);
	//raw is freed by zbar_image_destroy(image);
    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);

	return decoded_str;
}

void uyvyToGrey(unsigned char *dst, unsigned char *src,  unsigned int numberPixels)
{
	while(numberPixels > 0) 
	{
		uint8_t y1;
		uint8_t y2;
	      src++;
	      y1=*(src++);
	      src++;
	      y2=*(src++);
	      *(dst++)=y1;
	      *(dst++)=y2;
	      numberPixels-=2;
	}
}

//void create_view_qr_text();
bool is_digit(uint8_t chr)
{
	if (chr>=0x30 && chr<=0x39)
	{
		return true;
	}
	return false;
}

bool is_multi_part_qr(char* in_str, int* current_part, int* total_parts, char** ptr_out_str)
{
	int str_len = 0;
	int i = 0;
	int curr_part = 0;
	int total_part = 0;
	int curr_part_count = 0;
	int total_part_count = 0;
	uint8_t curr_part_str[4] = {0};
	uint8_t total_part_str[4] = {0};
	char* tmp_ptr = NULL;
	char* out_str_tmp = in_str;

	bool parsing_p = true;
	bool parsing_curr = false;
	bool parsing_of = false;
	bool parsing_total = false;
	bool parsing_space = false;
	bool parsing_str = false;

	if (in_str == NULL)
	{
		goto retfalse;
	}
	str_len = strlen(in_str);
	for (i=0; i<str_len; i++)
	{
		if (parsing_p)
		{
			if(in_str[i] != 'p')
			{
				goto retfalse;
			}
			parsing_p = false;
			parsing_curr = true;
			continue;
		} else if (parsing_curr) {
			if(is_digit(in_str[i]) == false && curr_part_count == 0)
			{
				goto retfalse;
			} else if (is_digit(in_str[i]) && curr_part_count < 3)
			{
				curr_part_str[curr_part_count] = in_str[i];
				curr_part_count++;
				continue;
			} else
			{
				parsing_curr = false;
				parsing_of = true;
				i--;
				continue;
			}
		} else if (parsing_of) {
			if(in_str[i] != 'o' || in_str[i+1] != 'f') {
				goto retfalse;
			} else
			{
				parsing_of = false;
				parsing_total = true;
				i++;
				continue;
			}
		} else if (parsing_total) {
			if(is_digit(in_str[i]) == false && total_part_count == 0)
			{
				goto retfalse;
			} else if (is_digit(in_str[i]) && total_part_count < 3)
			{
				total_part_str[total_part_count] = in_str[i];
				total_part_count++;
				continue;
			} else
			{
				parsing_total = false;
				parsing_space = true;
				i--;
				continue;
			}
		} else if (parsing_space) {
			if(in_str[i] != ' ') {
				goto retfalse;
			} else
			{
				parsing_space = false;
				parsing_str = true;
				continue;
			}
		} else if (parsing_str) {
			out_str_tmp = &in_str[i];
			break;
		} 
	}

	if (parsing_str)
	{
		tmp_ptr = NULL;
		curr_part = strtol(curr_part_str, &tmp_ptr, 10);
		
		tmp_ptr = NULL;
		total_part = strtol(total_part_str, &tmp_ptr, 10);

		if (curr_part > total_part)
		{
			goto retfalse;
		}
		if (curr_part > 999 || curr_part < 1)
		{
			goto retfalse;
		}
		if (total_part > 999 || total_part < 1)
		{
			goto retfalse;
		}

		if (current_part)
		{
			*current_part = curr_part;
		}
		if (total_parts)
		{
			*total_parts = total_part;
		}
		if (ptr_out_str)
		{
			(*ptr_out_str) = out_str_tmp;
		}
		return true;
	}

retfalse:
	if (current_part)
	{
		*current_part = 1;
	}
	if (total_parts)
	{
		*total_parts = 1;
	}
	if (ptr_out_str)
	{
		(*ptr_out_str) = in_str;
	}
	return false;
}

void reconstruct_multi_qr(char* tmp_qr_res)
{
	bool res = false;
	bool is_complete = true;
	int curr = 0;
	int tot = 0;
	char* ptr_str = NULL;
	int i =0;

	res = is_multi_part_qr(tmp_qr_res, &curr, &tot, &ptr_str);
	
	// single part
	if (res == false)
	{
		if (g_last_decoded_qr)
		{
			memset(g_last_decoded_qr, 0, strlen(g_last_decoded_qr));
			vm_free(g_last_decoded_qr);
		}
		g_last_decoded_qr = my_strdup(tmp_qr_res);
	} else {
		if (g_last_decoded_qr_arr[curr])
		{
			memset(g_last_decoded_qr_arr[curr], 0, strlen(g_last_decoded_qr_arr[curr]));
			vm_free(g_last_decoded_qr_arr[curr]);
		}
		g_last_decoded_qr_arr[curr] = my_strdup(ptr_str);
		 
		// check if arr is complete
		is_complete = true;
		for (i=1; i<=tot; i++)
		{
			if (g_last_decoded_qr_arr[i] == NULL)
			{
				is_complete = false;
				break;
			}
		}
		if (is_complete)
		{
			int total_len = 0;
			for (i=1; i<=tot; i++)
			{
				total_len += strlen(g_last_decoded_qr_arr[i]);
			}
			if (g_last_decoded_qr)
			{
				memset(g_last_decoded_qr, 0, strlen(g_last_decoded_qr));
				vm_free(g_last_decoded_qr);
			}
			g_last_decoded_qr = vm_calloc(total_len+1);
			for (i=1; i<=tot; i++)
			{
				strcat(g_last_decoded_qr, g_last_decoded_qr_arr[i]);
			}
		}
	}


	//g_last_decoded_qr_arr

	//g_last_decoded_qr
}

void cam_message_callback(vm_cam_notify_data_t* notify_data, void* user_data)
{
	int res = 0;
	if (notify_data != NULL)
	{
		char* tmp_qr_res = NULL;
		vm_camera_para_struct para;
		bool rot = false;
		vm_cam_size_t osd = {0,0};
		void* osd_buff = NULL;
		void* layer_buffer = NULL;
		vm_cam_frame_data_t frame;
		VM_CAMERA_HANDLE cam_handle = notify_data->handle;
		dbg("[+] cam_message_callback - msg: %d - status: %d\n", notify_data->cam_message, notify_data->cam_status);
  		switch (notify_data->cam_message)
  		{
			
		case VM_CAM_CAPTURE_DONE:
		case VM_CAM_CAPTURE_ABORT:
			break;
		case VM_CAM_PREVIEW_START_DONE:
			clean_last_decoded_qr(); // g_last_decoded_qr
			dbg("[+] clean_last_decoded_qr\n");
			break;

		case VM_CAM_PREVIEW_STOP_ABORT:
		case VM_CAM_PREVIEW_STOP_DONE:
			stop_camera();
			if (g_last_decoded_qr)
			{
				if (g_after_camera_action)
				{
					g_after_camera_action();
				}
			}
			break;
		case VM_CAM_PREVIEW_START_ABORT:
			stop_preview();
			break;

  		case VM_CAM_PREVIEW_FRAME_RECEIVED:
			dbg("[+] VM_CAM_PREVIEW_FRAME_RECEIVED\n");
			if (vm_camera_get_frame(cam_handle, &frame) == VM_CAM_SUCCESS)
  			{
  				VMUINT grey_app_frame_data_size = 0;
				VMUINT uyvy_app_frame_data_size = 0;
  				VMUINT8* grey_app_frame_data = NULL;
  				VMUINT8* uyvy_app_frame_data = NULL;
				VMUINT row_pixel = frame.row_pixel;
				VMUINT col_pixel = frame.col_pixel;
  				
				dbg("[+] frame.rgbformat: %d\n", frame.pixtel_format);
				if (frame.pixtel_format == PIXTEL_UYUV422)
				{
					uyvy_app_frame_data = frame.pixtel_data;
					uyvy_app_frame_data_size = row_pixel * col_pixel * 4;
					grey_app_frame_data_size = row_pixel * col_pixel * 2;
					grey_app_frame_data = vm_malloc(grey_app_frame_data_size);
					uyvyToGrey(grey_app_frame_data, uyvy_app_frame_data, grey_app_frame_data_size);
					tmp_qr_res = decode_qr(grey_app_frame_data, grey_app_frame_data_size, col_pixel, row_pixel);
					//avoid double free, grey_app_frame_data is freed by decode_qr->zbar_image_destroy(image);
					//vm_free(grey_app_frame_data);
					if (tmp_qr_res)
					{
						// part1of2 xyz
						reconstruct_multi_qr(tmp_qr_res);
						memset(tmp_qr_res, 0, strlen(tmp_qr_res));
						vm_free(tmp_qr_res);
					}
					// g_last_decoded_qr is set by reconstruct_multi_qr
					if (g_last_decoded_qr)
					{
						dbg("[+] g_last_decoded_qr: %s\n", g_last_decoded_qr);
						stop_preview();
					} else {
						dbg("[+] no QR found (yet) or multiple part scan in progress\n");
					}
				}
  				break;
				
  			}

			default:
				break;
		}
	}
}

void app_set_capture_size(VM_CAMERA_HANDLE camera_handle)
{
	vm_cam_size_t* ptr = NULL;
	VMUINT size = 0, i = 0;
	int res = 0;
	if (vm_camera_get_support_capture_size(camera_handle, &ptr, &size) == VM_CAM_SUCCESS)
	{
		vm_cam_size_t my_cam_size;
		for (i = 0; i < size; i++)
		{
			my_cam_size.width  = (ptr + i)->width;
			my_cam_size.height =(ptr + i) ->height;
			dbg("[+] vm_camera_get_support_capture_size: %dx%d\n", my_cam_size.height, my_cam_size.width);
			
			res = vm_camera_set_capture_size(camera_handle, &my_cam_size);
			dbg("[+] vm_camera_set_capture_size: %d -> %dx%d\n", res, my_cam_size.height, my_cam_size.width);
		}
	}
	if (vm_camera_get_support_preview_size(camera_handle, &ptr, &size) == VM_CAM_SUCCESS)
	{
		vm_cam_size_t my_cam_size;
		for (i = 0; i < size; i++)
		{
			my_cam_size.width  = (ptr + i)->width;
			my_cam_size.height =(ptr + i) ->height;
			dbg("[+] vm_camera_get_support_preview_size: %dx%d\n", my_cam_size.height, my_cam_size.width);
			
			res = vm_camera_set_preview_size(camera_handle, &my_cam_size);
			dbg("[+] vm_camera_set_preview_size: %d -> %dx%d\n", res, my_cam_size.height, my_cam_size.width);
		}
	}
}

static void draw_scanning(void) {
	int x1,x2,x3,x4,x5,x6,x7;						/* string's x coordinate */
	int y;						/* string's y coordinate */
	int wstr_len1,wstr_len2,wstr_len3,wstr_len4,wstr_len5,wstr_len6,wstr_len7;				/* string's length */
	vm_graphic_color color;		/* use to set screen and text color */

	unsigned short* s1 = L"[Scan the QR code now]";
	unsigned short* s2 = L"Due to OS limitations";
	unsigned short* s3 = L"preview only works once";
	unsigned short* s4 = L"- BUT CAMERA IS ACTIVE -";
	unsigned short* s5 = L"SCAN THE QR CODE NOW";
	unsigned short* s6 = L"(or press any key to exit)";
	unsigned short* s7 = L"(or restart app for preview)";

	/* calculate string length*/ 
	wstr_len1 = vm_graphic_get_string_width((VMWSTR)s1);
	wstr_len2 = vm_graphic_get_string_width((VMWSTR)s2);
	wstr_len3 = vm_graphic_get_string_width((VMWSTR)s3);
	wstr_len4 = vm_graphic_get_string_width((VMWSTR)s4);
	wstr_len5 = vm_graphic_get_string_width((VMWSTR)s5);
	wstr_len6 = vm_graphic_get_string_width((VMWSTR)s6);
	wstr_len7 = vm_graphic_get_string_width((VMWSTR)s7);

	/* calculate string's coordinate */
	x1 = (vm_graphic_get_screen_width() - wstr_len1) / 2;
	x2 = (vm_graphic_get_screen_width() - wstr_len2) / 2;
	x3 = (vm_graphic_get_screen_width() - wstr_len3) / 2;
	x4 = (vm_graphic_get_screen_width() - wstr_len4) / 2;
	x5 = (vm_graphic_get_screen_width() - wstr_len5) / 2;
	x6 = (vm_graphic_get_screen_width() - wstr_len6) / 2;
	x7 = (vm_graphic_get_screen_width() - wstr_len7) / 2;
	y = (vm_graphic_get_screen_height() - vm_graphic_get_character_height()) / 2;
	
	/* set screen color */
	color.vm_color_565 = VM_COLOR_WHITE;
	vm_graphic_setcolor(&color);
	
	/* fill rect with screen color */
	vm_graphic_fill_rect_ex(layer_hdl[0], 0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height());
	
	/* set text color */
	color.vm_color_565 = VM_COLOR_BLUE;
	vm_graphic_setcolor(&color);
	
	/* output text to layer */
	vm_graphic_textout_to_layer(layer_hdl[0],x1, y-60, (VMWSTR)s1, wstr_len1);
	vm_graphic_textout_to_layer(layer_hdl[0],x2, y-30, (VMWSTR)s2, wstr_len2);
	vm_graphic_textout_to_layer(layer_hdl[0],x3, y, (VMWSTR)s3, wstr_len3);
	vm_graphic_textout_to_layer(layer_hdl[0],x4, y+30, (VMWSTR)s4, wstr_len4);
	vm_graphic_textout_to_layer(layer_hdl[0],x5, y+60, (VMWSTR)s5, wstr_len5);
	vm_graphic_textout_to_layer(layer_hdl[0],x6, y+90, (VMWSTR)s6, wstr_len6);
	vm_graphic_textout_to_layer(layer_hdl[0],x7, y+120, (VMWSTR)s7, wstr_len7);
	
	/* flush the screen with text data */
	vm_graphic_flush_layer(layer_hdl, 1);
}

int scan_qr(int a)
{
	vm_cam_frame_data_t frame_data;
	vm_cam_frame_raw_data_t raw_frame_data;
	vm_cam_capture_data_t capture_data;
	VMINT res = 0;
	VM_CAMERA_HANDLE handle_ptr = 0;
	VM_CAMERA_STATUS cam_status = 0;
	vm_cam_size_t my_cam_size;
	VMUINT8 *buffer;
	void* all1 = NULL;
	void* all2 = NULL;
	int i = 0;
	int layer_count = 0;
	vm_camera_para_struct para;
	
	VMINT cid = vm_camera_get_main_camera_id();
	
	if (cid < 0)
	{
		dbg("[-] vm_camera_get_main_camera_id failed\n");
		return 0;
	}
	dbg("[+] vm_camera_get_main_camera_id: %d\n", cid);

	memset(&frame_data, 0, sizeof(frame_data));
	memset(&raw_frame_data, 0, sizeof(raw_frame_data));
	memset(&capture_data, 0, sizeof(capture_data));

	res = vm_create_camera_instance(cid, &handle_ptr);
	if (res != VM_CAM_SUCCESS)
	{
		dbg("[-] vm_camera_get_main_camera_id failed: %d\n", res);
		goto cleanup;
		return 0;
	}
	dbg("[+] vm_create_camera_instance: %d -> %p\n", res, handle_ptr);

	// important for preview, but it only works once...
	vm_camera_enable_osd_layer(TRUE);
	vm_camera_set_lcd_update(TRUE);

	//app_set_capture_size(handle_ptr);
	my_cam_size.width  = 240;
	my_cam_size.height = 320;
	res = vm_camera_set_preview_size(handle_ptr, &my_cam_size);
	dbg("[+] vm_camera_set_preview_size: %d -> %dx%d\n", res, my_cam_size.height, my_cam_size.width);

	// make it slow so we have time to decode the qr
	vm_camera_set_preview_fps(handle_ptr, 1);

#if 0
	// check if there is enough mem
	//vm_camera_use_anonymous_memory(TRUE);

	all1 = vm_malloc_nc(0x25800*2); // -> 320*240*4
	dbg("[+] vm_malloc_nc: %d\n", all1);
	all2 = vm_malloc_nc(0x25800*2);
	dbg("[+] vm_malloc_nc: %d\n", all2);

	vm_free(all1);
	vm_free(all2);
#endif

	res = vm_camera_register_notify(handle_ptr, cam_message_callback, NULL);
	dbg("[+] vm_camera_register_notify: %d\n", res);

	if (layer_hdl[0] != -1)
	{
		// paint something
		draw_scanning();

		// then delete every layer
		// or camera preview will not start
		// nor we will receive any camera frame
		res = vm_graphic_delete_layer(layer_hdl[0]); 
		dbg("[+] vm_graphic_delete_layer: %d -> %d\n", res, layer_hdl[0]);
	}

	g_callback_none = true;
	vm_reg_keyboard_callback (nk_mre_handle_key_event_none);
	
	res = vm_camera_preview_start(handle_ptr);
	dbg("[+] vm_camera_preview_start: %d\n", res);

	if (res != VM_CAM_SUCCESS)
	{
		g_callback_none = false;
		vm_reg_keyboard_callback (nk_mre_handle_key_event);
		goto cleanup;
	} else {
		g_camera_handle = handle_ptr;
		g_camera_preview = true;
		return 0;
	}

cleanup:
	res = vm_camera_preview_stop(handle_ptr);
	dbg("[+] vm_camera_preview_stop: %d\n", res);
	res = vm_release_camera_instance(handle_ptr);
	dbg("[+] vm_release_camera_instance: %d\n", res);

	return 0;
}

int scan_qr_mnemo(int idx)
{
	g_after_camera_action = scan_qr_mnemo_after;
	scan_qr(0);
}

int scan_qr_wif(int idx)
{
	g_after_camera_action = scan_qr_wif_after;
	scan_qr(0);
}

int sign_psbt_wif_qr(int idx)
{
	g_after_camera_action = sign_psbt_wif_qr_after;
	scan_qr(0);
}

int sign_psbt_mnemo_qr(int idx)
{
	g_after_camera_action = sign_psbt_mnemo_qr_after;
	scan_qr(0);
}

int go_back(int idx)
{
	struct mre_nk_view* view = get_mre_nk_view();
	set_view(view->previous_id);
	update_gui();
}

int clean_and_exit(int a)
{
	struct mre_nk_view* view;
	int i=0;
	int j=0;

	memset(wallet_name, 0, sizeof(wallet_name));
	memset(w_text_entropy, 0, sizeof(w_text_entropy));
	memset(w_text_input, 0, sizeof(w_text_input));
	memset(w_old_wallet_pass, 0, sizeof(w_old_wallet_pass));
	memset(w_old_wallet_path, 0, sizeof(w_old_wallet_path));
	memset(w_wallet_pass, 0, sizeof(w_wallet_pass));
	memset(w_wallet_name, 0, sizeof(w_wallet_name));
	memset(w_optional_passphrase, 0, sizeof(w_optional_passphrase));
	memset(w_custom_deriv_path, 0, sizeof(w_custom_deriv_path));
	memset(w_mnemo_rec, 0, sizeof(w_mnemo_rec));
	memset(w_wif_rec, 0, sizeof(w_wif_rec));
	memset(w_save_name, 0, sizeof(w_save_name));
	memset(w_psbt_path, 0, sizeof(w_psbt_path));

	clean_last_decoded_qr();

	cleanup_sensitive_data();

	for (i=NK_MRE_VIEW_NONE; i<NK_MRE_VIEW_MAX_ID; i++)
	{
		view = &g_view[i];
		if (view->components)
		{
			for(j=0; view->components[j] != NULL; j++){
				if (view->components[j]->text)
				{
					memset(view->components[j]->text, 0, strlen(view->components[j]->text));
					vm_free(view->components[j]->text);
				}
				vm_free(view->components[j]);
			}
		}
		if (view->title)
		{
			memset(view->title, 0, strlen(view->title));
			vm_free(view->title);
		}
	}
	memset(g_view, 0, sizeof(g_view));

	//while(1){
		vm_exit_app();
	//};
}

//////////

void create_view_recover_wallet(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;
	struct mre_nk_component *comp_button4;
	struct mre_nk_component *comp_button5;
	struct mre_nk_component *comp_button6;

	struct mre_nk_view* view;
	char str_title[] = "Recover wallet";

	view = &g_view[NK_MRE_VIEW_WALLET_RECOVERY];
	view->id = NK_MRE_VIEW_WALLET_RECOVERY;
	view->previous_id = NK_MRE_VIEW_WALLET_ACTION;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "12-24 words (text)", 1, 0, recover_mnemo);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "WIF (text)", 1, 0, recover_wif);
	comp_button3 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "12-24 words (QR)", 1, 0, scan_qr_mnemo);
	comp_button4 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "WIF (QR)", 1, 0, scan_qr_wif);
	//comp_button5 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "32 byte priv key", 1, 0, recover_pk);
	//comp_button6 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Root xpriv", 1, 0, recover_xpriv);

	view->components = vm_monitored_calloc(7*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	view->components[2] = comp_button3;
	view->components[3] = comp_button4;
	//view->components[4] = comp_button5;
	//view->components[5] = comp_button6;
	//view->components[3] = comp_button4;
	//view->components[3] = NULL; // calloc does the job
}


void create_view_entropy_gen_wait(){
	struct mre_nk_component *comp_label;

	struct mre_nk_view* view;
	char str_title[] = "Please wait...";

	view = &g_view[NK_MRE_VIEW_ENTROPY_GEN_WAIT];
	view->id = NK_MRE_VIEW_ENTROPY_GEN_WAIT;
	view->previous_id = NK_MRE_VIEW_NONE;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "Please wait, computing hash of a brief mic recording, random file / pictures, imsi / imei / operator code as extra entropy.", 0, 0, NULL);
	//comp_label = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "Please wait", 0, 0, NULL);
	
	view->components = vm_monitored_calloc(2*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_create_1(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "WARNING";

	view = &g_view[NK_MRE_VIEW_WALLET_CREATION_1];
	view->id = NK_MRE_VIEW_WALLET_CREATION_1;
	view->previous_id = NK_MRE_VIEW_WALLET_ACTION;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "!!! WARNING !!! IF YOU LOSE YOUR SEED PHRASE, YOU LOSE YOUR BTC. You MUST safely backup the seed phrase that is about to be generated. Do NOT type the seed on any electronic device except your offline and preferably airgapped signing device / hardware wallet. Do NOT store online. Do NOT store on your email. Do NOT store on your dropbox. You SHOULD stamp/punch the seed phrase on a steel plate and memorize it.", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "I understand", 1, 0, open_create_2);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_create_2(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;
	struct mre_nk_component *comp_button4;

	struct mre_nk_view* view;
	char str_title[] = "Mnemonic selection";

	view = &g_view[NK_MRE_VIEW_WALLET_CREATION_2];
	view->id = NK_MRE_VIEW_WALLET_CREATION_2;
	view->previous_id = NK_MRE_VIEW_WALLET_CREATION_1;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "24 words (BIP 39)", 1, 0, open_create_3);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "18 words (BIP 39)", 1, 0, open_create_3);
	comp_button3 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "12 words (BIP 39)", 1, 0, open_create_3);
	//comp_button4 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "WIF", 1, 0, open_create_3);

	view->components = vm_monitored_calloc(5*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	view->components[2] = comp_button3;
	//view->components[3] = comp_button4;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_create_3(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "Mnemonics";

	view = &g_view[NK_MRE_VIEW_WALLET_CREATION_3];
	view->id = NK_MRE_VIEW_WALLET_CREATION_3;
	view->previous_id = NK_MRE_VIEW_WALLET_CREATION_2;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "I saved them", 1, 0, open_create_pin);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_create_4(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "Wallet password";

	view = &g_view[NK_MRE_VIEW_WALLET_CREATION_4];
	view->id = NK_MRE_VIEW_WALLET_CREATION_4;
	view->previous_id = NK_MRE_VIEW_WALLET_CREATION_3;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "Please enter a strong password (minimum 16 chars, use Upper Case, Lower Case, Digits, Special Chars)", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Enter password", 1, 0, input_new_wallet_password);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_create_5(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "Wallet name";

	view = &g_view[NK_MRE_VIEW_WALLET_CREATION_5];
	view->id = NK_MRE_VIEW_WALLET_CREATION_5;
	view->previous_id = NK_MRE_VIEW_WALLET_CREATION_4;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "Please enter a wallet name (text only)", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Enter wallet name", 1, 0, input_new_wallet_name);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_select_wallet_pass(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "Wallet password";

	view = &g_view[NK_MRE_VIEW_WALLET_UNLOCK_PIN];
	view->id = NK_MRE_VIEW_WALLET_UNLOCK_PIN;
	view->previous_id = NK_MRE_VIEW_WALLET_ACTION;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "Please enter the wallet password", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Enter wallet password", 1, 0, input_old_wallet_password);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_wallet_main_1(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;
	struct mre_nk_component *comp_button4;

	struct mre_nk_view* view;
	char str_title[] = "BIP39 passphrase";

	view = &g_view[NK_MRE_VIEW_WALLET_MAIN_1];
	view->id = NK_MRE_VIEW_WALLET_MAIN_1;
	view->previous_id = NK_MRE_VIEW_WALLET_ACTION;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "No passphrase", 1, 0, open_wallet_without_passphrase);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Optional passphrase", 1, 0, open_wallet_with_passphrase);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	//view->components[2] = comp_button3;
	//view->components[3] = comp_button4;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_wallet_main_2(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;
	struct mre_nk_component *comp_button4;
	struct mre_nk_component *comp_button5;
	struct mre_nk_component *comp_button6;
	struct mre_nk_component *comp_button7;
	struct mre_nk_component *comp_button8;

	struct mre_nk_view* view;
	char str_title[] = "Derivation path";

	view = &g_view[NK_MRE_VIEW_WALLET_MAIN_2];
	view->id = NK_MRE_VIEW_WALLET_MAIN_2;
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_1;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "BIP84 (default)", 1, 0, open_bip84);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "BIP44", 1, 0, open_bip44);
	comp_button3 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "BIP49", 1, 0, open_bip49);
	comp_button4 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Bitcoin Core", 1, 0, open_btc_core);
	comp_button5 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Root xpub", 1, 0, open_root_xpub);
	comp_button6 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Custom", 1, 0, open_custom_dp);
	comp_button7 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Custom hardened", 1, 0, open_custom_dp_hardened);

	view->components = vm_monitored_calloc(10*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	view->components[2] = comp_button3;
	view->components[3] = comp_button4;
	view->components[4] = comp_button5;
	view->components[5] = comp_button6;
	view->components[6] = comp_button7;
	//view->components[3] = NULL; // calloc does the job
}


void create_view_wallet_main_wif_1(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;

	struct mre_nk_view* view;
	char str_title[] = "Address type";

	view = &g_view[NK_MRE_VIEW_WALLET_WIF_1];
	view->id = NK_MRE_VIEW_WALLET_WIF_1;
	view->previous_id = NK_MRE_VIEW_WALLET_ACTION;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Segwit (default)", 1, 0, set_addr_segwit);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Legacy", 1, 0, set_addr_legacy);
	comp_button3 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Nested segwit", 1, 0, set_addr_nested_segwit);

	view->components = vm_monitored_calloc(4*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	view->components[2] = comp_button3;
}

void create_view_psbt_signed(){
	struct mre_nk_component *comp_label1;
	struct mre_nk_component *comp_button2;

	struct mre_nk_view* view;
	char str_title[] = "PSBT signed";

	view = &g_view[NK_MRE_VIEW_WALLET_PSBT_SIGNED];
	view->id = NK_MRE_VIEW_WALLET_PSBT_SIGNED;
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "PSBT was signed and saved correctly (<psbt_filename or date>.signed.psbt). To finalize the PSBT, run 'bitcoin-cli utxoupdatepsbt <b64>' and 'bitcoin-cli finalizepsbt <b64>'.", 0, 0, NULL);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Clean exit", 1, 0, clean_and_exit);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button2;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_psbt_error(){
	struct mre_nk_component *comp_label1;
	struct mre_nk_component *comp_button2;

	struct mre_nk_view* view;
	char str_title[] = "PSBT signing error";

	view = &g_view[NK_MRE_VIEW_WALLET_PSBT_ERROR];
	view->id = NK_MRE_VIEW_WALLET_PSBT_ERROR;
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_2;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "PSBT was not signed correctly. Some error happened, please check if your PSBT is valid.", 0, 0, NULL);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Back", 1, 0, go_back);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button2;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_wallet_main_wif_2(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;
	struct mre_nk_component *comp_button4;
	struct mre_nk_component *comp_button5;
	struct mre_nk_component *comp_button6;
	struct mre_nk_component *comp_button7;

	struct mre_nk_view* view;
	char str_title[] = "Wallet actions";

	view = &g_view[NK_MRE_VIEW_WALLET_WIF_2];
	view->id = NK_MRE_VIEW_WALLET_WIF_2;
	view->previous_id = NK_MRE_VIEW_WALLET_WIF_1;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Sign PSBT (file)", 1, 0, sign_psbt_wif);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Sign PSBT (QR)", 1, 0, sign_psbt_wif_qr);
	comp_button3 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Display pub key", 1, 0, display_pub);
	comp_button4 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Display priv key", 1, 0, display_priv);
	comp_button5 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Save pub key", 1, 0, save_pub);
	comp_button6 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Save priv key (RISKY)", 1, 0, save_priv);
	comp_button7 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Clean exit", 1, 0, clean_and_exit);

	view->components = vm_monitored_calloc(9*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	view->components[2] = comp_button3;
	view->components[3] = comp_button4;
	view->components[4] = comp_button5;
	view->components[5] = comp_button6;
	view->components[6] = comp_button7;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_wallet_main_3(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;
	struct mre_nk_component *comp_button4;
	struct mre_nk_component *comp_button5;
	struct mre_nk_component *comp_button6;
	struct mre_nk_component *comp_button7;

	struct mre_nk_view* view;
	char str_title[] = "Wallet actions";

	view = &g_view[NK_MRE_VIEW_WALLET_MAIN_3];
	view->id = NK_MRE_VIEW_WALLET_MAIN_3;
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_2;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Sign PSBT file", 1, 0, sign_psbt_mnemo);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Sign PSBT (QR)", 1, 0, sign_psbt_mnemo_qr);
	comp_button3 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Display pub info", 1, 0, display_xpub);
	comp_button4 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Display prv info", 1, 0, display_xpriv);
	comp_button5 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Save pub info", 1, 0, save_xpub);
	comp_button6 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Save priv info (RISKY)", 1, 0, save_xpriv);
	comp_button7 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Clean exit", 1, 0, clean_and_exit);

	view->components = vm_monitored_calloc(9*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	view->components[2] = comp_button3;
	view->components[3] = comp_button4;
	view->components[4] = comp_button5;
	view->components[5] = comp_button6;
	view->components[6] = comp_button7;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_sensitive_save()
{
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "WARNING";

	view = &g_view[NK_MRE_VIEW_SENSITIVE_SAVE];
	view->id = NK_MRE_VIEW_SENSITIVE_SAVE;
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "!!! WARNING !!! This operation is EXTREMELY RISKY. The information that is about the be displayed or saved is EXTREMELY sensitive. Do not share with anyone. Do NOT store online. Do NOT store on your email. Do NOT store on your dropbox. You SHOULD stamp/punch the seed phrase on a steel plate and memorize it. If you are saving it temporarily on a SD card (PLEASE DON'T, UNLESS IT IS STRICTLY NECESSARY OR FOR TESTING PURPOSED), be sure to wipe it properly or do a nice dd if=/dev/urandom, oherwise it could be recovered.", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "I understand", 1, 0, save_sensitive);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
}

void create_view_sensitive_saved()
{
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "SAVED";

	view = &g_view[NK_MRE_VIEW_SENSITIVE_SAVED];
	view->id = NK_MRE_VIEW_SENSITIVE_SAVED;
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "Info were saved.", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Back", 1, 0, go_back);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
}

void create_view_warning_sensitive(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_label1;

	struct mre_nk_view* view;
	char str_title[] = "WARNING";

	view = &g_view[NK_MRE_VIEW_WARNING_SENSITIVE];
	view->id = NK_MRE_VIEW_WARNING_SENSITIVE;
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "!!! WARNING !!! The information that is about the be displayed is EXTREMELY sensitive. Do not share with anyone. Do NOT store online. Do NOT store on your email. Do NOT store on your dropbox. You SHOULD stamp/punch the seed phrase on a steel plate and memorize it.", 0, 0, NULL);
	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "I understand", 1, 0, display_sensitive);

	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_button1;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_display_sensitive(){
	struct mre_nk_component *comp_label1;
	struct mre_nk_component *comp_label2;

	struct mre_nk_view* view;
	char str_title[] = "SENSITIVE";

	view = &g_view[NK_MRE_VIEW_DISPLAY_SENSITIVE];
	view->id = NK_MRE_VIEW_DISPLAY_SENSITIVE;
	view->previous_id = NK_MRE_VIEW_WALLET_MAIN_3;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "", 0, 0, NULL);
	comp_label2 = mre_nk_component_create(NK_MRE_COMPONENT_TEXT, "", 0, 0, NULL);
	
	view->components = vm_monitored_calloc(3*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_label2;
	//view->components[3] = NULL; // calloc does the job
}

void create_view_wallet_action(){
	struct mre_nk_component *comp_button1;
	struct mre_nk_component *comp_button2;
	struct mre_nk_component *comp_button3;
	struct mre_nk_component *comp_button4;
	struct mre_nk_component *comp_button5;

	struct mre_nk_view* view;
	char str_title[] = "Wallet actions";

	view = &g_view[NK_MRE_VIEW_WALLET_ACTION];
	view->id = NK_MRE_VIEW_WALLET_ACTION;
	view->previous_id = NK_MRE_VIEW_NONE;
	
	view->title = vm_monitored_calloc(strlen(str_title)+1);
	strcpy(view->title, str_title);

	comp_button1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Select wallet", 1, 0, select_wallet);
	comp_button2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Recover wallet", 1, 0, recover_wallet);
	comp_button3 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Create wallet", 1, 0, create_wallet);
	comp_button4 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Clean exit", 1, 0, clean_and_exit);
	//comp_button5 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, "Scan QR", 1, 0, scan_qr);

	view->components = vm_monitored_calloc(6*sizeof(struct mre_nk_component *));
	view->components[0] = comp_button1;
	view->components[1] = comp_button2;
	view->components[2] = comp_button3;
	view->components[3] = comp_button4;
	//view->components[4] = comp_button5;
	//view->components[3] = NULL; // calloc does the job
}

// it leaks mem, cause we reallocate a new one at each test, but whatever
// it doesn't go into production, it will be #if 0
void create_view_qr_text()
{
	struct mre_nk_component *comp_label1;
	
	struct mre_nk_view* view;
	char str_gen_title[] = "Test";

	view = &g_view[NK_MRE_VIEW_QR_TEST];
	view->id = NK_MRE_VIEW_QR_TEST;
	view->previous_id = NK_MRE_VIEW_WALLET_ACTION;
	
	view->title = vm_monitored_calloc(strlen(str_gen_title)+1);
	strcpy(view->title, str_gen_title);

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, g_last_decoded_qr, 0, 0, NULL);

	view->components = vm_monitored_calloc(2*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
}

#if 0
void create_view_testtest(){

	VMSTR imsi = NULL;
	VMSTR imei = NULL;
	VMSTR imei0 = NULL;
	VMSTR imei1 = NULL;
	VMSTR imei2 = NULL;
	int res_query_op = -1;
	char operator_code[64];
	int i = 0;

	struct mre_nk_component *comp_label1;
	struct mre_nk_component *comp_label2;
	struct mre_nk_component *comp_label3;
	struct mre_nk_component *comp_label4;
	struct mre_nk_component *comp_label5;
	struct mre_nk_component *comp_label6;

	struct mre_nk_view* view;
	char str_gen_title[] = "Test";

	memset(operator_code, 0, sizeof(operator_code));

	view = &g_view[NK_MRE_VIEW_TESTTEST];
	view->id = NK_MRE_VIEW_TESTTEST;
	view->previous_id = NK_MRE_VIEW_NONE;
	
	view->title = vm_monitored_calloc(strlen(str_gen_title)+1);
	strcpy(view->title, str_gen_title);

	for (i=0; i<64; i++)
	{
		imsi = vm_get_imsi();
		imei = vm_get_imei();
		res_query_op = vm_query_operator_code(operator_code, sizeof(operator_code));
	}

	comp_label1 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, imsi, 0, 0, NULL);
	comp_label2 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, imei, 0, 0, NULL);
	comp_label6 = mre_nk_component_create(NK_MRE_COMPONENT_BUTTON, operator_code, 0, 0, NULL);

	view->components = vm_monitored_calloc(7*sizeof(struct mre_nk_component *));
	view->components[0] = comp_label1;
	view->components[1] = comp_label2;
	view->components[2] = comp_label6;
	view->components[3] = NULL;//comp_label4;
	view->components[4] = NULL;//comp_label5;
	view->components[5] = NULL;//comp_label6;
	//view->components[4] = NULL; // calloc does the job
}
#endif

static int g_current_view = NK_MRE_VIEW_NONE;

void set_view(int view)
{
	if (view > NK_MRE_VIEW_NONE && view < NK_MRE_VIEW_MAX_ID)
	{
		g_current_view = view;
	}
}

int get_view()
{
	return g_current_view;
}

struct mre_nk_view* get_mre_nk_view()
{
	return &g_view[g_current_view];
}

void populate_view(){ 
	//TODO: Check this. Changed from struct mre_nk_c** ptr_cmpts to mre_nk_c* ptr_cmpts
	/*Declare context*/
    struct nk_context *ctx;
	struct mre_nk_view* view;

	/* get the set context */
	ctx = &mre.ctx;

	view = get_mre_nk_view();

	/* GUI: Start(will allocate mem for context, etc) */
    {
		struct nk_panel layout;
		if (nk_begin(ctx, &layout, view->title, nk_rect(0, 0, 
			(float)(mre.width), (float)(mre.height)),
			NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE))
			//NK_WINDOW_SCALABLE|NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE
		{
			char *title; //Not using it right now..
			enum {EASY, HARD};
			static int op = EASY;
			static int property = 20;
			int i = 0;
			enum nk_button_behavior button_behavior;
			struct nk_image img;
			char *image_path;
			static char in_text[9][64];
			static int in_text_len[9];	
			
			int hovering_index = -1;
			int hoverable_count = 0;

			// compute starting hovering
			if (view->components && view->components[0])
			{
				for(i=0; view->components[i] != NULL; i++){
					struct mre_nk_component * comp = view->components[i];
					if (comp->is_hoverable)
					{
						hoverable_count++;
					}
					if (hovering_index == -1 && comp->hovering && comp->is_hoverable)
					{
						hovering_index = i;
					}
				}
			}
			// is no hovering specified but hoverable found
			// hover the first entry
			if (hovering_index == -1 && hoverable_count > 0 && view->components && view->components[0])
			{
				for(i=0; view->components[i] != NULL; i++){
					struct mre_nk_component * comp = view->components[i];
					if (comp->is_hoverable)
					{
						hovering_index = i;
						break;
					}
				}
			}

			if (hovering_index != -1)
			{
				view->components[hovering_index]->hovering = 1;
			}

			/* mre_nk_c loop - Components Loop */
			if (view->components && view->components[0])
			{
				for(i=0; view->components[i] != NULL; i++){
					struct mre_nk_component * comp = view->components[i];
					if (comp->type == NK_MRE_COMPONENT_SEPARATOR)
					{
						nk_layout_row_dynamic(ctx, GRAPHIC_ELEMENT_HEIGHT, 1);
					} else if (comp->type == NK_MRE_COMPONENT_BUTTON)
					{
						nk_layout_row_dynamic(ctx, GRAPHIC_ELEMENT_HEIGHT, 1);
						button_behavior = NK_BUTTON_DEFAULT;
						if (hovering_index == i){
							button_behavior = NK_BUTTON_HOVER;
						}
						ctx->button_behavior = button_behavior;
						nk_button_label(ctx, comp->text);
					} else if (comp->type == NK_MRE_COMPONENT_TEXT)
					{
						int text_len = strlen(comp->text);
						// ETTER HARDWARE WALLET I
						// T&C: USE AT YOUR OWN RI
						int rows = text_len / MAX_WORD_LEN_240x320;
						int reminder = text_len % MAX_WORD_LEN_240x320;
						if (reminder) {
							rows++;
						}
						//nk_layout_row_static(ctx, 30*rows, mre.width-30, 1);
						nk_layout_row_dynamic(ctx, GRAPHIC_ELEMENT_HEIGHT*rows, 1);
						if (rows > 1)
						{
							nk_label_wrap(ctx, comp->text);
						} else {
							nk_label(ctx, comp->text, NK_TEXT_ALIGN_LEFT);
						}
					} else if (comp->type == NK_MRE_COMPONENT_INPUT_TEXT)
					{
						int text_len = strlen(comp->text) + 1;

						nk_layout_row_dynamic(ctx, GRAPHIC_ELEMENT_HEIGHT, 1);
						button_behavior = NK_BUTTON_DEFAULT;
						if (hovering_index == i){
							button_behavior = NK_BUTTON_HOVER;
						}
						ctx->button_behavior = button_behavior;
						//nk_button_label(ctx, comp->text);
						nk_edit_string(ctx, NK_EDIT_BOX, comp->text, &text_len, text_len, nk_filter_ascii);
					} else if (comp->type == NK_MRE_COMPONENT_CHECKBOX)
					{
						nk_layout_row_dynamic(ctx, GRAPHIC_ELEMENT_HEIGHT, 1);
						button_behavior = NK_BUTTON_DEFAULT;
						if (hovering_index == i){
							button_behavior = NK_BUTTON_HOVER;
						}
						ctx->button_behavior = button_behavior;
						nk_checkbox_label(ctx, comp->text, &comp->checked);
					}
				}
			}
		}
		nk_end(ctx);
	}

}

void update_gui(){
	/*Declare context*/
    struct nk_context *ctx;

	struct mre_nk_view* view;
	VMUINT8 *buffer;

	// disable input while updating the gui
	vm_reg_keyboard_callback (nk_mre_handle_key_event_none);

	/* get the set context */
	ctx = &mre.ctx;

	printf("\nUpdating the current View..\n");

	
	if ( layer_hdl[0] != -1 )
	{
		vm_graphic_delete_layer (layer_hdl[0]);
		layer_hdl[0] = -1;
	}
	layer_hdl[0] = vm_graphic_create_layer (0, 0, vm_graphic_get_screen_width (), 
							vm_graphic_get_screen_height (), -1);
	vm_graphic_active_layer (layer_hdl[0]);
	/* set clip area */
	vm_graphic_set_clip (0, 0, 
			vm_graphic_get_screen_width (), 
			vm_graphic_get_screen_height ());
					
	/* get the target buffer from the layer */
	buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

	/*	fill the screen
		Clears it before everything gets drawn bit by bit
	*/
	vm_graphic_fill_rect (buffer, MRE_SET_SRC_LEFT_TOP_X, MRE_SET_SRC_LEFT_TOP_Y, 
							vm_graphic_get_screen_width (), vm_graphic_get_screen_height (), 
							VM_COLOR_WHITE, VM_COLOR_BLACK);

	/* Testing commands to View */
	populate_view();

	/*	Draw/Paint - Main render process
		Also set the color to Gray(30,30,30)
	*/
	
    nk_mre_render(nk_rgb(30,30,30));

	/*	Clears the drawing queue - After drawing
		The queue in context was adding up un necessarily
	*/
	nk_input_begin(ctx); 

	// re-install handler
	vm_reg_keyboard_callback (nk_mre_handle_key_event);
}

void initiate_nk_gui(void){
	MreFont* font;
	VMUINT8 *buffer;

	/*	Declare context
		This is the nuklear context object.
		It has nk_input input, style, memory,
		clipboard, widget state flags
	*/
    struct nk_context *ctx;
	//struct mre_nk_c * first_set_of_components;

    /* Variable declaration that are relevant to Implementation*/
    int running = 1;
    int needs_refresh = 1;
	//Some memory allocation issues in nk_mrefont_create..
	/*Memory Set and Allocation*/
	font = nk_mrefont_create("Arial", 10);

	/*Initialise the context*/ 
	ctx = nk_mre_init(font, vm_graphic_get_screen_width(), vm_graphic_get_screen_height());

	/* get the target buffer from the layer */
	buffer = vm_graphic_get_layer_buffer(layer_hdl[0]);

	// memset extra entropy
	memset(w_text_entropy, 0, sizeof(w_text_entropy));

	// create views
	memset(g_view, 0, sizeof(g_view));
	create_view_welcome_screen();
	create_view_entropy_gen();
	create_view_entropy_gen_wait();
	create_view_wallet_action();
	create_view_create_1();
	create_view_create_2();
	create_view_create_3();
	create_view_create_4();
	create_view_create_5();
	create_view_wallet_main_1();
	create_view_wallet_main_2();
	create_view_wallet_main_3();
	create_view_wallet_main_wif_1();
	create_view_wallet_main_wif_2();
	create_view_select_wallet_pass();
	create_view_recover_wallet();
	create_view_warning_sensitive();
	create_view_display_sensitive();
	create_view_psbt_signed();
	create_view_psbt_error();
	create_view_sensitive_save();
	create_view_sensitive_saved();

	//create_view_testtest();
	//create_view_wallet();
	//...
	//create_view_xyz();

	/* Set first view */
	set_view(NK_MRE_VIEW_WELCOME);

	/* Refreshes display */
	update_gui();
}
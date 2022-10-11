#if 1
#include "mre_menu.h"
#include "mre_io.h"
#include "mre_display.h"
#include "share.h"

#define vm_log_file do{}while(0);

extern VMINT       connect_status; //Don't give it a value over here..
extern VMINT		layer_handle[1];				////layer handle array.
extern VMINT g_mre_httpcontext; //Http Menu Context
extern VMINT g_mre_socketcontext; //Socket Menu Context

extern VMINT	layer_hdl[1];			//layer handle array.
extern VMINT	g_mre_curr_x;
extern VMINT	g_mre_curr_y;
extern VMWSTR	g_mre_textbox_text;
extern VMINT	g_mre_textbox_state;
extern VMINT	g_mre_subcontext;
extern VMINT	g_mre_drv;
extern VMINT	g_mre_fileiocontext;	//In file io context

/* image related */
extern VMINT g_mre_img_y_pos;
extern VMINT g_mre_img_x_pos;
extern VMINT g_mre_img_size_cng; 
extern VMINT g_mre_img_clockwise_cnt;
extern VMINT g_mre_img_dir;
extern VMINT g_mre_curr_x;
extern VMINT g_mre_curr_y;

void mre_set_subcontext (VMINT val)
{
    g_mre_subcontext = val;
}

void mre_set_global_data (void)
{
    vm_log_debug  ("mre_set_global_data function starts ");


    mre_set_curr_x (MRE_SET_X);
    mre_set_curr_y (MRE_SET_Y);
    mre_set_subcontext (FALSE);
    mre_set_textbox_state (MRE_SET_VAL);
    mre_set_textbox_text (NULL);
    mre_set_drv ();

    vm_log_debug  ("mre_set_global_data function exits ");
}
#endif
/**
	MRE IO: File operations, etc 
**/

#include "share.h"
#include "mre_io.h"

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


/*****************************************************************************
 * FUNCTION
 *  mre_get_drv
 * DESCRIPTION
 *  This function returns a no corresponding to a drive found
 * PARAMETERS
 *  none
 * RETURNS
 *  g_mre_drv            [OUT]      drive found on device
*****************************************************************************/

VMINT mre_get_drv (void)
{
    return g_mre_drv;
}

/*****************************************************************************
 * FUNCTION
 *  mre_set_drv
 * DESCRIPTION
 *  This function stores a no corresponding to a drive found
 * PARAMETERS
 *  none
 * RETURNS
 *  none
*****************************************************************************/
void mre_set_drv (void)
{
    VMINT drv;
    vm_log_debug ("Entering mre_set_drv function");
    if ( (drv = vm_get_removable_driver ()) <0 )
    {
        drv = vm_get_system_driver ();
    }
    g_mre_drv = drv;
    vm_log_debug ("Exiting mre_set_drv function");
}




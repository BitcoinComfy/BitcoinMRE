/****************************************************************************
MRE_API_Tests.c : Main file for Testing

*****************************************************************************/

////////////////////////////////Header Files/////////////////////////////////
#include "MRE_API_Tests.h"

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

////////////////////////////////MAIN FUNCTION/////////////////////////////////

/*****************************************************************************
 * FUNCTION
 *  vm_main
 * DESCRIPTION
 *  This function is main function which is invoked in start up 
 *  ie entry function.
 * PARAMETERS
 *  none
 * RETURNS
 *	none
*****************************************************************************/
void vm_main(void) 
{
	VMINT drv;

	/* Define logging */ 
	#ifdef MRE_LOG_ON /* if loging is on */
		VMCHAR log_file[MRE_STR_SIZE_MAX] = {0};
	    
		/* finding drive for saving log information in "sample1.log" file name */
		if ((drv = vm_get_removable_driver()) < 0)	
		{	
			/* if no removable drive then get system drive*/ 
			drv = mre_get_drv(); 
		}
		sprintf(log_file, "%c:\\mre.log", drv);
		vm_log_init(log_file, VM_DEBUG_LEVEL);			
	#endif

	//Setting the workable drive here at once (May reduce checks in the future
	// by storing it to a global variable . May change as removable media exists)
	mre_set_drv();
    
	vm_log_debug ("vm_main function starts");

	/*Initialising the layer handle*/
    layer_hdl[0] = -1;

	/*Setting Global data for Graphics*/
    mre_set_global_data();
	
	/* Set lang */ 
    vm_mul_lang_set_lang (VM_LANG_ENG);
	
	/*
		Event Mapping:
		The reason why below we have two seperate events is cause once we attach an event handler, we can not 
		change it. I could mix them both into one event handler but we already tested the API's. Now its time
		to test nuklear and make the application, its views around this.
	*/
	///*
	// For Nuklear Testing:
	vm_reg_sysevt_callback (nk_mre_handle_sys_event); //looks at implementation event handler (does thesame thing)
	vm_reg_keyboard_callback (nk_mre_handle_key_event);	//Keyboard callback
	vm_reg_pen_callback (nk_mre_handle_penevt);	//Pen callback - not really needed
	//initiate_nuklear_gui(); //Disabling, the method: nk_mre_handle_sys_event will call it.
	//*/
	
	/*
	//For API Testing:
    vm_mul_lang_set_lang (VM_LANG_ENG);
	vm_reg_sysevt_callback (handle_sysevt);
 	vm_reg_keyboard_callback (handle_keyevt);
	vm_reg_pen_callback (handle_penevt);
	*/ 
}

//end. 
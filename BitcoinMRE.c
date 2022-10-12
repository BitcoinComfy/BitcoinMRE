/****************************************************************************
BitcoinMRE.c : Main file for Testing

*****************************************************************************/

////////////////////////////////Header Files/////////////////////////////////
#include "BitcoinMRE.h"

extern VMINT	layer_handle[1];//layer handle array.
extern VMINT	layer_hdl[1];	//layer handle array.
extern VMINT	g_mre_drv;

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

	/* Set lang */ 
    vm_mul_lang_set_lang(VM_LANG_ENG);
	
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

}

//end. 
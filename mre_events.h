#ifndef __MRE_EVENTS__
#define __MRE_EVENTS__

#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "vmche.h"
#include "ResID.h"
#include "vm4res.h"
#include "vmlog.h"

#include "share.h"
#include "mre_menu.h"

/* 
	mre_events.h Events for the project 
*/

/*****************************************************************************
 * FUNCTION
 *  handle_sysevt
 * DESCRIPTION
 *  This function handles system events 
 * PARAMETERS
 *  event		    	[IN]      message id
 *  param               [IN]      parameter
 * RETURNS
 *	none
*****************************************************************************/
void handle_sysevt (VMINT message, VMINT param);

/*****************************************************************************
 * FUNCTION
 *  handle_keyevt
 * DESCRIPTION
 *  This function handles key events 
 * PARAMETERS
 *  event		    	[IN]      event no
 *  keycode             [IN]      keycode no
 * RETURNS
 *	none
*****************************************************************************/
void handle_keyevt (VMINT event, VMINT keycode);

/*****************************************************************************
 * FUNCTION
 *  handle_penevt
 * DESCRIPTION
 *  This function handles touch panel events 
 * PARAMETERS
 *  event				[IN]      event no
 *  x					[IN]      x cordinate of cursor point on screen
 *  y					[IN]      y cordinate of cursor point on screen
 * RETURNS
 *	none
*****************************************************************************/
void handle_penevt (VMINT event, VMINT x, VMINT y);

/*****************************************************************************
 * FUNCTION
 *  handle_sysevt
 * DESCRIPTION
 *  This function handles system events 
 * PARAMETERS
 *  event		    	[IN]      message id
 *  param               [IN]      parameter
 * RETURNS
 *	none
*****************************************************************************/
void handle_sysevt (VMINT message, VMINT param);

#endif // __MRE_EVENTS__
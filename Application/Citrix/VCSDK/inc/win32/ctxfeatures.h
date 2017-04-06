/***************************************************************************
*
*   ctxfeatures.h
*
*   This module contains platform-specific defines that are used to
*   control whether features are compiled or not.
*
*   $Id: //icaclientcore/ironcove/base/src/inc/win32/ctxfeatures.h#1 $
*
*   Copyright 2004 Citrix Systems, Inc.  All Rights Reserved.
*
****************************************************************************/

#ifndef _CTXFEATURES_H_
#define _CTXFEATURES_H_

/*
 * This define encapsulates code that relates to the speedbrowse project, for the client.
 */
#define SPEEDBROWSE

/* Features */
#define TEXT_MODE_ENABLED 1
#define SHAPED_WINDOWS 1
#define FLASHWINDOW_SUPPORT 1
#define PNP_SUPPORT         1
#define SUPPORT_ASYNC_DISK_WRITES  1
#define PNA_KEYPASSTHRU 1


#endif /* _CTXFEATURES_H_ */

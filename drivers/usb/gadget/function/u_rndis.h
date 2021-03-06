/*
 * u_rndis.h
 *
 * Utility definitions for the subset function
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef U_RNDIS_H
#define U_RNDIS_H

#include <linux/usb/composite.h>

struct f_rndis_opts {
	struct usb_function_instance	func_inst;
	u32				vendor_id;
	const char			*manufacturer;
	struct net_device		*net;
	bool				bound;
	bool				borrowed_net;

	struct config_group		*rndis_interf_group;
	struct usb_os_desc		rndis_os_desc;
	char				rndis_ext_compat_id[16];

	/* Tizne HACK:
	 * us : To check usb_string descriptor attached or not
	 * iad : To override rndis_iad_descritor by user space
	 * ctl_intf : To override rndis_control_intf by user space
	 */
	struct usb_string	*us;
	struct usb_interface_assoc_descriptor *iad;
	struct usb_interface_descriptor *ctl_intf;
	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex			lock;
	int				refcnt;
};

void rndis_borrow_net(struct usb_function_instance *f, struct net_device *net);

#endif /* U_RNDIS_H */

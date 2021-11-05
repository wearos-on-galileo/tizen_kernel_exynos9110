/*
 *  include/linux/sid_sys_stats.h
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SID_SYS_STATS_H
#define __SID_SYS_STATS_H

struct sid_sys_stats {
	char comm[TASK_COMM_LEN];
	u32 usid;
	uid_t uid;
	u32 sid;
	cputime_t utime;
	cputime_t stime;
	cputime_t ttime;
	u32 permil;
};

struct sid_io_stats {
	u32 usid;
	uid_t uid;
	u32 sid;
	u64 read_bytes;
	u64 write_bytes;
	u64 rchar;
	u64 wchar;
	u64 tchar; /* rchar + wchar */
	u64 fsync;
};

#ifdef CONFIG_SID_SYS_STATS
extern int get_sid_cputime(int type,
			struct sid_sys_stats *stat_array, size_t n);

#ifdef CONFIG_PID_SYS_STATS
extern int get_sid_cputime2(int type, struct sid_sys_stats *sid_array,
			struct sid_sys_stats *pid_array, size_t n);
#endif

extern int get_sid_io(int type,
			struct sid_io_stats *stat_array, size_t n);
#else
static inline int get_sid_cputime(int type, struct sid_sys_stats *stat_array)
{ return 0; }

static inline int get_sid_cputime2(int type, struct sid_sys_stats *sid_array,
			struct sid_sys_stats *pid_array, size_t n)
{ return 0; }

static inline int get_sid_io(int type, struct sid_io_stats *stat_array)
{ return 0; }
#endif

#endif /* __SID_SYS_STATS_H */


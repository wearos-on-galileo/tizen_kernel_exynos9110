 /* drivers/misc/sid_sys_stats.c
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
 * Author: Junho Jang <vincent.jang@samsung.com>
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

#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/hashtable.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/profile.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/security.h>
#include <linux/suspend.h>
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/sort.h>
#include <linux/sid_sys_stats.h>
#include <linux/power/energy_monitor.h>
#endif

#define SID_STAT_HASH_BITS	10
DECLARE_HASHTABLE(sid_hash_table, SID_STAT_HASH_BITS);

static DEFINE_RT_MUTEX(sid_lock);
static struct proc_dir_entry *cpu_parent;
static struct proc_dir_entry *io_parent;

struct io_stats {
	u64 read_bytes;
	u64 write_bytes;
	u64 rchar;
	u64 wchar;
	u64 fsync;
};

#define SID_STATE_FOREGROUND	0
#define SID_STATE_BACKGROUND	1
#define SID_STATE_BUCKET_SIZE	2

#define SID_STATE_TOTAL_CURR	2
#define SID_STATE_TOTAL_LAST	3
#define SID_STATE_DEAD_TASKS	4
#define SID_STATE_SIZE		5

struct pid_entry {
	char comm[TASK_COMM_LEN];
	pid_t pid;
	u32 key;
	cputime_t utime;
	cputime_t stime;
	cputime_t active_utime;
	cputime_t active_stime;
#ifdef CONFIG_ENERGY_MONITOR
	cputime_t emon_utime;
	cputime_t emon_stime;
#endif
	struct hlist_node hash;
};

struct sid_entry {
	u32 usid;
	u32 sid;
	uid_t uid;
	cputime_t utime;
	cputime_t stime;
	cputime_t active_utime;
	cputime_t active_stime;
	cputime_t delta_utime;
	cputime_t delta_stime;
	int state;
	struct io_stats io[SID_STATE_SIZE];
#ifdef CONFIG_ENERGY_MONITOR
	cputime_t emon_utime;
	cputime_t emon_stime;
	struct io_stats emon_io;
#endif
	struct hlist_node hash;
#ifdef CONFIG_PID_SYS_STATS
	DECLARE_HASHTABLE(pid_entries, SID_STAT_HASH_BITS);
#endif
};

static u64 compute_write_bytes(struct task_struct *task)
{
	if (task->ioac.write_bytes <= task->ioac.cancelled_write_bytes)
		return 0;

	return task->ioac.write_bytes - task->ioac.cancelled_write_bytes;
}

static void compute_io_bucket_stats(struct io_stats *io_bucket,
					struct io_stats *io_curr,
					struct io_stats *io_last,
					struct io_stats *io_dead)
{
	/* tasks could switch to another sid group, but its io_last in the
	 * previous sid group could still be positive.
	 * therefore before each update, do an overflow check first
	 */
	int64_t delta;

	delta = io_curr->read_bytes + io_dead->read_bytes -
		io_last->read_bytes;
	io_bucket->read_bytes += delta > 0 ? delta : 0;
	delta = io_curr->write_bytes + io_dead->write_bytes -
		io_last->write_bytes;
	io_bucket->write_bytes += delta > 0 ? delta : 0;
	delta = io_curr->rchar + io_dead->rchar - io_last->rchar;
	io_bucket->rchar += delta > 0 ? delta : 0;
	delta = io_curr->wchar + io_dead->wchar - io_last->wchar;
	io_bucket->wchar += delta > 0 ? delta : 0;
	delta = io_curr->fsync + io_dead->fsync - io_last->fsync;
	io_bucket->fsync += delta > 0 ? delta : 0;

	io_last->read_bytes = io_curr->read_bytes;
	io_last->write_bytes = io_curr->write_bytes;
	io_last->rchar = io_curr->rchar;
	io_last->wchar = io_curr->wchar;
	io_last->fsync = io_curr->fsync;

	memset(io_dead, 0, sizeof(struct io_stats));
}

static u32 find_usid(uid_t uid, u32 sid)
{
	u32 usid;

	if (uid >= 5000)
		usid = uid + sid;
	else
		usid = uid;

	return usid;
}

#ifdef CONFIG_PID_SYS_STATS
static struct pid_entry *find_pid_entry(struct sid_entry *sid_entry,
		struct task_struct *task, unsigned int key)
{
	struct pid_entry *pid_entry;

	hash_for_each_possible(sid_entry->pid_entries, pid_entry, hash, key) {
		if (key == pid_entry->key) {
			pr_debug("%s: exist_pid_entry: %s %u: %s %u",
				__func__, task->comm, key, pid_entry->comm, pid_entry->key);
			//if (strcmp(pid_entry->comm, task->comm)) {
				//pr_info("vincent:4:%s: wrong_key: %s %u: %s %u",
					//__func__, task->comm, key, pid_entry->comm, pid_entry->key);
				//return NULL;
			//}
			return pid_entry;
		}
	}

	pr_debug("%s: no_pid_entry: %s %u\n", __func__, task->comm, key);

	return NULL;
}

static struct pid_entry *find_or_register_pid(struct sid_entry *sid_entry,
		struct task_struct *task)
{
	struct pid_entry *pid_entry;
	pid_t pid = task->pid;
	unsigned int key;

	key = full_name_hash(NULL, task->comm, strlen(task->comm));
	pid_entry = find_pid_entry(sid_entry, task, key);
	if (pid_entry)
		return pid_entry;

	pid_entry = kzalloc(sizeof(struct pid_entry), GFP_ATOMIC);
	if (!pid_entry)
		return NULL;

	pid_entry->pid = pid;
	get_task_comm(pid_entry->comm, task);
	pid_entry->key = key;

	pr_info("%s: add_pid_entry_to_table: %u %s %u %u\n",
		__func__, sid_entry->usid, pid_entry->comm, pid, key);

	hash_add(sid_entry->pid_entries, &pid_entry->hash, key);

	return pid_entry;
}

static void add_pid_cpu_time(struct sid_entry *sid_entry,
		struct task_struct *task, cputime_t utime, cputime_t stime, int dead)
{
	struct pid_entry *pid_entry = find_or_register_pid(sid_entry, task);

	if (dead) {
		pid_entry->utime += utime;
		pid_entry->stime += stime;
	} else {
		pid_entry->active_utime+= utime;
		pid_entry->active_stime+= stime;
	}
}

static void set_pid_cpu_time_zero(struct sid_entry *sid_entry)
{
	struct pid_entry *pid_entry;
	unsigned long bkt_pid;

	hash_for_each(sid_entry->pid_entries, bkt_pid, pid_entry, hash) {
		pid_entry->active_stime = 0;
		pid_entry->active_utime = 0;
	}
}

static void show_pid_cpu_stats(struct seq_file *m,
	struct sid_entry *sid_entry, cputime_t profile_time)
{
	struct pid_entry *pid_entry;
	unsigned long bkt_pid;

	hash_for_each(sid_entry->pid_entries, bkt_pid, pid_entry, hash) {
		cputime_t total_utime = pid_entry->utime +
							pid_entry->active_utime;
		cputime_t total_stime = pid_entry->stime +
							pid_entry->active_stime;
		cputime_t total_time = total_utime + total_stime;
		u32 permil = (u32)div_u64(total_time, profile_time);

		seq_printf(m, "%u; pid; %lu; %s; %llu %llu %llu %u\n",
				sid_entry->usid,
				(unsigned long)pid_entry->pid,
				pid_entry->comm,
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_utime)),
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_stime)),
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_time)),
				permil);
	}
}
#endif

static struct sid_entry *find_or_register_usid(u32 usid, uid_t uid, u32 sid)
{
	struct sid_entry *sid_entry;

	hash_for_each_possible(sid_hash_table, sid_entry, hash, usid) {
		if (sid_entry->usid == usid)
			return sid_entry;
	}

	sid_entry = kzalloc(sizeof(struct sid_entry), GFP_ATOMIC);
	if (!sid_entry)
		return NULL;

	sid_entry->usid = usid;
	sid_entry->sid = sid;
	sid_entry->uid = uid;

#ifdef CONFIG_PID_SYS_STATS
	hash_init(sid_entry->pid_entries);
#endif
	hash_add(sid_hash_table, &sid_entry->hash, usid);

	return sid_entry;
}

static int sid_cputime_show(struct seq_file *m, void *v)
{
	struct sid_entry *sid_entry = NULL;
	struct task_struct *task, *temp;
	cputime_t profile_time = 0;
	cputime_t utime;
	cputime_t stime;
	unsigned long bkt;
	uid_t uid;
	u32 sid, usid;
	char *ctx = NULL;
	unsigned n;
	int err;

	rt_mutex_lock(&sid_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		sid_entry->active_stime = 0;
		sid_entry->active_utime = 0;
#ifdef CONFIG_PID_SYS_STATS
		set_pid_cpu_time_zero(sid_entry);
#endif
	}

	read_lock(&tasklist_lock);
	do_each_thread(temp, task) {
		uid = from_kuid_munged(current_user_ns(), task_uid(task));
		security_task_getsecid(task, &sid);
		usid = find_usid(uid, sid);
		if (!sid_entry || sid_entry->usid != usid)
			sid_entry = find_or_register_usid(usid, uid, sid);
		if (!sid_entry) {
			read_unlock(&tasklist_lock);
			rt_mutex_unlock(&sid_lock);
			pr_err("%s: failed to find the sid_entry for usid %u\n",
				__func__, usid);
			return -ENOMEM;
		}
		task_cputime_adjusted(task, &utime, &stime);
		sid_entry->active_utime += utime;
		sid_entry->active_stime += stime;
#ifdef CONFIG_PID_SYS_STATS
		/* uid 5000 - 9999 is for regular user */
		if (usid < 5000 && sid != 5)
			add_pid_cpu_time(sid_entry, task, utime, stime, 0);
#endif
	} while_each_thread(temp, task);
	read_unlock(&tasklist_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		profile_time += sid_entry->utime +
						sid_entry->active_utime +
						sid_entry->stime +
						sid_entry->active_stime;
	}
	profile_time = div_u64(profile_time, 1000ULL);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		cputime_t total_utime = sid_entry->utime +
							sid_entry->active_utime;
		cputime_t total_stime = sid_entry->stime +
							sid_entry->active_stime;
		cputime_t total_time = total_utime + total_stime;
		u32 permil = (u32)div_u64(total_time, profile_time);

		err = security_secid_to_secctx(sid_entry->sid, &ctx, &n);
		if (err) {
			seq_printf(m, "%u; %u; %u; %llu %llu %llu %u\n",
				sid_entry->usid, sid_entry->uid, sid_entry->sid,
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_utime)),
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_stime)),
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_time)),
				permil);

		}
		else {
			seq_printf(m, "%u; %u; %u; %s: %llu %llu %llu %u\n",
				sid_entry->usid, sid_entry->uid, sid_entry->sid, ctx,
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_utime)),
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_stime)),
				(unsigned long long)jiffies_to_msecs(
					cputime_to_jiffies(total_time)),
				permil);

			security_release_secctx(ctx, n);
		}
	}

#ifdef CONFIG_PID_SYS_STATS
	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		show_pid_cpu_stats(m, sid_entry, profile_time);
	}
#endif

	rt_mutex_unlock(&sid_lock);
	return 0;
}

static int sid_cputime_open(struct inode *inode, struct file *file)
{
	return single_open(file, sid_cputime_show, PDE_DATA(inode));
}

static const struct file_operations sid_cputime_fops = {
	.open		= sid_cputime_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void add_sid_io_stats(struct sid_entry *sid_entry,
			struct task_struct *task, int slot)
{
	struct io_stats *io_slot = &sid_entry->io[slot];

	io_slot->read_bytes += task->ioac.read_bytes;
	io_slot->write_bytes += compute_write_bytes(task);
	io_slot->rchar += task->ioac.rchar;
	io_slot->wchar += task->ioac.wchar;
	io_slot->fsync += task->ioac.syscfs;
}

static void update_io_stats_all_locked(void)
{
	struct sid_entry *sid_entry = NULL;
	struct task_struct *task, *temp;
	unsigned long bkt;
	uid_t uid;
	u32 sid, usid;

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		memset(&sid_entry->io[SID_STATE_TOTAL_CURR], 0,
			sizeof(struct io_stats));
	}

	rcu_read_lock();
	do_each_thread(temp, task) {
		uid = from_kuid_munged(current_user_ns(), task_uid(task));
		security_task_getsecid(task, &sid);
		usid = find_usid(uid, sid);
		if (!sid_entry || sid_entry->usid != usid)
			sid_entry = find_or_register_usid(usid, uid, sid);
		if (!sid_entry)
			continue;
		add_sid_io_stats(sid_entry, task, SID_STATE_TOTAL_CURR);
	} while_each_thread(temp, task);
	rcu_read_unlock();

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		compute_io_bucket_stats(&sid_entry->io[sid_entry->state],
					&sid_entry->io[SID_STATE_TOTAL_CURR],
					&sid_entry->io[SID_STATE_TOTAL_LAST],
					&sid_entry->io[SID_STATE_DEAD_TASKS]);
	}
}

static int sid_io_show(struct seq_file *m, void *v)
{
	struct sid_entry *sid_entry;
	unsigned long bkt;
	char *ctx = NULL;
	unsigned n;
	int err;

	rt_mutex_lock(&sid_lock);

	update_io_stats_all_locked();

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		err = security_secid_to_secctx(sid_entry->sid, &ctx, &n);
		if (err) {
			seq_printf(m, "%u; %u; %u; %llu %llu %llu %llu %llu "
							"%llu %llu %llu %llu %llu\n",
					sid_entry->usid, sid_entry->uid, sid_entry->sid,
					sid_entry->io[SID_STATE_FOREGROUND].rchar,
					sid_entry->io[SID_STATE_FOREGROUND].wchar,
					sid_entry->io[SID_STATE_FOREGROUND].read_bytes,
					sid_entry->io[SID_STATE_FOREGROUND].write_bytes,
					sid_entry->io[SID_STATE_BACKGROUND].rchar,
					sid_entry->io[SID_STATE_BACKGROUND].wchar,
					sid_entry->io[SID_STATE_BACKGROUND].read_bytes,
					sid_entry->io[SID_STATE_BACKGROUND].write_bytes,
					sid_entry->io[SID_STATE_FOREGROUND].fsync,
					sid_entry->io[SID_STATE_BACKGROUND].fsync);
		} else {
			seq_printf(m, "%u; %u; %u; %s: %llu %llu %llu %llu %llu"
							" %llu %llu %llu %llu %llu\n",
					sid_entry->usid, sid_entry->uid, sid_entry->sid, ctx,
					sid_entry->io[SID_STATE_FOREGROUND].rchar,
					sid_entry->io[SID_STATE_FOREGROUND].wchar,
					sid_entry->io[SID_STATE_FOREGROUND].read_bytes,
					sid_entry->io[SID_STATE_FOREGROUND].write_bytes,
					sid_entry->io[SID_STATE_BACKGROUND].rchar,
					sid_entry->io[SID_STATE_BACKGROUND].wchar,
					sid_entry->io[SID_STATE_BACKGROUND].read_bytes,
					sid_entry->io[SID_STATE_BACKGROUND].write_bytes,
					sid_entry->io[SID_STATE_FOREGROUND].fsync,
					sid_entry->io[SID_STATE_BACKGROUND].fsync);

			security_release_secctx(ctx, n);
		}
	}

	rt_mutex_unlock(&sid_lock);
	return 0;
}

static int sid_io_open(struct inode *inode, struct file *file)
{
	return single_open(file, sid_io_show, PDE_DATA(inode));
}

static const struct file_operations sid_io_fops = {
	.open		= sid_io_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int process_notifier(struct notifier_block *self,
			unsigned long cmd, void *v)
{
	struct task_struct *task = v;
	struct sid_entry *sid_entry;
	cputime_t utime, stime;
	uid_t uid;
	u32 sid, usid;

	if (!task)
		return NOTIFY_OK;

	rt_mutex_lock(&sid_lock);
	uid = from_kuid_munged(current_user_ns(), task_uid(task));
	security_task_getsecid(task, &sid);
	usid = find_usid(uid, sid);
	sid_entry = find_or_register_usid(usid, uid, sid);
	if (!sid_entry) {
		pr_err("%s: failed to find usid=%u\n", __func__, usid);
		goto exit;
	}

	task_cputime_adjusted(task, &utime, &stime);
	sid_entry->utime += utime;
	sid_entry->stime += stime;
#ifdef CONFIG_PID_SYS_STATS
	/* uid 5000 - 9999 is for regular user */
	if (usid < 5000 && sid != 5)
		add_pid_cpu_time(sid_entry, task, utime, stime, 1);
#endif
	add_sid_io_stats(sid_entry, task, SID_STATE_DEAD_TASKS);

exit:
	rt_mutex_unlock(&sid_lock);
	return NOTIFY_OK;
}

static struct notifier_block process_notifier_block = {
	.notifier_call	= process_notifier,
};

static void sid_cputime_print(void)
{
	struct sid_entry *sid_entry = NULL;
	struct task_struct *task, *temp;
	cputime_t utime;
	cputime_t stime;
	unsigned long bkt;
	uid_t uid;
	u32 sid, usid;
	int i = 0;

	rt_mutex_lock(&sid_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		sid_entry->active_stime = 0;
		sid_entry->active_utime = 0;
#ifdef CONFIG_PID_SYS_STATS
		set_pid_cpu_time_zero(sid_entry);
#endif
	}

	read_lock(&tasklist_lock);
	do_each_thread(temp, task) {
		uid = from_kuid_munged(current_user_ns(), task_uid(task));
		security_task_getsecid(task, &sid);
		usid = find_usid(uid, sid);
		if (!sid_entry || sid_entry->usid != usid)
			sid_entry = find_or_register_usid(usid, uid, sid);
		if (!sid_entry) {
			read_unlock(&tasklist_lock);
			rt_mutex_unlock(&sid_lock);
			pr_err("%s: failed to find the sid_entry for usid %u\n",
				__func__, usid);
			return;
		}
		task_cputime_adjusted(task, &utime, &stime);
		sid_entry->active_utime += utime;
		sid_entry->active_stime += stime;
#ifdef CONFIG_PID_SYS_STATS
		/* uid 5000 - 9999 is for regular user */
		if (usid < 5000 && sid != 5)
			add_pid_cpu_time(sid_entry, task, utime, stime, 0);
#endif
	} while_each_thread(temp, task);
	read_unlock(&tasklist_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		cputime_t total_utime = sid_entry->utime +
							sid_entry->active_utime -
							sid_entry->delta_utime;
		cputime_t total_stime = sid_entry->stime +
							sid_entry->active_stime -
							sid_entry->delta_stime;
		cputime_t total_time = total_utime + total_stime;

		i++;
		pr_cont("%u:%llu/", sid_entry->usid,
			(unsigned long long)jiffies_to_msecs(
			cputime_to_jiffies(total_time)));
		if (i % 7 == 0)
			pr_cont("\n");

		sid_entry->delta_utime = sid_entry->utime +
						sid_entry->active_utime;
		sid_entry->delta_stime = sid_entry->stime +
						sid_entry->active_stime;
	}
	pr_cont("\n");

	rt_mutex_unlock(&sid_lock);
}

static int sid_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *dummy)
{

	switch (event) {
	case PM_SUSPEND_PREPARE:
		sid_cputime_print();
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block pm_notifier_block = {
	.notifier_call = sid_pm_notifier,
};

#ifdef CONFIG_ENERGY_MONITOR
static int sid_emon_cmp_func(const void *a, const void *b)
{
	struct sid_sys_stats *pa = (struct sid_sys_stats *)(a);
	struct sid_sys_stats *pb = (struct sid_sys_stats *)(b);

	if (pb->ttime > pa->ttime)
		return 1;
	if (pb->ttime < pa->ttime)
		return -1;
	return 0;
}

int get_sid_cputime(int type,
			struct sid_sys_stats *stat_array, size_t n)
{
	struct sid_entry *sid_entry = NULL;
	struct task_struct *task, *temp;
	cputime_t profile_time = 0;
	cputime_t utime;
	cputime_t stime;
	unsigned long bkt;
	uid_t uid;
	u32 sid, usid;
	int i;

	if (!stat_array)
		return -EINVAL;

	memset(stat_array, 0, sizeof(struct sid_sys_stats) * n);

	i = 0;
	rt_mutex_lock(&sid_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		sid_entry->active_stime = 0;
		sid_entry->active_utime = 0;
#ifdef CONFIG_PID_SYS_STATS
		set_pid_cpu_time_zero(sid_entry);
#endif
	}

	read_lock(&tasklist_lock);
	do_each_thread(temp, task) {
		uid = from_kuid_munged(current_user_ns(), task_uid(task));
		security_task_getsecid(task, &sid);
		usid = find_usid(uid, sid);
		if (!sid_entry || sid_entry->usid != usid)
			sid_entry = find_or_register_usid(usid, uid, sid);
		if (!sid_entry) {
			read_unlock(&tasklist_lock);
			rt_mutex_unlock(&sid_lock);
			pr_err("%s: failed to find the sid_entry for usid %u\n",
				__func__, usid);
			return -ENOMEM;
		}
		task_cputime_adjusted(task, &utime, &stime);
		sid_entry->active_utime += utime;
		sid_entry->active_stime += stime;
#ifdef CONFIG_PID_SYS_STATS
		/* uid 5000 - 9999 is for regular user */
		if (usid < 5000 && sid != 5)
			add_pid_cpu_time(sid_entry, task, utime, stime, 0);
#endif
	} while_each_thread(temp, task);
	read_unlock(&tasklist_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		profile_time += sid_entry->utime +
						sid_entry->active_utime -
						sid_entry->emon_utime +
						sid_entry->stime +
						sid_entry->active_stime -
						sid_entry->emon_stime;
	}
	profile_time = div_u64(profile_time, 1000ULL);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		cputime_t total_utime = sid_entry->utime +
							sid_entry->active_utime -
							sid_entry->emon_utime;
		cputime_t total_stime = sid_entry->stime +
							sid_entry->active_stime -
							sid_entry->emon_stime;
		cputime_t total_time = total_utime + total_stime;
		u32 permil = (u32)div_u64(total_time, profile_time);

		if (i < n) {
			stat_array[i].usid = sid_entry->usid;
			stat_array[i].uid = sid_entry->uid;
			stat_array[i].sid = sid_entry->sid;
			stat_array[i].utime = total_utime;
			stat_array[i].stime = total_stime;
			stat_array[i].ttime = total_time;
			stat_array[i].permil = permil;
			i++;
			if (i == n)
				sort(&stat_array[0],
					n,
					sizeof(struct sid_sys_stats),
					sid_emon_cmp_func, NULL);
		} else {
			if (total_time > stat_array[n-1].ttime) {
				stat_array[n-1].usid = sid_entry->usid;
				stat_array[n-1].uid = sid_entry->uid;
				stat_array[n-1].sid = sid_entry->sid;
				stat_array[n-1].utime = total_utime;
				stat_array[n-1].stime = total_stime;
				stat_array[n-1].ttime = total_time;
				stat_array[n-1].permil = permil;
				sort(&stat_array[0],
					n,
					sizeof(struct sid_sys_stats),
					sid_emon_cmp_func, NULL);
			}
		}

		if (type != ENERGY_MON_TYPE_DUMP) {
			sid_entry->emon_utime = sid_entry->utime +
							sid_entry->active_utime;
			sid_entry->emon_stime = sid_entry->stime +
							sid_entry->active_stime;
		}
	}

	rt_mutex_unlock(&sid_lock);

	if (i < n && i != 0)
		sort(&stat_array[0],
			n,
			sizeof(struct sid_sys_stats),
			sid_emon_cmp_func, NULL);

	return 0;
}


#ifdef CONFIG_PID_SYS_STATS
int get_sid_cputime2(int type, struct sid_sys_stats *sid_array,
			struct sid_sys_stats *pid_array, size_t n)
{
	struct sid_entry *sid_entry = NULL;
	struct pid_entry *pid_entry = NULL;
	struct task_struct *task, *temp;
	cputime_t profile_time = 0;
	cputime_t utime;
	cputime_t stime;
	unsigned long bkt, bkt_pid;
	uid_t uid;
	u32 sid, usid;
	int i = 0, j = 0;

	if (!sid_array)
		return -EINVAL;
	memset(sid_array, 0, sizeof(struct sid_sys_stats) * n);

	if (!pid_array)
		return -EINVAL;
	memset(pid_array, 0, sizeof(struct sid_sys_stats) * n);

	rt_mutex_lock(&sid_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		sid_entry->active_stime = 0;
		sid_entry->active_utime = 0;
		set_pid_cpu_time_zero(sid_entry);
	}

	read_lock(&tasklist_lock);
	do_each_thread(temp, task) {
		uid = from_kuid_munged(current_user_ns(), task_uid(task));
		security_task_getsecid(task, &sid);
		usid = find_usid(uid, sid);
		if (!sid_entry || sid_entry->usid != usid)
			sid_entry = find_or_register_usid(usid, uid, sid);
		if (!sid_entry) {
			read_unlock(&tasklist_lock);
			rt_mutex_unlock(&sid_lock);
			pr_err("%s: failed to find the sid_entry for usid %u\n",
				__func__, usid);
			return -ENOMEM;
		}
		task_cputime_adjusted(task, &utime, &stime);
		sid_entry->active_utime += utime;
		sid_entry->active_stime += stime;

		/* uid 5000 - 9999 is for regular user */
		if (usid < 5000 && sid != 5)
			add_pid_cpu_time(sid_entry, task, utime, stime, 0);
	} while_each_thread(temp, task);
	read_unlock(&tasklist_lock);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		profile_time += sid_entry->utime +
						sid_entry->active_utime -
						sid_entry->emon_utime +
						sid_entry->stime +
						sid_entry->active_stime -
						sid_entry->emon_stime;
	}
	profile_time = div_u64(profile_time, 1000ULL);

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		cputime_t total_utime = sid_entry->utime +
							sid_entry->active_utime -
							sid_entry->emon_utime;
		cputime_t total_stime = sid_entry->stime +
							sid_entry->active_stime -
							sid_entry->emon_stime;
		cputime_t total_time = total_utime + total_stime;
		u32 permil = (u32)div_u64(total_time, profile_time);

		if (i < n) {
			sid_array[i].usid = sid_entry->usid;
			sid_array[i].uid = sid_entry->uid;
			sid_array[i].sid = sid_entry->sid;
			sid_array[i].utime = total_utime;
			sid_array[i].stime = total_stime;
			sid_array[i].ttime = total_time;
			sid_array[i].permil = permil;
			i++;
			if (i == n)
				sort(&sid_array[0],
					n,
					sizeof(struct sid_sys_stats),
					sid_emon_cmp_func, NULL);
		} else {
			if (total_time > sid_array[n-1].ttime) {
				sid_array[n-1].usid = sid_entry->usid;
				sid_array[n-1].uid = sid_entry->uid;
				sid_array[n-1].sid = sid_entry->sid;
				sid_array[n-1].utime = total_utime;
				sid_array[n-1].stime = total_stime;
				sid_array[n-1].ttime = total_time;
				sid_array[n-1].permil = permil;
				sort(&sid_array[0],
					n,
					sizeof(struct sid_sys_stats),
					sid_emon_cmp_func, NULL);
			}
		}

		if (type != ENERGY_MON_TYPE_DUMP) {
			sid_entry->emon_utime = sid_entry->utime +
							sid_entry->active_utime;
			sid_entry->emon_stime = sid_entry->stime +
							sid_entry->active_stime;
		}

		hash_for_each(sid_entry->pid_entries, bkt_pid, pid_entry, hash) {
			cputime_t total_pid_utime = pid_entry->utime +
								pid_entry->active_utime -
								pid_entry->emon_utime;
			cputime_t total_pid_stime = pid_entry->stime +
								pid_entry->active_stime -
								pid_entry->emon_stime;
			cputime_t total_pid_time = total_pid_utime + total_pid_stime;
			u32 permil = (u32)div_u64(total_pid_time, profile_time);

			if (j < n) {
				memcpy(&pid_array[j].comm, &pid_entry->comm, TASK_COMM_LEN);
				pid_array[j].utime = total_pid_utime;
				pid_array[j].stime = total_pid_stime;
				pid_array[j].ttime = total_pid_time;
				pid_array[j].permil = permil;
				j++;
				if (j == n)
					sort(&pid_array[0],
						n,
						sizeof(struct sid_sys_stats),
						sid_emon_cmp_func, NULL);
			} else {
				if (total_pid_time > pid_array[n-1].ttime) {
					memcpy(&pid_array[n-1].comm, &pid_entry->comm, TASK_COMM_LEN);
					pid_array[n-1].utime = total_pid_utime;
					pid_array[n-1].stime = total_pid_stime;
					pid_array[n-1].ttime = total_pid_time;
					pid_array[n-1].permil = permil;
					sort(&pid_array[0],
						n,
						sizeof(struct sid_sys_stats),
						sid_emon_cmp_func, NULL);
				}
			}

			if (type != ENERGY_MON_TYPE_DUMP) {
				pid_entry->emon_utime = pid_entry->utime +
								pid_entry->active_utime;
				pid_entry->emon_stime = pid_entry->stime +
								pid_entry->active_stime;
			};
		}
	}

	rt_mutex_unlock(&sid_lock);

	if (i < n && i != 0)
		sort(&sid_array[0],
			n,
			sizeof(struct sid_sys_stats),
			sid_emon_cmp_func, NULL);

	if (j < n && j != 0)
		sort(&pid_array[0],
			n,
			sizeof(struct sid_sys_stats),
			sid_emon_cmp_func, NULL);

	return 0;
}
#endif

static int sid_io_emon_cmp_func(const void *a, const void *b)
{
	struct sid_io_stats *pa = (struct sid_io_stats *)(a);
	struct sid_io_stats *pb = (struct sid_io_stats *)(b);

	if (pb->tchar > pa->tchar)
		return 1;
	if (pb->tchar < pa->tchar)
		return -1;
	return 0;
}

int get_sid_io(int type,
			struct sid_io_stats *stat_array, size_t n)
{
	struct sid_entry *sid_entry = NULL;
	unsigned long bkt;
	int i = 0;

	if (!stat_array)
		return -EINVAL;

	memset(stat_array, 0, sizeof(struct sid_io_stats) * n);

	rt_mutex_lock(&sid_lock);

	update_io_stats_all_locked();

	hash_for_each(sid_hash_table, bkt, sid_entry, hash) {
		u64 read_bytes = sid_entry->io[SID_STATE_FOREGROUND].read_bytes +
						sid_entry->io[SID_STATE_BACKGROUND].read_bytes -
						sid_entry->emon_io.read_bytes;
		u64 write_bytes = sid_entry->io[SID_STATE_FOREGROUND].write_bytes +
						sid_entry->io[SID_STATE_BACKGROUND].write_bytes -
						sid_entry->emon_io.write_bytes;
		u64 rchar = sid_entry->io[SID_STATE_FOREGROUND].rchar +
						sid_entry->io[SID_STATE_BACKGROUND].rchar -
						sid_entry->emon_io.rchar;
		u64 wchar = sid_entry->io[SID_STATE_FOREGROUND].wchar +
						sid_entry->io[SID_STATE_BACKGROUND].wchar -
						sid_entry->emon_io.wchar;
		u64 tchar = rchar + wchar;

		if (i < n) {
			stat_array[i].usid = sid_entry->usid;
			stat_array[i].uid = sid_entry->uid;
			stat_array[i].sid = sid_entry->sid;
			stat_array[i].read_bytes = read_bytes;
			stat_array[i].write_bytes = write_bytes;
			stat_array[i].rchar = rchar;
			stat_array[i].wchar = wchar;
			stat_array[i].tchar = tchar;
			i++;
			if (i == n)
				sort(&stat_array[0],
					n,
					sizeof(struct sid_io_stats),
					sid_io_emon_cmp_func, NULL);
		} else {
			if (tchar > stat_array[n-1].tchar) {
				stat_array[n-1].usid = sid_entry->usid;
				stat_array[n-1].uid = sid_entry->uid;
				stat_array[n-1].sid = sid_entry->sid;
				stat_array[n-1].read_bytes = read_bytes;
				stat_array[n-1].write_bytes = write_bytes;
				stat_array[n-1].rchar = rchar;
				stat_array[n-1].wchar = wchar;
				stat_array[n-1].tchar = tchar;
				sort(&stat_array[0],
					n,
					sizeof(struct sid_io_stats),
					sid_io_emon_cmp_func, NULL);
			}
		}

		if (type != ENERGY_MON_TYPE_DUMP) {
			sid_entry->emon_io.read_bytes =
						sid_entry->io[SID_STATE_FOREGROUND].read_bytes +
						sid_entry->io[SID_STATE_BACKGROUND].read_bytes;
			sid_entry->emon_io.write_bytes =
						sid_entry->io[SID_STATE_FOREGROUND].write_bytes +
						sid_entry->io[SID_STATE_BACKGROUND].write_bytes;
			sid_entry->emon_io.rchar =
						sid_entry->io[SID_STATE_FOREGROUND].rchar +
						sid_entry->io[SID_STATE_BACKGROUND].rchar;
			sid_entry->emon_io.wchar =
						sid_entry->io[SID_STATE_FOREGROUND].wchar +
						sid_entry->io[SID_STATE_BACKGROUND].wchar;
		}
	}

	rt_mutex_unlock(&sid_lock);

	if (i < n && i != 0)
		sort(&stat_array[0],
			n,
			sizeof(struct sid_io_stats),
			sid_io_emon_cmp_func, NULL);

	return 0;
}
#endif

static int __init proc_sid_sys_stats_init(void)
{
	hash_init(sid_hash_table);

	cpu_parent = proc_mkdir("sid_cputime", NULL);
	if (!cpu_parent) {
		pr_err("%s: failed to create sid_cputime proc entry\n",
			__func__);
		goto err;
	}

	proc_create_data("stat", 0400, cpu_parent,
		&sid_cputime_fops, NULL);

	io_parent = proc_mkdir("sid_io", NULL);
	if (!io_parent) {
		pr_err("%s: failed to create sid_io proc entry\n",
			__func__);
		goto err;
	}

	proc_create_data("stats", 0400, io_parent,
		&sid_io_fops, NULL);

	profile_event_register(PROFILE_TASK_EXIT, &process_notifier_block);

	register_pm_notifier(&pm_notifier_block);

	return 0;

err:
	remove_proc_subtree("sid_cputime", NULL);
	remove_proc_subtree("sid_io", NULL);
	return -ENOMEM;
}

early_initcall(proc_sid_sys_stats_init);

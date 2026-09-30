// SPDX-License-Identifier: MIT
/*
 * Copyright © 2026 Intel Corporation
 */

#include <linux/cleanup.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/kstrtox.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "xe_pm.h"
#include "xe_printk.h"
#include "xe_sysctrl.h"
#include "xe_sysctrl_debugfs.h"
#include "xe_sysctrl_mailbox.h"
#include "xe_sysctrl_mailbox_types.h"
#include "xe_sysctrl_types.h"

static ssize_t xe_sysctrl_loopback_write(struct file *file, const char __user *ubuf,
					 size_t len, loff_t *offp)
{
	char *kbuf __free(kfree) = NULL;
	u8 *input __free(kfree) = NULL;
	struct seq_file *m = file->private_data;
	struct xe_sysctrl_debugfs_entry *entry = m->private;
	struct xe_device *xe = sc_to_xe(entry->sc);
	struct xe_sysctrl_mailbox_command cmd = {};
	char *token, *tmp;
	unsigned long val;
	size_t input_len = 0;
	size_t max_input;
	size_t out_len = 0;
	int status;

	if (*offp)
		return -ESPIPE;

	if (len == 0 || len >= PAGE_SIZE)
		return -EINVAL;

	kbuf = kmalloc(len + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	max_input = min_t(size_t, len, XE_SYSCTRL_MB_MAX_DATA_SIZE);
	input = kmalloc(max_input, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(kbuf, ubuf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	tmp = kbuf;
	while ((token = strsep(&tmp, " \t\n")) != NULL) {
		if (*token == '\0')
			continue;

		if (input_len >= max_input) {
			xe_err(xe, "sysctrl: loopback payload too large (max %zu bytes)\n",
			       max_input);
			return -EINVAL;
		}

		if (kstrtoul(token, 0, &val) || val > 0xFF) {
			xe_err(xe, "sysctrl: invalid loopback token '%s'\n", token);
			return -EINVAL;
		}

		input[input_len++] = (u8)val;
	}

	if (input_len == 0) {
		xe_err(xe, "sysctrl: no loopback payload given\n");
		return -EINVAL;
	}

	xe_sysctrl_create_command(&cmd, entry->group, entry->command,
				  input, input_len, entry->response_buf, input_len);

	scoped_guard(mutex, &entry->lock) {
		guard(xe_pm_runtime)(xe);
		entry->status = xe_sysctrl_send_command(entry->sc, &cmd, &out_len);
		entry->response_len = entry->status ? 0 : out_len;
		status = entry->status;
	}

	return status ? status : len;
}

static int xe_sysctrl_loopback_show(struct seq_file *m, void *data)
{
	struct xe_sysctrl_debugfs_entry *entry = m->private;
	size_t i;

	guard(mutex)(&entry->lock);

	seq_printf(m, "Command: group=0x%02x cmd=0x%02x\n", entry->group, entry->command);
	seq_printf(m, "Status: %d (%s)\n", entry->status, entry->status ? "FAILED" : "SUCCESS");
	seq_printf(m, "Response: %zu bytes\n", entry->response_len);

	if (entry->response_len) {
		seq_puts(m, "Response data:\n");
		for (i = 0; i < entry->response_len; i++) {
			if (i && (i % 16) == 0)
				seq_putc(m, '\n');
			seq_printf(m, "%02x ", entry->response_buf[i]);
		}
		seq_putc(m, '\n');
	}

	seq_puts(m, "\nUsage:\n");
	seq_puts(m, "  echo \"<byte0> [byte1 ...]\" > loopback\n");
	seq_puts(m, "  cat loopback\n");

	return 0;
}

static int xe_sysctrl_loopback_open(struct inode *inode, struct file *file)
{
	return single_open(file, xe_sysctrl_loopback_show, inode->i_private);
}

static const struct file_operations xe_sysctrl_loopback_fops = {
	.owner = THIS_MODULE,
	.open = xe_sysctrl_loopback_open,
	.read = seq_read,
	.write = xe_sysctrl_loopback_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void xe_sysctrl_register_entry(struct dentry *root, struct xe_sysctrl_debugfs_entry *entry,
				      struct xe_sysctrl *sc, const char *name,
				      u8 group, u8 command,
				      const struct file_operations *fops)
{
	struct xe_device *xe = sc_to_xe(sc);

	if (devm_mutex_init(xe->drm.dev, &entry->lock)) {
		xe_err(xe, "sysctrl: failed to init %s debugfs entry lock\n", name);
		return;
	}

	entry->sc = sc;
	entry->group = group;
	entry->command = command;
	entry->response_len = 0;
	entry->status = 0;

	debugfs_create_file(name, 0600, root, entry, fops);
}

/**
 * xe_sysctrl_debugfs_register - Register debugfs entries for System Controller
 * @sc: xe_sysctrl instance
 * @parent: parent debugfs directory
 */
void xe_sysctrl_debugfs_register(struct xe_sysctrl *sc, struct dentry *parent)
{
	struct dentry *root;

	root = debugfs_create_dir("sc", parent);
	if (IS_ERR(root))
		return;

	sc->debugfs.root = root;

	xe_sysctrl_register_entry(root, &sc->debugfs.loopback, sc, "loopback",
				  XE_SYSCTRL_GROUP_CORE, XE_SYSCTRL_CMD_LOOPBACK,
				  &xe_sysctrl_loopback_fops);
}

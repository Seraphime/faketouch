//#define pr_fmt(x) KMOD_BUILDNAME ": " x

#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>

MODULE_AUTHOR("Seraphime Kirkovski <kirkseraph@gmail.com>");
MODULE_DESCRIPTION("Want to test your touch sensor based program without hardware ?"
		  "If you can plugin a keyboard, this is your thing");
MODULE_LICENSE("GPL");

static bool faketouch_value;
static DECLARE_WAIT_QUEUE_HEAD(faketouch_wq);

static bool get_last_observed_value(struct file *f)
{
	return (bool)((unsigned long)f->private_data);
}

static inline void set_last_observed_value(struct file *f, bool value)
{
	f->private_data = (void *)((unsigned long)value);
}

static int faketouch_open(struct inode *i, struct file *f)
{
	return try_module_get(THIS_MODULE) ? 0 : -ENOENT;
}

static int faketouch_release(struct inode *i, struct file *f)
{
	module_put(THIS_MODULE);
	return 0;
}

static ssize_t faketouch_read(struct file *filep, char __user *ubuff,
			      size_t size, loff_t *off)
{
	int r;
	char cpy[3];

	if (*off >= sizeof cpy)
		return 0; /* should lseek for the new value */

	if (!access_ok(VERIFY_WRITE, ubuff, sizeof cpy))
		return -EFAULT;

	size = min_t(size_t, size, sizeof cpy);

	cpy[0] = '0' + faketouch_value;
	cpy[1] = '\n';
	cpy[2] = 0;
	r = copy_to_user(ubuff, cpy + *off, size);
	if (r < 0)
		return r;

	set_last_observed_value(filep, *cpy);
	*off += size;
	return *off;
}

static unsigned int faketouch_poll(struct file *filep,
				   struct poll_table_struct *ptable)
{
	bool poll = get_last_observed_value(filep) == faketouch_value;

	poll_wait(filep, &faketouch_wq, ptable);
	if (poll)
		return 0;

	return POLLIN | POLLPRI;
}

loff_t faketouch_llseek(struct file *filep, loff_t off, int whence)
{
	loff_t newpos;

	switch (whence) {
	case SEEK_SET:
		newpos = off;
		break;
	case SEEK_CUR:
		newpos = filep->f_pos + off;
		break;
	case SEEK_END:
		newpos = 4;
		break;
	default:
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;
	filep->f_pos = newpos;
	return newpos;
}

static const struct file_operations faketouch_fops = {
	.open = faketouch_open,
	.release = faketouch_release,
	.read = faketouch_read,
	.poll = faketouch_poll,
	.llseek = faketouch_llseek,
};

static int faketouch_kb_callback(struct notifier_block *block,
				 unsigned long code,
				 void *p)
{
	struct keyboard_notifier_param *kbd = p;

	if (KTYP(kbd->value) == 0xf0 &&
		KVAL(kbd->value) == ' ' && !!kbd->down != faketouch_value) {
		faketouch_value = !!kbd->down;
		wake_up_interruptible(&faketouch_wq);
	}

	return NOTIFY_OK;
}

static struct notifier_block faketouch_kb = {
	.notifier_call = faketouch_kb_callback
};

struct dentry *faketouch_file;

static int __init faketouch_init(void)
{
	int r;

	r = register_keyboard_notifier(&faketouch_kb);
	if (r < 0) {
		pr_err("Cannot register keyboard notifier\n");
		return r;
	}

	faketouch_file = debugfs_create_file("faketouch", 0444, NULL, NULL, &faketouch_fops);
	if (!faketouch_file) {
		unregister_keyboard_notifier(&faketouch_kb);
		return -ENOMEM;
	}

	return 0;
}

static void __exit faketouch_exit(void)
{
	unregister_keyboard_notifier(&faketouch_kb);
	debugfs_remove(faketouch_file);
}

module_init(faketouch_init);
module_exit(faketouch_exit);

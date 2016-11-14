/*
 * atc_tod.c -- ATC TOD driver with PPS support
 *
 * Copyright (C) 2015 Doug Crawford <doug.crawford@intelight-its.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pps_kernel.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/rtc.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <atc.h>

static int pl_freq = 60;
module_param(pl_freq, int, 0644);
MODULE_PARM_DESC(pl_freq, "Power Line Frequency (HZ)");

static char *timesrc = "LINESYNC";
module_param(timesrc, charp, 0644);
MODULE_PARM_DESC(timesrc, "ATC Time Source Name");

#define RTC_UPDATE_DELAY 500000000

/* Info for each registered platform device */
struct atc_tod_data {
	int irq;
	struct pps_device *pps;
	struct pps_source_info info;
	struct timespec ts;
	struct miscdevice miscdev;
	struct fasync_struct *tick_async_queue;
	struct fasync_struct *onchange_async_queue;
	struct work_struct rtc_read_work;
	struct work_struct rtc_write_work;
	struct rtc_time rtc_tm;
	bool rtc_sync;
	bool rtc_loaded;
	int tick_sig_num;
    int onchange_sig_num;
	bool linesync_sync;
	int count;
	int frequency;
	int timesrc;
	unsigned int gpio_pin;
	int rtc_errors;
};

static struct atc_tod_data *global_dev;

static int atc_tod_fasync(int fd, struct file *filp, int on)
{
	struct atc_tod_data *p_data = (struct atc_tod_data *)filp->private_data;

	return fasync_helper(fd, filp, on, &p_data->tick_async_queue);
}

static long atc_tod_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct atc_tod_data *dd = (struct atc_tod_data *)filp->private_data;
	unsigned long size;
	int ret = 0;
	char *timesrc_str = NULL;

	// lock structures
	size = (cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
	if (cmd & IOC_IN) {
		if (!access_ok(VERIFY_READ, (void __user *)arg, size))
			return -EFAULT;
	}
	if (cmd & IOC_OUT) {
		if (!access_ok(VERIFY_WRITE, (void __user *)arg, size))
			return -EFAULT;
	}
	switch (cmd) {
	case ATC_TOD_GET_TIMESRC:
		ret = dd->timesrc;
		break;
	case ATC_TOD_SET_TIMESRC: {
		unsigned long src;
		if (copy_from_user(&src, (void __user *)arg, sizeof(unsigned long)))
			return -EFAULT;
		switch (src) {
		case ATC_TIMESRC_LINESYNC:
			timesrc_str = "LINESYNC";
			// probably should re-sync to RTC?
			break;
		//case ATC_TIMESRC_RTCSQWR:
			//timesrc_str = "RTCSQWR";
			//pl_freq = 50;
			//break;
		case ATC_TIMESRC_CRYSTAL:
			timesrc_str = "CRYSTAL";
			break;
		case ATC_TIMESRC_EXTERNAL1:
			timesrc_str = "EXTERNAL1";
			break;
		case ATC_TIMESRC_EXTERNAL2:
			timesrc_str = "EXTERNAL2";
			break;
		default:
			ret = -EINVAL;
		}
		if (ret == 0) {
			pr_info( "atc_tod: setting time source to %s\n", timesrc_str);
			dd->timesrc = src;
		}
		break;
	}
	case ATC_TOD_GET_INPUT_FREQ:
		if ((dd->timesrc == ATC_TIMESRC_LINESYNC)
				|| (dd->timesrc == ATC_TIMESRC_RTCSQWR))
			ret = pl_freq;
		else if (dd->timesrc == ATC_TIMESRC_CRYSTAL)
			ret = HZ;
		else
			ret = 0; // unknown
		break;
	case ATC_TOD_REQUEST_TICK_SIG: {
		unsigned long sig = arg;
		//if (copy_from_user(&sig, (void __user *)arg, sizeof(unsigned long)))
		//	return -EFAULT;
		if (!valid_signal(sig) || (dd->timesrc != ATC_TIMESRC_LINESYNC)) {
			ret = -EINVAL;
		} else {
			dd->tick_sig_num = sig;
			// perform equivalent of F_SETOWN fcntl
			f_setown(filp, current->pid, 1);
			// perform equivalent of F_SETSIG fcntl
			filp->f_owner.signum = sig;
			// use fasync_helper and tick_async_queue
			if (fasync_helper(0, filp, 1, &dd->tick_async_queue) < 0) {
				pr_debug("atc_tod_ioctl: tick sig err=%d\n", ret); 
				ret = -EINVAL;
			}
		}
		break;
	}
	case ATC_TOD_CANCEL_TICK_SIG:
		if (dd->tick_sig_num)
			dd->tick_sig_num = 0;
		fasync_helper(0, filp, 0, &dd->tick_async_queue);
		break;
	case ATC_TOD_REQUEST_ONCHANGE_SIG: {
		unsigned long sig = arg;
		//if (copy_from_user(&sig, (void __user *)arg, sizeof(unsigned long)))
		//	return -EFAULT;
		if (!valid_signal(sig)) {
			ret = -EINVAL;
		} else {
			dd->onchange_sig_num = sig;
			// perform equivalent of F_SETOWN fcntl
			f_setown(filp, current->pid, 1);
			// perform equivalent of F_SETSIG fcntl
			filp->f_owner.signum = sig;
			// use fasync_helper and onchange_async_queue
			if (fasync_helper(0, filp, 1, &dd->onchange_async_queue) < 0) {
				pr_debug("atc_tod_ioctl: onchange sig err=%d\n", ret); 
				ret = -EINVAL;
			}
		}
		break;
	}
	case ATC_TOD_CANCEL_ONCHANGE_SIG:
		if (dd->onchange_sig_num)
			dd->onchange_sig_num = 0;
		fasync_helper(0, filp, 0, &dd->onchange_async_queue);
		break;
	default:
		ret = -ENOTTY;
	}
	// unlock structures
	pr_debug("atc_tod_ioctl: cmd=%x arg=%x ret=%d\n", (int)cmd, (int)arg, ret);
	return ret;
}

static int atc_tod_open(struct inode *inode, struct file *filp)
{
        filp->private_data = global_dev;

        return 0;
}

static int atc_tod_close(struct inode *inode, struct file *filp)
{
	atc_tod_fasync(-1, filp, 0);
	filp->private_data = NULL;

	return 0;
}

static const struct file_operations atc_tod_fops = {
	.owner = THIS_MODULE,
/*	.read = atc_tod_read,
	.write = atc_tod_write,*/
	.open = atc_tod_open,
	.release = atc_tod_close,
	.unlocked_ioctl	= atc_tod_ioctl,
	.fasync = atc_tod_fasync,
};

static void atc_tod_rtc_read(struct work_struct *work)
{
	struct atc_tod_data *dd = container_of(work, struct atc_tod_data, rtc_read_work);
	struct rtc_device *rtc;
	struct timespec ts;
	struct rtc_time tm = {0};

	if((dd->rtc_loaded == true) || (dd->rtc_errors > 5)) {
		dd->rtc_loaded = true;
		return;
	}

	rtc = rtc_class_open("rtc0");
	if(!rtc) {
		pr_err("failed to open read rtc0\n");
		dd->rtc_errors++;
		return;
	}

	if(rtc_read_time(rtc, &tm) || rtc_valid_tm(&tm)) {
		pr_err("failed to get RTC time\n");
		rtc_class_close(rtc);
		dd->rtc_tm.tm_sec = 0;
		dd->rtc_errors++;
		return;
	}

	/* Look for a RTC second rollover */
	if(dd->rtc_tm.tm_sec != 0 && (dd->rtc_tm.tm_sec != tm.tm_sec)) {
		rtc_tm_to_time(&tm, &ts.tv_sec);
		ts.tv_nsec = 0;
		do_settimeofday(&ts);
		pr_info("setting system clock to "
				"%d-%02d-%02d %02d:%02d:%02d UTC\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
		dd->linesync_sync = false;
		dd->rtc_sync = true;
		dd->rtc_loaded = true;
		dd->ts.tv_sec = 0;
		dd->count = 0;
	}

	dd->rtc_tm = tm;
	rtc_class_close(rtc);
}

static void atc_tod_rtc_write(struct work_struct *work)
{
	struct rtc_device *rtc;
	struct timespec ts;
	struct rtc_time tm = {0};
	ktime_t timeout;

	rtc = rtc_class_open("rtc0");
	if(!rtc) {
		pr_err("failed to open write rtc0\n");
		return;
	}

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	__set_current_state(TASK_UNINTERRUPTIBLE);
	timeout = ktime_set(0, 1000000000 - RTC_UPDATE_DELAY - ts.tv_nsec);
	schedule_hrtimeout_range(&timeout, 1000, HRTIMER_MODE_REL);

	if (rtc_set_time(rtc, &tm))
		pr_err("rtc_set_time error\n");
	else
		pr_info("setting rtc clock to "
				"%d-%02d-%02d %02d:%02d:%02d UTC\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);

	rtc_class_close(rtc);
}

static irqreturn_t atc_tod_irq_handler(int irq, void *data)
{
	struct atc_tod_data *dd = data;
	struct pps_event_time ts;
	long delta;

	dd->count++;

	if (unlikely(dd->rtc_loaded == false))
		schedule_work(&dd->rtc_read_work);

	if(dd->linesync_sync) {
		if(dd->count >= dd->frequency * 2) {
			/* Get the PPS time stamp first */
			pps_get_ts(&ts);

			dd->count = 0;

			/* The new timestamp must be 1 second ahead with no more than 200ms error */
			delta = 0;
			if(ts.ts_real.tv_sec == dd->ts.tv_sec) {
				delta = 1000000000 - ts.ts_real.tv_nsec - dd->ts.tv_nsec;
			} else if(ts.ts_real.tv_sec == dd->ts.tv_sec + 1) {
				delta = abs(ts.ts_real.tv_nsec - dd->ts.tv_nsec);
			} else if(ts.ts_real.tv_sec == dd->ts.tv_sec + 2) {
				delta = 1000000000 + ts.ts_real.tv_nsec - dd->ts.tv_nsec;
			} else {
				dd->linesync_sync = false;
			}
			dd->ts = ts.ts_real;

			if(delta > 200000000)
				dd->linesync_sync = false;
			
			/* send PPS assert if linesync_sync on second boundaries */
			if(dd->linesync_sync) {
				pps_event(dd->pps, &ts, PPS_CAPTUREASSERT, NULL);
			} else {
				dd->rtc_sync = false;
				dd->ts.tv_sec = 0;
				if (dd->onchange_async_queue != NULL)
					kill_fasync(&dd->onchange_async_queue, SIGIO, POLL_IN);
			}
		}
	} else {
		pps_get_ts(&ts);
		if((dd->rtc_loaded == true) && (dd->ts.tv_sec != 0) && (ts.ts_real.tv_sec != dd->ts.tv_sec)) {
			pps_event(dd->pps, &ts, PPS_CAPTUREASSERT, NULL);
			dd->linesync_sync = true;
			dd->count = 0;
			if(dd->rtc_sync == false) {
				dd->rtc_sync = true;
				schedule_work(&dd->rtc_write_work);
			}
			pr_info("linesync pps synchronized with second\n");			
		}
		dd->ts = ts.ts_real;
	}

	if (dd->tick_async_queue != NULL)
		kill_fasync(&dd->tick_async_queue, SIGIO, POLL_IN);

	return IRQ_HANDLED;
}


static int atc_tod_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct atc_tod_data *data;
	const char *gpio_label;
	int ret;
	int pps_default_params;

	/* allocate space for device info */
	data = devm_kzalloc(&pdev->dev, sizeof(struct atc_tod_data),
			GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* assign device structure to a global poiner */
	global_dev = data;

	/* get linesync gpio from device tree */
	ret = of_get_gpio(np, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get GPIO from device tree\n");
		return ret;
	}
	data->gpio_pin = ret;
	gpio_label = "atc-linesync";

	INIT_WORK(&data->rtc_read_work, atc_tod_rtc_read);
	INIT_WORK(&data->rtc_write_work, atc_tod_rtc_write);

	/* Initialize variables */
	data->frequency = pl_freq;
	data->linesync_sync = false;
	data->count = 0;
	data->ts.tv_sec = 0;
	data->tick_async_queue = NULL;
	data->onchange_async_queue = NULL;
	data->tick_sig_num = 0;
	data->onchange_sig_num = 0;
	data->rtc_sync = false;
	data->rtc_loaded = false;
	data->rtc_errors = 0;
	data->rtc_tm.tm_sec = 0;

	/* setup ioctl handling */
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.fops = &atc_tod_fops;
	data->miscdev.name = kstrdup(np->name, GFP_KERNEL);
	ret = misc_register(&data->miscdev);

	/* GPIO setup */
	ret = devm_gpio_request(&pdev->dev, data->gpio_pin, gpio_label);
	if (ret) {
		dev_err(&pdev->dev, "failed to request GPIO %u\n",
			data->gpio_pin);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_pin);
	if (ret) {
		dev_err(&pdev->dev, "failed to set pin direction\n");
		return -EINVAL;
	}

	/* IRQ setup */
	ret = of_irq_to_resource(np, 0, NULL);
	if (ret < 0) {
		ret = gpio_to_irq(data->gpio_pin);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to map GPIO to IRQ: %d\n", ret);
			return -EINVAL;
		}
	}
	data->irq = ret;

	/* initialize PPS specific parts of the bookkeeping data structure. */
	data->info.mode = PPS_CAPTUREASSERT | PPS_OFFSETASSERT |
		PPS_ECHOASSERT | PPS_CANWAIT | PPS_TSFMT_TSPEC;
	data->info.owner = THIS_MODULE;
	snprintf(data->info.name, PPS_MAX_NAME_LEN - 1, "%s", pdev->name);

	/* register PPS source */
	pps_default_params = PPS_CAPTUREASSERT | PPS_OFFSETASSERT;
	data->pps = pps_register_source(&data->info, pps_default_params);
	if (data->pps == NULL) {
		dev_err(&pdev->dev, "failed to register IRQ %d as PPS source\n",
			data->irq);
		return -EINVAL;
	}

	/* register IRQ interrupt handler */
	ret = request_irq(data->irq, atc_tod_irq_handler,
			0, "atc-linesync", data);
	if (ret) {
		pps_unregister_source(data->pps);
		misc_deregister(&data->miscdev);
		dev_err(&pdev->dev, "failed to acquire IRQ %d\n", data->irq);
		return -EINVAL;
	}

	platform_set_drvdata(pdev, data);
	dev_info(data->pps->dev, "Registered IRQ %d as PPS source\n",
		 data->irq);

	return 0;
}

static int atc_tod_remove(struct platform_device *pdev)
{
	struct atc_tod_data *data = platform_get_drvdata(pdev);

	free_irq(data->irq, data);
	pps_unregister_source(data->pps);
	misc_deregister(&data->miscdev);
	dev_info(&pdev->dev, "removed IRQ %d as PPS source\n", data->irq);
	return 0;
}

static const struct of_device_id atc_tod_dt_ids[] = {
	{ .compatible = "linux,atc-tod", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, atc_tod_dt_ids);

static struct platform_driver atc_tod_driver = {
	.probe		= atc_tod_probe,
	.remove		= atc_tod_remove,
	.driver		= {
		.name	= "atc_tod",
		.owner	= THIS_MODULE,
		.of_match_table	= atc_tod_dt_ids,
	},
};

module_platform_driver(atc_tod_driver);
MODULE_DESCRIPTION("ATC platform time-of-day handler");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

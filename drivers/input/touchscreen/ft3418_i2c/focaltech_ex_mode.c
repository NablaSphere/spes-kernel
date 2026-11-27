// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * FocalTech ftxxxx TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
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

/*****************************************************************************
 *
 * File Name: focaltech_ex_mode.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-08-31
 *
 * Abstract:
 *
 * Reference:
 *
 *****************************************************************************/

/*****************************************************************************
 * 1.Included header files
 *****************************************************************************/
#ifdef CONFIG_POWER_SUPPLY
#include <linux/power_supply.h>
#endif
#include "focaltech_core.h"

/*****************************************************************************
 * 2.Private constant and macro definitions using #define
 *****************************************************************************/
#define FTS_CHARGER_HW_UNKNOWN	(-1)

/*****************************************************************************
 * 3.Private enumerations, structures and unions using typedef
 *****************************************************************************/
enum _ex_mode {
	MODE_GLOVE = 0,
	MODE_COVER,
	MODE_CHARGER,
};

/*****************************************************************************
 * 4.Static variables
 *****************************************************************************/

/*****************************************************************************
 * 5.Global variable or extern global variabls/functions
 *****************************************************************************/

/*****************************************************************************
 * 6.Static function prototypes
 *******************************************************************************/
static inline bool fts_is_pm_suspended(const struct fts_ts_data *ts_data)
{
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	return READ_ONCE(ts_data->pm_suspend);
#else
	return false;
#endif
}

static int fts_ex_mode_switch(enum _ex_mode mode, u8 value)
{
	int ret = 0;
	u8 m_val = 0;

	if (value)
		m_val = 0x01;
	else
		m_val = 0x00;

	switch (mode) {
	case MODE_GLOVE:
		ret = fts_write_reg(FTS_REG_GLOVE_MODE_EN, m_val);
		if (ret < 0)
			FTS_ERROR("MODE_GLOVE switch to %d fail", m_val);
		break;
	case MODE_COVER:
		ret = fts_write_reg(FTS_REG_COVER_MODE_EN, m_val);
		if (ret < 0)
			FTS_ERROR("MODE_COVER switch to %d fail", m_val);
		break;
	case MODE_CHARGER:
		ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, m_val);
		if (ret < 0)
			FTS_ERROR("MODE_CHARGER switch to %d fail", m_val);
		break;
	default:
		FTS_ERROR("mode(%d) unsupport", mode);
		ret = -EINVAL;
		break;
	}

	return ret < 0 ? ret : 0;
}

static int fts_charger_apply_locked(struct fts_ts_data *ts_data)
{
	int target;
	int ret;

	if (ts_data->charger_suspended || READ_ONCE(ts_data->suspended) ||
	    fts_is_pm_suspended(ts_data) || READ_ONCE(ts_data->fw_loading)) {
		ts_data->charger_hw_state = FTS_CHARGER_HW_UNKNOWN;
		return 0;
	}

	target = ts_data->charger_mode ? 1 : 0;
	if (ts_data->charger_hw_state == target)
		return 0;

	ret = fts_ex_mode_switch(MODE_CHARGER, target ? ENABLE : DISABLE);
	if (ret < 0) {
		ts_data->charger_hw_state = FTS_CHARGER_HW_UNKNOWN;
		return ret;
	}

	ts_data->charger_hw_state = target;
	return 0;
}

#ifdef CONFIG_POWER_SUPPLY
static int fts_charger_read_usb_online(bool *online)
{
	struct power_supply *psy;
	union power_supply_propval val = { 0 };
	int ret;

	psy = power_supply_get_by_name("usb");
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
	power_supply_put(psy);
	if (ret < 0)
		return ret;

	*online = val.intval > 0;
	return 0;
}

static void fts_charger_work_func(struct work_struct *work)
{
	struct fts_ts_data *ts_data =
		container_of(work, struct fts_ts_data, charger_work);
	bool online = false;
	int ret;

	ret = fts_charger_read_usb_online(&online);
	if (ret < 0)
		return;

	mutex_lock(&ts_data->charger_lock);
	ts_data->charger_mode = online ? ENABLE : DISABLE;
	fts_charger_apply_locked(ts_data);
	mutex_unlock(&ts_data->charger_lock);
}

static int fts_psy_notifier_cb(struct notifier_block *nb,
			       unsigned long event, void *data)
{
	struct fts_ts_data *ts_data =
		container_of(nb, struct fts_ts_data, psy_notif);
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED || !psy || !psy->desc ||
	    !psy->desc->name)
		return NOTIFY_OK;

	if (!strcmp(psy->desc->name, "usb") && ts_data->ts_workqueue)
		queue_work(ts_data->ts_workqueue, &ts_data->charger_work);

	return NOTIFY_OK;
}
#endif

static ssize_t fts_glove_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	fts_read_reg(FTS_REG_GLOVE_MODE_EN, &val);
	count = snprintf(buf + count, PAGE_SIZE, "Glove Mode:%s\n",
			 ts_data->glove_mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Glove Reg(0xC0):%d\n", val);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_glove_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	if (FTS_SYSFS_ECHO_ON(buf)) {
		if (!ts_data->glove_mode) {
			FTS_DEBUG("enter glove mode");
			ret = fts_ex_mode_switch(MODE_GLOVE, ENABLE);
			if (!ret)
				ts_data->glove_mode = ENABLE;
		}
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		if (ts_data->glove_mode) {
			FTS_DEBUG("exit glove mode");
			ret = fts_ex_mode_switch(MODE_GLOVE, DISABLE);
			if (!ret)
				ts_data->glove_mode = DISABLE;
		}
	}

	FTS_DEBUG("glove mode: %d", ts_data->glove_mode);
	return count;
}

static ssize_t fts_cover_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	fts_read_reg(FTS_REG_COVER_MODE_EN, &val);
	count = snprintf(buf + count, PAGE_SIZE, "Cover Mode:%s\n",
			 ts_data->cover_mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Cover Reg(0xC1):%d\n", val);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_cover_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	if (FTS_SYSFS_ECHO_ON(buf)) {
		if (!ts_data->cover_mode) {
			FTS_DEBUG("enter cover mode");
			ret = fts_ex_mode_switch(MODE_COVER, ENABLE);
			if (!ret)
				ts_data->cover_mode = ENABLE;
		}
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		if (ts_data->cover_mode) {
			FTS_DEBUG("exit cover mode");
			ret = fts_ex_mode_switch(MODE_COVER, DISABLE);
			if (!ret)
				ts_data->cover_mode = DISABLE;
		}
	}

	FTS_DEBUG("cover mode: %d", ts_data->cover_mode);
	return count;
}

static ssize_t fts_charger_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	bool mode;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev;

	if (!ts_data || !ts_data->input_dev)
		return -ENODEV;

	input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	mutex_lock(&ts_data->charger_lock);
	mode = ts_data->charger_mode;
	if (!ts_data->charger_suspended && !READ_ONCE(ts_data->suspended) &&
	    !fts_is_pm_suspended(ts_data) && !READ_ONCE(ts_data->fw_loading))
		fts_read_reg(FTS_REG_CHARGER_MODE_EN, &val);
	mutex_unlock(&ts_data->charger_lock);

	count = snprintf(buf + count, PAGE_SIZE, "Charger Mode:%s\n",
			 mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE - count,
			  "Charger Reg(0x8B):%d\n", val);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_charger_mode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret = 0;
	bool enable;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	if (!ts_data)
		return -ENODEV;

	if (FTS_SYSFS_ECHO_ON(buf))
		enable = true;
	else if (FTS_SYSFS_ECHO_OFF(buf))
		enable = false;
	else
		return count;

	mutex_lock(&ts_data->charger_lock);
	ts_data->charger_mode = enable ? ENABLE : DISABLE;
	ret = fts_charger_apply_locked(ts_data);
	mutex_unlock(&ts_data->charger_lock);

	FTS_DEBUG("charger mode: %d", enable);
	return ret < 0 ? ret : count;
}

/* read and write charger mode
 * read example: cat fts_glove_mode  ---read  glove mode
 * write example:echo 1 > fts_glove_mode  ---write glove mode to 01
 */
static DEVICE_ATTR_RW(fts_glove_mode);
static DEVICE_ATTR_RW(fts_cover_mode);
static DEVICE_ATTR_RW(fts_charger_mode);

static struct attribute *fts_touch_mode_attrs[] = {
	&dev_attr_fts_glove_mode.attr,
	&dev_attr_fts_cover_mode.attr,
	&dev_attr_fts_charger_mode.attr,
	NULL,
};

static struct attribute_group fts_touch_mode_group = {
	.attrs = fts_touch_mode_attrs,
};

int fts_ex_mode_recovery(struct fts_ts_data *ts_data)
{
	int ret;

	if (ts_data->glove_mode)
		fts_ex_mode_switch(MODE_GLOVE, ENABLE);

	if (ts_data->cover_mode)
		fts_ex_mode_switch(MODE_COVER, ENABLE);

	mutex_lock(&ts_data->charger_lock);
	ts_data->charger_hw_state = FTS_CHARGER_HW_UNKNOWN;
	ret = fts_ex_mode_switch(MODE_CHARGER,
				 ts_data->charger_mode ? ENABLE : DISABLE);
	if (ret == 0)
		ts_data->charger_hw_state = ts_data->charger_mode ? 1 : 0;
	mutex_unlock(&ts_data->charger_lock);

	return 0;
}

void fts_ex_mode_suspend(struct fts_ts_data *ts_data)
{
	mutex_lock(&ts_data->charger_lock);
	ts_data->charger_suspended = true;
	ts_data->charger_hw_state = FTS_CHARGER_HW_UNKNOWN;
	mutex_unlock(&ts_data->charger_lock);
}

void fts_ex_mode_resume(struct fts_ts_data *ts_data)
{
	mutex_lock(&ts_data->charger_lock);
	ts_data->charger_suspended = false;
	fts_charger_apply_locked(ts_data);
	mutex_unlock(&ts_data->charger_lock);
}

void fts_ex_mode_pm_suspend(struct fts_ts_data *ts_data)
{
	mutex_lock(&ts_data->charger_lock);
	mutex_unlock(&ts_data->charger_lock);
}

void fts_ex_mode_pm_resume(struct fts_ts_data *ts_data)
{
	mutex_lock(&ts_data->charger_lock);
	fts_charger_apply_locked(ts_data);
	mutex_unlock(&ts_data->charger_lock);
}

void fts_ex_mode_fwupg_start(struct fts_ts_data *ts_data)
{
	mutex_lock(&ts_data->charger_lock);
	ts_data->charger_hw_state = FTS_CHARGER_HW_UNKNOWN;
	mutex_unlock(&ts_data->charger_lock);
}

void fts_ex_mode_fwupg_end(struct fts_ts_data *ts_data)
{
	mutex_lock(&ts_data->charger_lock);
	ts_data->charger_hw_state = FTS_CHARGER_HW_UNKNOWN;
	fts_charger_apply_locked(ts_data);
	mutex_unlock(&ts_data->charger_lock);
}

int fts_ex_mode_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	ts_data->glove_mode = DISABLE;
	ts_data->cover_mode = DISABLE;
	ts_data->charger_mode = DISABLE;
	ts_data->charger_hw_state = FTS_CHARGER_HW_UNKNOWN;
	ts_data->charger_suspended = false;
#ifdef CONFIG_POWER_SUPPLY
	ts_data->psy_notif_registered = false;
	INIT_WORK(&ts_data->charger_work, fts_charger_work_func);
#endif

	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_touch_mode_group);
	if (ret < 0) {
		FTS_ERROR("create sysfs(ex_mode) fail");
		return ret;
	}

#ifdef CONFIG_POWER_SUPPLY
	ts_data->psy_notif.notifier_call = fts_psy_notifier_cb;
	ret = power_supply_reg_notifier(&ts_data->psy_notif);
	if (ret < 0) {
		FTS_ERROR("register power_supply notifier fail: %d", ret);
		sysfs_remove_group(&ts_data->dev->kobj, &fts_touch_mode_group);
		return ret;
	}
	ts_data->psy_notif_registered = true;

	if (ts_data->ts_workqueue)
		queue_work(ts_data->ts_workqueue, &ts_data->charger_work);
#endif

	return 0;
}

int fts_ex_mode_exit(struct fts_ts_data *ts_data)
{
#ifdef CONFIG_POWER_SUPPLY
	if (ts_data->psy_notif_registered) {
		power_supply_unreg_notifier(&ts_data->psy_notif);
		ts_data->psy_notif_registered = false;
	}
	cancel_work_sync(&ts_data->charger_work);
#endif
	sysfs_remove_group(&ts_data->dev->kobj, &fts_touch_mode_group);
	return 0;
}

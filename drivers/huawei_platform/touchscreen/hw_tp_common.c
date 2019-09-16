/*Add for huawei TP*/
/*
 * Copyright (c) 2014 Huawei Device Company
 *
 * This file provide common requeirment for different touch IC.
 * 
 * 2014-01-04:Add "tp_get_touch_screen_obj" by sunlibin
 *
 */
#include <linux/module.h>
#include <huawei_platform/touchscreen/hw_tp_common.h>
#include <linux/atomic.h>
/*echo debug_level > sys/module/hw_tp_common/paramers/hw_tp_common_debug_mask */
int hw_tp_common_debug_mask = TP_INFO;
module_param_named(hw_tp_common_debug_mask, hw_tp_common_debug_mask, int, 0664);
bool pt_test_enable_tp = 0;
module_param_named(pt_test_enable_tp, pt_test_enable_tp, bool, 0664);
static int g_tp_type = UNKNOW_PRODUCT_MODULE;
static struct kobject *touch_screen_kobject_ts = NULL;
static struct kobject *touch_glove_func_ts = NULL;
struct kobject *virtual_key_kobject_ts = NULL;
/*init atomic touch_detected_flag*/
atomic_t touch_detected_flag = ATOMIC_INIT(0);

/**
 * set_touch_probe_flag - to set the touch_detected_flag
 *
 * @detected: the value of touch_detected_flag,if detected is 1, set touch_detected_flag to 1;
 *                  if detected is 0, set touch_detected_flag to 0.
 *
 * This function to set the touch_detected_flag,notice it is atomic.
 *
 * This function have not returned value.
 */
static void set_touch_probe_flag(int detected)
{
	if(detected >= 0)
	{
		atomic_set(&touch_detected_flag, 1);
	}
	else
	{
		atomic_set(&touch_detected_flag, 0);
	}

	return;
}

/**
 * read_touch_probe_flag - to read the touch_detected_flag
 *
 * @no input value
 *
 * This function to read the touch_detected_flag,notice it is atomic.
 *
 * The atomic of touch_detected_flag will be returned.
 */
static int read_touch_probe_flag(void)
{
	return atomic_read(&touch_detected_flag);
}

/*It is a whole value inoder to set of read touch_detected_flag status*/
struct touch_hw_platform_data touch_hw_data =
{
	.set_touch_probe_flag = set_touch_probe_flag,
	.read_touch_probe_flag = read_touch_probe_flag,
};

/**
 * tp_get_touch_screen_obj - it is a common function,tp can call it to creat /sys/touch_screen file node
 *
 * @no input value
 *
 * This function is tp call it to creat /sys/touch_screen file node.
 *
 * The kobject of touch_screen_kobject_ts will be returned,notice it is static.
 */
struct kobject* tp_get_touch_screen_obj(void)
{
	if( NULL == touch_screen_kobject_ts )
	{
		touch_screen_kobject_ts = kobject_create_and_add("touch_screen", NULL);
		if (!touch_screen_kobject_ts)
		{
			tp_log_err("%s: create touch_screen kobjetct error!\n", __func__);
			return NULL;
		}
		else
		{
			tp_log_debug("%s: create sys/touch_screen successful!\n", __func__);
		}
	}
	else
	{
		tp_log_debug("%s: sys/touch_screen already exist!\n", __func__);
	}

	return touch_screen_kobject_ts;
}
/**
 * tp_get_virtual_key_obj - it is a common function,tp can call it to creat virtual_key file node in /sys/
 *
 * @no input value
 *
 * This function is tp call it to creat virtual_key file node in /sys/
 *
 * The kobject of virtual_key_kobject_ts will be returned
 */
struct kobject* tp_get_virtual_key_obj(char *name)
{
	if( NULL == virtual_key_kobject_ts )
	{
		virtual_key_kobject_ts = kobject_create_and_add(name, NULL);
		if (!virtual_key_kobject_ts)
		{
			tp_log_err("%s: create virtual_key kobjetct error!\n", __func__);
			return NULL;
		}
		else
		{
			tp_log_debug("%s: create virtual_key successful!\n", __func__);
		}
	}
	else
	{
		tp_log_debug("%s: virtual_key already exist!\n", __func__);
	}

	return virtual_key_kobject_ts;
}
/**
 * tp_get_glove_func_obj - it is a common function,tp can call it to creat glove_func file node in /sys/touch_screen/
 *
 * @no input value
 *
 * This function is tp call it to creat glove_func file node in /sys/touch_screen/.
 *
 * The kobject of touch_glove_func_ts will be returned,notice it is static.
 */
struct kobject* tp_get_glove_func_obj(void)
{
	struct kobject *properties_kobj;
	
	properties_kobj = tp_get_touch_screen_obj();
	if( NULL == properties_kobj )
	{
		tp_log_err("%s: Error, get kobj failed!\n", __func__);
		return NULL;
	}
	
	if( NULL == touch_glove_func_ts )
	{
		touch_glove_func_ts = kobject_create_and_add("glove_func", properties_kobj);
		if (!touch_glove_func_ts)
		{
			tp_log_err("%s: create glove_func kobjetct error!\n", __func__);
			return NULL;
		}
		else
		{
			tp_log_debug("%s: create sys/touch_screen/glove_func successful!\n", __func__);
		}
	}
	else
	{
		tp_log_debug("%s: sys/touch_screen/glove_func already exist!\n", __func__);
	}

	return touch_glove_func_ts;
}
/*add api func for sensor to get TP module*/
/*tp type define in enum f54_product_module_name*/
/*tp type store in global variable g_tp_type */
/*get_tp_type:sensor use to get tp type*/
int get_tp_type(void)
{
	return g_tp_type;
}
/*set_tp_type:drivers use to set tp type*/
void set_tp_type(int type)
{
	g_tp_type = type;
	tp_log_err("%s:tp_type=%d\n",__func__,type);
}
#define SIM_DSI_ID "dsi:0:"
#define WORD_PARTITION_SYMBOL "_"

#define MDSS_MAX_PANEL_LEN      256
#define PARTITION_SYMBOL_COUNT_START 2
#define PARTITION_SYMBOL_COUNT_WORDS 2

static char g_lcd_panel_info[MDSS_MAX_PANEL_LEN] = {0};

static int __init early_lcd_panel_info_cmdline(char *arg)
{
	strncpy(g_lcd_panel_info, arg, MDSS_MAX_PANEL_LEN);
	return 0;
}

early_param("mdss_mdp.panel", early_lcd_panel_info_cmdline);

/*
 * description : find #tag in #src, if find success,
                 return the end address of #tag in #src
 * parameter - src : char array to search
 * parameter - tag : target char array to search
 * parameter - src_size : the size of src
 * return : if success, return the end address of #tag in #src, else return NULL
 */
char * str_find_and_skip(char * src, char * tag, unsigned int src_size)
{
	char * result_str = NULL;

	result_str = strnstr(src, tag, src_size);
	if (NULL == result_str) {
		return NULL;
	}

	result_str += strlen(tag);
	return result_str;
}

/*
 * description : get lcd panel name from cmdline
 * parameter - lcd_panel_name : buffer to receive lcd panel name
 * parameter - buf_size       : #lcd_panel_name size
 * return : if success, return 0, else return error code
 */
int get_lcd_panel_name(char *lcd_panel_name, unsigned int buf_size)
{
	int i = 0;
	int panel_name_length = 0;
	char* panel_name_start = NULL;
	char* panel_name_end   = NULL;

	if (NULL == lcd_panel_name || NULL == g_lcd_panel_info) {
		tp_log_err("%s[%d]:output buffer is null!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	/*
	 * skip #SIM_DSI_ID, SIM_DSI_ID is define in file
	 * bootable/bootloader/lk/dev/gcdb/display/include/display_resource.h
	 */
	panel_name_start =
		str_find_and_skip(g_lcd_panel_info, SIM_DSI_ID, MDSS_MAX_PANEL_LEN);

	if (!panel_name_start) {
		tp_log_err("%s[%d]:panel name start flag search error, src=%s, tag=%s\n",
			__func__, __LINE__, g_lcd_panel_info, SIM_DSI_ID);
		return -EINVAL;
	}

	/*
	 * find the start address of panel name
	 * typical #panel_name_start is : qcom,mdss_dsi_auo_nt35521_6p0_720p_video
	 * so we should skip two '_' to get get panel name start index
	 */
	for (i = 0; i < PARTITION_SYMBOL_COUNT_START; i++) {

		panel_name_start = str_find_and_skip(panel_name_start,
			WORD_PARTITION_SYMBOL, MDSS_MAX_PANEL_LEN);

		if (!panel_name_start) {
			tp_log_err("%s[%d]:panel name start flag search error, src=%s, tag=%s\n",
				__func__, __LINE__, panel_name_start, WORD_PARTITION_SYMBOL);
			return -EINVAL;
		}
	}

	/*
	 * find the end address of panel name
	 * typical #panel_name_start is : auo_nt35521_6p0_720p_video
	 * so we should skip '_' to get get panel name start index
	 */
	panel_name_end = str_find_and_skip(panel_name_start,
		WORD_PARTITION_SYMBOL, MDSS_MAX_PANEL_LEN);

	if (!panel_name_end) {
		tp_log_err("%s[%d]:panel name end flag search error, src=%s, tag=%s\n",
				__func__, __LINE__, panel_name_start, WORD_PARTITION_SYMBOL);
		return -EINVAL;
	}

	panel_name_end =
		strnstr(panel_name_end, WORD_PARTITION_SYMBOL, MDSS_MAX_PANEL_LEN);
	if (!panel_name_end) {
		tp_log_err("%s[%d]:panel name end flag search error, src=%s, tag=%s\n",
				__func__, __LINE__, panel_name_end, WORD_PARTITION_SYMBOL);
		return -EINVAL;
	}

	panel_name_length = panel_name_end - panel_name_start;
	if (panel_name_length > buf_size - 1) {
		panel_name_length = buf_size - 1;
	}

	/* copy panel name to #panel_name, and set the last char to 0x00 */
	memcpy(lcd_panel_name, panel_name_start, panel_name_length);
	lcd_panel_name[panel_name_length] = '\0';

	return 0;
}

char * get_lcd_panel_info(void)
{
	return g_lcd_panel_info;
}

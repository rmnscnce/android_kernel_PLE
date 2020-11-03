/**************************************************************************************************/
/** 
	@file		TunerDev.c
	@brief		Tuner Device Control
*/
/**************************************************************************************************/

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/err.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <asm/uaccess.h> /* for access_ok() */
#include <linux/regulator/consumer.h> /* for regulator_xx() */
#include <linux/qpnp/pin.h>

#include <asm/io.h>
//#include <sharp/sh_smem.h>
#include "gpio_def.h"

#ifdef CONFIG_SHARP_SHTERM
#include "misc/shterm_k.h"
#endif

static int gpio_init(void);
static int gpio_get(unsigned int id, int *val);
static int gpio_set(unsigned int id, int val);
static int tuner_vreg_enable(void);
static int tuner_vreg_disable(void);
static int tuner_clk_enable(void);
static int tuner_clk_disable(void);
static int hw_revision_get(unsigned int *val);
static int tuner_perf_lock(void);
static int tuner_perf_unlock(void);
static int tuner_vreg12v_enable(void);
static int tuner_vreg12v_disable(void);

#ifdef DTV_DRV_SDM660
static int tuner_init_spi(void);
static void tuner_cleanup_spi(void);
static int gpio_init_spi(void);
#endif

//static struct clk *gp_clk;

stGPIO_DEF use_gpiono[] = {
	/* GPIO No (ID)			, GPIO PORT No			, Direction		, Init Value	, Initialized	, diag-label	*/
#if VALID_GPIO_DTVEN
	{GPIO_DTVEN_PORTID		, GPIO_DTVEN_PORTNO		, DirctionOut	, 0				, 0				, "qcom,dtv-en-gpio"	},
#endif
#if VALID_GPIO_DTVRST
	{GPIO_DTVRST_PORTID 	, GPIO_DTVRST_PORTNO	, DirctionOut	, 0				, 0				, "qcom,dtv-rst-gpio"	},
#endif
#if VALID_GPIO_DTVLNAEN
	{GPIO_DTVLNAEN_PORTID	, GPIO_DTVLNAEN_PORTNO	, DirctionOut	, 0				, 0				, "define if you need"	},
#endif
#if VALID_GPIO_DTVANTSW
	{GPIO_DTVANTSW_PORTID	, GPIO_DTVANTSW_PORTNO	, DirctionOut	, 0				, 0				, "define if you need"	},
#endif
#if VALID_GPIO_DTVMANTSL
	{GPIO_DTVMANTSL_PORTID	, GPIO_DTVMANTSL_PORTNO	, DirctionOut	, 0				, 0				, "define if you need"	},
#endif
#if VALID_GPIO_DTVUANTSL
	{GPIO_DTVUANTSL_PORTID	, GPIO_DTVUANTSL_PORTNO	, DirctionOut	, 0				, 0				, "define if you need"	},
#endif
#if VALID_GPIO_DTVCANTSL
	{GPIO_DTVCANTSL_PORTID	, GPIO_DTVCANTSL_PORTNO	, DirctionOut	, 0				, 0				, "define if you need"	},
#endif
#if VALID_GPIO_DTVHWREV
	{GPIO_DTVHWREV_PORTID	, GPIO_DTVHWREV_PORTNO	, DirctionIn	, 0				, 0				, "gpio_dtv_hwrev"	},
#endif
};

#ifdef DTV_DRV_SDM660
stGPIO_DEF_spi use_gpiono_spi[] = {
	/* GPIO PORT No			, Direction		, Init Value	, Initialized	, diag-label	*/
	{GPIO_DTVSPIMOSI_PORTNO	, DirctionOut	, 0				, 0				, "qcom,spi-mosi-gpio" },
	{GPIO_DTVSPIMISO_PORTNO	, DirctionIn	, 0				, 0				, "qcom,spi-miso-gpio" },
	{GPIO_DTVSPICSN_PORTNO	, DirctionOut	, 1				, 0				, "qcom,spi-csn-gpio" },
	{GPIO_DTVSPICLK_PORTNO	, DirctionOut	, 1				, 0				, "qcom,spi-clk-gpio" },
};
#endif

#ifdef DTV_DRV_PINCTRL
struct device_node *np;
#endif

#if 1
static struct regulator *reg;
static const char *reg_id = "pm8937_l15";
static const int min_uV = 2800000;
static const int max_uV = 2800000;
#endif

/**************************************************************************************************/
/**
	@brief	tuner_open
	@param	struct inode	*inode		[I]
	@param	struct file		*file		[I]
	@retval	0	Success
	@retval	-1	Failed
*/
/**************************************************************************************************/
static int tuner_open(struct inode *inode, struct file *file)
{
	int ret;

	ret  = gpio_init();

	if (ret == 1) {
		/* Failed */
		printk("%s:%d !!!tuner_open gpio_init() error \n", __FILE__, __LINE__);
		return (-1);
	}
	return (0);
}

/**************************************************************************************************/
/**
	@brief	tuner_release
	@param	struct inode	*inode		[I]
	@param	struct file		*file		[I]
	@retval	0	Success
	@retval	-1	Failed
*/
/**************************************************************************************************/
static int tuner_release(struct inode *inode, struct file *file)
{
	return (0);
}

/**************************************************************************************************/
/**
	@brief	tuner_release
	@param	struct file		*file	[I]
	@param	unsigned int	cmd		[I]
	@param	unsigned long	arg		[I]
	@retval	0		Suncess
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static long tuner_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	ioctl_cmd io_cmd_work;
	ioctl_cmd *io_cmd = &io_cmd_work;

	if (copy_from_user(io_cmd, (ioctl_cmd*)arg, sizeof(ioctl_cmd))) {
		printk("%s:%d  !!!tuner_ioctl copy_from_user error %08x\n", __FILE__, __LINE__, (unsigned int)arg);
		return -EFAULT;
	}

	switch ( cmd ) {
	case IOC_GPIO_VAL_SET:
		ret = gpio_set(io_cmd->id, io_cmd->val);
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[cmd %x val %d][ret:%d]\n", __FILE__, __LINE__, io_cmd->id, io_cmd->val, ret);
			return -EINVAL;
		}
		break;
	case IOC_GPIO_VAL_GET:
		ret = gpio_get(io_cmd->id, &(io_cmd->val));
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl get error[cmd %x val %d][ret:%d]\n", __FILE__, __LINE__, io_cmd->id, io_cmd->val, ret);
			return -EINVAL;
		}
		if (copy_to_user((ioctl_cmd*)arg, io_cmd, sizeof(ioctl_cmd))) {
			printk("%s:%d  !!!tuner_ioctl copy_to_user error %08x\n", __FILE__, __LINE__, (unsigned int)arg);
			return -EFAULT;
		}
		break;
	case IOC_VREG_ENABLE:
		ret = tuner_vreg_enable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_VREG_DISABLE:
		ret = tuner_vreg_disable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_CLK_ENABLE:
		ret = tuner_clk_enable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_CLK_DISABLE:
		ret = tuner_clk_disable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_HW_REV_GET:
		ret = hw_revision_get(&(io_cmd->val));
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl get error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		if (copy_to_user((ioctl_cmd*)arg, io_cmd, sizeof(ioctl_cmd))) {
			printk("%s:%d  !!!tuner_ioctl copy_to_user error %08x\n", __FILE__, __LINE__, (unsigned int)arg);
			return -EFAULT;
		}
		break;
	case IOC_PERF_LOCK:
		ret = tuner_perf_lock();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl perf_lock error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_PERF_UNLOCK:
		ret = tuner_perf_unlock();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl perf_unlock error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_VREG12V_ENABLE:
		ret = tuner_vreg12v_enable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_VREG12V_DISABLE:
		ret = tuner_vreg12v_disable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	default:
		/* NOTE:  returning a fault code here could cause trouble
		 * in buggy userspace code.  Some old kernel bugs returned
		 * zero in this case, and userspace code might accidentally
		 * have depended on that bug.
		 */
		return -ENOTTY;
	}
	return 0;
}

static struct file_operations tuner_fops = {
	.owner				= THIS_MODULE,
	.compat_ioctl		= tuner_ioctl,
	.unlocked_ioctl		= tuner_ioctl,
	.open				= tuner_open,
	.release			= tuner_release,
};

static struct miscdevice tuner_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tunctrl",
	.fops = &tuner_fops,
};

/**************************************************************************************************/
/**
	@brief	tuner_init
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int __init tuner_init(void)
{
	int ret, i ;
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
#ifdef DTV_DRV_PINCTRL
	int portno;
#else
	unsigned int gpio_no;
#endif

	ret = misc_register(&tuner_dev);
	if (ret) {
		printk("%s.%s.%d !!! fail to misc_register (MISC_DYNAMIC_MINOR)\n", __FILE__, __func__, __LINE__);
		return ret;
	}

#ifdef DTV_DRV_PINCTRL
	np = of_find_node_by_name( NULL, "sharp,tunctrl" );
	if ( np ) {
		for ( i=0; i<loop; i++ ) {
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
			ret = gpio_request( portno, NULL );

			if ( ret < 0 ) {
				printk(KERN_DEBUG "%s gpio_request() error : %d\n", use_gpiono[i].label, ret);
			}
		}
	}
	else {
		printk(KERN_DEBUG "of_find_node_by_name() error \n");
#if 1
		misc_deregister(&tuner_dev);
#endif
		return -1;
	}
#else
	for(i=0; i<loop; i++){
		/* PM8952_GPIO */
		if ( use_gpiono[i].no > PM8952_GPIO_BASE ) {
			gpio_no = use_gpiono[i].no - PM8952_GPIO_BASE;
			ret = gpio_request( qpnp_pin_map("pm8950-gpio", gpio_no),  use_gpiono[i].label );
		}
		/* others */
		else {
			ret = gpio_request(use_gpiono[i].no , use_gpiono[i].label);
		}
		if(ret < 0){
			printk(KERN_DEBUG "%s gpio_request() error : %d\n", use_gpiono[i].label, ret);
		}
	}
#endif

#if 1
	reg = regulator_get(tuner_dev.this_device, reg_id);
	if (IS_ERR(reg)) {
		printk("Unable to get %s regulator\n", reg_id);
		misc_deregister(&tuner_dev);
		return -1;
	}

	ret = regulator_set_voltage(reg, min_uV, max_uV);
	if (ret != 0) {
		printk("Unable to set %s regulator voltage[ret:%d]\n", reg_id, ret);
		regulator_put(reg);
		misc_deregister(&tuner_dev);
		return ret;
	}
#endif

	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_cleanup
	@param	none
	@retval	none
*/
/**************************************************************************************************/
static void __exit tuner_cleanup(void)
{
	int i ;
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
#ifdef DTV_DRV_PINCTRL
	int portno;
#else
	unsigned int gpio_no;
#endif

#if 1
	regulator_put(reg);
#endif

#ifdef DTV_DRV_PINCTRL
	np = of_find_node_by_name( NULL, "sharp,tunctrl" );
	if ( np ) {
		for ( i=0; i<loop; i++ ) {
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
			gpio_free( portno );
		}
	}
	else {
		printk(KERN_DEBUG "of_find_node_by_name() error \n");
	}
#else
	for(i=0; i<loop; i++){
		/* PM8952_GPIO */
		if ( use_gpiono[i].no > PM8952_GPIO_BASE ) {
			gpio_no = use_gpiono[i].no - PM8952_GPIO_BASE;
			gpio_free( qpnp_pin_map("pm8950-gpio", gpio_no) );
		}
		/* others */
		else {
			gpio_free(use_gpiono[i].no);
		}
	}
#endif

	misc_deregister(&tuner_dev);
}

/**************************************************************************************************/
/**
	@brief	gpio_init
	@param	none
	@retval	0	Success
	@retval	1	Failed
*/
/**************************************************************************************************/
static int gpio_init(void)
{
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	int errcnt = 0;
	stGPIO_DEF *p = &use_gpiono[0];
	int i;
#ifdef DTV_DRV_PINCTRL
	int portno;
#else
	unsigned int gpio_no;
#endif
	
	for (i=0; i<loop; i++, p++) {
		if (p->direction == DirctionIn) {
			/* GPIO Input */
#ifdef DTV_DRV_PINCTRL
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
		//	printk(KERN_DEBUG "of_get_named_gpio() -- gpio_direction_input [%d][%s] \n", portno, use_gpiono[i].label);
			if ( gpio_direction_input( portno ) < 0 ) {
				errcnt ++;
				printk( "%s:%d gpio_direction_input error %s \n", __FILE__,__LINE__, use_gpiono[i].label );
				continue;
			}
#else
			if ( p->no > PM8952_GPIO_BASE ) {
				/* PM8952_GPIO */
				gpio_no = p->no - PM8952_GPIO_BASE;

				if (gpio_direction_input( qpnp_pin_map("pm8950-gpio", gpio_no) ) < 0) {
					/* Failed */
					errcnt ++;
					printk( "%s:%d gpio_direction_input error NO.%d \n", __FILE__,__LINE__, p->no);
					continue;
				}
			}
			else {
				if (gpio_direction_input(p->no) < 0) {
					/* Failed */
					errcnt ++;
					printk( "%s:%d gpio_direction_input error NO.%d \n", __FILE__,__LINE__, p->no);
					continue;
				}
			}
#endif
			p->init_done = 1;
			continue;
		}
		if (p->direction == DirctionOut) {
			/* GPIO Output */
#ifdef DTV_DRV_PINCTRL
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
		//	printk(KERN_DEBUG "of_get_named_gpio() -- direction_output [%d][%s] \n", portno, use_gpiono[i].label);
			if ( gpio_direction_output( portno, use_gpiono[i].out_val ) < 0 ) {
				errcnt ++;
				printk( "%s:%d gpio_direction_output error %s \n", __FILE__,__LINE__, use_gpiono[i].label );
				continue;
			}

#else
			if ( p->no > PM8952_GPIO_BASE ) {
				/* PM8952_GPIO */
				gpio_no = p->no - PM8952_GPIO_BASE;

				if (gpio_direction_output( qpnp_pin_map("pm8950-gpio", gpio_no), p->out_val ) < 0) {
					/* Failed */
					errcnt ++;
					printk("%s: gpio_direction_output error NO.%d \n", __FILE__, p->no);
					continue;
				}
			}
			else {
				if (gpio_direction_output(p->no, p->out_val) < 0) {
					/* Failed */
					errcnt ++;
					printk("%s: gpio_direction_output error NO.%d \n", __FILE__, p->no);
					continue;
				}
			}
#endif
			p->init_done = 1;
			continue;
		}
	}

	if (errcnt != 0) {
		printk("%s: gpio_init error count %d\n", __FILE__, errcnt);
		return 1;
	}
	return 0;
}

static int tuner_clk_enable(void)
{
#if 0	/* NOT necessary when it's already been initialized. */
	unsigned 	gpio_cfg;
	int			ret;

//	gpio_cfg = GPIO_CFG( CLOCK_CONTROL_PORTNO, 5, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_6MA );	/* gp_clk_2b */
	gpio_cfg = GPIO_CFG( CLOCK_CONTROL_PORTNO, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_6MA );	/* gp_clk_2a */

	ret = gpio_tlmm_config( gpio_cfg, GPIO_CFG_ENABLE );
	if ( ret < 0 ) {
		printk("%s: gpio_cfg error %d\n", __FILE__, ret);
		return 1;
	}

	gp_clk = clk_get( NULL, "gp2_clk" );
	clk_set_rate( gp_clk, 19200000 );
	clk_prepare( gp_clk );
	clk_enable( gp_clk );

//	writel(0xA00, MSM_CLK_CTL_BASE + 0x2D24 + 32*( 2 ));		/* gp_clk_2b */

#endif
	return 0;
}

static int tuner_clk_disable(void)
{
#if 0
	clk_disable( gp_clk );
	clk_unprepare( gp_clk );

//	writel(0x0000, MSM_CLK_CTL_BASE + 0x2D24 + 32*( 2 ));		/* gp_clk_2b */

#endif
	return 0;
}
	

/**************************************************************************************************/
/**
	@brief	gpio_set
	@param	unsigned int	id			[I]	GPIO No
	@param	int				value		[I]	GPIO Value
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int gpio_set(unsigned int id, int value)
{
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	stGPIO_DEF *p = use_gpiono;
	int flag = 0;
	int i;
	int portno ;
#ifndef DTV_DRV_PINCTRL
	unsigned int gpio_no;
#endif

	for(i=0; i<loop; i++, p++){
		if (p->id == id){
			flag = 1;

#ifdef DTV_DRV_PINCTRL
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
#else
			if ( p->no > PM8952_GPIO_BASE ) {
				/* PM8952_GPIO */
				gpio_no = p->no - PM8952_GPIO_BASE;
				portno = qpnp_pin_map("pm8950-gpio", gpio_no);
			}
			else {
				portno = p->no ;
			}
#endif
			break;
		}
	}
	if (flag == 0) {
		printk("%s: !!! gpio_set() error No.%d value %d \n", __FILE__, id, value);
		return EINVAL;
	}
	gpio_set_value_cansleep(portno, value);
	return 0;
}

/**************************************************************************************************/
/**
	@brief	gpio_get
	@param	unsigned int	id			[I]	GPIO No
	@param	int				*val		[I]	GPIO Value
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int gpio_get(unsigned int id, int *val)
{
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	stGPIO_DEF *p = &use_gpiono[0];
	int flag = 0;
	int i;
	int portno ;
#ifndef DTV_DRV_PINCTRL
	unsigned int gpio_no;
#endif

	*val = 0;

	for(i=0; i<loop; i++, p++){
		if (p->id == id){
			flag = 1;

#ifdef DTV_DRV_PINCTRL
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
			printk(KERN_DEBUG "of_get_named_gpio() [%d][%s] \n", portno, use_gpiono[i].label);
#else
			if ( p->no > PM8952_GPIO_BASE ) {
				/* PM8952_GPIO */
				gpio_no = p->no - PM8952_GPIO_BASE;
				portno = qpnp_pin_map("pm8950-gpio", gpio_no);
			}
			else {
				portno = p->no ;
			}
#endif
			break;
		}
	}
	if (flag == 0) {
		printk("%s: !!! gpio_get() No.%d error \n", __FILE__, id);
		return EINVAL;
	}
	*val = gpio_get_value_cansleep(portno);

	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_vreg_enable
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_vreg_enable(void)
{

	int	ret;
#if 0
	struct regulator *reg;
	struct device *dev = tuner_dev.this_device;
	const char *id = "pm8950_l11";
	int min_uV = 2950000, max_uV = 2950000;
#endif

#if 0
	reg= regulator_get(dev, id);
	if (IS_ERR(reg)) {
		printk("Unable to get %s regulator\n", id);
		return -1;
	}

    regulator_set_voltage(reg, min_uV, max_uV);

	if (!regulator_is_enabled(reg)) {
		ret = regulator_enable(reg);
	}

	regulator_put(reg);
#else
	ret = regulator_enable(reg);
	if (ret != 0) {
		printk("Unable to enable %s regulator[ret:%d]\n", reg_id, ret);
		return ret;
	}
#endif

#ifdef DTV_DRV_SDM660
	ret = tuner_init_spi();
	if (ret != 0) {
		printk("%s:%d  !!!tuner_vreg_enable tuner_init_spi() error[ret:%d]\n", __FILE__, __LINE__, ret);
		return -1;
	}

	ret = gpio_init_spi();
	if (ret != 0) {
		printk("%s:%d  !!!tuner_vreg_enable gpio_init_spi() error[ret:%d]\n", __FILE__, __LINE__, ret);
		return -1;
	}
#endif

#ifdef CONFIG_SHARP_SHTERM
	shterm_k_set_info( SHTERM_INFO_DTB, 1 );
#endif

	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_vreg_disable
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_vreg_disable(void)
{
	int	ret;
#if 0
	struct regulator *reg;
	struct device *dev = tuner_dev.this_device;
	const char *id = "pm8950_l11";
#endif

#ifdef CONFIG_SHARP_SHTERM
	shterm_k_set_info( SHTERM_INFO_DTB, 0 );
#endif

#ifdef DTV_DRV_SDM660
	tuner_cleanup_spi();
#endif

#if 0
	reg= regulator_get(dev, id);
	if (IS_ERR(reg)) {
		printk("Unable to get %s regulator\n", id);
		return -1;
	}

	if (regulator_is_enabled(reg)) {
		ret = regulator_disable(reg);
	}

	regulator_put(reg);
#else
	ret = regulator_disable(reg);
	if (ret != 0) {
		printk("Unable to disable %s regulator[ret:%d]\n", reg_id, ret);
		return ret;
	}
#endif

	return 0;
}

/**************************************************************************************************/
/**
	@brief	hw_revision_get
	@param	int		   *val		[I]	sh_hw_revision value
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int hw_revision_get(unsigned int *val)
{
#if 0
	sharp_smem_common_type *sh_smem_common = NULL;

	sh_smem_common = (sharp_smem_common_type *)sh_smem_get_common_address();
	if (sh_smem_common != NULL) {
		*val = (unsigned int)sh_smem_common->sh_hw_revision;
	}
	else{
		return -1;
	}
#endif
	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_perf_lock
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_perf_lock(void)
{
	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_perf_unlock
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_perf_unlock(void)
{
	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_vreg12v_enable
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_vreg12v_enable(void)
{
#if 0
	int	ret;
	struct regulator *reg;
	struct device *dev = tuner_dev.this_device;
	const char *id = "8916_s3";
	int min_uV = 1200000, max_uV = 1200000;

	reg= regulator_get(dev, id);
	if (IS_ERR(reg)) {
		printk("Unable to get %s regulator\n", id);
		return -1;
	}

    regulator_set_voltage(reg, min_uV, max_uV);

	if (!regulator_is_enabled(reg)) {
		ret = regulator_enable(reg);
	}

	regulator_put(reg);
#endif
	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_vreg12v_disable
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_vreg12v_disable(void)
{
#if 0
	int	ret;
	struct regulator *reg;
	struct device *dev = tuner_dev.this_device;
	const char *id = "8916_s3";

	reg= regulator_get(dev, id);
	if (IS_ERR(reg)) {
		printk("Unable to get %s regulator\n", id);
		return -1;
	}

	if (regulator_is_enabled(reg)) {
		ret = regulator_disable(reg);
	}

	regulator_put(reg);
#endif
	return 0;
}

#ifdef DTV_DRV_SDM660
/**************************************************************************************************/
/**
	@brief	tuner_init_spi
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_init_spi(void)
{
	int ret, i ;
	int loop = sizeof(use_gpiono_spi)/sizeof(stGPIO_DEF_spi);
#ifdef DTV_DRV_PINCTRL
	int portno;
#else
	unsigned int gpio_no;
#endif

#ifdef DTV_DRV_PINCTRL
	np = of_find_node_by_name( NULL, "sharp,tunctrl" );
	if ( np ) {
		for ( i=0; i<loop; i++ ) {
			portno = of_get_named_gpio( np, use_gpiono_spi[i].label ,0 );
			ret = gpio_request( portno, NULL );

			if ( ret < 0 ) {
				printk(KERN_DEBUG "%s gpio_request() error : %d\n", use_gpiono_spi[i].label, ret);
			}
		}
	}
	else {
		printk(KERN_DEBUG "of_find_node_by_name() error \n");
		return -1;
	}
#else
	for(i=0; i<loop; i++){
		/* PM8952_GPIO */
		if ( use_gpiono_spi[i].no > PM8952_GPIO_BASE ) {
			gpio_no = use_gpiono_spi[i].no - PM8952_GPIO_BASE;
			ret = gpio_request( qpnp_pin_map("pm8950-gpio", gpio_no),  use_gpiono_spi[i].label );
		}
		/* others */
		else {
			ret = gpio_request(use_gpiono_spi[i].no , use_gpiono_spi[i].label);
		}
		if(ret < 0){
			printk(KERN_DEBUG "%s gpio_request() error : %d\n", use_gpiono_spi[i].label, ret);
		}
	}
#endif

	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_cleanup_spi
	@param	none
	@retval	none
*/
/**************************************************************************************************/
static void tuner_cleanup_spi(void)
{
	int i ;
	int loop = sizeof(use_gpiono_spi)/sizeof(stGPIO_DEF_spi);
#ifdef DTV_DRV_PINCTRL
	int portno;
#else
	unsigned int gpio_no;
#endif

#ifdef DTV_DRV_PINCTRL
	np = of_find_node_by_name( NULL, "sharp,tunctrl" );
	if ( np ) {
		for ( i=0; i<loop; i++ ) {
			portno = of_get_named_gpio( np, use_gpiono_spi[i].label ,0 );
			gpio_free( portno );
		}
	}
	else {
		printk(KERN_DEBUG "of_find_node_by_name() error \n");
	}
#else
	for(i=0; i<loop; i++){
		/* PM8952_GPIO */
		if ( use_gpiono_spi[i].no > PM8952_GPIO_BASE ) {
			gpio_no = use_gpiono_spi[i].no - PM8952_GPIO_BASE;
			gpio_free( qpnp_pin_map("pm8950-gpio", gpio_no) );
		}
		/* others */
		else {
			gpio_free(use_gpiono_spi[i].no);
		}
	}
#endif
}

/**************************************************************************************************/
/**
	@brief	gpio_init_spi
	@param	none
	@retval	0	Success
	@retval	1	Failed
*/
/**************************************************************************************************/
static int gpio_init_spi(void)
{
	int loop = sizeof(use_gpiono_spi)/sizeof(stGPIO_DEF_spi);
	int errcnt = 0;
	stGPIO_DEF_spi *p = &use_gpiono_spi[0];
	int i;
#ifdef DTV_DRV_PINCTRL
	int portno;
#else
	unsigned int gpio_no;
#endif
	
	for (i=0; i<loop; i++, p++) {
		if (p->direction == DirctionIn) {
			/* GPIO Input */
#ifdef DTV_DRV_PINCTRL
			portno = of_get_named_gpio( np, use_gpiono_spi[i].label ,0 );
		//	printk(KERN_DEBUG "of_get_named_gpio() -- gpio_direction_input [%d][%s] \n", portno, use_gpiono_spi[i].label);
			if ( gpio_direction_input( portno ) < 0 ) {
				errcnt ++;
				printk( "%s:%d gpio_direction_input error %s \n", __FILE__,__LINE__, use_gpiono_spi[i].label );
				continue;
			}
#else
			if ( p->no > PM8952_GPIO_BASE ) {
				/* PM8952_GPIO */
				gpio_no = p->no - PM8952_GPIO_BASE;

				if (gpio_direction_input( qpnp_pin_map("pm8950-gpio", gpio_no) ) < 0) {
					/* Failed */
					errcnt ++;
					printk( "%s:%d gpio_direction_input error NO.%d \n", __FILE__,__LINE__, p->no);
					continue;
				}
			}
			else {
				if (gpio_direction_input(p->no) < 0) {
					/* Failed */
					errcnt ++;
					printk( "%s:%d gpio_direction_input error NO.%d \n", __FILE__,__LINE__, p->no);
					continue;
				}
			}
#endif
			p->init_done = 1;
			continue;
		}
		if (p->direction == DirctionOut) {
			/* GPIO Output */
#ifdef DTV_DRV_PINCTRL
			portno = of_get_named_gpio( np, use_gpiono_spi[i].label ,0 );
		//	printk(KERN_DEBUG "of_get_named_gpio() -- direction_output [%d][%s] \n", portno, use_gpiono_spi[i].label);
			if ( gpio_direction_output( portno, use_gpiono_spi[i].out_val ) < 0 ) {
				errcnt ++;
				printk( "%s:%d gpio_direction_output error %s \n", __FILE__,__LINE__, use_gpiono_spi[i].label );
				continue;
			}

#else
			if ( p->no > PM8952_GPIO_BASE ) {
				/* PM8952_GPIO */
				gpio_no = p->no - PM8952_GPIO_BASE;

				if (gpio_direction_output( qpnp_pin_map("pm8950-gpio", gpio_no), p->out_val ) < 0) {
					/* Failed */
					errcnt ++;
					printk("%s: gpio_direction_output error NO.%d \n", __FILE__, p->no);
					continue;
				}
			}
			else {
				if (gpio_direction_output(p->no, p->out_val) < 0) {
					/* Failed */
					errcnt ++;
					printk("%s: gpio_direction_output error NO.%d \n", __FILE__, p->no);
					continue;
				}
			}
#endif
			p->init_done = 1;
			continue;
		}
	}

	if (errcnt != 0) {
		printk("%s: gpio_init error count %d\n", __FILE__, errcnt);
		return 1;
	}
	return 0;
}
#endif

MODULE_LICENSE("GPL");
module_init(tuner_init);
module_exit(tuner_cleanup);

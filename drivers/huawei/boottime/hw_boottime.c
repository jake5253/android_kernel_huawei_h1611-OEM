#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#define BOOT_STR_SIZE 128
#define BOOT_LOG_NUM 64
unsigned int sbl1_time = 0;
unsigned int lk_time=0;
struct boot_log_struct
{    
    u32 Second;	
    u32 mSecond;    
    char event[BOOT_STR_SIZE];
}hw_boottime[BOOT_LOG_NUM];

int boot_log_count = 0;
static DEFINE_MUTEX(hw_boottime_lock);
static int hw_boottime_enabled = 1;

static unsigned long Second(unsigned long long nsec, unsigned long divs)
{   
    if( (long long)nsec < 0 )   
    {       
        nsec = -nsec;       
        do_div(nsec, divs);     
        return -nsec;    
    }    
    do_div(nsec, divs);    
    return nsec;
}

static unsigned long mSecond(unsigned long long nsec, unsigned long divs)
{   
    if( (long long)nsec < 0 )       
        nsec = -nsec;   
    return do_div(nsec, divs);
}

void log_boot(char *str)
{	
    unsigned long long ts, tmp;	
    if(0 == hw_boottime_enabled)		
        return; 
    ts = sched_clock(); 
    if(boot_log_count >= BOOT_LOG_NUM)  
    {       
        printk("[boottime] no enough boottime buffer\n");       
        return; 
    }   
    mutex_lock(&hw_boottime_lock);  
    tmp = ts;   
    hw_boottime[boot_log_count].Second = Second(tmp, 1000000000);   
    tmp = ts;   
    hw_boottime[boot_log_count].mSecond = mSecond(tmp, 1000000000); 
    strncpy( (char*)&hw_boottime[boot_log_count].event, str, BOOT_STR_SIZE );   
    printk("boottime %d.%d : %s\n", hw_boottime[boot_log_count].Second, hw_boottime[boot_log_count].mSecond, hw_boottime[boot_log_count].event);    
    boot_log_count++;   
    mutex_unlock(&hw_boottime_lock);
}
EXPORT_SYMBOL(log_boot);

static int hw_boottime_show(struct seq_file *m, void *v)
{   
    int i;     
    seq_printf(m, "----------- BOOT TIME (sec) -----------\n"); 
    seq_printf(m, "%11d : %s\n", sbl1_time, "SBL1 End");
    seq_printf(m, "%11d : %s\n", lk_time, "LK End");
    for(i=0; i<boot_log_count; i++) 
    {       
        seq_printf(m, "%d.%09u : %s\n", hw_boottime[i].Second, hw_boottime[i].mSecond, hw_boottime[i].event); 
    }   
    seq_printf(m, "\n   %s", hw_boottime_enabled?"starting...":"start done");   
    return 0;
}

static int hw_boottime_open(struct inode *inode, struct file *file) 
{     
    return single_open(file, hw_boottime_show, inode->i_private); 
}

static int __init get_sbl1_time(char *str)
{
    int tmp;
    if (get_option(&str, &tmp)) 
    {
        sbl1_time=tmp;
        return 0;
    }
    return -EINVAL;
}
early_param("sbl1_boottime", get_sbl1_time);

static int __init get_lk_time(char *str)
{
    int tmp;
    if (get_option(&str, &tmp)) 
    {
        lk_time=tmp;
        return 0;
    }
    return -EINVAL;
}
early_param("lk_boottime", get_lk_time);

static ssize_t hw_boottime_write(struct file *filp, const char *ubuf,size_t cnt, loff_t *data)
{
    char buf[BOOT_STR_SIZE];
    size_t copy_size = cnt;
    if (cnt >= sizeof(buf))
        copy_size = BOOT_STR_SIZE - 1;
    if (copy_from_user(&buf, ubuf, copy_size))
        return -EFAULT;
    if(cnt==1)
    {
        if(buf[0] == '0')
            hw_boottime_enabled = 0;  // boot up complete
        else if(buf[0] == '1')
            hw_boottime_enabled = 1;
    }
    buf[copy_size] = 0;
    log_boot(buf);
    return cnt;
    
}



static const struct file_operations hw_boottime_fops = {     
    .open = hw_boottime_open,     
    .write = hw_boottime_write,    
    .read = seq_read,     
    .llseek = seq_lseek,     
    .release = single_release, 
};

static int __init init_boot_time(void){    
    struct proc_dir_entry *pe;    
    pe = proc_create("boottime", 0664, NULL, &hw_boottime_fops);    
    if (!pe)      
        return -ENOMEM;    
    return 0;
}
__initcall(init_boot_time);
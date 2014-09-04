/*
 *
 * BRIEF MODULE DESCRIPTION
 *	79S334A 4 digits display.
 *
 * Copyright 2002 THOMSON multimedia.
 * Author: Stephane Fillod & Guillaume Lorand
 *         	fillods@thmulti.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/rc32300/rc32300.h>
#include <asm/rc32300/idtdisplay.h>


/**
 * @name module parameters
 * @param minor minor number of the device (major is gived by misc device)
 * @param delay delay in milliseconds between scrolling on the display
 */
//@{
MODULE_PARM(minor, "i") ;
MODULE_PARM(delay, "i") ;
//@}

/**
 * @name locales variables
 */
//@{
static unsigned int minor __initdata = IDTDISPLAY_MINOR ; /*< minor number of the device */
static unsigned int delay = IDTDISPLAY_DELAY ; /*< delay between scrolling in ms */
static DECLARE_MUTEX(idtdisp_sem) ;
//@}


/**
 * internal function which clean the display
 */
static inline void idtdisp_clean(void)
{
  readb(KSEG1ADDR(LCD_CLEAR)) ;
}

/**
 * internal function which display a char on the display
 * @param i display number
 * @param c character to write
 */
static inline void idtdisp_char(int i, char c)
{
  switch(i) {
  case 0: writeb(c, KSEG1ADDR(LCD_DIGIT0)) ; break ;
  case 1: writeb(c, KSEG1ADDR(LCD_DIGIT1)) ; break ;
  case 2: writeb(c, KSEG1ADDR(LCD_DIGIT2)) ; break ;
  case 3: writeb(c, KSEG1ADDR(LCD_DIGIT3)) ; break ;
  default: writeb('?', KSEG1ADDR(LCD_DIGIT0)) ; break ;
  }
}


/**
 * internal function which display a string on the display
 * @param s string to write
 */
static inline void idtdisp_str(char *s)
{
  int i;

  if(s == NULL) {
	idtdisp_clean();
	return;
  }

  for(i = 0; i < 4 && s[i]; i++)
	idtdisp_char(i, s[i]);
}


/**
 * open the display device
 * @return always 0
 */
static int idtdisp_open(struct inode *inode, struct file *file) ;

/**
 * close the display device
 * @return always 0
 */
static int idtdisp_release(struct inode *inode, struct file *file) ;

/**
 * write to the display device
 * @return an errno code if it failed or else the number of writen bytes
 */
static ssize_t idtdisp_write(struct file *file, const char *buffer, size_t count, loff_t *ppos) ;


/**
 * clean the display device
 * @return an errno code if it failed or else the number of readen bytes
 */
static ssize_t idtdisp_read(struct file *file, char *buffer, size_t count, loff_t *ppos) ;

/**
 * perform ioctl request on the display device
 * @return an errno code if it failed or 0 if it succed.
 */
static int idtdisp_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg ) ;


/**
 * initialize the display device
 * @return the result of misc registration
 */
int __init idtdisp_init(void) ;


/**
 * release previous allocation
 */
static void __exit idtdisp_exit (void) ;



static struct file_operations displaydev_fops = {
  owner: THIS_MODULE,
  read : idtdisp_read,
  write: idtdisp_write,
  ioctl: idtdisp_ioctl,
  open: idtdisp_open,
  release: idtdisp_release,
};

static struct miscdevice displaydev = {
  minor: IDTDISPLAY_MINOR,
  name : "idtdisplay",
  fops : &displaydev_fops,
};



/*
 *	Now all the various file operations that we export.
 */


static int idtdisp_open(struct inode *inode, struct file *file)
{
  MOD_INC_USE_COUNT;

  return 0;
}


static int idtdisp_release(struct inode *inode, struct file *file)
{

  MOD_DEC_USE_COUNT;

  return 0;
}

static ssize_t idtdisp_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
  char contents [IDTDISPLAY_BUF_SIZE] ;
  int buf_size = (count > IDTDISPLAY_BUF_SIZE) ? IDTDISPLAY_BUF_SIZE : count;
  int cpt = 0 ;
  int retval;

  if (copy_from_user (contents, buffer, buf_size))
    return -EFAULT;

  if (down_interruptible (&idtdisp_sem))
    return -EINTR;

  idtdisp_str(&contents[cpt]);

  while (cpt++ < buf_size-4) {

    current->state = TASK_INTERRUPTIBLE;
    retval = schedule_timeout(MS_TO_HZ(delay)) ;
    if (retval != 0)
	    break;

    idtdisp_str(&contents[cpt]);
  }

  up(&idtdisp_sem) ;

  return cpt+4 > buf_size ? buf_size : cpt+4;
}


static ssize_t idtdisp_read (struct file *file, char *buffer, size_t count, loff_t *ppos)
{
  idtdisp_clean();

  return 0;
}

static int idtdisp_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg )
{
  switch(cmd) {

  case IDTDISPLAY_IOCTL_DELAY:
	if (arg > 5000)
		  return -EINVAL;
    	delay = arg ;
    	break ;

  case IDTDISPLAY_IOCTL_CLEAN:
	idtdisp_clean();
	break;

  case IDTDISPLAY_IOCTL_WRITE_CHAR:
    {
      struct idtdisp_wc_struct wc;

      if (copy_from_user(&wc, (struct idtdisp_wc_struct*)arg, sizeof(struct idtdisp_wc_struct)))
	return -EFAULT ;
      
      if (down_interruptible (&idtdisp_sem))
	return -EINTR;

      idtdisp_char(wc.nb, wc.ch);

      up (&idtdisp_sem) ;

      break;
    }

  case IDTDISPLAY_IOCTL_WRITE_4:
    {
      char w4[4] ;

      if (copy_from_user(w4, (char *)arg, 4) )
	return -EFAULT ;
      
      if (down_interruptible (&idtdisp_sem))
	return -EINTR;

      idtdisp_str(w4);

      up (&idtdisp_sem) ;

      break;
    }

  default:
    return -ENOSYS;
    break ;
  }

  return 0;
}


int __init idtdisp_init(void)
{
	int ret;

	displaydev.minor = minor ;
	  
	ret = misc_register(&displaydev);

	return ret ;
}


static void __exit idtdisp_exit (void)
{
  misc_deregister(&displaydev);
}

/**
 * @name init/exit functions
 * fuction use by insmod to run and stop the module
 */
//@{
module_init(idtdisp_init);
module_exit(idtdisp_exit);
//@}

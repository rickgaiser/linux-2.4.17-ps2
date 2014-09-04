/******************************************************
 *
 * This is klids, the LIDS kernel thread.
 * It is used to send security alerts through network
 * to remote mail servers (or other listening daemon)
 * without the help of any user space program.
 *
 * Philippe BIONDI (philippe.biondi@webmotion.com)
 * March 25, 2000
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/smp_lock.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <net/sock.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/version.h>
#include <linux/lids.h>

#define POLLIN_SET (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#define DEFAULT_POLLMASK (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)

/* debug messages : */
#if 0
#define dprintk(fmt, args...) printk(KERN_DEBUG fmt , ## args)
#else
#define dprintk(fmt, args...)
#endif

#ifdef CONFIG_LIDS_HIDE_KLIDS
#define eprintk(fmt, args...)
#else
#define eprintk(fmt, args...) printk(KERN_ERR fmt , ## args)
#endif


/* 
 * Pseudo-scripting language commands
 *
 * About the Pseudo-scripting language :
 * C purists won't like it, saying that code
 * which is not pretty is bad code. (see latter :))
 * I'll say that is the prettiest manner,
 * until they give me a prettier one (if they can)
 *
 */

#define lids_send(x) lids_send_sock(sock,x,strlen(x));
#define lids_expect(x) if (lids_expect_sock(sock,x)<=0) { dprintk("Didn't get %s\n",x); goto err_expect; }
#define LIDS_MESSAGE lids_msgqueue[0].msg[lids_msgqueue[0].head]

extern void poll_freewait(poll_table * p);
extern int sock_map_fd(struct socket * );
extern char lids_mail_source[64],lids_mail_from[64],
        lids_mail_to[64],lids_mail_subject[128],lids_rcpt_to[64];

extern	int lids_mail_switch;
extern	unsigned long int lids_mail_relay;
extern unsigned int lids_mail_port;

typedef struct t_lids_msgqueue {
  	char *msg[CONFIG_LIDS_MSGQUEUE_SIZE];
  	int msg_size[CONFIG_LIDS_MSGQUEUE_SIZE];
  	int head, tail, nbmsg;
  	wait_queue_head_t writeq, readq; /* read and write queues */
} lids_msgqueue_s;


lids_msgqueue_s lids_msgqueue[1]; /* Actually, there is just one. For now.. */

static struct semaphore lids_msgqueue_mutex; 
static struct semaphore klids_sem;
static int klids_pid, klids_ppid, klids_stop;
struct tq_struct klids_tq;


int lids_send_sock(struct socket *sock, const char *buffer,const size_t length)
{
	struct msghdr	msg;
	struct iovec	iov;
	int 		len;
	
	if (!sock->sk) 
		return -ECONNRESET;

	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	msg.msg_iov	 = &iov;
	msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = MSG_NOSIGNAL;    
	msg.msg_iov->iov_base = (char*) buffer;
	msg.msg_iov->iov_len  = (__kernel_size_t)length
			;
	len = sock_sendmsg(sock,&msg,(size_t)(length));
	return len;	
}



int lids_expect_sock(struct socket *sock, char *msgnum)
{
	struct msghdr		msg;
	struct iovec		iov;
	int			len;
	char	       buffer[1024];   
	
	unsigned long  mask;
	poll_table    wait_table, *wait;
	int            retval;
	long           timeout = CONFIG_LIDS_NET_TIMEOUT*HZ;
	struct file   *file;

	if (sock->sk==NULL)
		return -ECONNRESET;

	poll_initwait(&wait_table);
	wait = &wait_table;

	/* wait tables initialization */
	/*
	wait_table = (poll_table *) __get_free_page(GFP_KERNEL);
	if (!wait_table)
		return -ENOMEM;

	wait_table->nr = 0;
	wait_table->entry = (struct poll_table_entry *)(wait_table + 1);
	wait_table->next = NULL;
	wait = wait_table;
*/

	/* polling */

	lock_kernel();

	retval=0;
	file = sock->file;
	for (;;) {
		current->state = TASK_INTERRUPTIBLE;
		mask = POLLNVAL;
		if (file) {
			mask = DEFAULT_POLLMASK;
			if (file->f_op && file->f_op->poll)
				mask = file->f_op->poll(file, wait);
		}
		if ((mask & POLLIN_SET) || !timeout || signal_pending(current))
			break;
		wait = NULL;
		timeout = schedule_timeout(timeout);
	}
	current->state = TASK_RUNNING;
	poll_freewait(&wait_table);

	unlock_kernel();

	/* receiving */

	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	msg.msg_iov	 = &iov;
	msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = MSG_DONTWAIT;
		
		
	msg.msg_iov->iov_base = buffer;
	msg.msg_iov->iov_len  = (__kernel_size_t)1024;
	
	len = sock_recvmsg(sock,&msg,1024,MSG_DONTWAIT);
	if (len > 0)
		buffer[len-1]=0;
	return ((len > 0) && (strstr(buffer,msgnum)));
}
int lids_klids_sentmsg(int *try)
{
	struct socket *sock;
	struct sockaddr_in loc;
	struct sockaddr_in rem;
	int retval;

	lock_kernel();
	/* create socket */
#ifdef CONFIG_LIDS_NET_UDP
	retval = sock_create(PF_INET,SOCK_DGRAM,IPPROTO_UDP,&sock);
#else
	retval = sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&sock);
#endif
	if (retval < 0) {
		eprintk("klids: Error %d: can't create socket\n",retval);
		goto err_crsock;
	}
		/* create a file descriptor for the socket */
		/*
		sockfd=retval = sock_map_fd(sock); 
		if (retval < 0) {
		eprintk("klids: Error %d: can't create socket fd\n",retval);
		goto err_crfd;
		}
		
		sock->file = fcheck(retval);
		*/
				
	unlock_kernel();
		/* bind */
	loc.sin_family	     = AF_INET;
	loc.sin_addr.s_addr  = INADDR_ANY;
	loc.sin_port         = 0; /* any port */
				
	retval = sock->ops->bind(sock,(struct sockaddr*)&loc,sizeof(loc));
	if (retval < 0) {
		eprintk("klids: Error %d: can't bind socket\n",retval);
		goto err_bind;
	}
				
		/* connect */
	sock->sk->reuse   = 1; /* setsockopt... */
	sock->sk->linger  = 1;
		
	rem.sin_family      = AF_INET;
	rem.sin_addr.s_addr = lids_mail_relay;
	rem.sin_port=htons(lids_mail_port);
				
	retval=sock->ops->connect(sock,(struct sockaddr *)&rem,sizeof(rem),0);	
				
	if (retval < 0) {
		eprintk("klids: Error %d: can't connect to %s:%u\n",-retval,in_ntoa(lids_mail_relay),lids_mail_port);
		goto err_conn;
	}
				
	/* scripting ... */

/*
 * Well, that seem strange, at first sight.
 * Let's say that it is a special language that
 * need to be preprocessed before having gcc fed with.
 * It just happens that the preprocessor is also the C
 * preprocessor.
 *
 */

#include "lids_mail_script.c"

	*try=1; /* as if it was the last try */
	dprintk("Message sent successfully\n");
	goto all_ok;

	err_expect:
		eprintk("Timeout/Protocol error\n");
				
	all_ok:
	err_conn:
	err_bind:
		sock_release(sock);
				
	err_crsock: 
	/* nothing to dealloc */
				
	/* sleep a while */
	if (--(*try) > 0) {
		dprintk("sleep a while before retrying\n");
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(CONFIG_LIDS_NET_TIMEOUT*HZ);
		return 0;
	}
	return -1;
}

int lids_klids_thread(void *m)
{	
	int try;
	int retval;
#ifdef CONFIG_LIDS_HIDE_KLIDS
	struct lids_sys_acl *current_sys_acl;
#endif
	

#ifdef CONFIG_LIDS_HIDE_KLIDS
	current_sys_acl = (struct lids_sys_acl *)kmalloc(sizeof(struct lids_sys_acl), GFP_KERNEL);
	if(!current_sys_acl) {
		return -1;
	}
	current_sys_acl->lids_cap = 0;
	set_bit(CAP_HIDDEN,&current_sys_acl->lids_cap);
#endif

	lock_kernel();
	strcpy(current->comm, "klids");
	
	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));
	klids_stop = 0;	
	klids_pid = current->pid;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,7)
	klids_ppid = current->parent->pid;
#else
	klids_ppid = current->p_pptr->pid;
#endif

#ifdef CONFIG_LIDS_HIDE_KLIDS
	current->security = current_sys_acl;
#endif
	unlock_kernel();
	up(&klids_sem);

	LIDS_DBG(":staring klids %d , ppid=%d\n",klids_pid, klids_ppid);
	/* Allow args to be in kernel space. */
	set_fs(KERNEL_DS);

	while (klids_stop==0) {
	/* if switch is off, do not send msg, FIXME: or sleep */
		if (!lids_mail_switch) {
			LIDS_DBG("sleep a while before retrying\n");
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(CONFIG_LIDS_NET_TIMEOUT*HZ);
			continue;
		}
					
		if (lids_msgqueue[0].nbmsg == 0) {
			LIDS_DBG("klids: go to sleep...\n");
			interruptible_sleep_on(&(lids_msgqueue[0].readq));
			LIDS_DBG("klids: wake up\n");
		}

		while (lids_msgqueue[0].nbmsg > 0) {
			try=CONFIG_LIDS_NET_MAX_TRIES;
			while(1) {
				retval = lids_klids_sentmsg(&try);
				if(retval < 0 ) break; 
			}
			/* Remove the message, even if it has not 
			   been successfully sent. It is maybe the cause
			   of the error and it is blocking the whole
			   message queue. */
			dprintk("Removing message from the queue\n");
			down(&lids_msgqueue_mutex);
			lids_msgqueue[0].nbmsg--;
			kfree(lids_msgqueue[0].msg[lids_msgqueue[0].head]);
			lids_msgqueue[0].head++;
			lids_msgqueue[0].head %= CONFIG_LIDS_MSGQUEUE_SIZE;
			up(&lids_msgqueue_mutex);
			wake_up_interruptible(&(lids_msgqueue[0].writeq));
		}
	}
	lock_kernel();
	mb();
	up(&klids_sem);

	return 0;
}


void lids_send_message(char *msg,int len)
{
	down(&lids_msgqueue_mutex);
	if (lids_msgqueue[0].nbmsg < CONFIG_LIDS_MSGQUEUE_SIZE) {
		lids_msgqueue[0].msg[lids_msgqueue[0].tail] = msg;
		lids_msgqueue[0].msg_size[lids_msgqueue[0].tail++] = len;
		lids_msgqueue[0].tail %= CONFIG_LIDS_MSGQUEUE_SIZE;
		lids_msgqueue[0].nbmsg++;
		
		dprintk("Msg : <%s>\n",msg);
		dprintk("head:%d tail:%d nbmsg:%d\n",lids_msgqueue[0].head,lids_msgqueue[0].tail,lids_msgqueue[0].nbmsg);
		up(&lids_msgqueue_mutex);

		wake_up_interruptible(&(lids_msgqueue[0].readq));
	}
	else {
		up(&lids_msgqueue_mutex);
		/* It is safer to loose the message
		   than to block the process */
		dprintk("Message queue full. One message lost.\n");
		kfree(msg);
	}
}

void lids_klids_stop(void )
{
	lock_kernel();

	init_MUTEX_LOCKED(&klids_sem);
	
	mb();
	klids_stop = 1;
	mb();
	kill_proc(klids_pid, SIGKILL, 1);

/* block till thread terminated */
	down(&klids_sem);
	unlock_kernel();
/* let it parent get rid of the zombie process here */
	kill_proc(klids_ppid, SIGCHLD, 1);
}

static void klids_launcher(void *data)
{
       	kernel_thread(lids_klids_thread,(void *)0,0);
}

int lids_klids_init(void)
{
	init_MUTEX(&lids_msgqueue_mutex);
	/* Initialize lids_msgqueue */
	lids_msgqueue[0].nbmsg=0;
	lids_msgqueue[0].tail=0;
	lids_msgqueue[0].head=0;
	init_waitqueue_head(&(lids_msgqueue[0].readq));
	init_waitqueue_head(&(lids_msgqueue[0].writeq));
	/*
	lids_msgqueue[0].readq=NULL;
	lids_msgqueue[0].writeq=NULL;
	*/
	
        init_MUTEX_LOCKED(&klids_sem);

        /* initialize the task queue structure */
        klids_tq.sync = 0;
        INIT_LIST_HEAD(&klids_tq.list);
        klids_tq.routine =  klids_launcher;
        klids_tq.data = (void *)NULL ;

        schedule_task(&klids_tq);

        down(&klids_sem);

	return 0;
}

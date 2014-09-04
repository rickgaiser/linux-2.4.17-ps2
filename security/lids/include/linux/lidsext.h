#ifndef LIDSEXT_H
#define LIDSEXT_H

/*
 * This file contains LIDS macros needed for logging and debugging,
 * used about everywhere in the kernel.
 *
 */

/* needed extern declarations */

#include <linux/config.h>

extern void lids_cap_log(int);
extern int lids_cap_time_checker(const int);
extern int lids_local_off(void);
extern int lids_reload_conf;
extern int lids_load;
extern int lids_local_on;
extern int lids_local_pid;
extern int lids_first_time;


#ifdef CONFIG_LIDS_DEBUG
#define LIDS_DEBUG
#endif

#define LIDS_STR2(x) #x
#define LIDS_STR(X) LIDS_STR2(X)

#ifdef LIDS_DEBUG
#define LIDS_DBG(msg...)  printk(KERN_DEBUG "LIDS." __FUNCTION__ ".l" LIDS_STR(__LINE__) ": " ##msg)
#else
#define LIDS_DBG(msg...)
#endif


#ifdef CONFIG_LIDS_HANGUP
extern void lids_hangup_console(void);
#else
#define lids_hangup_console() do {} while (0)
#endif


extern void lids_log(int flood, char *message, ...);


#ifdef CONFIG_LIDS_NO_FLOOD_LOG

#define lids_security_alert(message, args...)                                       \
do {                                                                       	    \
	if (lids_load && lids_local_load) {					    \
		static unsigned long warning_time = 0, no_flood_yet = 0;            \
		static spinlock_t lids_security_alert_lock = SPIN_LOCK_UNLOCKED;    \
									   	    \
		spin_lock(&lids_security_alert_lock);                               \
										    \
/* Make sure at least CONFIG_LIDS_TIMEOUT_AFTER_FLOOD 			   	    \
 * passed since the last warning logged 				   	    \
 */ 									   	    \
		if ((!warning_time) || 					            \
		    (jiffies-warning_time > CONFIG_LIDS_TIMEOUT_AFTER_FLOOD*HZ)) {  \
			warning_time = jiffies; no_flood_yet = 1;                   \
			lids_log(0, message , ## args);                             \
		} else if (no_flood_yet) {                                          \
			warning_time = jiffies; no_flood_yet = 0;                   \
			lids_log(1, message , ## args);                             \
		}                                                           	    \
		spin_unlock(&lids_security_alert_lock);                             \
		lids_hangup_console();                                              \
	}									    \
} while(0)

#else /* CONFIG_LIDS_NO_FLOOD_LOG */

#define lids_security_alert(message, args...)                                      \
do {                                                                       	   \
	if (lids_load && lids_local_load) {					   \
		static spinlock_t lids_security_alert_lock = SPIN_LOCK_UNLOCKED;   \
										   \
		spin_lock(&lids_security_alert_lock);                              \
		lids_log(0, message , ## args);                                    \
		spin_unlock(&lids_security_alert_lock);                            \
		lids_hangup_console();    					   \
	}				   					   \
} while(0)

#endif /* CONFIG_LIDS_NO_FLOOD_LOG */



#ifdef CONFIG_LIDS_ALLOW_SWITCH
#define lids_local_load ( lids_local_on || (!lids_local_off()) )
#else
#define lids_local_load 1
#endif /* CONFIG_LIDS_ALLOW_SWITCH */




#endif /* LIDSEXT_H */

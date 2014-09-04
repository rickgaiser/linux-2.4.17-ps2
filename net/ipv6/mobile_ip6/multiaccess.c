/*  
 * 2001 (c) Oy L M Ericsson Ab
 *
 * Author: NomadicLab / Ericsson Research <ipv6@nomadiclab.com>
 *
 * $Id: multiaccess.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 */

#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/if_arp.h>
#include <net/ipv6.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>
#include "multiaccess.h"
#include "multiaccess_ext.h"
#include "multiaccess_ctl.h"
#include "debug.h"
#include "mdetect.h"
#include "mn.h"

/*
 * Callable function for device notifier
 */
static struct notifier_block ma_dev_notf = {
    ma_dev_notify,
    NULL,
    0
};

/*
 * Local function prototypes
 */
static int ma_if_find(struct ma_if**, int);
static int ma_if_create(struct ma_if**, int);
static int ma_if_delete(int);
static int ma_if_cleanup(void);
static int ma_move_haddr(int);

/*
 * Local interface list
 */
static struct ma_if *if_list = NULL;

/*
 * Desired router
 */
static struct router *desired_if = NULL;

/*
 * Local function: ma_if_find
 * Description: Used to find an interface
 */
static int ma_if_find(struct ma_if** if_entry, int ifindex) {
    int result = MULTIACCESS_FAIL;
    struct ma_if* list = if_list;
    
    DEBUG_FUNC();
    
    while ((NULL != list) && (list->ifindex != ifindex)) {
        list = list->next;
    }
    (*if_entry) = list;
    
    if (NULL != list) {
        result = MULTIACCESS_OK;
        DEBUG((DBG_DATADUMP, "Content of if_entry:\n"
               "State %d\nifindex: %d\n", list->state, list->ifindex));
    }
    
    return result;
}

/*
 * Local function: ma_if_create
 * Description: Used to create a new interface
 */
static int ma_if_create(struct ma_if** if_entry, int ifindex) {
    struct ma_if* list = if_list;
    struct net_device *dev = NULL;

    DEBUG_FUNC();
    
    if (NULL == if_list) { 
        /* First item to the list */
        if_list = (struct ma_if*) kmalloc(sizeof(struct ma_if), 
                                             GFP_ATOMIC);
        
        if (NULL == if_list) {
            DEBUG((DBG_CRITICAL, 
                   "Couldn't allocate memory for interface list"));
            return MULTIACCESS_FAIL;
        }
        list = if_list;
        list->prev = NULL;
    }
    else {
        while (NULL != list->next) {
            list = list->next;
        }
        list->next = (struct ma_if*) kmalloc(sizeof(struct ma_if),
                                                GFP_ATOMIC);
        if (NULL == list->next) {
            DEBUG((DBG_CRITICAL, 
                   "Couldn't allocate memory for interface list"));
            return MULTIACCESS_FAIL;
        }
        list->next->prev = list;
        list = list->next;
    }
    
    list->next = NULL;
    list->ifindex = ifindex;
    list->state = MULTIACCESS_UNAVAILABLE;
    list->rtr = NULL;
    
    (*if_entry) = list;
    dev = dev_get_by_index(list->ifindex);
    DEBUG((DBG_DATADUMP, "if_entry: name=%s state=%d ifindex=%d", 
           dev->name,
           list->state, 
           list->ifindex));
    dev_put(dev);
    return MULTIACCESS_OK;
}

/*
 * Local function: ma_if_delete
 * Description: Used to delete the interface
 */
static int ma_if_delete(int ifindex) {
    int result = MULTIACCESS_OK;
    struct ma_if* list = NULL;
    
    DEBUG_FUNC();

    if (MULTIACCESS_OK == ma_if_find(&list, ifindex)) {
      if (list == NULL) {
        DEBUG((DBG_CRITICAL, 
               "Couldn't delete interface entry. The list is NULL."));
        result = MULTIACCESS_FAIL;
        return result;
      }
      if (list->next != NULL)
        list->next->prev = list->prev;
      if (list->prev != NULL)
        list->prev->next = list->next;
      if (NULL != list->rtr) {
        kfree(list->rtr);
        DEBUG((DBG_INFO, "Router entry removed"));
      }
      kfree(list);
      DEBUG((DBG_INFO, "If entry removed"));
    } else {
      DEBUG((DBG_CRITICAL, 
             "Couldn't delete interface entry. No such interface"));
      result = MULTIACCESS_FAIL;
    }
    return result;
}

/*
 * Local function: ma_if_cleanup
 * Description: Used to cleanup the ma_if list
 */
static int ma_if_cleanup(void) {

    struct ma_if *p = if_list;
    struct ma_if *tmp = NULL;

    DEBUG_FUNC();

    while (p != NULL) {

        // tmp is needed to store the pointer before
        // freeing up the structure pointed by p
        tmp = p->next;

        // Free up the list item
        if (p->rtr != NULL) {
            kfree(p->rtr);
            DEBUG((DBG_DATADUMP, "Router entry removed"));
        }
        kfree(p);
        DEBUG((DBG_DATADUMP, "Interface entry removed"));

        // Move to next item
        p = tmp;
    }

    DEBUG((DBG_INFO, "Interface list cleaned up"));
    return MULTIACCESS_OK;

}

/*
 * Public function: ma_dev_notify
 * Description: See header file
 */
int ma_dev_notify(struct notifier_block *this, 
                     unsigned long event, void *data) {
    
    struct net_device *dev;
    struct ma_if *m_if = NULL;

    DEBUG_FUNC();
    
    dev = (struct net_device *)data;
    
    switch(event) {
        
    case NETDEV_UP:
        DEBUG((DBG_INFO, "ma_dev_notify: iface up"));

        /* Check that we don't have this interface already in our list */
        if (MULTIACCESS_OK != ma_if_find(&m_if, dev->ifindex)) {
            /* Create a new ma_interface entry */
            if (MULTIACCESS_OK == ma_if_create(&m_if, dev->ifindex)) {
                /* Inform control module now we have a new interface */
                ma_ctl_add_iface(dev->ifindex, dev->name);
            }
        }
        break;
        
    case NETDEV_DOWN:
        DEBUG((DBG_INFO, "ma_dev_notify: iface down"));

        /* Check that we have this interface in our list */
        if (MULTIACCESS_OK == ma_if_find(&m_if, dev->ifindex)) {
            /* Delete the ma_if entry */
          if (MULTIACCESS_OK == ma_if_delete(dev->ifindex)) {
            /* Inform control module to remove this interface */
            ma_ctl_del_iface(dev->ifindex);
          }
        }
        break;
        
    }
    
    return NOTIFY_OK;
    
}

/*
 * Local function: ma_move_haddr
 * Description: Moves Home Address to a new interface
 */
static int ma_move_haddr(int ifindex) {
    
    struct in6_ifreq ifreq;
    struct in6_addr home_addr;
    struct net_device *dev = NULL;

    DEBUG_FUNC();

    if ((ifreq.ifr6_prefixlen = mipv6_mn_get_homeaddr(&home_addr)) < 0)
	    return MULTIACCESS_FAIL;
   
    ipv6_addr_copy(&ifreq.ifr6_addr, &home_addr);
    addrconf_del_ifaddr((void *)&ifreq);
    ifreq.ifr6_ifindex = ifindex;

    addrconf_add_ifaddr((void *)&ifreq);
    dev = dev_get_by_index(ifindex);

    DEBUG((DBG_INFO, "Moved HA to %s", dev->name));
    dev_put(dev);

    return MULTIACCESS_OK;

}

/*
 * Public function: ma_change_iface
 * Description: See header file
 */
int ma_change_iface(int ifindex) {

    int result = MULTIACCESS_FAIL;
    struct ma_if *if_entry = NULL;
    struct net_device* dev = NULL;
    struct rt6_info* rt = NULL;
    struct neighbour* neigh = NULL; 

    DEBUG_FUNC();
    
    if (MULTIACCESS_OK == (result = ma_if_find(&if_entry, ifindex))) {
        desired_if = if_entry->rtr;
        
        if (NULL != (dev = dev_get_by_index(desired_if->ifindex))) {
          DEBUG((DBG_INFO, "Changing interface to %s", 
                 dev->name));
          if (NULL == 
              (rt = rt6_add_dflt_router(&desired_if->ll_addr, dev))) {
            DEBUG((DBG_WARNING, "Adding new def. router failed"));
            dev_put(dev);
            return MULTIACCESS_FAIL;
          }

          if (NULL == (neigh = rt->rt6i_nexthop)) {
            DEBUG((DBG_WARNING, "Adding new def. router: null neighbours"));
            dst_release(&rt->u.dst);
            dev_put(dev);
            return MULTIACCESS_FAIL;
          }
          
          neigh->flags |= NTF_ROUTER;
          rt6_purge_dflt_routers(RTF_ALLONLINK);
          rt->rt6i_expires = jiffies + (HZ * desired_if->lifetime);
          mipv6_change_router();
          result = ma_move_haddr(ifindex);
          desired_if = NULL;
          dev_put(dev);
        } else {
          DEBUG((DBG_WARNING, "Device (index=%d) not present", 
                 desired_if->ifindex));
          result = MULTIACCESS_FAIL;
        }

    } else {
       DEBUG((DBG_ERROR, "Couldn't find interface entry. " \
              "No such interface (ifindex = %d)", ifindex));
    }
    
    return result;
}

/*
 * Public function: ma_check_if_availability
 * Description: See header file
 */
int ma_check_if_availability(int ifindex) {
    int status = MULTIACCESS_AVAILABLE;
    struct ma_if* list = NULL;

    DEBUG_FUNC();
    
    if (MULTIACCESS_OK == ma_if_find(&list, ifindex)) {
        status = list->state;
        switch (status) {
          case MULTIACCESS_AVAILABLE:
            DEBUG((DBG_DATADUMP, "Interface has the Global IPv6 address"));
            break;
          case MULTIACCESS_UNAVAILABLE:
            DEBUG((DBG_DATADUMP, "Interface has no Global IPv6 address"));
            break;
          default:
            DEBUG((DBG_WARNING, "State of interface UNKNOWN (%d), "\
                   "using UNAVAILABLE state insted of UNKNOWN",
                   list->state));
            status = MULTIACCESS_UNAVAILABLE;
            break;
        }
        DEBUG((DBG_DATADUMP, "State of interface is %d", list->state));
    } else {
        DEBUG((DBG_ERROR,
               "Couldn't find interface entry. " \
               "No such interface (ifindex = %d)", ifindex));
    }
    return status;
}

/*
 * Public function: ma_get_if_rtr
 * Description: See header file
 */
struct router* ma_if_get_rtr(void) {
    
    DEBUG_FUNC();

    if (NULL != desired_if) {
        DEBUG((DBG_DATADUMP, "Data set for default router"));
    } else {  
        DEBUG((DBG_DATADUMP, "No data set for default router"));
    }
    return desired_if;

}

/*
 * Public function: ma_if_set_rtr
 * Description: See header file
 */
int ma_if_set_rtr(struct router* nrt, int ifindex) {
    int result = MULTIACCESS_OK;
    struct ma_if* list = NULL;
    
    DEBUG_FUNC();
    
    if (MULTIACCESS_OK == ma_if_find(&list, ifindex)) {
        if (NULL == list->rtr) {
          // New router entry
          list->rtr = (struct router*) kmalloc(sizeof(struct router),
                                               GFP_ATOMIC);
          if (NULL == list->rtr) {
            DEBUG((DBG_CRITICAL, "Couldn't allocate memory for router"));
            return MULTIACCESS_FAIL;
          }
          memcpy(list->rtr, nrt, sizeof(struct router));
          list->state = MULTIACCESS_AVAILABLE;
          DEBUG((DBG_DATADUMP, "Data for router copied"));
        } else {
            // Update current router entry. Copy all information received.
            memcpy(list->rtr, nrt, sizeof(struct router));
            DEBUG((DBG_DATADUMP, "Data for router copied (update)"));
        }
    } else {
      DEBUG((DBG_ERROR, 
             "Couldn't find interface entry. " \
             "No such interface (ifindex = %d)", ifindex));
      result = MULTIACCESS_FAIL;
    }

    return result;
}

/*
 * Public function: ma_init
 * Description: See header file
 */
void ma_init(void) {

    struct net_device *dev;
    struct ma_if *m_if;

    DEBUG_FUNC();

    // Add ma_dev_notify() to the list of callable
    // functions in a case of interface state change
    register_netdevice_notifier(&ma_dev_notf);

    // Add all known interfaces to our if_list and
    // give them to the control module
    for (dev = dev_base; dev; dev = dev->next) {
        if (((dev->flags & IFF_UP) && (dev->type == ARPHRD_ETHER)) ||
             (strncmp(dev->name, "sit1", 4) == 0)) {
            ma_if_create(&m_if, dev->ifindex);
            ma_ctl_add_iface(dev->ifindex, dev->name);
        }
    } 

}

/*
 * Public function: ma_cleanup
 * Description: See header file
 */
void ma_cleanup(void) {

    DEBUG_FUNC();

    // Remove ma_dev_notify() from the list of
    // callable functions in a case of if state change
    unregister_netdevice_notifier(&ma_dev_notf);

    // Clean up if_list
    if (ma_if_cleanup() != MULTIACCESS_OK) {
        DEBUG((DBG_CRITICAL, "Interface list cleanup failed"));
    }

}


/*
 * Public function: ma_if_set_unavailable
 * Description: See header file.
 */
void ma_if_set_unavailable(int ifindex) {
  struct ma_if* list = NULL;
  
  DEBUG_FUNC();
  
  if (MULTIACCESS_OK == ma_if_find(&list, ifindex)) {
    list->state = MULTIACCESS_UNAVAILABLE;
    DEBUG((DBG_DATADUMP, "State changed to MULTIACCESS_UNAVAILABLE (ifindex = %d)",
           ifindex));
    
  }
  else {
    DEBUG((DBG_ERROR, 
           "Couldn't find interface entry. " \
           "No such interface (ifindex = %d)", ifindex));
  }
}

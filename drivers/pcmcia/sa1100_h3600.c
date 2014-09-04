/*
 * drivers/pcmcia/sa1100_h3600.c
 *
 * PCMCIA implementation routines for H3600
 * All true functionality is shuttled off to the
 * pcmcia implementation for the current sleeve
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/i2c.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/pcmcia.h>
#include <asm/arch/h3600-sleeve.h>

static struct pcmcia_init  sa1100_h3600_pcmcia_init;       // Store the interrupt handler
struct pcmcia_low_level   *sa1100_h3600_pcmcia_ops = NULL; // Initialize with no sleeve

/* Forward declaractions */
int sa1100_h3600_common_pcmcia_get_irq_info( struct pcmcia_irq_info *info );

/***********************************************/
/* Stub routines called by the PCMCIA device.  */
/***********************************************/

static int h3600_pcmcia_init(struct pcmcia_init *init)
{
	sa1100_h3600_pcmcia_init = *init;

	if ( sa1100_h3600_pcmcia_ops != NULL
	     && sa1100_h3600_pcmcia_ops->init != NULL
	     && init->handler != NULL )
		return sa1100_h3600_pcmcia_ops->init( init );

	return 2;   /* Return by default */
}

static int h3600_pcmcia_shutdown(void)
{
	if ( sa1100_h3600_pcmcia_ops != NULL 
	     && sa1100_h3600_pcmcia_ops->shutdown != NULL )
		sa1100_h3600_pcmcia_ops->shutdown();

	return 0;   /* Not examined */
}

static int h3600_pcmcia_socket_state(struct pcmcia_state_array *state_array)
{
	if ( sa1100_h3600_pcmcia_ops != NULL 
	     && sa1100_h3600_pcmcia_ops->socket_state != NULL )
		return sa1100_h3600_pcmcia_ops->socket_state( state_array );

	/* Default actions */
        if ( state_array->size < 2 ) 
		return -1;

        memset(state_array->state, 0, (state_array->size)*sizeof(struct pcmcia_state));
	return 0;
}

static int h3600_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	if ( sa1100_h3600_pcmcia_ops != NULL 
	     && sa1100_h3600_pcmcia_ops->get_irq_info != NULL )
		return sa1100_h3600_pcmcia_ops->get_irq_info( info );

	return
		sa1100_h3600_common_pcmcia_get_irq_info( info );
}

static int h3600_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	if ( sa1100_h3600_pcmcia_ops != NULL 
	     && sa1100_h3600_pcmcia_ops->configure_socket != NULL )
		return sa1100_h3600_pcmcia_ops->configure_socket( configure );

        return 0;
}

struct pcmcia_low_level h3600_pcmcia_ops = { 
        h3600_pcmcia_init,
        h3600_pcmcia_shutdown,
        h3600_pcmcia_socket_state,
        h3600_pcmcia_get_irq_info,
        h3600_pcmcia_configure_socket
};

/****************************************************/
/*  Swapping functions for PCMCIA operations        */
/****************************************************/

void sa1100_h3600_pcmcia_change_sleeves(struct pcmcia_low_level *ops)
{
	if ( ops != sa1100_h3600_pcmcia_ops ) {
		h3600_pcmcia_shutdown();
		sa1100_h3600_pcmcia_ops = ops;
		h3600_pcmcia_init( &sa1100_h3600_pcmcia_init );
	}
}

void sa1100_h3600_pcmcia_remove_sleeve( void )
{
	if ( sa1100_h3600_pcmcia_ops != NULL ) {
		h3600_pcmcia_shutdown();
		sa1100_h3600_pcmcia_ops = NULL;
	}
}

EXPORT_SYMBOL(sa1100_h3600_pcmcia_change_sleeves);
EXPORT_SYMBOL(sa1100_h3600_pcmcia_remove_sleeve);


/****************************************************/
/*  Common functions used by the different sleeves */
/****************************************************/

int sa1100_h3600_common_pcmcia_init( struct pcmcia_init *init )
{
        int irq, res;

        /* Enable PCMCIA/CF bus: */
	set_h3600_egpio(IPAQ_EGPIO_OPT_ON);
        clr_h3600_egpio(IPAQ_EGPIO_OPT_RESET);
        clr_h3600_egpio(IPAQ_EGPIO_CARD_RESET);

        /* Set transition detect */
        set_GPIO_IRQ_edge( GPIO_H3600_PCMCIA_CD0 | GPIO_H3600_PCMCIA_CD1, GPIO_BOTH_EDGES );
        set_GPIO_IRQ_edge( GPIO_H3600_PCMCIA_IRQ0| GPIO_H3600_PCMCIA_IRQ1, GPIO_FALLING_EDGE );

        /* Register interrupts */
        irq = IRQ_GPIO_H3600_PCMCIA_CD0;
        res = request_irq( irq, init->handler, SA_INTERRUPT, "PCMCIA_CD0", NULL );
        if( res < 0 ) { 
		printk( KERN_ERR __FUNCTION__ ": Request for IRQ %u failed\n", irq );
		return -1;
	}

        irq = IRQ_GPIO_H3600_PCMCIA_CD1;
        res = request_irq( irq, init->handler, SA_INTERRUPT, "PCMCIA_CD1", NULL );
        if( res < 0 ) { 
		printk( KERN_ERR __FUNCTION__ ": Request for IRQ %u failed\n", irq );
		return -1;
	}

        return 2;  /* Always allow for two PCMCIA devices */
}

int sa1100_h3600_common_pcmcia_shutdown( void )
{
	/* disable IRQs */
	free_irq( IRQ_GPIO_H3600_PCMCIA_CD0, NULL );
	free_irq( IRQ_GPIO_H3600_PCMCIA_CD1, NULL );
  
	/* Disable CF bus: */
	clr_h3600_egpio(IPAQ_EGPIO_OPT_ON);
	set_h3600_egpio(IPAQ_EGPIO_OPT_RESET);
	return 0;
}

int sa1100_h3600_common_pcmcia_socket_state( struct pcmcia_state_array *state_array )
{
        unsigned long levels;

        if (state_array->size < 2) 
		return -1;

        memset(state_array->state, 0, (state_array->size)*sizeof(struct pcmcia_state));
	
	levels=GPLR;
	
	state_array->state[0].detect=((levels & GPIO_H3600_PCMCIA_CD0)==0)?1:0;
	state_array->state[0].ready=(levels & GPIO_H3600_PCMCIA_IRQ0)?1:0;
	state_array->state[1].detect=((levels & GPIO_H3600_PCMCIA_CD1)==0)?1:0;
	state_array->state[1].ready=(levels & GPIO_H3600_PCMCIA_IRQ1)?1:0;

	return 0;
}

int sa1100_h3600_common_pcmcia_get_irq_info( struct pcmcia_irq_info *info )
{
        switch (info->sock) {
        case 0:
                info->irq=IRQ_GPIO_H3600_PCMCIA_IRQ0;
                break;
        case 1:
                info->irq=IRQ_GPIO_H3600_PCMCIA_IRQ1;
                break;
        default:
                return -1;
        }
        return 0;
}

EXPORT_SYMBOL(sa1100_h3600_common_pcmcia_init);
EXPORT_SYMBOL(sa1100_h3600_common_pcmcia_shutdown);
EXPORT_SYMBOL(sa1100_h3600_common_pcmcia_socket_state);
EXPORT_SYMBOL(sa1100_h3600_common_pcmcia_get_irq_info);



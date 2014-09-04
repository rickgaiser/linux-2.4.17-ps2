/*
 *  smap.c -- PlayStation 2 Ethernet device driver
 *
 *	Copyright (C) 2001, 2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 */

#if defined(linux)

#include "smap.h"

struct smap_chan *smap_chan = NULL;

/*--------------------------------------------------------------------------*/

static void smap_skb_queue_init(struct smap_chan *smap, struct sk_buff_head *head);
static void smap_skb_enqueue(struct sk_buff_head *head, struct sk_buff *newsk);
static void smap_skb_requeue(struct sk_buff_head *head, struct sk_buff *newsk);
static struct sk_buff * smap_skb_dequeue(struct sk_buff_head *head);
static void smap_skb_queue_clear(struct smap_chan *smap, struct sk_buff_head *head);
static int  smap_start_xmit(struct sk_buff *skb, struct net_device *net_dev);
static int  smap_start_xmit2(struct smap_chan *smap);
static void smap_tx_intr(struct net_device *net_dev);
static void smap_rx_intr(struct net_device *net_dev);
static void smap_emac3_intr(struct net_device *net_dev);
static void smap_interrupt(int irq, void *dev_id, struct pt_regs *pt_regs);
static u_int8_t smap_bitrev(u_int8_t val);
static u_int32_t smap_crc32(u_int32_t crcval, u_int8_t cval);
static u_int32_t smap_calc_crc32(struct smap_chan *smap, u_int8_t *addr);
static int  smap_store_new_mc_list(struct smap_chan *smap);
static void smap_multicast_list(struct net_device *net_dev);
static struct net_device_stats * smap_get_stats(struct net_device *net_dev);
static int  smap_open(struct net_device *net_dev);
static int  smap_close(struct net_device *net_dev);
static int  smap_ioctl(struct net_device *net_dev, struct ifreq *ifr, int cmd);
#ifdef HAVE_TX_TIMEOUT
static void smap_tx_timeout(struct net_device *net_dev);
static int  smap_timeout_thread(void *arg);
#endif /* HAVE_TX_TIMEOUT */
static void smap_clear_all_interrupt(struct smap_chan *smap);
static void smap_interrupt_XXable(struct smap_chan *smap, int enable_flag);
static void smap_txrx_XXable(struct smap_chan *smap, int enable_flag);
static void smap_txbd_init(struct smap_chan *smap);
static void smap_rxbd_init(struct smap_chan *smap);
static int  smap_read_phy(struct smap_chan *smap, u_int32_t phyadr, u_int32_t regadr);
static int  smap_write_phy(struct smap_chan *smap, u_int32_t phyadr, u_int32_t regadr, u_int16_t data);
static int smap_fifo_reset(struct smap_chan *smap);
static void smap_reg_init(struct smap_chan *smap);
static int  smap_emac3_soft_reset(struct smap_chan *smap);
static void smap_emac3_set_defvalue(struct smap_chan *smap);
static void smap_emac3_init(struct smap_chan *smap, int reset_only);
static void smap_emac3_re_init(struct smap_chan *smap);
static int  smap_phy_init(struct smap_chan *smap, int reset_only);
static int  smap_phy_reset(struct smap_chan *smap);
static int  smap_auto_negotiation(struct smap_chan *smap, int enable_auto_nego);
static int  smap_confirm_auto_negotiation(struct smap_chan *smap);
static void smap_force_spd_100M(struct smap_chan *smap);
static void smap_force_spd_10M(struct smap_chan *smap);
static void smap_confirm_force_spd(unsigned long arg);
static void smap_phy_set_dsp(struct smap_chan *smap);
static void smap_reset(struct smap_chan *smap, int reset_only);
static void smap_print_mac_address(struct smap_chan *smap, u_int8_t *addr);
static int  smap_get_node_addr(struct smap_chan *smap);
static void smap_base_init(struct smap_chan *smap);
static void smap_dump_packet(struct smap_chan *smap, u_int8_t *ptr, int length);
static void smap_dump_txbd(struct smap_chan *smap);
static void smap_dump_rxbd(struct smap_chan *smap);
static void smap_dump_reg(struct smap_chan *smap);
static void smap_dump_emac3_reg(struct smap_chan *smap);
static void smap_dma_force_break(struct smap_chan *smap);
static void smap_rpcend_notify(void *arg);
static void smap_dma_setup(struct smap_chan *smap);
static void smap_run(struct smap_chan *smap);
static int  smap_thread(void *arg);
static void smap_chk_linkvalid(struct smap_chan *smap);
static int  smap_chk_linkvalid_thread(void *arg);
static int  smap_init_thread(void *arg);

/*--------------------------------------------------------------------------*/

static void
smap_skb_queue_init(struct smap_chan *smap, struct sk_buff_head *head)
{
	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);
	(void)skb_queue_head_init(head);
	spin_unlock_irqrestore(&smap->spinlock, flags);

	return;
}

static void
smap_skb_enqueue(struct sk_buff_head *head, struct sk_buff *newsk)
{
	(void)skb_queue_tail(head, newsk);
	return;
}

static void
smap_skb_requeue(struct sk_buff_head *head, struct sk_buff *newsk)
{
	(void)skb_queue_head(head, newsk);
	return;
}

static struct sk_buff *
smap_skb_dequeue(struct sk_buff_head *head)
{
	struct sk_buff *skb;

	skb = skb_dequeue(head);
	return(skb);
}

static void
smap_skb_queue_clear(struct smap_chan *smap, struct sk_buff_head *head)
{
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);
	while (head->qlen) {
		skb = smap_skb_dequeue(head);
		if (skb)
			dev_kfree_skb(skb);
	}
	spin_unlock_irqrestore(&smap->spinlock, flags);

	return;
}

/*--------------------------------------------------------------------------*/

/* return value: 0 if success, !0 if error */
static int
smap_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;
	int qmax;
	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);

	if ((smap->flags & SMAP_F_LINKVALID) == 0) {
		printk("%s: xmit: link not valid\n", net_dev->name);
		spin_unlock_irqrestore(&smap->spinlock, flags);
		return(-ENETDOWN);
	}

	qmax = (smap->flags & SMAP_F_DMA_TX_ENABLE) ? SMAP_BD_MAX_ENTRY/2 : 0;
	if (smap->txqueue.qlen > qmax) {
		netif_stop_queue(net_dev);
		spin_unlock_irqrestore(&smap->spinlock, flags);
		return(-EAGAIN);
	}

	(void)smap_skb_enqueue(&smap->txqueue, skb);
	wake_up_interruptible(&smap->wait_smaprun);
	spin_unlock_irqrestore(&smap->spinlock, flags);
	return(0);
}

static int
smap_start_xmit2(struct smap_chan *smap)
{
	struct net_device *net_dev = smap->net_dev;
	struct sk_buff *skb = NULL;
	int retval;
	int i, txlen;
	int tx_re_q;	/* re-queue and do again */
	u_int16_t tmp_txbwp;
	u_int32_t *datap;
	volatile struct smapbd *txbd;
	struct completion compl;
	unsigned long flags;

	tx_re_q = CLEAR;
	tmp_txbwp = smap->txbwp;
	smap->txdma_request.count = smap->txdma_request.size = 0;
	for (i = 0; i < SMAP_DMA_ENTRIES; i++) {
		skb = NULL;

		if (((smap->flags & SMAP_F_DMA_TX_ENABLE) == 0) && (i > 0))
			break;
		spin_lock_irqsave(&smap->spinlock, flags);
		skb = smap_skb_dequeue(&smap->txqueue);
		spin_unlock_irqrestore(&smap->spinlock, flags);
		if (skb == NULL)
			break;
		if (smap->txbdusedcnt + smap->txdma_request.count
						>= (SMAP_BD_MAX_ENTRY-2)) {
			printk("%s: xmit: txbd is full\n", net_dev->name);
			tx_re_q |= (1 << i);
			break;
		}

		/* check datalen and free buffer space */
		/* txlen is multiple of 4 */
		txlen = (skb->len + 3) & ~3;

		if (smap->txdma_request.size + txlen > smap->txfreebufsize) {
			tx_re_q |= (1 << i);
			break;
		}

		if (smap->flags & SMAP_F_DMA_TX_ENABLE) {
			memcpy((char *)(smap->txdma_ibuf + smap->txdma_request.size), skb->data, skb->len);
			smap->txdma_request.sdd[i].i_addr = 
				(unsigned int)(smap->txdma_ibuf + smap->txdma_request.size);
		} else {
			smap->txdma_request.sdd[i].i_addr = 0;
		}
		smap->txdma_request.sdd[i].f_addr = (tmp_txbwp & 0x0FFC);
		smap->txdma_request.sdd[i].size = txlen;
		smap->txdma_request.sdd[i].sdd_misc = (unsigned int)skb;
		smap->txdma_request.size += txlen;
		smap->txdma_request.count++;

		tmp_txbwp = SMAP_TXBUFBASE +
			((tmp_txbwp + txlen - SMAP_TXBUFBASE)%SMAP_TXBUFSIZE);
	}
	if (tx_re_q && skb) {
		spin_lock_irqsave(&smap->spinlock, flags);
		(void)smap_skb_requeue(&smap->txqueue, skb);
		spin_unlock_irqrestore(&smap->spinlock, flags);
	}
	if (i == 0)
		goto end;

	if (smap->flags & SMAP_F_DMA_TX_ENABLE) {
		init_completion(&compl);
		ps2sif_writebackdcache((void *)smap->txdma_ibuf,
						smap->txdma_request.size);
		smap->txdma_request.command =
				smap->txdma_request.sdd[0].f_addr;	/*XXX*/
		smap->txdma_request.devctrl = 0;
		smap->dma_result = 0;

		if (ps2sif_callrpc(&smap->cd_smap_tx, SIFNUM_SmapDmaWrite,
				SIF_RPCM_NOWAIT,
				(void *)&smap->txdma_request,
				sizeof(int) * 4,
				&smap->dma_result, sizeof(u_int32_t),
				(ps2sif_endfunc_t)smap_rpcend_notify,
				(void *)&compl) != 0) {
			if (smap->flags & SMAP_F_PRINT_MSG) {
				printk("%s: xmit: callrpc failed, do pio to send this packet.pkt(%d)\n", net_dev->name,smap->txdma_request.count);
			}
			goto smappiosend;
		}
		wait_for_completion(&compl);
		if (smap->dma_result != 0) {
			printk("%s: xmit: dma break (%d)\n",
					net_dev->name,smap->dma_result);
			goto end;
		}

	} else {
smappiosend:
		skb = (struct sk_buff *)smap->txdma_request.sdd[0].sdd_misc;
		/* clear padding bytes */
		*(int *)&smap->txbuf[skb->len & ~3] = 0;
		memcpy(smap->txbuf, skb->data, skb->len);

		spin_lock_irqsave(&smap->spinlock, flags);
		/* send from memory to FIFO */
		SMAPREG16(smap,SMAP_TXFIFO_WR_PTR) =
				smap->txdma_request.sdd[0].f_addr;
		datap = (u_int32_t *)smap->txbuf;
		for (i = 0; i < smap->txdma_request.sdd[0].size; i += 4) {
							/* memory -> FIFO */
			SMAPREG32(smap,SMAP_TXFIFO_DATA) = *datap++;
		}
		spin_unlock_irqrestore(&smap->spinlock, flags);

		/* re-queue unsend packets */
		for (i = smap->txdma_request.count - 1; i > 0; i--) {
			skb = (struct sk_buff *)smap->txdma_request.sdd[i].sdd_misc;
			smap->txdma_request.sdd[i].sdd_misc = 0;
			if (skb) {
				spin_lock_irqsave(&smap->spinlock, flags);
				(void)smap_skb_requeue(&smap->txqueue, skb);
				spin_unlock_irqrestore(&smap->spinlock, flags);
				tx_re_q |= (1 << i);
			}
		}
		smap->txdma_request.count = 1;
	}

	if (smap->flags & SMAP_F_PRINT_PKT) {
		for (i = 0; i < smap->txdma_request.count; i++) {
			skb = (struct sk_buff *)smap->txdma_request.sdd[i].sdd_misc;
			printk("%s: xmit: mem->fifo done,len=%d,%d,ptr=0x%04x\n",
				net_dev->name,skb->len,
				smap->txdma_request.sdd[i].size,
				smap->txdma_request.sdd[i].f_addr);
			smap_dump_packet(smap, skb->data,
					(skb->len < 60) ? skb->len: 60);

		}
	}

	spin_lock_irqsave(&smap->spinlock, flags);
	for (i = 0; i < smap->txdma_request.count; i++) {
		txbd = &smap->txbd[smap->txbds];
		skb = (struct sk_buff *)smap->txdma_request.sdd[i].sdd_misc;

		smap->txfreebufsize -= smap->txdma_request.sdd[i].size;

		/* send from FIFO to ethernet */
		txbd->length = skb->len;
		txbd->pointer = SMAP_TXBUFBASE + smap->txdma_request.sdd[i].f_addr;

		SMAPREG8(smap,SMAP_TXFIFO_FRAME_INC) = 1;

		txbd->ctrl_stat =
			(SMAP_BD_TX_READY|SMAP_BD_TX_GENFCS|SMAP_BD_TX_GENPAD);
		smap->txbdusedcnt++;

		/* renew buffer descriptor */
		SMAP_BD_NEXT(smap->txbds);
	}

	/* MULTI PACKET MODE */
	EMAC3REG_WRITE(smap, SMAP_EMAC3_TxMODE0, E3_TX_GNP_0);
						/* FIFO->ethernet */
	if (smap->flags & SMAP_F_TXDNV_DISABLE) {
		smap->flags &= ~SMAP_F_TXDNV_DISABLE;
		SMAPREG16(smap,SMAP_INTR_ENABLE) |= INTR_TXDNV;
		SMAPREG16(smap,SMAP_INTR_CLR) = INTR_TXDNV;
	}
	spin_unlock_irqrestore(&smap->spinlock, flags);

	/* renew write pointer */
	if (smap->txdma_request.count > 0) {
		i = smap->txdma_request.count - 1;
		smap->txbwp = SMAP_TXBUFBASE +
			((smap->txdma_request.sdd[i].f_addr + smap->txdma_request.sdd[i].size)%SMAP_TXBUFSIZE);
	} else {
		smap->txbwp = SMAP_TXBUFBASE;
	}

	net_dev->trans_start = jiffies;		/* save the timestamp */

end:
	for (i = 0; i < smap->txdma_request.count; i++) {
		skb = (struct sk_buff *)smap->txdma_request.sdd[i].sdd_misc;
		smap->txdma_request.sdd[i].sdd_misc = 0;
		if (skb) {
			dev_kfree_skb(skb);
		}
	}
	spin_lock_irqsave(&smap->spinlock, flags);
	if (tx_re_q || smap->txqueue.qlen > 0)
		retval = -EAGAIN;
	else
		retval = 0;
	spin_unlock_irqrestore(&smap->spinlock, flags);
	return(retval);
}

/*--------------------------------------------------------------------------*/

static void
smap_tx_intr(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;
	volatile struct smapbd *txbd;
	int txlen, error;
	u_int16_t txstat;
	unsigned long flags;

	txbd = &smap->txbd[smap->txbdi];
	txstat = txbd->ctrl_stat;

	while (((txstat & SMAP_BD_TX_READY) == 0) && (smap->txbdusedcnt > 0)) {
		if (smap->flags & SMAP_F_PRINT_PKT) {
			printk("%s: tx intr: process packet,"
				"[%d]=stat=0x%04x,len=%d,ptr=0x%04x\n",
					net_dev->name,smap->txbdi,txstat,
					txbd->length,txbd->pointer);
		}
		/* txlen is multiple of 4 */
		txlen = (txbd->length + 3) & ~3;
		smap->txfreebufsize += txlen;

		error = 0;
		if (txstat & 0x7FFF) {
			if (txstat & SMAP_BD_TX_BADFCS) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: bad FCS\n", net_dev->name);
				}
			}
			if (txstat & SMAP_BD_TX_BADPKT) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: bad previous packet\n", net_dev->name);
				}
			}
			if (txstat & SMAP_BD_TX_LOSSCR) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: loss of carrier\n", net_dev->name);
				}
			}
			if (txstat & SMAP_BD_TX_EDEFER) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: excessive deferral\n", net_dev->name);
				}
			}
			if (txstat & SMAP_BD_TX_ECOLL) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: excessive collisions\n", net_dev->name);
				}
			}
			if (txstat & SMAP_BD_TX_LCOLL) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: late collision\n", net_dev->name);
				}
			}
			if (txstat & SMAP_BD_TX_MCOLL) {
				smap->net_stats.collisions++;	/*XXX*/
			}
			if (txstat & SMAP_BD_TX_SCOLL) {
				smap->net_stats.collisions++;
			}
			if (txstat & SMAP_BD_TX_UNDERRUN) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: underrun\n", net_dev->name);
				}
			}
			if (txstat & SMAP_BD_TX_SQE) {
				error++;
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:tx intr: sqe test failed\n", net_dev->name);
				}
			}
			if ((error > 0) && (smap->flags & SMAP_F_PRINT_MSG)) {
				printk("%s:Tx intr: [%d]=stat(0x%04x, 0x%04x), len(%d, 0x%04x), ptr(0x%04x)\n", net_dev->name, smap->txbdi,txstat,txbd->ctrl_stat,txbd->length,txbd->length,txbd->pointer);
			}
		}

		if (error == 0) {
			smap->net_stats.tx_packets++;
			smap->net_stats.tx_bytes += txbd->length;
		} else {
			smap->net_stats.tx_errors++;
		}

		smap->txbdusedcnt--;

#if 0
		spin_lock_irqsave(&smap->spinlock, flags);
		/* renew txbd */
		txbd->length = 0;
		txbd->pointer = 0;
		txbd->ctrl_stat = 0;
		spin_unlock_irqrestore(&smap->spinlock, flags);
#endif

		/* renew buffer descriptor */
		SMAP_BD_NEXT(smap->txbdi);
		txbd = &smap->txbd[smap->txbdi];
		txstat = txbd->ctrl_stat;
	}

	if (smap->flags & SMAP_F_OPENED) {
		netif_wake_queue(net_dev);
	}

	spin_lock_irqsave(&smap->spinlock, flags);
	smap->txicnt--;
	spin_unlock_irqrestore(&smap->spinlock, flags);
	return;
}

static void
smap_rx_intr(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;
	volatile struct smapbd *rxbd;
	u_int16_t rxstat;
	int pkt_err, pioflag;
	int pkt_len;
	int i, rxlen;
	u_int32_t *datap;
	struct sk_buff *skb;
	u_int8_t *rxbp;
	int l_rxbdi;
	int validrcvpkt, rcvpkt;
	struct completion compl;
	unsigned long flags;

	pkt_err = pioflag = CLEAR;
	validrcvpkt = rcvpkt = 0;
	smap->rxdma_request.size = 0;

	l_rxbdi = smap->rxbdi;

	for (i = 0; i < SMAP_DMA_ENTRIES; i++) {
		rxbd = &smap->rxbd[l_rxbdi];
		rxstat = rxbd->ctrl_stat;

		if (rxstat & SMAP_BD_RX_EMPTY)
			break;

		if (((smap->flags & SMAP_F_DMA_RX_ENABLE) == 0) && (i > 0))
			break;

		if (rxstat & 0x7FFF) {
			if (rxstat & SMAP_BD_RX_OVERRUN) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): overrun\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_PFRM) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): pause frame\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_BADFRM) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): bad frame\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_RUNTFRM) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): runt frame\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_SHORTEVNT) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): short event\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_ALIGNERR) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): align error\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_BADFCS) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): bad FCS\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_FRMTOOLONG) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): frame too long\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_OUTRANGE) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): out range error\n", net_dev->name, i);
				}
			}
			if (rxstat & SMAP_BD_RX_INRANGE) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): in range error\n", net_dev->name, i);
				}
			}
			if (smap->flags & SMAP_F_PRINT_MSG) {
				printk("%s:Rx intr(%d): [%d]=stat(0x%04x, 0x%04x), len(%d, 0x%04x), ptr(0x%04x)\n", net_dev->name, i, l_rxbdi, rxstat,rxbd->ctrl_stat,rxbd->length,rxbd->length,rxbd->pointer);
			} 
			pkt_err |= (1 << i);
			break;
		}

		pkt_len = rxbd->length;

		if ((pkt_len < SMAP_RXMINSIZE) || (pkt_len > SMAP_RXMAXSIZE)) {
			if (smap->flags & SMAP_F_PRINT_MSG) {
				printk("%s:rx intr(%d): packet length error (%d)\n", net_dev->name, i, pkt_len);
			}
			pkt_err |= (1 << i);
			break;
		}

		/* rxlen is multiple of 4 */
		rxlen = (pkt_len + 3) & ~3;

		if (smap->flags & SMAP_F_DMA_RX_ENABLE) {
			smap->rxdma_request.sdd[i].i_addr =
			  (unsigned int)(smap->rxdma_ibuf+smap->rxdma_request.size);
		} else {
			smap->rxdma_request.sdd[i].i_addr = 0;
		}
		smap->rxdma_request.sdd[i].f_addr =
					(unsigned int)(rxbd->pointer&0x3FFC);
		smap->rxdma_request.sdd[i].size = rxlen;
		smap->rxdma_request.sdd[i].sdd_misc = pkt_len;
		smap->rxdma_request.size += rxlen;

		SMAP_BD_NEXT(l_rxbdi);
	}
	validrcvpkt = rcvpkt = i;
	if (pkt_err)
		rcvpkt++;

	if (validrcvpkt == 0)
		goto end;

	if (smap->flags & SMAP_F_DMA_RX_ENABLE) {
		init_completion(&compl);
		ps2sif_writebackdcache((void *)smap->rxdma_ibuf,
						smap->rxdma_request.size);
		smap->rxdma_request.command =
				smap->rxdma_request.sdd[0].f_addr;	/*XXX*/
		smap->rxdma_request.devctrl = 0;
		smap->rxdma_request.count = validrcvpkt;
		smap->dma_result = 0;

		if (ps2sif_callrpc(&smap->cd_smap_rx,SIFNUM_SmapDmaRead,
				SIF_RPCM_NOWAIT,
				(void *)&smap->rxdma_request,
				sizeof(int) * 4,
				&smap->dma_result, sizeof(u_int32_t),
				(ps2sif_endfunc_t)smap_rpcend_notify,
				(void *)&compl) != 0) {
			if (smap->flags & SMAP_F_PRINT_MSG) {
				printk("%s:rx intr: callrpc failed, do pio to receive this packet.\n", net_dev->name);
			}
			goto smappiorecv;
		}
		wait_for_completion(&compl);
		if (smap->dma_result != 0) {
			printk("%s: recv: dma break (%d)\n",
					net_dev->name,smap->dma_result);
			goto end;
		}

	} else {
smappiorecv:
		spin_lock_irqsave(&smap->spinlock, flags);
		/* recv from FIFO to memory */
		SMAPREG16(smap,SMAP_RXFIFO_RD_PTR) =
			(u_int16_t)smap->rxdma_request.sdd[0].f_addr;
		datap = (u_int32_t *)smap->rxbuf;
		rxlen = smap->rxdma_request.sdd[0].size;
		for (i = 0; i < rxlen; i += 4) {	/* FIFO -> memory */
			*datap++ = SMAPREG32(smap,SMAP_RXFIFO_DATA);
		}
		spin_unlock_irqrestore(&smap->spinlock, flags);
		pkt_err = CLEAR;
		pioflag = SET;
		validrcvpkt = rcvpkt = 1;
	}
	if (smap->flags & SMAP_F_PRINT_PKT) {
		l_rxbdi = smap->rxbdi;
		for (i = 0; i < validrcvpkt; i++) {
			rxbd = &smap->rxbd[l_rxbdi];
			printk("%s: rx: fifo->mem done,"
				"[%d]=stat=0x%04x,len=%d,ptr=0x%04x\n",
				net_dev->name,l_rxbdi,rxbd->ctrl_stat,
				rxbd->length,rxbd->pointer);
			if (pioflag) {
				rxbp = smap->rxbuf;
			} else {
				rxbp = (u_int8_t *)(smap->rxdma_request.sdd[i].i_addr);
			}
			smap_dump_packet(smap, rxbp, (rxbd->length < 60) ? rxbd->length : 60);
			SMAP_BD_NEXT(l_rxbdi);
		}
	}
#if 1	/*XXX*//* it may be able to delete this clause if 2.4 */
	{
	/*
	 * eth_copy_and_sum() expects that ETH_P_IP packet has
	 * 34 or more bytes in "length" argument unconditionally.
	 */
		for (i = 0; i < rcvpkt; i++) {
			struct ethhdr *eth;
			if (pkt_err & (1 << i))
				continue;
			if (pioflag) {
				rxbp = smap->rxbuf;
			} else {
				rxbp = (u_int8_t *)(smap->rxdma_request.sdd[i].i_addr);
			}
			eth = (struct ethhdr *)rxbp;
			pkt_len = smap->rxdma_request.sdd[i].sdd_misc;
			if ( (eth->h_proto == htons(ETH_P_IP)) &&
						(pkt_len < 14 + 20) ) {
				if (smap->flags & SMAP_F_PRINT_MSG) {
					printk("%s:rx intr(%d): IP packet len(%d) error\n",
						net_dev->name, i, pkt_len);
				}
				pkt_err |= (1 << i);
			}
		}
	}
#endif
	for (i = 0; i < rcvpkt; i++) {
		if (pkt_err & (1 << i))
			continue;
		if (pioflag) {
			rxbp = smap->rxbuf;
		} else {
			rxbp = (u_int8_t *)(smap->rxdma_request.sdd[i].i_addr);
		}
		pkt_len = smap->rxdma_request.sdd[i].sdd_misc;

		skb = dev_alloc_skb(pkt_len + 2);
		if (skb == NULL) {
			printk("%s:rx intr(%d): skb alloc error\n",
						net_dev->name, i);
			break;
		}
		skb_reserve(skb, 2);	/* 16 byte align the data fields */
#if 1
		eth_copy_and_sum(skb, rxbp, pkt_len, 0);
		skb_put(skb, pkt_len);
#else
		memcpy(skb_put(skb, pkt_len), rxbp, pkt_len);
#endif
		skb->dev = net_dev;
		skb->protocol = eth_type_trans(skb, net_dev);
		net_dev->last_rx = jiffies;
		netif_rx(skb);
	}

end:
	spin_lock_irqsave(&smap->spinlock, flags);
	rxbd = &smap->rxbd[smap->rxbdi];
	for (i = 0; i < rcvpkt; i++) {
		SMAPREG8(smap,SMAP_RXFIFO_FRAME_DEC) = 1;

		if (pkt_err & (1 << i)) {
			smap->net_stats.rx_errors++;
		} else {
			smap->net_stats.rx_packets++;
			smap->net_stats.rx_bytes += smap->rxdma_request.sdd[i].sdd_misc;
		}

		/* renew rxbd */
#if 0
		rxbd->length = 0;
		rxbd->pointer = 0;
#endif
		rxbd->ctrl_stat = SMAP_BD_RX_EMPTY;

		/* renew buffer descriptor */
		SMAP_BD_NEXT(smap->rxbdi);
		rxbd = &smap->rxbd[smap->rxbdi];
	}

	if ((smap->flags & SMAP_F_RXDNV_DISABLE) && (rcvpkt > 0)) {
		smap->flags &= ~SMAP_F_RXDNV_DISABLE;
		SMAPREG16(smap,SMAP_INTR_ENABLE) |= INTR_RXDNV;
		SMAPREG16(smap,SMAP_INTR_CLR) = INTR_RXDNV;
	}

	smap->rxicnt--;
	spin_unlock_irqrestore(&smap->spinlock, flags);

	return;
}

static void
smap_emac3_intr(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;
	u_int32_t stat, ena;

	stat = EMAC3REG_READ(smap, SMAP_EMAC3_INTR_STAT);
	ena = EMAC3REG_READ(smap, SMAP_EMAC3_INTR_ENABLE);

	/* clear emac3 interrupt */
	EMAC3REG_WRITE(smap, SMAP_EMAC3_INTR_STAT, stat);

	stat &= (ena|E3_DEAD_ALL);
	if (stat & E3_INTR_OVERRUN) {		/* this bit does NOT WORKED */
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx overrun\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_PF) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx pause frame\n",net_dev->name);
		}
	}
	if (stat & E3_INTR_BAD_FRAME) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx bad frame\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_RUNT_FRAME) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx runt frame\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_SHORT_EVENT) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx short event\n",net_dev->name);
		}
	}
	if (stat & E3_INTR_ALIGN_ERR) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx alignment error\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_BAD_FCS) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx bad FCS\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_TOO_LONG) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx frame too long\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_OUT_RANGE_ERR) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx out range error\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_IN_RANGE_ERR) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: rx in range error\n", net_dev->name);
		}
	}

	if (stat & E3_INTR_DEAD_DEPEND) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: tx dead in dependent mode\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_DEAD_0) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: tx dead in channel 0\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_SQE_ERR_0) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: tx sqe test error in channel 0\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_TX_ERR_0) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: tx transmit error in channel 0\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_DEAD_1) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: tx dead in channel 1\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_SQE_ERR_1) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: tx sqe test error in channel 1\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_TX_ERR_1) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: tx transmit error in channel 1\n", net_dev->name);
		}
	}
	if (stat & E3_INTR_MMAOP_FAIL) {
		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s:emac3 intr: phy operation failed\n", net_dev->name);
		}
	}

	return;
}

static void
smap_interrupt(int irq, void *dev_id, struct pt_regs *pt_regs)
{
	struct net_device *net_dev = (struct net_device *)dev_id;
	struct smap_chan *smap = net_dev->priv;
	unsigned long flags;
	u_int16_t stat, ena;

	spin_lock_irqsave(&smap->spinlock, flags);

	stat = SMAPREG16(smap,SMAP_INTR_STAT) & INTR_BITMSK;
	ena =  SMAPREG16(smap,SMAP_INTR_ENABLE) & INTR_BITMSK;
	stat &= ena;
	if (stat == 0)
		goto end;

	if (stat & INTR_TXDNV) {
		/* disable TXDNV interrupt */
		SMAPREG16(smap,SMAP_INTR_ENABLE) &= ~INTR_TXDNV;
		smap->flags |= SMAP_F_TXDNV_DISABLE;
		/* clear interrupt */
		SMAPREG16(smap,SMAP_INTR_CLR) = INTR_TXDNV;

		smap->txicnt++;
		wake_up_interruptible(&smap->wait_smaprun);
		EMAC3REG_WRITE(smap, SMAP_EMAC3_INTR_STAT, E3_DEAD_ALL);
	}
	if (stat & INTR_RXDNV) {
		/* disable RXDNV interrupt */
		SMAPREG16(smap,SMAP_INTR_ENABLE) &= ~INTR_RXDNV;
		smap->flags |= SMAP_F_RXDNV_DISABLE;
		/* clear interrupt */
		SMAPREG16(smap,SMAP_INTR_CLR) = INTR_RXDNV;

		if (smap->flags & SMAP_F_PRINT_MSG) {
			printk("%s: intr: RX desc not valid\n",net_dev->name);
		}
		smap->rxicnt++;
		wake_up_interruptible(&smap->wait_smaprun);
	}
	if (stat & INTR_TXEND) {
		SMAPREG16(smap,SMAP_INTR_CLR) = INTR_TXEND;
		/* workaround for race condition of TxEND/RxEND */
		if (SMAPREG8(smap,SMAP_RXFIFO_FRAME_CNT) > 0) {
			smap->rxicnt++;
		}
		smap->txicnt++;
		wake_up_interruptible(&smap->wait_smaprun);
	}
	if (stat & INTR_RXEND) {
		SMAPREG16(smap,SMAP_INTR_CLR) = INTR_RXEND;
		/* workaround for race condition of TxEND/RxEND */
		if ((smap->txbdusedcnt > 0) &&
		    (smap->txbdusedcnt > SMAPREG8(smap,SMAP_TXFIFO_FRAME_CNT))
		   ) {
			smap->txicnt++;
		}
		smap->rxicnt++;
		wake_up_interruptible(&smap->wait_smaprun);
	}
	if (stat & INTR_EMAC3) {
		(void)smap_emac3_intr(net_dev);
	}
end:
	spin_unlock_irqrestore(&smap->spinlock, flags);
	return;
}

/*--------------------------------------------------------------------------*/

#define	POLY32	0x04C11DB7

static u_int8_t
smap_bitrev(u_int8_t val)
{
	int i;
	u_int8_t ret = 0;

	for (i = 0; i < 8; i++) {
		ret <<= 1;
		ret |= (val & 0x01) ? 1 : 0;
		val >>= 1;
	}
	return(ret);
}

static u_int32_t
smap_crc32(u_int32_t crcval, u_int8_t cval)
{
	int i;

	crcval ^= cval << 24;
	for (i = 0; i < 8; i++) {
		crcval = crcval & 0x80000000 ? (crcval << 1) ^ POLY32 : crcval << 1;
	}

	return(crcval);
}

static u_int32_t
smap_calc_crc32(struct smap_chan *smap, u_int8_t *addr)
{
	int i;
	u_int32_t crc;

	crc = 0xFFFFFFFF;
	for (i = 0; i < ETH_ALEN; i++)
		crc = smap_crc32(crc, smap_bitrev(*addr++));

	return(crc ^ 0xFFFFFFFF);
}

static int
smap_store_new_mc_list(struct smap_chan *smap)
{
	struct net_device *net_dev = smap->net_dev;
	struct dev_mc_list *mcp;
	int idx, reg, bit, sethtbl = 0;
	u_int32_t val[4];

	/* clear HW gourp list */
	EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH1, 0x0);
	EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH2, 0x0);
	EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH3, 0x0);
	EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH4, 0x0);
	val[0] = val[1] = val[2] = val[3] = 0;

	for (mcp = net_dev->mc_list; mcp; mcp = mcp->next) {
		/* set new HW group list */

		if ((mcp->dmi_addr[0]&0x1) == 0)
			continue;
		if (mcp->dmi_addrlen != ETH_ALEN)
			continue;
		idx = smap_calc_crc32(smap, mcp->dmi_addr);
		idx = (idx >> 26) & 0x3f;
		reg = idx/16;
		bit = 15 - (idx%16);
		val[reg] |= (1 << bit);
		sethtbl = 1;
	}
	if (sethtbl) {
		/* set HW group list */
		EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH1, val[0]);
		EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH2, val[1]);
		EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH3, val[2]);
		EMAC3REG_WRITE(smap, SMAP_EMAC3_GROUP_HASH4, val[3]);
	}
	return(sethtbl);
}

static void
smap_multicast_list(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;
	u_int32_t e3v;

	/* stop tx/rx */
	(void)smap_txrx_XXable(smap, DISABLE);

	/* disable promisc, all multi, indvi hash and group hash mode */
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_RxMODE);
	e3v &= ~(E3_RX_PROMISC|E3_RX_PROMISC_MCAST|E3_RX_INDIVID_HASH|E3_RX_MCAST);
	EMAC3REG_WRITE(smap, SMAP_EMAC3_RxMODE, e3v);

	if (net_dev->flags & IFF_PROMISC) {
		e3v |= E3_RX_PROMISC;
	} else if (net_dev->flags & IFF_ALLMULTI) {
		e3v |= E3_RX_PROMISC_MCAST;
	} else if (net_dev->mc_count == 0) {
	    /* Nothing to do, because INDIVID_ADDR & BCAST are already set */
	} else {
		if (smap_store_new_mc_list(smap))
			e3v |= E3_RX_MCAST;
	}

	/* set RxMODE register */
	EMAC3REG_WRITE(smap, SMAP_EMAC3_RxMODE, e3v);

	/* start tx/rx */
	(void)smap_txrx_XXable(smap, ENABLE);

	return;
}

/*--------------------------------------------------------------------------*/

static struct net_device_stats *
smap_get_stats(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;

	return(&smap->net_stats);
}

static int
smap_open(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;
	int r;

	if (smap->flags & SMAP_F_PRINT_MSG) {
		printk("%s: PlayStation 2 SMAP open\n", net_dev->name);
	}

	for (r = 22 * 5; r; r--) {
		if ( (smap->flags & SMAP_F_LINKVALID) &&
		     (smap->flags & SMAP_F_INITDONE) )
			break;
		interruptible_sleep_on_timeout(&smap->wait_linkvalid, HZ/5);
	}
	if (!((smap->flags & SMAP_F_LINKVALID) &&
				(smap->flags & SMAP_F_INITDONE))) {
		printk("%s: link not valid\n", net_dev->name);
		return(-EPERM);
	}
	if (smap->flags & SMAP_F_OPENED) {
		printk("%s: already opend\n", net_dev->name);
		return(-EBUSY);
	}
	(void)smap_fifo_reset(smap);
	(void)smap_emac3_re_init(smap);
	(void)smap_txbd_init(smap);
	(void)smap_rxbd_init(smap);

	if (smap->irq == 0) {
		printk("%s: invalid irq\n", net_dev->name);
		return(-ENODEV);
	}
	r = request_irq(smap->irq, smap_interrupt, SA_INTERRUPT|SA_SHIRQ,
					"PlayStation 2 Ethernet", net_dev);
	if (r) {
		printk("%s: re-try request_irq(now error=%d)\n",
							net_dev->name,r);
		r = request_irq(smap->irq, smap_interrupt, SA_SHIRQ,
					"PlayStation 2 Ethernet", net_dev);
		if (r) {
			printk("%s: request_irq error(%d)\n", net_dev->name,r);
			return(-ENODEV);
		}
	}

	(void)smap_skb_queue_init(smap, &smap->txqueue);
	smap->txicnt = smap->rxicnt = 0;

	(void)smap_clear_all_interrupt(smap);
	(void)smap_interrupt_XXable(smap, ENABLE);

	(void)smap_txrx_XXable(smap, ENABLE);

	netif_start_queue(net_dev);

	smap->flags |= SMAP_F_OPENED;

	return(0);	/* success */
}

static int
smap_close(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;
	unsigned long flags;
	struct completion compl;

	if (smap->flags & SMAP_F_PRINT_MSG) {
		printk("%s: PlayStation 2 SMAP close\n", net_dev->name);
	}

	if ((smap->flags & SMAP_F_OPENED) == 0) {
		printk("PlayStation 2 SMAP: not opened\n");
		return(-EINVAL);
	}

	spin_lock_irqsave(&smap->spinlock, flags);
	smap->flags &= ~SMAP_F_OPENED;

	if (smap->txqueue.qlen > 0) {
		init_completion(&compl);
		smap->tx_qpkt_compl = &compl;
		wake_up_interruptible(&smap->wait_smaprun);
		spin_unlock_irqrestore(&smap->spinlock, flags);

		/* wait packet sending done */
		wait_for_completion(&compl);
		smap->tx_qpkt_compl = NULL;
	} else {
		spin_unlock_irqrestore(&smap->spinlock, flags);
	}

	netif_stop_queue(net_dev);

	/* stop DMA */
	(void)smap_dma_force_break(smap);

	(void)smap_skb_queue_clear(smap, &smap->txqueue);

	(void)smap_txrx_XXable(smap, DISABLE);

	(void)smap_interrupt_XXable(smap, DISABLE);
	(void)smap_clear_all_interrupt(smap);
	smap->txicnt = smap->rxicnt = 0;

	(void)free_irq(smap->irq, net_dev);

	return(0);	/* success */
}

/*--------------------------------------------------------------------------*/

static int
smap_ioctl(struct net_device *net_dev, struct ifreq *ifr, int cmd)
{
	struct smap_chan *smap = net_dev->priv;
	struct mii_ioctl_data *miidata =(struct mii_ioctl_data *)&ifr->ifr_data;
	int retval = 0;

	switch (cmd) {
	case SIOCGMIIPHY:
		miidata->phy_id = DsPHYTER_ADDRESS;
		break;

	case SIOCGMIIREG:
		miidata->val_out = (u_short)smap_read_phy(smap,
						miidata->phy_id & 0x1f,
						miidata->reg_num & 0x1f);
		break;

#if 0
	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN)) {
			printk("%s: SIOCSMIIREG: not available.\n", net_dev->name);
			retval = -EPERM;
			break;
		}
		(void)smap_write_phy(smap, miidata->phy_id & 0x1f,
				miidata->reg_num & 0x1f, miidata->val_in);
		break;
#endif

	case SMAP_IOC_PRTMODE:
	{
		u_int16_t phyval = 0;

		if (ifr == NULL) {
			printk("%s: ifr is NULL\n", net_dev->name);
			retval = -EINVAL;
			break;
		}

		phyval = (u_int16_t)smap_read_phy(smap,
					DsPHYTER_ADDRESS, DsPHYTER_BMSR);
		if (phyval == (u_int16_t)-1) {
			printk("%s: read phy error\n", net_dev->name);
			retval = -EBUSY;
			break;
		}
		if (!(phyval & PHY_BMSR_LINK)) {
			printk("%s: link not valid(0x%04x)\n",
							net_dev->name, phyval);
		} else {
			u_int16_t anar = 0;
			u_int16_t anlpar = 0;

			anar = (u_int16_t)smap_read_phy(smap,
					DsPHYTER_ADDRESS, DsPHYTER_ANAR);
			anlpar = (u_int16_t)smap_read_phy(smap,
					DsPHYTER_ADDRESS, DsPHYTER_ANLPAR);

			smap->flags &= ~(SMAP_F_SPD_100M|SMAP_F_DUP_FULL);
			if (anar & anlpar & (PHY_ANAR_100FD|PHY_ANAR_100HD))
				smap->flags |= SMAP_F_SPD_100M;
			if (anar & anlpar & (PHY_ANAR_100FD|PHY_ANAR_10FD))
				smap->flags |= SMAP_F_DUP_FULL;

			printk("%s: %s: speed=%s, dupmode=%s.\n",
				net_dev->name,
				(phyval & PHY_BMSR_ANCP) ?
					"Auto-Negotiation" : "force mode",
				(smap->flags & SMAP_F_SPD_100M) ? "100Mbps" : "10Mbps",
				(smap->flags & SMAP_F_DUP_FULL) ? "FDX" : "HDX");
		}
		(int)ifr->ifr_data = (int)phyval;
	}
		break;

	case SMAP_IOC_DUMPREG:
		(void)smap_dump_reg(smap);
		(void)smap_dump_emac3_reg(smap);
		break;

	case SMAP_IOC_DUMPBD:
		(void)smap_dump_txbd(smap);
		(void)smap_dump_rxbd(smap);
		break;

	case SMAP_IOC_DUMPFLAG:
		printk("%s: flags = 0x%08x, txmode_val = 0x%08x\n",
				net_dev->name, smap->flags, smap->txmode_val);
		break;

	case SMAP_IOC_DUMPPHYSTAT:
		printk("%s: PHY ID1 = 0x%04x(0x%04x), ID2 = 0x%04x(0x%04x),"
			" BMSR = 0x%04x\n",
			net_dev->name,
			smap_read_phy(smap, DsPHYTER_ADDRESS,DsPHYTER_PHYIDR1),
			PHY_IDR1_VAL,
			smap_read_phy(smap, DsPHYTER_ADDRESS,DsPHYTER_PHYIDR2),
			PHY_IDR2_VAL,
			smap_read_phy(smap, DsPHYTER_ADDRESS,DsPHYTER_BMSR));
		break;

	case SMAP_IOC_PRINT_MSG:
		if (ifr == NULL) {
			printk("%s: ifr is NULL\n", net_dev->name);
			retval = -EINVAL;
			break;
		}
		if ((int)ifr->ifr_data != 0)
			smap->flags |= SMAP_F_PRINT_MSG;
		else
			smap->flags &= ~SMAP_F_PRINT_MSG;
		break;

	case SMAP_IOC_DUMP_PKT:
		if (ifr == NULL) {
			printk("%s: ifr is NULL\n", net_dev->name);
			retval = -EINVAL;
			break;
		}
		if ((int)ifr->ifr_data != 0)
			smap->flags |= SMAP_F_PRINT_PKT;
		else
			smap->flags &= ~SMAP_F_PRINT_PKT;
		break;

	default:
		retval = -EOPNOTSUPP;
		break;
	}

	return(retval);
}

#ifdef HAVE_TX_TIMEOUT
static void
smap_tx_timeout(struct net_device *net_dev)
{
	struct smap_chan *smap = net_dev->priv;

#if 0
	/* this entry point function is called when queue is stopped. */
	netif_stop_queue(net_dev);
#endif
	wake_up_interruptible(&smap->wait_timeout);

	return;
}

static int
smap_timeout_thread(void *arg)
{
	struct smap_chan *smap = (struct smap_chan *)arg;
	struct net_device *net_dev = (struct net_device *)smap->net_dev;
	unsigned long flags;

	spin_lock_irqsave(&current->sigmask_lock, flags);
	siginitsetinv(&current->blocked,
			sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	lock_kernel();
	daemonize();
	sprintf(current->comm, "smap timeout");
	unlock_kernel();

	smap->timeout_task = current;

	while(1) {
		interruptible_sleep_on(&smap->wait_timeout);
		if (signal_pending(current))
			break;

		printk("%s: tx timeout ticks = %ld\n",
		       net_dev->name, jiffies - net_dev->trans_start);

#if 1
		/* for confirmation */
		netif_stop_queue(net_dev);
#endif
		smap->flags &= ~(SMAP_F_LINKESTABLISH|SMAP_F_LINKVALID);
		netif_carrier_off(net_dev);
		(void)smap_dma_force_break(smap);
		(void)smap_reset(smap, RESET_INIT);
		(void)smap_txrx_XXable(smap, DISABLE);
		netif_carrier_on(net_dev);
		(void)smap_txbd_init(smap);
		(void)smap_rxbd_init(smap);
		(void)smap_clear_all_interrupt(smap);
		(void)smap_interrupt_XXable(smap, ENABLE);
		(void)smap_txrx_XXable(smap, ENABLE);
		(void)smap_multicast_list(smap->net_dev);

		net_dev->trans_start = jiffies;		/* save new timestamp */
		smap->net_stats.tx_errors++;
		netif_wake_queue(net_dev);
	}

	smap->timeout_task = NULL;
	if (smap->timeout_compl != NULL)
		complete(smap->timeout_compl);/* notify that we've exited */

	return(0);
}
#endif /* HAVE_TX_TIMEOUT */

/*--------------------------------------------------------------------------*/

static void
smap_clear_all_interrupt(struct smap_chan *smap)
{
	SMAPREG16(smap,SMAP_INTR_CLR) = INTR_CLR_ALL;

	EMAC3REG_WRITE(smap, SMAP_EMAC3_INTR_STAT, E3_INTR_ALL);

	return;
}

static void
smap_interrupt_XXable(struct smap_chan *smap, int enable_flag)
{
	if (enable_flag) {
		/* enable interrupt */
		SMAPREG16(smap,SMAP_INTR_ENABLE) |= INTR_ENA_ALL;
		EMAC3REG_WRITE(smap, SMAP_EMAC3_INTR_ENABLE, E3_INTR_ALL);
	} else {
		/* disable interrupt */
		SMAPREG16(smap,SMAP_INTR_ENABLE) &= ~INTR_ENA_ALL;
		EMAC3REG_WRITE(smap, SMAP_EMAC3_INTR_ENABLE, 0);
	}
	return;
}

static void
smap_txrx_XXable(struct smap_chan *smap, int enable_flag)
{
	int i;
	u_int32_t e3v;

	if (enable_flag) {
		/* enable tx/rx */
		EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE0,
					E3_TXMAC_ENABLE|E3_RXMAC_ENABLE);
	} else {
		/* disable tx/rx */
		e3v = EMAC3REG_READ(smap, SMAP_EMAC3_MODE0);
		e3v &= ~(E3_TXMAC_ENABLE|E3_RXMAC_ENABLE);
		EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE0, e3v);

		/* check EMAC3 idle status */
		for (i = SMAP_LOOP_COUNT; i; i--) {
			e3v = EMAC3REG_READ(smap, SMAP_EMAC3_MODE0);
			if ( (e3v & E3_RXMAC_IDLE) && (e3v & E3_TXMAC_IDLE) )
				break;
		}
		if (i == 0) {
			printk("%s: emac3 is still running(%x).\n",
					smap->net_dev->name, e3v);
		}
	}
	return;
}

static void
smap_txbd_init(struct smap_chan *smap)
{
	int i;
	unsigned long flags;
	volatile struct smapbd *bd = smap->txbd;

	spin_lock_irqsave(&smap->spinlock, flags);
	smap->txfreebufsize = SMAP_TXBUFSIZE;
	smap->txbwp = SMAP_TXBUFBASE;
	smap->txbds = smap->txbdi = smap->txbdusedcnt = 0;
	for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, bd++) {
		bd->ctrl_stat = 0;		/* clear ready bit */
		bd->reserved = 0;		/* must be zero */
		bd->length = 0;
		bd->pointer = 0;
	}
	spin_unlock_irqrestore(&smap->spinlock, flags);
	return;
}

static void
smap_rxbd_init(struct smap_chan *smap)
{
	int i;
	unsigned long flags;
	volatile struct smapbd *bd = smap->rxbd;

	spin_lock_irqsave(&smap->spinlock, flags);
	smap->rxbrp = SMAP_RXBUFBASE;
	smap->rxbdi = 0;
	for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, bd++) {
		bd->ctrl_stat = SMAP_BD_RX_EMPTY;	/* set empty bit */
		bd->reserved = 0;			/* must be zero */
		bd->length = 0;
		bd->pointer = 0;
	}
	spin_unlock_irqrestore(&smap->spinlock, flags);
	return;
}

static int
smap_read_phy(struct smap_chan *smap, u_int32_t phyadr, u_int32_t regadr)
{
	int i;
	u_int32_t e3v;

	/* check complete bit */
	for (i = SMAP_LOOP_COUNT; i; i--) {
		e3v = EMAC3REG_READ(smap, SMAP_EMAC3_STA_CTRL);
		if (e3v & E3_PHY_OP_COMP)
			break;
	}
	if (i == 0) {
		printk("%s: read phy: busy\n", smap->net_dev->name);
		return(-1);
	}

	/* write phy address and register address */
	EMAC3REG_WRITE(smap, SMAP_EMAC3_STA_CTRL,
			E3_PHY_READ |
			((phyadr&E3_PHY_ADDR_MSK)<<E3_PHY_ADDR_BITSFT) |
			(regadr&E3_PHY_REG_ADDR_MSK) );

	/* check complete bit */
	for (i = SMAP_LOOP_COUNT; i; i--) {
		e3v = EMAC3REG_READ(smap, SMAP_EMAC3_STA_CTRL);
		if (e3v & E3_PHY_OP_COMP)
			break;
	}
	if (i == 0) {
		printk("%s: read phy: write address busy, val = %x\n",
						smap->net_dev->name, e3v);
		return(-1);
	}

	/* workarrund: it may be needed to re-read to get correct phy data */
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_STA_CTRL);
	return(e3v >> E3_PHY_DATA_BITSFT);
}

static int
smap_write_phy(struct smap_chan *smap,
		u_int32_t phyadr, u_int32_t regadr, u_int16_t data)
{
	int i;
	u_int32_t e3v;

	/* check complete bit */
	for (i = SMAP_LOOP_COUNT; i; i--) {
		e3v = EMAC3REG_READ(smap, SMAP_EMAC3_STA_CTRL);
		if (e3v & E3_PHY_OP_COMP)
			break;
	}
	if (i == 0) {
		printk("%s: write phy: busy\n", smap->net_dev->name);
		return(-1);
	}

	/* write data, phy address and register address */
	e3v = ( ((data&E3_PHY_DATA_MSK)<<E3_PHY_DATA_BITSFT) |
			E3_PHY_WRITE |
			((phyadr&E3_PHY_ADDR_MSK)<<E3_PHY_ADDR_BITSFT) |
			(regadr&E3_PHY_REG_ADDR_MSK) );
	EMAC3REG_WRITE(smap, SMAP_EMAC3_STA_CTRL, e3v);

	/* check complete bit */
	for (i = SMAP_LOOP_COUNT; i; i--) {
		e3v = EMAC3REG_READ(smap, SMAP_EMAC3_STA_CTRL);
		if (e3v & E3_PHY_OP_COMP)
			break;
	}
	if (i == 0) {
		printk("%s: write phy: write data busy, val = %x\n",
						smap->net_dev->name, e3v);
		return(-1);
	}

	return(0);
}

static int
smap_fifo_reset(struct smap_chan *smap)
{
	int i, retval = 0;
	struct net_device *net_dev = smap->net_dev;

	/* reset TX FIFO */
	SMAPREG8(smap,SMAP_TXFIFO_CTRL) = TXFIFO_RESET;
	/* reset RX FIFO */
	SMAPREG8(smap,SMAP_RXFIFO_CTRL) = RXFIFO_RESET;

	/* confirm reset done */
	for (i = SMAP_LOOP_COUNT; i; i--) {
		if (!(SMAPREG8(smap,SMAP_TXFIFO_CTRL) & TXFIFO_RESET))
			break;
	}
	if (i == 0) {
		printk("%s: Txfifo reset is in progress\n", net_dev->name);
		retval |= 1;
	}

	for (i = SMAP_LOOP_COUNT; i; i--) {
		if (!(SMAPREG8(smap,SMAP_RXFIFO_CTRL) & RXFIFO_RESET))
			break;
	}
	if (i == 0) {
		printk("%s: Rxfifo reset is in progress\n", net_dev->name);
		retval |= 2;
	}

	return(retval);
}

static void
smap_reg_init(struct smap_chan *smap)
{

	(void)smap_interrupt_XXable(smap, DISABLE);
	(void)smap_clear_all_interrupt(smap);

	/* BD mode */
	SMAPREG8(smap,SMAP_BD_MODE) = 0;	/* swap */

	/* reset TX/RX FIFO */
	(void)smap_fifo_reset(smap);

	return;
}

static int
smap_emac3_soft_reset(struct smap_chan *smap)
{
	int i;
	u_int32_t e3v;

	EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE0, E3_SOFT_RESET);
	for (i = SMAP_LOOP_COUNT; i; i--) {
		e3v = EMAC3REG_READ(smap, SMAP_EMAC3_MODE0);
		if (!(e3v & E3_SOFT_RESET))
			break;
	}
	if (i == 0) {
		printk("%s: emac3 reset is in progress\n", smap->net_dev->name);
		return(-1);
	}
	return(0);
}

static void
smap_emac3_set_defvalue(struct smap_chan *smap)
{
	u_int32_t e3v;

	/* set HW address */
	e3v = ( (smap->hwaddr[0] << 8) | smap->hwaddr[1] );
	EMAC3REG_WRITE(smap, SMAP_EMAC3_ADDR_HI, e3v);
	e3v = ( (smap->hwaddr[2] << 24) | (smap->hwaddr[3] << 16) |
			(smap->hwaddr[4] << 8) | smap->hwaddr[5] );
	EMAC3REG_WRITE(smap, SMAP_EMAC3_ADDR_LO, e3v);

	/* Inter-frame GAP */
	EMAC3REG_WRITE(smap, SMAP_EMAC3_INTER_FRAME_GAP, 4);

	/* Rx mode */
	e3v = (E3_RX_STRIP_PAD|E3_RX_STRIP_FCS|
				E3_RX_INDIVID_ADDR|E3_RX_BCAST);
	EMAC3REG_WRITE(smap, SMAP_EMAC3_RxMODE, e3v);

	/* Tx fifo value for request priority */
	/* low = 7*8=56, urgent = 15*8=120 */
	e3v = ( (7<<E3_TX_LOW_REQ_BITSFT) | (15<<E3_TX_URG_REQ_BITSFT) );
	EMAC3REG_WRITE(smap, SMAP_EMAC3_TxMODE1, e3v);

	/* TX threshold, (12+1)*64=832 */
	e3v = ((12&E3_TX_THRESHLD_MSK)<<E3_TX_THRESHLD_BITSFT);
	EMAC3REG_WRITE(smap, SMAP_EMAC3_TX_THRESHOLD, e3v);

	/* Rx watermark, low = 16*8=128, hi = 128*8=1024 */
	e3v = ( ((16&E3_RX_LO_WATER_MSK)<<E3_RX_LO_WATER_BITSFT) |
			((128&E3_RX_HI_WATER_MSK)<<E3_RX_HI_WATER_BITSFT) );
	EMAC3REG_WRITE(smap, SMAP_EMAC3_RX_WATERMARK, e3v);

	return;
}

static void
smap_emac3_init(struct smap_chan *smap, int reset_only)
{
	/* reset emac3 */
	(void)smap_emac3_soft_reset(smap);

	/* EMAC3 operating MODE */
	EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE1, SMAP_EMAC3_MODE1_DEF);

	/* phy init */
	if (smap_phy_init(smap, reset_only) < 0) {
		printk("%s: phy init error\n", smap->net_dev->name);
		return;
	}
	if (reset_only)		/* this flag may be set when unloading */
		return;

	/* clear interrupt */
	(void)smap_clear_all_interrupt(smap);
	/* disable interrupt */
	(void)smap_interrupt_XXable(smap, DISABLE);

	/* permanently set to default value */
	(void)smap_emac3_set_defvalue(smap);

	return;
}

static void
smap_emac3_re_init(struct smap_chan *smap)
{
	(void)smap_emac3_soft_reset(smap);
	EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE1, smap->txmode_val);
	(void)smap_emac3_set_defvalue(smap);
	return;
}

/* return value 0: success, <0: error */
static int
smap_phy_init(struct smap_chan *smap, int reset_only)
{
	int val;

	val = smap_phy_reset(smap);
	if (val < 0)
		return val;

	if (reset_only)		/* this flag may be set when unloading */
		return 0;

	/* auto-negotiation */
	val = smap_auto_negotiation(smap, ENABLE);
	if (val == 0) {
		smap->flags |= SMAP_F_LINKESTABLISH;
		(void)smap_phy_set_dsp(smap);
		return(0);	/* auto-negotiation is succeeded */
	}

	/* force 100Mbps(HDX) or 10Mbps(HDX) */
	(void)smap_force_spd_100M(smap);

	return(0);
}

static int
smap_phy_reset(struct smap_chan *smap)
{
	int i;
	u_int16_t phyval = 0;

	/* set reset bit */
	smap_write_phy(smap, DsPHYTER_ADDRESS, DsPHYTER_BMCR, PHY_BMCR_RST);

	udelay(300);				/* wait 300us */

	/* confirm reset done */
	for (i = SMAP_LOOP_COUNT; i; i--) {
		phyval = (u_int16_t)smap_read_phy(smap,
					DsPHYTER_ADDRESS, DsPHYTER_BMCR);
		if (!(phyval & PHY_BMCR_RST))
			break;
		udelay(300);
	}
	if (i == 0) {
		printk("%s: PHY reset not complete(BMCR=0x%x)\n",
					smap->net_dev->name, phyval);
		return(-1);
	}
	return(0);
}

static int
smap_auto_negotiation(struct smap_chan *smap, int enable_auto_nego)
{
	int i, val;

	if (enable_auto_nego) {
		/* set auto-negotiation */
		smap_write_phy(smap, DsPHYTER_ADDRESS, DsPHYTER_BMCR,
				PHY_BMCR_100M|PHY_BMCR_ANEN|PHY_BMCR_DUPM);
	}

	val = smap_confirm_auto_negotiation(smap);
	for (i = SMAP_AUTONEGO_RETRY; i; i--) {
		if (val < 0) {	/* timeout, error */
			/* restart auto-negotiation */
			smap_write_phy(smap, DsPHYTER_ADDRESS, DsPHYTER_BMCR,
					PHY_BMCR_100M|PHY_BMCR_ANEN|PHY_BMCR_DUPM|PHY_BMCR_RSAN);
			val = smap_confirm_auto_negotiation(smap);
		} else
			break;
	}
	if (val == 0)
		return(0);
	else
		return(-1);	/* error */
}

static int
smap_confirm_auto_negotiation(struct smap_chan *smap)
{
	int i;
	u_int16_t phyval = 0;
	u_int16_t anar = 0, anlpar = 0;
	u_int32_t e3v;

	for (i = SMAP_AUTONEGO_TIMEOUT; i; i--) {
						/* auto nego timeout is 3s */
		phyval = (u_int16_t)smap_read_phy(smap,
					DsPHYTER_ADDRESS, DsPHYTER_BMSR);
		if (phyval & PHY_BMSR_ANCP)
			break;
		interruptible_sleep_on_timeout(&smap->wait_linknego, HZ/100);	/* wait 10ms */
	}
	if (i == 0) {
		printk("%s: Auto-negotiation timeout, not complete(BMSR=%x)\n",
					smap->net_dev->name, phyval);
		return(-1);
	}

	/* auto negotiation completed. */
	for (i = 3; i; i--) {
		/* it needs to wait 3 seconds. */
		interruptible_sleep_on_timeout(&smap->wait_linknego, HZ);
	}

	/* confirm speed & duplex mode */
	for (i = SMAP_LOOP_COUNT; i; i--) {
		phyval = (u_int16_t)smap_read_phy(smap,
					DsPHYTER_ADDRESS, DsPHYTER_BMSR);
		if ((phyval & PHY_BMSR_ANCP) && (phyval & PHY_BMSR_LINK))
			break;
		udelay(1000);
	}
	if (i == 0) {	/* error */
		printk("%s: Auto-negotiation error?? (BMSR=%x)\n",
					smap->net_dev->name, phyval);
		return(-2);
	}

	anar = (u_int16_t)smap_read_phy(smap,
			DsPHYTER_ADDRESS, DsPHYTER_ANAR);
	anlpar = (u_int16_t)smap_read_phy(smap,
			DsPHYTER_ADDRESS, DsPHYTER_ANLPAR);
	smap->flags &= ~(SMAP_F_SPD_100M|SMAP_F_DUP_FULL);
	if (anar & anlpar & (PHY_ANAR_100FD|PHY_ANAR_100HD))
		smap->flags |= SMAP_F_SPD_100M;
	if (anar & anlpar & (PHY_ANAR_100FD|PHY_ANAR_10FD))
		smap->flags |= SMAP_F_DUP_FULL;

	printk("%s: Auto-negotiation complete. %s %s duplex mode.\n",
			smap->net_dev->name,
			(smap->flags & SMAP_F_SPD_100M) ? "100Mbps" : "10Mbps",
			(smap->flags & SMAP_F_DUP_FULL) ? "Full" : "Half");
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_MODE1);
	if (smap->flags & SMAP_F_DUP_FULL) {
		/* Full duplex mode */
		e3v |= (E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);
	} else {
		/* Half duplex mode */
		e3v &= ~(E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);
	}
	e3v &= ~E3_MEDIA_MSK;
	if (smap->flags & SMAP_F_SPD_100M)
		e3v |= E3_MEDIA_100M;
	else
		e3v |= E3_MEDIA_10M;
	EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE1, e3v);

	return(0);
}

static void
smap_force_spd_100M(struct smap_chan *smap)
{
	printk("%s: try 100Mbps Half duplex mode...\n", smap->net_dev->name);

	/* set 100Mbps, half duplex */
	smap_write_phy(smap, DsPHYTER_ADDRESS, DsPHYTER_BMCR, PHY_BMCR_100M);

	/* delay 2s */
	smap->flags |= SMAP_F_CHECK_FORCE100M;
	interruptible_sleep_on_timeout(&smap->wait_linknego, 2*HZ);

	smap_confirm_force_spd((unsigned long)smap);

	return;
}

static void
smap_force_spd_10M(struct smap_chan *smap)
{
	printk("%s: try 10Mbps Half duplex mode...\n", smap->net_dev->name);

	/* set 10Mbps, half duplex */
	smap_write_phy(smap, DsPHYTER_ADDRESS, DsPHYTER_BMCR, PHY_BMCR_10M);

	/* delay 2s */
	smap->flags |= SMAP_F_CHECK_FORCE10M;
	interruptible_sleep_on_timeout(&smap->wait_linknego, 2*HZ);

	smap_confirm_force_spd((unsigned long)smap);

	return;
}

static void
smap_confirm_force_spd(unsigned long arg)
{
	struct smap_chan *smap = (struct smap_chan *)arg;
	int i;
	u_int16_t phyval = 0;
	u_int32_t e3v;

	/* confirm link status, wait 1s is needed */
	for (i = SMAP_FORCEMODE_TIMEOUT; i; i--) {
		phyval = (u_int16_t)smap_read_phy(smap,
					DsPHYTER_ADDRESS, DsPHYTER_BMSR);
		if (phyval & PHY_BMSR_LINK)
			break;
		interruptible_sleep_on_timeout(&smap->wait_linknego, HZ/100);	/* wait 10ms */
	}
	if (i) {
validlink:
		e3v = EMAC3REG_READ(smap, SMAP_EMAC3_MODE1);
		e3v &= ~(E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF|E3_MEDIA_MSK);
		if (smap->flags & SMAP_F_CHECK_FORCE100M) {
			printk("%s: 100Mbps Half duplex mode\n", smap->net_dev->name);
			e3v |= E3_MEDIA_100M;
		} else if (smap->flags & SMAP_F_CHECK_FORCE10M) {
			printk("%s: 10Mbps Half duplex mode\n", smap->net_dev->name);
			e3v |= E3_MEDIA_10M;
		}
		EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE1, e3v);
		smap->flags &= ~(SMAP_F_CHECK_FORCE100M|SMAP_F_CHECK_FORCE10M);
		smap->flags |= SMAP_F_LINKESTABLISH;
		(void)smap_phy_set_dsp(smap);
		return;			/* success */
	}

	if (smap->flags & SMAP_F_CHECK_FORCE100M) {
		smap->flags &= ~SMAP_F_CHECK_FORCE100M;
		(void)smap_force_spd_10M(smap);
	} else if (smap->flags & SMAP_F_CHECK_FORCE10M) {
		smap->flags &= ~SMAP_F_CHECK_FORCE10M;
		phyval = (u_int16_t)smap_read_phy(smap,
				DsPHYTER_ADDRESS, DsPHYTER_BMSR);
		if (phyval & PHY_BMSR_LINK) {
			/* valid link */
			smap->flags |= SMAP_F_CHECK_FORCE10M;
			goto validlink;
		} else {
			printk("%s: fail force speed mode."
				" link not valid.  phystat=0x%04x\n",
						smap->net_dev->name, phyval);
		}
	}
	return;
}

static int smap_phy_fcscr_threshold = PHY_FCSCR_THRESHOLD;
MODULE_PARM(smap_phy_fcscr_threshold, "i");

static void
smap_phy_set_dsp(struct smap_chan *smap)
{
	u_int16_t id1, id2, phyval, recr, fcscr;

	if (!(smap->flags & SMAP_F_LINKESTABLISH))  /* link not established */
		return;

	/* this value is used in emac3 re-init without phy init */
	smap->txmode_val = EMAC3REG_READ(smap, SMAP_EMAC3_MODE1);

	id1 = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,DsPHYTER_PHYIDR1);
	id2 = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,DsPHYTER_PHYIDR2);

	if (!((id1 == PHY_IDR1_VAL) && ((id2&PHY_IDR2_MSK) == PHY_IDR2_VAL))) {
		smap->flags |= SMAP_F_LINKVALID;
		return;
	}

	if (smap->flags & SMAP_F_LINKVALID)
		return;

{
	recr = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,
							DsPHYTER_RECR);
	fcscr = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,
							DsPHYTER_FCSCR);
	interruptible_sleep_on_timeout(&smap->wait_linknego, HZ/2);
	recr = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,
							DsPHYTER_RECR);
	fcscr = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,
							DsPHYTER_FCSCR);
	if ((recr > 0) || (fcscr > smap_phy_fcscr_threshold)) {
		printk("%s: detect REL(%d) or FCSL(%d). force 10M HDX mode.\n",
			smap->net_dev->name, recr, fcscr);
		/* EMAC3 default operating MODE */
		EMAC3REG_WRITE(smap, SMAP_EMAC3_MODE1, SMAP_EMAC3_MODE1_DEF);
		smap->flags &= ~(SMAP_F_LINKESTABLISH|SMAP_F_LINKVALID);
		(void)smap_force_spd_10M(smap);
		return;
	}
}

	if ((id2&PHY_IDR2_REV_MSK) == 0x0) {
		smap_write_phy(smap, DsPHYTER_ADDRESS, 0x13, 0x0001);
		smap_write_phy(smap, DsPHYTER_ADDRESS, 0x19, 0x1898);
		smap_write_phy(smap, DsPHYTER_ADDRESS, 0x1f, 0x0000);
		smap_write_phy(smap, DsPHYTER_ADDRESS, 0x1d, 0x5040);
		smap_write_phy(smap, DsPHYTER_ADDRESS, 0x1e, 0x008c);
		smap_write_phy(smap, DsPHYTER_ADDRESS, 0x13, 0x0000);
	}
	phyval = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,
							DsPHYTER_PHYSTS);
	if ( (phyval & (PHY_STS_DUPS|PHY_STS_SPDS|PHY_STS_LINK)) ==
				(PHY_STS_HDX|PHY_STS_10M|PHY_STS_LINK) ) {
		smap_write_phy(smap, DsPHYTER_ADDRESS, 0x1a, 0x0104);
	}

	smap->flags |= SMAP_F_LINKVALID;
	return;
}

static void
smap_reset(struct smap_chan *smap, int reset_only)
{
	smap_reg_init(smap);
	smap_emac3_init(smap, reset_only);

	return;
}

/*--------------------------------------------------------------------------*/

/* 1 clock with putting data */
static inline void
smap_eeprom_clock_dataout(struct smap_chan *smap, int val)
{
	SMAP_PP_SET_D(smap, val);

	SMAP_PP_CLK_OUT(smap, 0);
	udelay(1);	/* tDIS */

	SMAP_PP_CLK_OUT(smap, 1);
	udelay(1);	/* tSKH, tDIH */

	SMAP_PP_CLK_OUT(smap, 0);
	udelay(1);	/* tSKL */
}

/* 1 clock with getting data */
static inline int
smap_eeprom_clock_datain(struct smap_chan *smap)
{
	int r;

	SMAP_PP_SET_D(smap, 0);
	SMAP_PP_CLK_OUT(smap, 0);
	udelay(1);	/* tSKL */

	SMAP_PP_CLK_OUT(smap, 1);
	udelay(1);	/* tSKH, tPD0,1 */
	r = SMAP_PP_GET_Q(smap);

	SMAP_PP_CLK_OUT(smap, 0);
	udelay(1);	/* tSKL */

	return(r);
}

/* put address(6bit) */
static void
smap_eeprom_put_addr(struct smap_chan *smap, u_int8_t addr)
{
	int i;

	addr &= 0x3f;
	for (i = 0; i < 6; i++) {
		smap_eeprom_clock_dataout(smap, (addr & 0x20)?1:0);
		addr <<= 1;
	}
}

/* get data(16bit) */
static u_int16_t
smap_eeprom_get_data(struct smap_chan *smap)
{
	int i;
	u_int16_t data = 0;

	for (i = 0; i < 16; i++) {
		data <<= 1;
		data |= smap_eeprom_clock_datain(smap);
	}

	return(data);
}

/* instruction start(rise S, put start bit, op code) */
static void
smap_eeprom_start_op(struct smap_chan *smap, int op)
{
	/* set port direction */    
	SMAPREG8(smap, SMAP_PIOPORT_DIR) = (PP_SCLK | PP_CSEL | PP_DIN);

	/* rise chip select */
	SMAP_PP_SET_S(smap, 0);
	SMAP_PP_SET_D(smap, 0);
	SMAP_PP_CLK_OUT(smap, 0);
	udelay(1);	/* tSKS */

	SMAP_PP_SET_S(smap, 1);
	SMAP_PP_SET_D(smap, 0);
	SMAP_PP_CLK_OUT(smap, 0);
	udelay(1);	/* tCSS */

	/* put start bit */
	smap_eeprom_clock_dataout(smap, 1);

	/* put op code */
	smap_eeprom_clock_dataout(smap, (op >> 1) & 1);
	smap_eeprom_clock_dataout(smap, op & 1);
}

/* chip select low */
static void
smap_eeprom_cs_low(struct smap_chan *smap)
{
	SMAP_PP_SET_S(smap, 0);
	SMAP_PP_SET_D(smap, 0);
	SMAP_PP_CLK_OUT(smap, 0);
	udelay(2);	/* tSLSH */
}

/*
 *   EEPROM instruction
 */
/* read instruction */
static void
smap_eeprom_exec_read(struct smap_chan *smap,
					u_int8_t addr, u_int16_t *datap, int n)
{
	int i;

	smap_eeprom_start_op(smap, PP_OP_READ);
	smap_eeprom_put_addr(smap, addr);
	for (i = 0; i < n; i++) {
		*datap++ = smap_eeprom_get_data(smap);
	}
	smap_eeprom_cs_low(smap);
}

/*
 *   read EEPROM
 */
static void
smap_eeprom_read(struct smap_chan *smap, u_int8_t addr, u_int16_t *datap, int n)
{
	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);
	smap_eeprom_exec_read(smap, addr, datap, n);
	spin_unlock_irqrestore(&smap->spinlock, flags);
}

/*--------------------------------------------------------------------------*/

static void
smap_print_mac_address(struct smap_chan *smap, u_int8_t *addr)
{
	int i;

	printk("%s: MAC address ", smap->net_dev->name);
	for (i = 0; i < 6; i++) {
		printk("%02x", addr[i]);
		if (i != 5)
			printk(":");
	}
	printk("\n");
	return;
}

static int
smap_get_node_addr(struct smap_chan *smap)
{
	int i;
	u_int16_t *macp, cksum, sum = 0;

	macp = (u_int16_t *)smap->hwaddr;
	smap_eeprom_read(smap, 0x0, macp, 3);
	smap_eeprom_read(smap, 0x3, &cksum, 1);

	for (i = 0; i < 3; i++) {
		sum += *macp++;
	}
	if (sum != cksum) {
		printk("%s: MAC address read error\n", smap->net_dev->name);
		printk("checksum %04x is read from EEPROM, "
			"and %04x is calculated by mac address read now.\n",
							cksum, sum);
		smap_print_mac_address(smap, smap->hwaddr);
		memset(smap->hwaddr, 0, 6);
		return(-1);
	}
	smap_print_mac_address(smap, smap->hwaddr);
	if (!smap->net_dev) {
		printk("%s: net_dev is error(null).\n", smap->net_dev->name);
		memset(smap->hwaddr, 0, 6);
		return(-1);
	}
	memcpy(smap->net_dev->dev_addr, smap->hwaddr, 6);
	return(0);
}

static void
smap_base_init(struct smap_chan *smap)
{
	/* we can access register&BD after this routine returned. */

	smap->base = (volatile u_int8_t *)SMAP_BASE;
	smap->txbd = (volatile struct smapbd *)(smap->base + SMAP_BD_BASE_TX);
	smap->rxbd = (volatile struct smapbd *)(smap->base + SMAP_BD_BASE_RX);

	smap->txfreebufsize = SMAP_TXBUFSIZE;
	smap->txbwp = SMAP_TXBUFBASE;
	smap->txbds = smap->txbdi = smap->txbdusedcnt = 0;

	smap->rxbrp = SMAP_RXBUFBASE;
	smap->rxbdi = 0;

	smap->txicnt = smap->rxicnt = 0;

	return;
}

/*--------------------------------------------------------------------------*/

static void
smap_dump_packet(struct smap_chan *smap, u_int8_t *ptr, int length)
{
	int i;

	printk("%s: dump packet(dump len = %d):\n", smap->net_dev->name, length);
	for (i = 0; i < length; i++) {
		printk("%02x", *(ptr + i));
		if ((i%20)==19)
			printk("\n");
		else if ((i%4)==3)
			printk(" ");
	}
	printk("\n");
	return;
}

static void
smap_dump_txbd(struct smap_chan *smap)
{
	int i;
	volatile struct smapbd *bd = smap->txbd;

	printk("Tx Buffer Descriptor\n");
	for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, bd++) {
		printk("%02d: stat(0x%04x),rsv(0x%04x),len(%d,0x%04x),ptr(0x%04x), ",
					i, bd->ctrl_stat, bd->reserved,
					bd->length, bd->length, bd->pointer);
		if ((i%2)==1)
			printk("\n");
	}
	printk("tx buf w_ptr(0x%04x), free buf size(%d), bd used cnt(%d)\n",
		smap->txbwp, smap->txfreebufsize, smap->txbdusedcnt);
	printk("txbds(%d), txbdi(%d)\n", smap->txbds, smap->txbdi);
}

static void
smap_dump_rxbd(struct smap_chan *smap)
{
	int i;
	volatile struct smapbd *bd = smap->rxbd;

	printk("Rx Buffer Descriptor\n");
	for (i = 0; i < SMAP_BD_MAX_ENTRY; i++, bd++) {
		printk("%02d: stat(0x%04x),rsv(0x%04x),len(%d,0x%04x),ptr(0x%04x), ",
					i, bd->ctrl_stat, bd->reserved,
					bd->length, bd->length, bd->pointer);
		if ((i%2)==1)
			printk("\n");
	}
	printk("rx buf r_ptr(0x%04x), rxbdi(%d)\n", smap->rxbrp, smap->rxbdi);
}

static void
smap_dump_reg(struct smap_chan *smap)
{
	printk("PlayStation 2 SMAP register\n");
	printk("DMA MODE(0x%02x), BD MODE(0x%02x)\n",
		SMAPREG8(smap,SMAP_DMA_MODE),SMAPREG8(smap,SMAP_BD_MODE));
	printk("INTR STAT(0x%04x), ENA(0x%04x)\n",
		SMAPREG16(smap,SMAP_INTR_STAT),
		SMAPREG16(smap,SMAP_INTR_ENABLE));
	printk("TX:CTRL(0x%02x), PTR(0x%04x), SLICE(0x%04x), FRM CNT(0x%02x)\n",
		SMAPREG8(smap,SMAP_TXFIFO_CTRL),
		SMAPREG16(smap,SMAP_TXFIFO_WR_PTR),
		SMAPREG16(smap,SMAP_TXFIFO_DMA_SLICE_CNT),
		SMAPREG8(smap,SMAP_TXFIFO_FRAME_CNT));
	printk("RX:CTRL(0x%02x), PTR(0x%04x), SLICE(0x%04x), FRM CNT(0x%02x)\n",
		SMAPREG8(smap,SMAP_RXFIFO_CTRL),
		SMAPREG16(smap,SMAP_RXFIFO_RD_PTR),
		SMAPREG16(smap,SMAP_RXFIFO_DMA_SLICE_CNT),
		SMAPREG8(smap,SMAP_RXFIFO_FRAME_CNT));
}

static void
smap_dump_emac3_reg(struct smap_chan *smap)
{
	u_int32_t e3v;

	printk("EMAC3 register\n");
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_MODE0);
	printk("mode0(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_MODE1);
	printk("mode1(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_TxMODE0);
	printk("TXmode0(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_TxMODE1);
	printk("TXmode1(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_RxMODE);
	printk("RXmode(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_INTR_STAT);
	printk("INTR stat(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_INTR_ENABLE);
	printk("INTR enable(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_ADDR_HI);
	printk("addr HI(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_ADDR_LO);
	printk("LO(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_VLAN_TPID);
	printk("vlan TPID(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_VLAN_TCI);
	printk("vlan TCI(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_PAUSE_TIMER);
	printk("pause(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_INDIVID_HASH1);
	printk("Indivi 1(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_INDIVID_HASH2);
	printk("2(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_INDIVID_HASH3);
	printk("3(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_INDIVID_HASH4);
	printk("4(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_GROUP_HASH1);
	printk("Group 1(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_GROUP_HASH2);
	printk("2(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_GROUP_HASH3);
	printk("3(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_GROUP_HASH4);
	printk("4(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_LAST_SA_HI);
	printk("LAST SA HI(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_LAST_SA_LO);
	printk("LO(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_INTER_FRAME_GAP);
	printk("IFG(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_STA_CTRL);
	printk("STA ctrl(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_TX_THRESHOLD);
	printk("TX threshold(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_RX_WATERMARK);
	printk("RX watermark(0x%08x)\n", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_TX_OCTETS);
	printk("TX octets(0x%08x), ", e3v);
	e3v = EMAC3REG_READ(smap, SMAP_EMAC3_RX_OCTETS);
	printk("RX octets(0x%08x)\n", e3v);
}

/*--------------------------------------------------------------------------*/

static void
smap_dma_force_break(struct smap_chan *smap)
{
	int i, ret;
	struct net_device *net_dev = smap->net_dev;
	struct completion compl;

	if ((smap->flags & SMAP_F_DMA_ENABLE) == 0)
		return;

	init_completion(&compl);
	if (smap->flags & SMAP_F_DMA_TX_ENABLE) {
		i = 100;
		do {
			if (--i == 0)
				break;
			ret = ps2sif_callrpc(&smap->cd_smap_tx_end, 0,
				SIF_RPCM_NOWAIT, NULL, 0, NULL, 0,
				(ps2sif_endfunc_t)smap_rpcend_notify,
				(void *)&compl);
			switch (ret) {
			case 0:
				break;
			case -SIF_RPCE_SENDP:
				break;
			default:
				printk("%s: TX DMA stop callrpc failed(%d)\n",
							net_dev->name, ret);
				break;
			}
		} while (ret == -SIF_RPCE_SENDP);
		if (i == 0) {
			printk("%s: TX DMA stop callrpc failed2\n",
								net_dev->name);
		} else {
			if (ret == 0) {
				wait_for_completion(&compl);
			}
		}
	}
	if (smap->flags & SMAP_F_DMA_RX_ENABLE) {
		i = 100;
		do {
			if (--i == 0)
				break;
			ret = ps2sif_callrpc(&smap->cd_smap_rx_end, 0,
				SIF_RPCM_NOWAIT, NULL, 0, NULL, 0,
				(ps2sif_endfunc_t)smap_rpcend_notify,
				(void *)&compl);
			switch (ret) {
			case 0:
				break;
			case -SIF_RPCE_SENDP:
				break;
			default:
				printk("%s: RX DMA stop callrpc failed(%d)\n",
							net_dev->name, ret);
				break;
			}
		} while (ret == -SIF_RPCE_SENDP);
		if (i == 0) {
			printk("%s: RX DMA stop callrpc failed2\n",
								net_dev->name);
		} else {
			if (ret == 0) {
				wait_for_completion(&compl);
			}
		}
	}
	return;
}

static void
smap_rpcend_notify(void *arg)
{
	complete((struct completion *)arg);
	return;
}

static void
smap_dma_setup(struct smap_chan *smap)
{
	int loop;
	volatile int j;
	struct completion compl;

	init_completion(&compl);
	smap->flags &= ~(SMAP_F_DMA_ENABLE|SMAP_F_DMA_TX_ENABLE|SMAP_F_DMA_RX_ENABLE);

	/* bind DMA relay module */
	for (loop = 100; loop; loop--) {
		ps2sif_bindrpc(&smap->cd_smap_tx, SIFNUM_SMAP_TX_DMA_BEGIN,
			SIF_RPCM_NOWAIT, smap_rpcend_notify, (void *)&compl);
		wait_for_completion(&compl);
		if (smap->cd_smap_tx.serve != 0)
			break;
		j = 0x010000;
		while (j--) ;
	}
	if (smap->cd_smap_tx.serve == 0) {
		printk("%s: dma setup: bind error 1, use PIO\n", smap->net_dev->name);
		return;
	}

	for (loop = 100; loop; loop--) {
		ps2sif_bindrpc(&smap->cd_smap_tx_end, SIFNUM_SMAP_TX_DMA_END,
			SIF_RPCM_NOWAIT, smap_rpcend_notify, (void *)&compl);
		wait_for_completion(&compl);
		if (smap->cd_smap_tx_end.serve != 0)
			break;
		j = 0x010000;
		while (j--) ;
	}
	if (smap->cd_smap_tx_end.serve == 0) {
		printk("%s: dma setup: bind error 2, use PIO\n", smap->net_dev->name);
		return;
	}

	for (loop = 100; loop; loop--) {
		ps2sif_bindrpc(&smap->cd_smap_rx, SIFNUM_SMAP_RX_DMA_BEGIN,
			SIF_RPCM_NOWAIT, smap_rpcend_notify, (void *)&compl);
		wait_for_completion(&compl);
		if (smap->cd_smap_rx.serve != 0)
			break;
		j = 0x010000;
		while (j--) ;
	}
	if (smap->cd_smap_rx.serve == 0) {
		printk("%s: dma setup: bind error 3, use PIO\n", smap->net_dev->name);
		return;
	}

	for (loop = 100; loop; loop--) {
		ps2sif_bindrpc(&smap->cd_smap_rx_end, SIFNUM_SMAP_RX_DMA_END,
			SIF_RPCM_NOWAIT, smap_rpcend_notify, (void *)&compl);
		wait_for_completion(&compl);
		if (smap->cd_smap_rx_end.serve != 0)
			break;
		j = 0x010000;
		while (j--) ;
	}
	if (smap->cd_smap_rx_end.serve == 0) {
		printk("%s: dma setup: bind error 4, use PIO\n", smap->net_dev->name);
		return;
	}

	/* get buffer address for TX DMA */
	if (ps2sif_callrpc(&smap->cd_smap_tx, SIFNUM_SmapGetTxBufAddr,
			SIF_RPCM_NOWAIT,
			NULL, 0, &smap->txdma_ibuf, sizeof(u_int32_t),
			smap_rpcend_notify, (void *)&compl) != 0) {
		printk("%s: dma setup: get dma Txbuf address error, use PIO\n",
							smap->net_dev->name);
		return;
	}
	wait_for_completion(&compl);
	smap->txdma_ibuf = (u_int32_t)KSEG0ADDR(ps2sif_bustovirt(smap->txdma_ibuf));
	smap->flags |= (SMAP_F_DMA_TX_ENABLE|SMAP_F_DMA_ENABLE);

	if (smap->flags & SMAP_F_PRINT_MSG) {
		printk("%s: dma setup: txdma_ibuf = 0x%08x\n",
				smap->net_dev->name, smap->txdma_ibuf);
	}

	/* get buffer address for RX DMA */
	if (ps2sif_callrpc(&smap->cd_smap_rx, SIFNUM_SmapGetRxBufAddr,
			SIF_RPCM_NOWAIT,
			NULL, 0, &smap->rxdma_ibuf, sizeof(u_int32_t),
			smap_rpcend_notify, (void *)&compl) != 0) {
		printk("%s: dma setup: get dma Rxbuf address error, use PIO\n",
							smap->net_dev->name);
		return;
	}
	wait_for_completion(&compl);
	smap->rxdma_ibuf = (u_int32_t)KSEG0ADDR(ps2sif_bustovirt(smap->rxdma_ibuf));
	smap->flags |= (SMAP_F_DMA_RX_ENABLE|SMAP_F_DMA_ENABLE);

	if (smap->flags & SMAP_F_PRINT_MSG) {
		printk("%s: dma setup: rxdma_ibuf = 0x%08x\n",
				smap->net_dev->name, smap->rxdma_ibuf);
	}

	return;
}

/*--------------------------------------------------------------------------*/

static void
smap_run(struct smap_chan *smap)
{
	unsigned long flags;

	if ((smap->flags & SMAP_F_LINKVALID) == 0)
		return;
	if ((smap->flags & SMAP_F_INITDONE) == 0)
		return;
	if ((smap->flags & SMAP_F_OPENED) == 0) {
		spin_lock_irqsave(&smap->spinlock, flags);
		while (smap->txqueue.qlen > 0) {
			spin_unlock_irqrestore(&smap->spinlock, flags);
			(void)smap_start_xmit2(smap);
			(void)smap_tx_intr(smap->net_dev);
			spin_lock_irqsave(&smap->spinlock, flags);
		}
		spin_unlock_irqrestore(&smap->spinlock, flags);
		if (smap->tx_qpkt_compl != NULL)
			complete(smap->tx_qpkt_compl); /*notify all pkts sent*/
		netif_stop_queue(smap->net_dev);
		return;
	}

	for (;;) {
		if ((smap->flags & SMAP_F_OPENED) == 0) {
			spin_lock_irqsave(&smap->spinlock, flags);
			while (smap->txqueue.qlen > 0) {
				spin_unlock_irqrestore(&smap->spinlock, flags);
				(void)smap_start_xmit2(smap);
				(void)smap_tx_intr(smap->net_dev);
				spin_lock_irqsave(&smap->spinlock, flags);
			}
			spin_unlock_irqrestore(&smap->spinlock, flags);
			if (smap->tx_qpkt_compl != NULL)
				complete(smap->tx_qpkt_compl); /*notify all pkts sent*/
			netif_stop_queue(smap->net_dev);
			break;
		}

		spin_lock_irqsave(&smap->spinlock, flags);
		if ((smap->txqueue.qlen > 0) && (smap->flags & SMAP_F_OPENED)) {
			spin_unlock_irqrestore(&smap->spinlock, flags);
			(void)smap_start_xmit2(smap);
		} else
			spin_unlock_irqrestore(&smap->spinlock, flags);

		spin_lock_irqsave(&smap->spinlock, flags);
		if ((smap->txicnt > 0) && (smap->flags & SMAP_F_OPENED)) {
			spin_unlock_irqrestore(&smap->spinlock, flags);
			(void)smap_tx_intr(smap->net_dev);
		} else
			spin_unlock_irqrestore(&smap->spinlock, flags);

		spin_lock_irqsave(&smap->spinlock, flags);
		if ((smap->rxicnt > 0) && (smap->flags & SMAP_F_OPENED)) {
			spin_unlock_irqrestore(&smap->spinlock, flags);
			(void)smap_rx_intr(smap->net_dev);
		} else
			spin_unlock_irqrestore(&smap->spinlock, flags);

		spin_lock_irqsave(&smap->spinlock, flags);
		if ( (smap->txqueue.qlen == 0) && (smap->txicnt == 0) &&
		     (smap->rxicnt == 0) ) {
			spin_unlock_irqrestore(&smap->spinlock, flags);
			break;
		} else
			spin_unlock_irqrestore(&smap->spinlock, flags);
	}

	return;
}

static int
smap_thread(void *arg)
{
	struct smap_chan *smap = (struct smap_chan *)arg;
	unsigned long flags;

	spin_lock_irqsave(&current->sigmask_lock, flags);
	siginitsetinv(&current->blocked,
			sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	lock_kernel();

	/* get rid of all our resources related to user space */
	daemonize();

	/* Set the name of this process. */
	sprintf(current->comm, "smap");		/* up to 16B */
        
	unlock_kernel();

	smap->smaprun_task = current;

	while (1) {
		wait_queue_t wait;
		init_waitqueue_entry(&wait, current);
		add_wait_queue(&smap->wait_smaprun, &wait);
		set_current_state(TASK_INTERRUPTIBLE);

		(void)smap_run(smap);

		schedule();
		remove_wait_queue(&smap->wait_smaprun, &wait);
		if (signal_pending(current))
			break;
	}

	smap->smaprun_task = NULL;
	if (smap->smaprun_compl != NULL)
		complete(smap->smaprun_compl);	/* notify that we've exited */

	return(0);
}

/*--------------------------------------------------------------------------*/

static void
smap_chk_linkvalid(struct smap_chan *smap)
{
	u_int16_t phyval = 0;
	struct net_device *net_dev = smap->net_dev;

	if ((smap->flags & SMAP_F_INITDONE) == 0)
		return;

	phyval = (u_int16_t)smap_read_phy(smap, DsPHYTER_ADDRESS,DsPHYTER_BMSR);
	if ((phyval & PHY_BMSR_LINK) == 0) {
		if (smap->flags & SMAP_F_LINKVALID) {
			smap->flags &= ~(SMAP_F_LINKESTABLISH|SMAP_F_LINKVALID);
			printk("%s: link down\n", smap->net_dev->name);
			netif_carrier_off(net_dev);
			netif_stop_queue(net_dev);
			(void)smap_dma_force_break(smap);
			(void)smap_reset(smap, RESET_ONLY);
			(void)smap_skb_queue_clear(smap, &smap->txqueue);
		}
	}
	if (phyval & PHY_BMSR_LINK) {
		if ((smap->flags & SMAP_F_LINKVALID) == 0) {
			(void)smap_reset(smap, RESET_INIT);
			(void)smap_txrx_XXable(smap, DISABLE);
			netif_carrier_on(net_dev);
			if (smap->flags & SMAP_F_OPENED) {
				(void)smap_txbd_init(smap);
				(void)smap_rxbd_init(smap);
				(void)smap_skb_queue_init(smap, &smap->txqueue);
				(void)smap_clear_all_interrupt(smap);
				(void)smap_interrupt_XXable(smap, ENABLE);
				(void)smap_txrx_XXable(smap, ENABLE);
				(void)smap_multicast_list(smap->net_dev);
				netif_wake_queue(net_dev);
			} else {
				printk("%s: you need \"ifup ethX\"\n",
							smap->net_dev->name);
			}
		}
	}

	return;
}

static int smap_chklv_ival = 1;
MODULE_PARM(smap_chklv_ival, "i");

static int
smap_chk_linkvalid_thread(void *arg)
{
	struct smap_chan *smap = (struct smap_chan *)arg;
	unsigned long flags;

	spin_lock_irqsave(&current->sigmask_lock, flags);
	siginitsetinv(&current->blocked,
			sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	lock_kernel();

	/* get rid of all our resources related to user space */
	daemonize();

	/* Set the name of this process. */
	sprintf(current->comm, "smap chk lv %d", smap_chklv_ival);
						/* up to 16B */

	unlock_kernel();

	smap->chk_linkvalid_task = current;

	while (1) {
		(void)smap_chk_linkvalid(smap);

		interruptible_sleep_on_timeout(&smap->wait_chk_linkvalid,
						HZ * smap_chklv_ival);
		if (signal_pending(current))
			break;
	}

	smap->chk_linkvalid_task = NULL;
	if (smap->chk_linkvalid_compl != NULL)
		complete(smap->chk_linkvalid_compl);/* notify that we've exited */

	return(0);
}

/*--------------------------------------------------------------------------*/

static int smap_dma_enable = 1;
MODULE_PARM(smap_dma_enable, "i");

static int
smap_init_thread(void *arg)
{
	struct smap_chan *smap = (struct smap_chan *)arg;
	unsigned long flags;

	spin_lock_irqsave(&current->sigmask_lock, flags);
	siginitsetinv(&current->blocked,
			sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);

	lock_kernel();

	/* get rid of all our resources related to user space */
	daemonize();

#if 1
	reparent_to_init();
#endif
	/* Set the name of this process. */
	sprintf(current->comm, "smap init");	/* up to 16B */

	unlock_kernel();

	smap_reset(smap, RESET_INIT);
	smap_txrx_XXable(smap, DISABLE);
	smap_txbd_init(smap);
	smap_rxbd_init(smap);

	if (smap_dma_enable) {
		smap_dma_setup(smap);
	} else {
		printk("PlayStation 2 SMAP: PIO mode.\n");
		smap->flags &= ~(SMAP_F_DMA_ENABLE|SMAP_F_DMA_TX_ENABLE|SMAP_F_DMA_RX_ENABLE);
	}

	if (smap->flags & SMAP_F_LINKVALID)
		netif_carrier_on(smap->net_dev);
	else
		netif_carrier_off(smap->net_dev);

	smap->irq = IRQ_SBUS_PCIC;

	smap->flags |= SMAP_F_INITDONE;

	return(0);
}

extern int ps2_pccard_present;

int
smap_probe(struct net_device *net_dev)
{
	struct smap_chan *smap = NULL;
	int modflag = 0;
	int r;

	if (net_dev == NULL)
		modflag = 1;

	if (ps2_pccard_present != 0x0100) {
		printk("PlayStation 2 HDD/Ethernet device NOT present.\n");
		return(-ENODEV);
	}

	if (smap_chan != NULL) {
		printk("PlayStation 2 SMAP: already used\n");
		return(-ENODEV);
	}

	/* alloc & clear control structure */
	smap = kmalloc(sizeof(struct smap_chan), GFP_KERNEL);
	if (smap == NULL) {
		printk("PlayStation 2 SMAP: momory alloc error\n");
		return(-ENOMEM);
	}
	memset(smap, 0, sizeof(struct smap_chan));

	/* get & init network device structure */
	if (modflag) {
		net_dev = init_etherdev(NULL, 0);
		if (net_dev == NULL) {
			printk("PlayStation 2 SMAP: init_etherdev error\n");
			goto error;
		}
	} else {
		ether_setup(net_dev);
	}
	smap->net_dev = net_dev;
	net_dev->priv = smap;
	SET_MODULE_OWNER(net_dev);

	net_dev->open = smap_open;
	net_dev->stop = smap_close;
	net_dev->do_ioctl = smap_ioctl;
	net_dev->hard_start_xmit = smap_start_xmit;
	net_dev->set_multicast_list = smap_multicast_list;
	net_dev->get_stats = smap_get_stats;
	net_dev->set_mac_address = NULL;
#ifdef HAVE_TX_TIMEOUT
	net_dev->tx_timeout = smap_tx_timeout;
	net_dev->watchdog_timeo = 5 * HZ;
#endif /* HAVE_TX_TIMEOUT */

	/* alloc tx/rx buffer(16B align) */
	smap->dtxbuf = kmalloc(SMAP_BUFSIZE+SMAP_ALIGN+SMAP_TXMAXTAILPAD,
								GFP_KERNEL);
	smap->drxbuf = kmalloc(SMAP_BUFSIZE+SMAP_ALIGN+SMAP_RXMAXTAILPAD,
								GFP_KERNEL);
	if ((smap->dtxbuf == NULL) || (smap->drxbuf == NULL)){
		printk("PlayStation 2 SMAP: tx(%p)/rx(%p) buffer alloc error\n",
				smap->dtxbuf, smap->drxbuf);
		goto error;
	}
	if (((int)smap->dtxbuf & (SMAP_ALIGN-1)) == 0)
		smap->txbuf = smap->dtxbuf;
	else
		smap->txbuf = (u_int8_t *)((int)smap->dtxbuf & ~(SMAP_ALIGN-1))
						+ SMAP_ALIGN;

	if (((int)smap->drxbuf & (SMAP_ALIGN-1)) == 0)
		smap->rxbuf = smap->drxbuf;
	else
		smap->rxbuf = (u_int8_t *)((int)smap->drxbuf & ~(SMAP_ALIGN-1))
						+ SMAP_ALIGN;

	if ( ((int)smap->txbuf & (SMAP_ALIGN-1)) ||
				((int)smap->rxbuf & (SMAP_ALIGN-1)) ) {
		printk("PlayStation 2 SMAP: buffer alignment error, "
			"tx buf=0x%p, rx buf=0x%p\n", smap->txbuf, smap->rxbuf);
	}

	smap_base_init(smap);
	spin_lock_init(&smap->spinlock);
	init_waitqueue_head(&smap->wait_linknego);
	init_waitqueue_head(&smap->wait_linkvalid);
	init_waitqueue_head(&smap->wait_chk_linkvalid);
	init_waitqueue_head(&smap->wait_smaprun);
#ifdef HAVE_TX_TIMEOUT
	init_waitqueue_head(&smap->wait_timeout);
#endif /* HAVE_TX_TIMEOUT */
	r = smap_get_node_addr(smap);
	if (r < 0)
		goto error;
	smap_chan = smap;

	/* create and start thread */
	kernel_thread(smap_init_thread, (void *)smap, 0);
	if (smap_chklv_ival > 0) {
		if (smap_chklv_ival > 999)
			smap_chklv_ival = 999;	/* MAX: a three-digit number */
		kernel_thread(smap_chk_linkvalid_thread, (void *)smap, 0);
	}
#ifdef HAVE_TX_TIMEOUT
	kernel_thread(smap_timeout_thread, (void *)smap, 0);
#endif /* HAVE_TX_TIMEOUT */
	kernel_thread(smap_thread, (void *)smap, 0);

	printk("PlayStation 2 SMAP(Ethernet) device driver.\n");

	return(0);	/* success */

error:
	if (smap) {
		if (smap->dtxbuf) {
			kfree(smap->dtxbuf);
		}
		if (smap->drxbuf) {
			kfree(smap->drxbuf);
		}
		if (smap->net_dev) {
			if (smap->net_dev->flags & IFF_UP)
				dev_close(smap->net_dev);
			if (modflag) {
				unregister_netdev(smap->net_dev);
				kfree(smap->net_dev);
			}
		}
		kfree(smap);
	}
	smap_chan = NULL;
	return(-ENODEV);
}

void
smap_cleanup_module(void)
{
	struct smap_chan *smap = smap_chan;
	struct net_device *net_dev;

	if (smap == NULL) {
		printk("smap control structure error(null).\n");
		return;
	}

	if (smap->chk_linkvalid_task != NULL) {
		struct completion compl;

		init_completion(&compl);
		smap->chk_linkvalid_compl = &compl;
		send_sig(SIGKILL, smap->chk_linkvalid_task, 1);

		/* wait the thread exit */
		wait_for_completion(&compl);
		smap->chk_linkvalid_compl = NULL;
	}
#ifdef HAVE_TX_TIMEOUT
	if (smap->timeout_task != NULL) {
		struct completion compl;

		init_completion(&compl);
		smap->timeout_compl = &compl;
		send_sig(SIGKILL, smap->timeout_task, 1);

		/* wait the thread exit */
		wait_for_completion(&compl);
		smap->timeout_compl = NULL;
	}
#endif /* HAVE_TX_TIMEOUT */
	if (smap->smaprun_task != NULL) {
		struct completion compl;

		init_completion(&compl);
		smap->smaprun_compl = &compl;
		send_sig(SIGKILL, smap->smaprun_task, 1);

		/* wait the thread exit */
		wait_for_completion(&compl);
		smap->smaprun_compl = NULL;
	}

	if (smap->net_dev == NULL)
		goto end;

	net_dev = smap->net_dev;
	printk("%s: unloading...", net_dev->name);

	if (net_dev->flags & IFF_UP)
		dev_close(net_dev);

	unregister_netdev(net_dev);

end:
	if (smap) {
		(void)smap_reset(smap, RESET_ONLY);

		if (smap->dtxbuf) {
			kfree(smap->dtxbuf);
		}
		if (smap->drxbuf) {
			kfree(smap->drxbuf);
		}
		if (smap->net_dev); {
			kfree(smap->net_dev);
		}
		kfree(smap);
	}
	smap_chan = NULL;
	printk(" done\n");
	return;
}

int __init
smap_init_module(void)
{
	return(smap_probe(NULL));
}

module_init(smap_init_module);
module_exit(smap_cleanup_module);

MODULE_AUTHOR("Sony Computer Entertainment Inc.");
MODULE_DESCRIPTION("PlayStation 2 ethernet device driver");
MODULE_LICENSE("GPL");

/*--------------------------------------------------------------------------*/

#endif /* linux */

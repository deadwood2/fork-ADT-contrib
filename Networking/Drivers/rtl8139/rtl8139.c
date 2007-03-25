/*
 * $Id$
 */

/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston,
    MA 02111-1307, USA.
*/

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <exec/ports.h>

#include <aros/libcall.h>
#include <aros/macros.h>
#include <aros/io.h>

#include <oop/oop.h>

#include <devices/sana2.h>
#include <devices/sana2specialstats.h>

#include <utility/utility.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>

#include <hidd/pci.h>
#include <hidd/irq.h>

#include <proto/oop.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/battclock.h>

#include <stdlib.h>

#include "rtl8139.h"
#include "unit.h"
#include LC_LIBDEFS_FILE

/* A bit fixed linux stuff here :) */

#undef LIBBASE
#define LIBBASE (dev->rtl8139u_device)

#define net_device RTL8139Unit

#define TIMER_RPROK 3599597124UL

static ULONG usec2tick(ULONG usec)
{
    ULONG ret, timer_rpr = TIMER_RPROK;
    asm volatile("movl $0,%%eax; divl %2":"=a"(ret):"d"(usec),"m"(timer_rpr));
    return ret;
}

void udelay(LONG usec)
{
    int oldtick, tick;
    usec = usec2tick(usec);

    BYTEOUT(0x43, 0x80);
    oldtick = BYTEIN(0x42);
    oldtick += BYTEIN(0x42) << 8;

    while (usec > 0)
    {
        BYTEOUT(0x43, 0x80);
        tick = BYTEIN(0x42);
        tick += BYTEIN(0x42) << 8;

        usec -= (oldtick - tick);
        if (tick > oldtick) usec -= 0x10000;
        oldtick = tick;
    }
}

static inline struct fe_priv *get_pcnpriv(struct net_device *dev)
{
    return dev->rtl8139u_fe_priv;
}

static inline UBYTE *get_hwbase(struct net_device *dev)
{
    return (UBYTE*)dev->rtl8139u_BaseMem;
}

static int read_eeprom(long base, int location, int addr_len)
{
  int i;
  unsigned retval = 0;
  long rtlprom_addr = base + RTLr_Cfg9346;
  int read_cmd = location | (EE_READ_CMD << addr_len);

  BYTEOUT(rtlprom_addr, EE_ENB & ~EE_CS);
  BYTEOUT(rtlprom_addr, EE_ENB);

  // Shift the read command bits out
  for (i = 4 + addr_len; i >= 0; i--) 
  {
    int dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
    BYTEOUT(rtlprom_addr, EE_ENB | dataval);
    eeprom_delay(rtlprom_addr);
    BYTEOUT(rtlprom_addr, EE_ENB | dataval | EE_SHIFT_CLK);
    eeprom_delay(rtlprom_addr);
  }
  BYTEOUT(rtlprom_addr, EE_ENB);
  eeprom_delay(rtlprom_addr);

  for (i = 16; i > 0; i--) 
  {
    BYTEOUT(rtlprom_addr, EE_ENB | EE_SHIFT_CLK);
    eeprom_delay(rtlprom_addr);
    retval = (retval << 1) | ((BYTEIN(rtlprom_addr) & EE_DATA_READ) ? 1 : 0);
    BYTEOUT(rtlprom_addr, EE_ENB);
    eeprom_delay(rtlprom_addr);
  }

  // Terminate EEPROM access
  BYTEOUT(rtlprom_addr, ~EE_CS);
  return retval;
}

// Syncronize the MII management interface by shifting 32 one bits out
static char mii_2_8139_map[8] = 
{
  RTLr_MII_BMCR, RTLr_MII_BMSR, 0, 0, RTLr_NWayAdvert, RTLr_NWayLPAR, RTLr_NWayExpansion, 0 
};

static void mdio_sync(long base)
{
	int i;

	for (i = 32; i >= 0; i--) 
	{
		BYTEOUT(base, MDIO_WRITE1);
		mdio_delay(base);
		BYTEOUT(base, MDIO_WRITE1 | MDIO_CLK);
		mdio_delay(base);
	}
}

static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	struct fe_priv *np = get_pcnpriv(dev);
	UBYTE *base = get_hwbase(dev);
	int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	int retval = 0;
	int i;

	if (phy_id > 31) 
	{  
		// Really a 8139.  Use internal registers
		return location < 8 && mii_2_8139_map[location] ? WORDIN(base + mii_2_8139_map[location]) : 0;
	}

	mdio_sync(base + RTLr_MII_SMI);

	// Shift the read command bits out
	for (i = 15; i >= 0; i--) 
	{
		int dataval = (mii_cmd & (1 << i)) ? MDIO_DATA_OUT : 0;

		BYTEOUT(base + RTLr_MII_SMI, MDIO_DIR | dataval);
		mdio_delay(base + RTLr_MII_SMI);
		BYTEOUT(base + RTLr_MII_SMI, MDIO_DIR | dataval | MDIO_CLK);
		mdio_delay(base + RTLr_MII_SMI);
	}

	// Read the two transition, 16 data, and wire-idle bits
	for (i = 19; i > 0; i--) 
	{
		BYTEOUT(base + RTLr_MII_SMI, 0);
		mdio_delay(base + RTLr_MII_SMI);
		retval = (retval << 1) | ((BYTEIN(base + RTLr_MII_SMI) & MDIO_DATA_IN) ? 1 : 0);
		BYTEOUT(base + RTLr_MII_SMI, MDIO_CLK);
		mdio_delay(base + RTLr_MII_SMI);
	}

	return (retval >> 1) & 0xffff;
}

static void rtl8139nic_start_rx(struct net_device *dev)
{
    struct fe_priv *np = get_pcnpriv(dev);
    UBYTE *base = get_hwbase(dev);

D(bug("%s: rtl8139nic_start_rx\n", dev->rtl8139u_name));
    // Already running? Stop it.
#warning "TODO: Handle starting/stopping Rx"
}

static void rtl8139nic_stop_rx(struct net_device *dev)
{
    UBYTE *base = get_hwbase(dev);

D(bug("%s: rtl8139nic_stop_rx\n", dev->rtl8139u_name));
#warning "TODO: Handle starting/stopping Rx"
}

static void rtl8139nic_start_tx(struct net_device *dev)
{
    UBYTE *base = get_hwbase(dev);

D(bug("%s: rtl8139nic_start_tx()\n", dev->rtl8139u_name));
#warning "TODO: Handle starting/stopping Tx"
}

static void rtl8139nic_stop_tx(struct net_device *dev)
{
    UBYTE *base = get_hwbase(dev);

D(bug("%s: rtl8139nic_stop_tx()\n", dev->rtl8139u_name));
#warning "TODO: Handle starting/stopping Tx"
}

static void rtl8139nic_txrx_reset(struct net_device *dev)
{
    struct fe_priv *np = get_pcnpriv(dev);
    UBYTE *base = get_hwbase(dev);

D(bug("%s: rtl8139nic_txrx_reset()\n", dev->rtl8139u_name));
}

/*
 * rtl8139nic_set_multicast: dev->set_multicast function
 * Called with dev->xmit_lock held.
 */
static void rtl8139nic_set_multicast(struct net_device *dev)
{
    struct fe_priv *np = get_pcnpriv(dev);
    UBYTE *base = get_hwbase(dev);
    ULONG addr[2];
    ULONG mask[2];
    ULONG pff;

D(bug("%s: rtl8139nic_set_multicast()\n", dev->rtl8139u_name));

    memset(addr, 0, sizeof(addr));
    memset(mask, 0, sizeof(mask));
}

static void rtl8139nic_deinitialize(struct net_device *dev)
{

}

static void rtl8139nic_initialize(struct net_device *dev)
{
    struct fe_priv *np = dev->rtl8139u_fe_priv;
    UBYTE *base = get_hwbase(dev);
    int i, config1;

	config1 = BYTEIN(base + RTLr_Config1);
	if (dev->rtl8139u_rtl_chipcapabilities & RTLc_HAS_MII_XCVR)
	{
		BYTEOUT(base + RTLr_Config1, config1 & ~0x03);
	}
D(bug("%s: Chipset brought out of low power mode.\n", dev->rtl8139u_name));

	{
		int addr_len = read_eeprom(base, 0, 8) == 0x8129 ? 8 : 6;
		int mac_add = 0;
		for (i = 0; i < 3; i++)
		{
			UWORD mac_curr = AROS_LE2WORD(read_eeprom(base, i + 7, addr_len));
			np->orig_mac[mac_add++] = mac_curr & 0xff;
			np->orig_mac[mac_add++] = (mac_curr >> 8) & 0xff;
		}
	}

	int phy, phy_idx = 0;
	if (dev->rtl8139u_rtl_chipcapabilities & RTLc_HAS_MII_XCVR)
	{
		for (phy = 0; phy < 32 && phy_idx < sizeof(np->mii_phys); phy++)
		{
			int mii_status = mdio_read(dev, phy, 1);
			if (mii_status != 0xffff && mii_status != 0x0000)
			{
				np->mii_phys[phy_idx++] = phy;
				np->advertising = mdio_read(dev, phy, 4);
D(bug("%s: MII transceiver %d status 0x%4.4x advertising %4.4x\n", dev->rtl8139u_name,
																										  phy, mii_status, np->advertising));

			}
		}
	}

	if (phy_idx ==0)
	{
D(bug("%s: No MII transceiver found, Assuming SYM transceiver\n", dev->rtl8139u_name));
		np->mii_phys[0] = 32;
	}
		
    dev->rtl8139u_dev_addr[0] = dev->rtl8139u_org_addr[0] = np->orig_mac[0];
    dev->rtl8139u_dev_addr[1] = dev->rtl8139u_org_addr[1] = np->orig_mac[1];
    dev->rtl8139u_dev_addr[2] = dev->rtl8139u_org_addr[2] = np->orig_mac[2];
    dev->rtl8139u_dev_addr[3] = dev->rtl8139u_org_addr[3] = np->orig_mac[3];

    dev->rtl8139u_dev_addr[4] = dev->rtl8139u_org_addr[4] = np->orig_mac[4];
    dev->rtl8139u_dev_addr[5] = dev->rtl8139u_org_addr[5] = np->orig_mac[5];

D(bug("%s: MAC Address %02x:%02x:%02x:%02x:%02x:%02x\n", dev->rtl8139u_name,
            dev->rtl8139u_dev_addr[0], dev->rtl8139u_dev_addr[1], dev->rtl8139u_dev_addr[2],
            dev->rtl8139u_dev_addr[3], dev->rtl8139u_dev_addr[4], dev->rtl8139u_dev_addr[5]));
  
//	BYTEOUT(base + RTLr_Cfg9346, 0xc0);
//	if (dev->rtl8139u_rtl_chipcapabilities & RTLc_HAS_MII_XCVR)
//	{
//		BYTEOUT(base + RTLr_Config1, 0x03);
//	}
//	BYTEOUT(base + RTLr_HltClk, 'H'); //Disable the chips clock ('R' enables)
//D(bug("%s: Chipset put into low power mode.\n", dev->rtl8139u_name));
}

static void rtl8139nic_drain_tx(struct net_device *dev)
{
    struct fe_priv *np = get_pcnpriv(dev);
    int i;
    for (i = 0; i < NUM_TX_DESC; i++) {
#warning "TODO: rtl8139nic_drain_tx does nothing atm."
    }
}

static void rtl8139nic_drain_rx(struct net_device *dev)
{
    struct fe_priv *np = get_pcnpriv(dev);
    int i;
    for (i = 0; i < RX_RING_SIZE; i++) {
#warning "TODO: rtl8139nic_drain_rx does nothing atm."
    }
}


static void drain_ring(struct net_device *dev)
{
    rtl8139nic_drain_tx(dev);
    rtl8139nic_drain_rx(dev);
}

static int request_irq(struct net_device *dev)
{
    OOP_Object *irq = OOP_NewObject(NULL, CLID_Hidd_IRQ, NULL);
    BOOL ret;

D(bug("%s: request_irq()\n", dev->rtl8139u_name));

    if (irq)
    {
        ret = HIDD_IRQ_AddHandler(irq, dev->rtl8139u_irqhandler, dev->rtl8139u_IRQ);
        HIDD_IRQ_AddHandler(irq, dev->rtl8139u_touthandler, vHidd_IRQ_Timer);

D(bug("%s: request_irq: IRQ Handlers configured\n", dev->rtl8139u_name));

        OOP_DisposeObject(irq);

        if (ret)
        {
            return 0;
        }
    }
    return 1;
}

static void free_irq(struct net_device *dev)
{
    OOP_Object *irq = OOP_NewObject(NULL, CLID_Hidd_IRQ, NULL);
    if (irq)
    {
        HIDD_IRQ_RemHandler(irq, dev->rtl8139u_irqhandler);
        HIDD_IRQ_RemHandler(irq, dev->rtl8139u_touthandler);
        OOP_DisposeObject(irq);
    }
}

int rtl8139nic_set_rxmode(struct net_device *dev)
{
	struct fe_priv *np = get_pcnpriv(dev);
	UBYTE *base = get_hwbase(dev);
	unsigned long mc_filter[2];         //Multicast hash filter.
	unsigned int rx_mode;
	
#warning "TODO: Fix set_rxmode.. temp Running in promiscuous mode for now .."
	rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
	mc_filter[1] = mc_filter[0] = 0xffffffff;

	LONGOUT(base + RTLr_RxConfig, np->rx_config | rx_mode);
	LONGOUT(base + RTLr_MAR0 + 0, mc_filter[0]);
	LONGOUT(base + RTLr_MAR0 + 4, mc_filter[1]);
	
	return 0;
}

static void rtl8139nic_set_mac(struct net_device *dev)
{
   UBYTE *base = get_hwbase(dev);
   int i,j;

	BYTEOUT(base + RTLr_Cfg9346, 0xc0);
	j=0;
	for (i = 0; i < 3; i++)
	{
		UWORD mac_curr = dev->rtl8139u_dev_addr[j++];
		mac_curr += dev->rtl8139u_dev_addr[j++] << 8;
		WORDOUT(base +RTLr_MAC0 + i, mac_curr);
	}
	
	BYTEOUT(base + RTLr_Cfg9346, 0x00);

}

static int rtl8139nic_open(struct net_device *dev)
{
   struct fe_priv *np = get_pcnpriv(dev);
   UBYTE *base = get_hwbase(dev);
   int ret, i, rx_buf_len_idx;

   rx_buf_len_idx = RX_BUF_LEN_IDX;

	ret = request_irq(dev);
	if (ret)
		goto out_drain;

   do
   {
		np->rx_buf_len = 8192 << rx_buf_len_idx;
		
		np->rx_buffer = HIDD_PCIDriver_AllocPCIMem(
                        dev->rtl8139u_PCIDriver,
                        (np->rx_buf_len + 16 + (TX_BUF_SIZE * NUM_TX_DESC)));

		if (np->rx_buffer != NULL)
		{
			np->tx_buffer = (struct eth_frame *) (np->rx_buffer + np->rx_buf_len + 16);
		}
   } while (np->rx_buffer == NULL && (--rx_buf_len_idx >= 0));

   if (np->rx_buffer != NULL)
   {
D(bug("%s: rtl8139nic_open: Allocated IO Buffers [ %d x Tx @ %x] [ Rx @ %x, %d bytes]\n",dev->rtl8139u_name,
                        NUM_TX_DESC, np->tx_buffer,
                        np->rx_buffer,  np->rx_buf_len));
   
		np->tx_dirty = np->tx_current = 0;

		for (i = 0; i < NUM_TX_DESC; i++)
		{
			np->tx_pbuf[i] = NULL;
			np->tx_buf[i]   = np->tx_buffer + (i * TX_BUF_SIZE);
		}
D(bug("%s: rtl8139nic_open: TX Buffers initialised\n",dev->rtl8139u_name));

		np->tx_flag = (TX_FIFO_THRESH << 11) & 0x003f0000;
		np->rx_config = (RX_FIFO_THRESH << 13) | (rx_buf_len_idx << 11) | (RX_DMA_BURST << 8);
		
		BYTEOUT(base + RTLr_ChipCmd, CmdReset);
		for (i = 1000; i > 0; i--)
		{
			if ((BYTEIN(base + RTLr_ChipCmd) & CmdReset) ==0) break;
		}
D(bug("%s: rtl8139nic_open: NIC Reset\n",dev->rtl8139u_name));		

        rtl8139nic_set_mac(dev);
D(bug("%s: rtl8139nic_open: copied MAC address\n",dev->rtl8139u_name));

		np->rx_current = 0;

		BYTEOUT(base + RTLr_Cfg9346, 0xc0);

		//Enable Tx/Rx so we can set the config(s)
		BYTEOUT(base + RTLr_ChipCmd, CmdRxEnb | CmdTxEnb);
		LONGOUT(base + RTLr_RxConfig, np->rx_config);
		LONGOUT(base + RTLr_TxConfig, TX_DMA_BURST << 8);

D(bug("%s: rtl8139nic_open: Enabled Tx/Rx\n",dev->rtl8139u_name));

		/* check_duplex */
		if (np->mii_phys[0] >= 0 || (dev->rtl8139u_rtl_chipcapabilities & RTLc_HAS_MII_XCVR))
		{
			unsigned short mii_reg5 = mdio_read(dev, np->mii_phys[0], 5);
			if (mii_reg5 != 0xffff)
			{
				if (((mii_reg5 & 0x0100) == 0x0100) || ((mii_reg5 & 0x00c0) == 0x0040))
				{
					np->full_duplex = 1;
				}
			}

			if ((mii_reg5 == 0 ) || !(mii_reg5 & 0x0180))
			{
			   dev->rtl8139u_rtl_LinkSpeed = 10000000;
			}
			else
			{
			   dev->rtl8139u_rtl_LinkSpeed = 100000000;				
			}
			
D(bug("%s: rtl8139nic_open: Setting %s%s-duplex based on auto-neg partner ability %4.4x\n",dev->rtl8139u_name,
			                                                                                                                                     mii_reg5 == 0 ? "" : (mii_reg5 & 0x0180) ? "100mbps " : "10mbps ",
			                                                                                                                                     np->full_duplex ? "full" : "half", mii_reg5));
		}

		if (dev->rtl8139u_rtl_chipcapabilities & RTLc_HAS_MII_XCVR)
		{
			BYTEOUT(base + RTLr_Config1, np->full_duplex ? 0x60 : 0x20);
		}
		BYTEOUT(base + RTLr_Cfg9346, 0x00);

		//Start the chips Tx/Rx processes
		LONGOUT(base + RTLr_RxMissed, 0);
		LONGOUT(base + RTLr_RxBuf, np->rx_buffer);

		rtl8139nic_set_rxmode(dev);
		BYTEOUT(base + RTLr_ChipCmd, CmdRxEnb | CmdTxEnb);

		//Enable all known interrupts by setting the interrupt mask ..
		WORDOUT(base + RTLr_IntrMask, PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK);
		
	   dev->rtl8139u_flags |= IFF_UP;
	   ReportEvents(LIBBASE, dev, S2EVENT_ONLINE);
D(bug("%s: rtl8139nic_open: Device set as ONLINE\n",dev->rtl8139u_name));
   }
   return 0;

out_drain:
    drain_ring(dev);
    return ret;
}

static int rtl8139nic_close(struct net_device *dev)
{
    struct fe_priv *np = get_pcnpriv(dev);
    UBYTE *base;

    dev->rtl8139u_flags &= ~IFF_UP;

    ObtainSemaphore(&np->lock);
    np->in_shutdown = 1;
    ReleaseSemaphore(&np->lock);

    dev->rtl8139u_toutNEED = FALSE;

    netif_stop_queue(dev);
    ObtainSemaphore(&np->lock);
    
    rtl8139nic_deinitialize(dev);    // Stop the chipset and set it in 16bit-mode

    base = get_hwbase(dev);

    ReleaseSemaphore(&np->lock);

    free_irq(dev);

    drain_ring(dev);

    HIDD_PCIDriver_FreePCIMem(dev->rtl8139u_PCIDriver, np->rx_buffer);
    HIDD_PCIDriver_FreePCIMem(dev->rtl8139u_PCIDriver, np->tx_buffer);

    ReportEvents(LIBBASE, dev, S2EVENT_OFFLINE);

    return 0;
}


void rtl8139nic_get_functions(struct net_device *Unit)
{
    Unit->initialize = rtl8139nic_initialize;
    Unit->deinitialize = rtl8139nic_deinitialize;
    Unit->start = rtl8139nic_open;
    Unit->stop = rtl8139nic_close;
    Unit->set_mac_address = rtl8139nic_set_mac;
    Unit->set_multicast = rtl8139nic_set_multicast;
}
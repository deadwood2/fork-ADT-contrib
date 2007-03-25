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
#include <exec/errors.h>

#include <aros/io.h>

#include <devices/sana2.h>
#include <devices/sana2specialstats.h>
#include <devices/newstyle.h>
#include <devices/timer.h>

#include <utility/utility.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/battclock.h>
#include <proto/oop.h>
#include <proto/timer.h>
#include <proto/utility.h>

#include <stdlib.h>
#include <stdio.h>

#include "rtl8139.h"
#include "unit.h"
#include LC_LIBDEFS_FILE

/*
 * Report incoming events to all hyphotetical event receivers
 */
VOID ReportEvents(struct RTL8139Base *RTL8139DeviceBase, struct RTL8139Unit *unit, ULONG events)
{
    struct IOSana2Req *request, *tail, *next_request;
    struct List *list;

    list = &unit->rtl8139u_request_ports[EVENT_QUEUE]->mp_MsgList;
    next_request = (APTR)list->lh_Head;
    tail = (APTR)&list->lh_Tail;

    /* Go through list of event listeners. If send messages to receivers if event found */
    Disable();
    while(next_request != tail)
    {
        request = next_request;
        next_request = (APTR)request->ios2_Req.io_Message.mn_Node.ln_Succ;

        if((request->ios2_WireError&events) != 0)
        {
            request->ios2_WireError = events;
            Remove((APTR)request);
            ReplyMsg((APTR)request);
        }
    }
    Enable();

    return;
}

struct TypeStats *FindTypeStats(struct RTL8139Base *RTL8139DeviceBase, struct RTL8139Unit *unit, 
                    struct MinList *list, ULONG packet_type)
{
    struct TypeStats *stats, *tail;
    BOOL found = FALSE;

    stats = (APTR)list->mlh_Head;
    tail = (APTR)&list->mlh_Tail;

    while(stats != tail && !found)
    {
        if(stats->packet_type == packet_type)
            found = TRUE;
        else
            stats = (APTR)stats->node.mln_Succ;
    }

    if(!found)
        stats = NULL;

    return stats;
}

void FlushUnit(LIBBASETYPEPTR LIBBASE, struct RTL8139Unit *unit, UBYTE last_queue, BYTE error)
{
    struct IORequest *request;
    UBYTE i;
    struct Opener *opener, *tail;

D(bug("%s unit.FlushUnit\n", unit->rtl8139u_name));

    /* Abort queued operations */

    for (i=0; i <= last_queue; i++)
    {
        while ((request = (APTR)GetMsg(unit->rtl8139u_request_ports[i])) != NULL)
        {
            request->io_Error = IOERR_ABORTED;
            ReplyMsg((struct Message *)request);
        }
    }

    opener = (APTR)unit->rtl8139u_Openers.mlh_Head;
    tail = (APTR)unit->rtl8139u_Openers.mlh_Tail;

    /* Flush every opener's read queue */

    while(opener != tail)
    {
        while ((request = (APTR)GetMsg(&opener->read_port)) != NULL)
        {
            request->io_Error = error;
            ReplyMsg((struct Message *)request);
        }
        opener = (struct Opener *)opener->node.mln_Succ;
    }
}

/*
 * Interrupt handler called whenever RTL8139 NIC interface generates interrupt.
 * It's duty is to iterate throgh RX queue searching for new packets.
 * 
 * Please note, that allthough multicast support could be done on interface 
 * basis, it is done in this function as result of quick integration of both
 * the forcedeth driver (IFF_ALLMULTI flag) and etherling3 driver (AddressMatch
 * filter function).
 */
AROS_UFH3(void, RTL8139_RX_IntF,
    AROS_UFHA(struct RTL8139Unit *,  unit, A1),
    AROS_UFHA(APTR,             dummy, A5),
    AROS_UFHA(struct ExecBase *,SysBase, A6))
{
    AROS_USERFUNC_INIT

    struct RTL8139Base *RTL8139DeviceBase = unit->rtl8139u_device;
    struct fe_priv *np = unit->rtl8139u_fe_priv;
    UWORD Flags;
    struct TypeStats *tracker;
    ULONG packet_type;
    struct Opener *opener, *opener_tail;
    struct IOSana2Req *request, *request_tail;
    BOOL accepted, is_orphan;

D(bug("%s: RTL8139_RX_IntF() !!!!\n", unit->rtl8139u_name));
    np->cur_rx = 0;

    /* Endless loop, with break from inside */
    for(;;)
    {
        int i;
        UWORD len=0;
        struct eth_frame *frame;

        if (np->cur_rx >= RX_RING_SIZE)
            break;	/* we scanned the whole ring - do not continue */

        /* Get the in-queue number */
        i = np->cur_rx % RX_RING_SIZE;
        Flags = AROS_LE2WORD(((struct rx_ring_desc *)np->ring_addr)[i].BufferStatus);
        len = AROS_LE2WORD(((struct rx_ring_desc *)np->ring_addr)[i].BufferMsgLength);

D(bug("%s: RTL8139_RX_IntF: looking at packet %d:%d, Flags 0x%x, len %d\n",
                unit->rtl8139u_name, np->cur_rx, i, Flags, len));

        /* Do we own the packet or the chipset? */
        if ((Flags & (1 << 15))!=0)
        {
D(bug("%s: RTL8139_RX_IntF: packet owned by chipset\n", unit->rtl8139u_name));
            goto next_pkt;	 /* still owned by hardware, */
        }

D(bug("%s: RTL8139_RX_IntF: packet is for us\n", unit->rtl8139u_name));

        /* the packet is for us - get it :) */

            if (Flags & (1 << 7)) { // Bus Parity Error
D(bug("%s: RTL8139_RX_IntF: packet has Bus Parity error!\n", unit->rtl8139u_name));
                ReportEvents(LIBBASE, unit, S2EVENT_ERROR | S2EVENT_HARDWARE | S2EVENT_RX);
                unit->rtl8139u_stats.BadData++;
                goto next_pkt;
            }

            if (Flags & (1 << 8)) { // End of Packet
D(bug("%s: RTL8139_RX_IntF: END of Packet\n", unit->rtl8139u_name));
            }
            if (Flags & (1 << 9)) { // Start of Packet
D(bug("%s: RTL8139_RX_IntF: START of Packet\n", unit->rtl8139u_name));
            }

            if (Flags & (1 << 10)) { // Buffer Error
D(bug("%s: RTL8139_RX_IntF: packet has CRC error!\n", unit->rtl8139u_name));
                ReportEvents(LIBBASE, unit, S2EVENT_ERROR | S2EVENT_HARDWARE | S2EVENT_RX);
                unit->rtl8139u_stats.BadData++;
                goto next_pkt;
            }
            if (Flags & (1 << 11)) { // CRC Error
D(bug("%s: RTL8139_RX_IntF: packet has CRC error!\n", unit->rtl8139u_name));
                ReportEvents(LIBBASE, unit, S2EVENT_ERROR | S2EVENT_HARDWARE | S2EVENT_RX);
                unit->rtl8139u_stats.BadData++;
                goto next_pkt;
            }
            if (Flags & (1 << 12)) { // OVERFLOW Error
D(bug("%s: RTL8139_RX_IntF: packet has OVERFLOW error!\n", unit->rtl8139u_name));
                ReportEvents(LIBBASE, unit, S2EVENT_ERROR | S2EVENT_HARDWARE | S2EVENT_RX);
                unit->rtl8139u_stats.BadData++;
                goto next_pkt;
            }
            if (Flags & (1 << 13)) { // Framing Error
                ReportEvents(LIBBASE, unit, S2EVENT_ERROR | S2EVENT_HARDWARE | S2EVENT_RX);
                unit->rtl8139u_stats.BadData++;
                goto next_pkt;
            }

D(bug("%s: RTL8139_RX_IntF: packet doesnt report errors\n", unit->rtl8139u_name));

        /* got a valid packet - forward it to the network core */
        frame = &np->rx_buffer[i];
        is_orphan = TRUE;

        /* Dump contents of frame if DEBUG enabled */
#ifdef DEBUG
                {
                int j;
                    for (j=0; j<64; j++) {
                        if ((j%16) == 0)
                            D(bug("\n%03x:", j));
                        D(bug(" %02x", ((unsigned char*)frame)[j]));
                    }
                    D(bug("\n"));
                }
#endif

        /* Check for address validity */
        if(AddressFilter(LIBBASE, unit, frame->eth_packet_dest))
        {
            /* Packet is addressed to this driver */
            packet_type = AROS_BE2WORD(frame->eth_packet_type);
D(bug("%s: RTL8139_RX_IntF: Packet IP accepted with type = %d\n", unit->rtl8139u_name, packet_type));

            opener = (APTR)unit->rtl8139u_Openers.mlh_Head;
            opener_tail = (APTR)&unit->rtl8139u_Openers.mlh_Tail;

            /* Offer packet to every opener */
            while(opener != opener_tail)
            {
               request = (APTR)opener->read_port.mp_MsgList.lh_Head;
               request_tail = (APTR)&opener->read_port.mp_MsgList.lh_Tail;
               accepted = FALSE;

               /* Offer packet to each request until it's accepted */
               while((request != request_tail) && !accepted)
               {
                  if((request->ios2_PacketType == packet_type)
                     || ((request->ios2_PacketType <= ETH_MTU)
                          && (packet_type <= ETH_MTU)))
                  {
D(bug("%s: RTL8139_RX_IntF: copy packet for opener ..\n", unit->rtl8139u_name));
                     CopyPacket(LIBBASE, unit, request, len, packet_type, frame);
                     accepted = TRUE;
                  }
                  request =
                     (struct IOSana2Req *)request->ios2_Req.io_Message.mn_Node.ln_Succ;
               }

               if(accepted)
                  is_orphan = FALSE;

               opener = (APTR)opener->node.mln_Succ;
            }

            /* If packet was unwanted, give it to S2_READORPHAN request */
            if(is_orphan)
            {
                unit->rtl8139u_stats.UnknownTypesReceived++;

                if(!IsMsgPortEmpty(unit->rtl8139u_request_ports[ADOPT_QUEUE]))
                {
                    CopyPacket(LIBBASE, unit,
                        (APTR)unit->rtl8139u_request_ports[ADOPT_QUEUE]->
                        mp_MsgList.lh_Head, len, packet_type, frame);
D(bug("%s: RTL8139_RX_IntF: packet copied to orphan queue\n", unit->rtl8139u_name));
                }
            }

            /* Update remaining statistics */

            tracker =
                FindTypeStats(LIBBASE, unit, &unit->rtl8139u_type_trackers, packet_type);

            if(tracker != NULL)
            {
                tracker->stats.PacketsReceived++;
                tracker->stats.BytesReceived += len;
            }
        }

        unit->rtl8139u_stats.PacketsReceived++;
        ((struct rx_ring_desc *)np->ring_addr)[i].BufferStatus = AROS_WORD2LE((1 << 8)|(1 << 9)|(1 << 15)); // Mark packet as available again
next_pkt:
        np->cur_rx++;
    }

    AROS_USERFUNC_EXIT
}

/*
 * Interrupt generated by Cause() to push new packets into the NIC interface
 */
AROS_UFH3(void, RTL8139_TX_IntF,
    AROS_UFHA(struct RTL8139Unit *,  unit, A1),
    AROS_UFHA(APTR,             dummy, A5),
    AROS_UFHA(struct ExecBase *,SysBase, A6))
{
    AROS_USERFUNC_INIT

    struct fe_priv *np = unit->rtl8139u_fe_priv;
    struct RTL8139Base *RTL8139DeviceBase = unit->rtl8139u_device;
    int nr, try_count=1;
    BOOL proceed = FALSE; /* Fails by default */

D(bug("%s: RTL8139_TX_IntF()\n", unit->rtl8139u_name));

    /* send packet only if there is free space on tx queue. Otherwise do nothing */
    if (!netif_queue_stopped(unit))
    {
        UWORD packet_size, data_size;
        struct IOSana2Req *request;
        struct Opener *opener;
        UBYTE *buffer;
        ULONG wire_error=0;
        BYTE error;
        struct MsgPort *port;
        struct TypeStats *tracker;

        proceed = TRUE; /* Success by default */
		UBYTE *base = (UBYTE*) unit->rtl8139u_BaseMem;
        port = unit->rtl8139u_request_ports[WRITE_QUEUE];

        /* Still no error and there are packets to be sent? */
        while(proceed && (!IsMsgPortEmpty(port)))
        {
            nr = np->tx_current % NUM_TX_DESC;
            error = 0;

		   request = (APTR)port->mp_MsgList.lh_Head;
		   data_size = packet_size = request->ios2_DataLength;

		   opener = (APTR)request->ios2_BufferManagement;

			np->tx_pbuf[nr] = np->tx_buf[nr];
		   if((request->ios2_Req.io_Flags & SANA2IOF_RAW) == 0)
		   {
			  packet_size += ETH_PACKET_DATA;
			  CopyMem(request->ios2_DstAddr, &((struct eth_frame *)np->tx_buf[nr])->eth_packet_dest, ETH_ADDRESSSIZE);
			  CopyMem(unit->rtl8139u_dev_addr, &((struct eth_frame *)np->tx_buf[nr])->eth_packet_source, ETH_ADDRESSSIZE);
			  ((struct eth_frame *)np->tx_buf[nr])->eth_packet_type = AROS_WORD2BE(request->ios2_PacketType);

			  buffer = &((struct eth_frame *)np->tx_buf[nr])->eth_packet_data;
		   }
		   else
			  buffer = np->tx_buf[nr];

		   if (!opener->tx_function(buffer, request->ios2_Data, data_size))
		   {
			  error = S2ERR_NO_RESOURCES;
			  wire_error = S2WERR_BUFF_ERROR;
			  ReportEvents(LIBBASE, unit,
				 S2EVENT_ERROR | S2EVENT_SOFTWARE | S2EVENT_BUFF
				 | S2EVENT_TX);
		   }

		   /* Now the packet is already in TX buffer, update flags for NIC */
		   if (error == 0)
		   {
			  Disable();
D(bug("%s: RTL8139_TX_IntF: packet %d  @ %x [type = %d] queued for transmission.", unit->rtl8139u_name, nr, np->tx_buf[nr], ((struct eth_frame *)np->tx_buf[nr])->eth_packet_type));

			  /* DEBUG? Dump frame if so */
#ifdef DEBUG
			  {
			  int j;
				   for (j=0; j<64; j++) {
					  if ((j%16) == 0)
						  D(bug("\n%03x:", j));
					  D(bug(" %02x", ((unsigned char*)np->tx_buf[nr])[j]));
				 }
				  D(bug("\n"));
			  }
#endif

			  Enable();

			  /* Set the ring details for the packet .. */
			  LONGOUT(base + RTLr_TxAddr0 + (nr *4), np->tx_buf[nr]);
			  LONGOUT(base + RTLr_TxStatus0 + (nr * 4), np->tx_flag | (packet_size >= ETH_ZLEN ? packet_size : ETH_ZLEN));
D(bug("%s: RTL8139_TX_IntF: Packet Queued.\n", unit->rtl8139u_name));
		   }

		   /* Reply packet */

		   request->ios2_Req.io_Error = error;
		   request->ios2_WireError = wire_error;
		   Disable();
		   Remove((APTR)request);
		   Enable();
		   ReplyMsg((APTR)request);

		   /* Update statistics */

		   if(error == 0)
		   {
			   tracker = FindTypeStats(LIBBASE, unit, &unit->rtl8139u_type_trackers,
				   request->ios2_PacketType);
			   if(tracker != NULL)
			   {
				   tracker->stats.PacketsSent++;
				   tracker->stats.BytesSent += packet_size;
			   }
		   }
		   //try_count=0;

		   np->tx_current++;
            //try_count++;
            
            /* 
             * If we've just run out of free space on the TX queue, stop
             * it and give up pushing further frames

            if ( (try_count + 1) >= TX_RING_SIZE)
            {
D(bug("%s: output queue full!. Stopping [count = %d, TX_RING_SIZE = %d\n", unit->rtl8139u_name, try_count, TX_RING_SIZE));
               netif_stop_queue(unit);
               proceed = FALSE;
            }             */
        }
    }

    /* Was there success? Enable incomming of new packets */    
    if(proceed)
        unit->rtl8139u_request_ports[WRITE_QUEUE]->mp_Flags = PA_SOFTINT;
    else
        unit->rtl8139u_request_ports[WRITE_QUEUE]->mp_Flags = PA_IGNORE;

    AROS_USERFUNC_EXIT
}

/*
 * Handle timeouts and other strange cases
 */
static void RTL8139_TimeoutHandlerF(HIDDT_IRQ_Handler *irq, HIDDT_IRQ_HwInfo *hw)
{
    struct RTL8139Unit *dev = (struct RTL8139Unit *) irq->h_Data;
    struct timeval time;
    struct Device *TimerBase = dev->rtl8139u_TimerSlowReq->tr_node.io_Device;

    GetSysTime(&time);
//D(bug("%s: RTL8139_TimeoutHandlerF()\n", dev->rtl8139u_name));

    /*
     * If timeout timer is expected, and time elapsed - regenerate the 
     * interrupt handler 
     */
    if (dev->rtl8139u_toutNEED && (CmpTime(&time, &dev->rtl8139u_toutPOLL ) < 0))
    {
        dev->rtl8139u_toutNEED = FALSE;
        //Cause(&dev->rtl8139u_tx_end_int);
    }
}

/*
 * The interrupt handler - schedules code execution to proper handlers
 */
static void RTL8139_IntHandlerF(HIDDT_IRQ_Handler *irq, HIDDT_IRQ_HwInfo *hw)
{
    struct RTL8139Unit *dev = (struct RTL8139Unit *) irq->h_Data;
    struct fe_priv *np = dev->rtl8139u_fe_priv;
    UBYTE *base = (UBYTE*) dev->rtl8139u_BaseMem;
    ULONG events;
    int i, link_changed, interrupt_work = 20;
    struct Device *TimerBase = dev->rtl8139u_TimerSlowReq->tr_node.io_Device;
    struct timeval time;

D(bug("%s: RTL8139_IntHandlerF()!!!!!!!\n", dev->rtl8139u_name));
    
    while (1)
    {
		int status = WORDIN(base + RTLr_IntrStatus);

		if (status & RxUnderrun) link_changed = WORDIN(base + RTLr_CSCR) & CSCR_LinkChangeBit;
        WORDOUT(base + RTLr_IntrStatus, status);

        if ((status & (PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK)) == 0) break;
		
		if ( status & (RxOK | RxUnderrun | RxOverflow | RxFIFOOver) ) // Chipset has Recieved packet(s)
		{
D(bug("%s: RTL8139_IntHandlerF: Packet Reception Attempt detected!\n", dev->rtl8139u_name));
			Cause(&dev->rtl8139u_rx_int);
		}

		if ( status & (TxOK | TxErr) ) // Chipset has attempted to Send packet(s)
		{
D(bug("%s: RTL8139_IntHandlerF: Packet Transmition Attempt detected!\n", dev->rtl8139u_name));
			unsigned int dirty_tx = np->tx_dirty;

			while ( np->tx_current - dirty_tx > 0)
			{
				int nr = dirty_tx % NUM_TX_DESC;
				int txstatus = LONGIN(base + RTLr_TxStatus0 + (nr * 4));
				
				if (!(txstatus & (TxStatOK | TxUnderrun | TxAborted))) break; // Transmition not complete yet ..

				// N.B: TxCarrierLost is always asserted at 100mbps.
                if (txstatus & (TxOutOfWindow | TxAborted))
				{
D(bug("%s: RTL8139_IntHandlerF: Transmition Error! Tx status %8.8x\n", dev->rtl8139u_name, txstatus));
					if (txstatus & TxAborted)
					{
						LONGOUT(base + RTLr_TxConfig, TX_DMA_BURST <<8);
					}
				}
				else
				{
					if (txstatus & TxUnderrun)
					{
						  if (np->tx_flag < 0x00300000) np->tx_flag += 0x00020000;
					}
				}
				np->tx_pbuf[nr] = NULL;
				dirty_tx++;
			}
			np->tx_dirty = dirty_tx;
		}

		if ( status & (PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | TxErr | RxErr) ) // Chipset has Reported an ERROR
		{
D(bug("%s: RTL8139_IntHandlerF: ERROR Detected\n", dev->rtl8139u_name));
			if (status = 0xffff)	break; // Missing Chip!
		}
		
		if (--interrupt_work < 0)
		{
D(bug("%s: RTL8139_IntHandlerF: MAX interrupt work reached.\n", dev->rtl8139u_name));
			WORDOUT(base + RTLr_IntrStatus, 0xffff);
			break;
		}
    }

   return;
}

VOID CopyPacket(struct RTL8139Base *RTL8139DeviceBase, struct RTL8139Unit *unit, 
    struct IOSana2Req *request, UWORD packet_size, UWORD packet_type,
    struct eth_frame *buffer)
{
    struct Opener *opener;
    BOOL filtered = FALSE;
    UBYTE *ptr;

D(bug("%s: CopyPacket(packet @ %x, len = %d)\n", unit->rtl8139u_name, buffer, packet_size));

    /* Set multicast and broadcast flags */

    request->ios2_Req.io_Flags &= ~(SANA2IOF_BCAST | SANA2IOF_MCAST);
    if((*((ULONG *)(buffer->eth_packet_dest)) == 0xffffffff) &&
       (*((UWORD *)(buffer->eth_packet_dest + 4)) == 0xffff))
    {
       request->ios2_Req.io_Flags |= SANA2IOF_BCAST;
D(bug("%s: CopyPacket: BROADCAST Flag set\n", unit->rtl8139u_name));
    }
    else if((buffer->eth_packet_dest[0] & 0x1) != 0)
    {
       request->ios2_Req.io_Flags |= SANA2IOF_MCAST;
D(bug("%s: CopyPacket: MULTICAST Flag set\n", unit->rtl8139u_name));
    }

    /* Set source and destination addresses and packet type */
    CopyMem(buffer->eth_packet_source, request->ios2_SrcAddr, ETH_ADDRESSSIZE);
    CopyMem(buffer->eth_packet_dest, request->ios2_DstAddr, ETH_ADDRESSSIZE);
    request->ios2_PacketType = packet_type;

    /* Adjust for cooked packet request */

    if((request->ios2_Req.io_Flags & SANA2IOF_RAW) == 0)
    {
        packet_size -= ETH_PACKET_DATA;
        ptr = (UBYTE*)&buffer->eth_packet_data[0];
    }
    else
    {
        ptr = (UBYTE*)buffer;
    }

    request->ios2_DataLength = packet_size;

D(bug("%s: CopyPacket: packet @ %x (%d bytes)\n", unit->rtl8139u_name, ptr, packet_size));

    /* Filter packet */

    opener = request->ios2_BufferManagement;
    if((request->ios2_Req.io_Command == CMD_READ) &&
        (opener->filter_hook != NULL))
        if(!CallHookPkt(opener->filter_hook, request, ptr))
        {
D(bug("%s: CopyPacket: packet filtered\n", unit->rtl8139u_name));
            filtered = TRUE;
        }

    if(!filtered)
    {
        /* Copy packet into opener's buffer and reply packet */
D(bug("%s: CopyPacket: opener recieve packet .. ", unit->rtl8139u_name));
        if(!opener->rx_function(request->ios2_Data, ptr, packet_size))
        {
D(bug("ERROR occured!!\n"));
            request->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
            request->ios2_WireError = S2WERR_BUFF_ERROR;
            ReportEvents(LIBBASE, unit, S2EVENT_ERROR | S2EVENT_SOFTWARE | S2EVENT_BUFF | S2EVENT_RX);
        }
        else
        {
D(bug("SUCCESS!!\n"));
        }
        Disable();
        Remove((APTR)request);
        Enable();
        ReplyMsg((APTR)request);
D(bug("%s: CopyPacket: opener notified.\n", unit->rtl8139u_name));
    }
}

BOOL AddressFilter(struct RTL8139Base *RTL8139DeviceBase, struct RTL8139Unit *unit, UBYTE *address)
{
    struct AddressRange *range, *tail;
    BOOL accept = TRUE;
    ULONG address_left;
    UWORD address_right;

    /* Check whether address is unicast/broadcast or multicast */

    address_left = AROS_BE2LONG(*((ULONG *)address));
    address_right = AROS_BE2WORD(*((UWORD *)(address + 4)));

    if((address_left & 0x01000000) != 0 &&
        !(address_left == 0xffffffff && address_right == 0xffff))
    {
        /* Check if this multicast address is wanted */

        range = (APTR)unit->rtl8139u_multicast_ranges.mlh_Head;
        tail = (APTR)&unit->rtl8139u_multicast_ranges.mlh_Tail;
        accept = FALSE;

        while((range != tail) && !accept)
        {
            if((address_left > range->lower_bound_left ||
                (address_left == range->lower_bound_left &&
                address_right >= range->lower_bound_right)) &&
                (address_left < range->upper_bound_left ||
                (address_left == range->upper_bound_left &&
                address_right <= range->upper_bound_right)))
                accept = TRUE;
            range = (APTR)range->node.mln_Succ;
        }

        if(!accept)
            unit->rtl8139u_special_stats[S2SS_ETHERNET_BADMULTICAST & 0xffff]++;
    }
    return accept;
}

/*
 * Unit process
 */
AROS_UFH3(void, RTL8139_Schedular,
    AROS_UFHA(STRPTR,              argPtr, A0),
    AROS_UFHA(ULONG,               argSize, D0),
    AROS_UFHA(struct ExecBase *,   SysBase, A6))
{
    AROS_USERFUNC_INIT

    struct RTL8139Unit *dev = FindTask(NULL)->tc_UserData;
    LIBBASETYPEPTR LIBBASE = dev->rtl8139u_device;
    APTR BattClockBase;
    struct MsgPort *reply_port, *input;

D(bug("%s RTL8139_Schedular()\n", dev->rtl8139u_name));
D(bug("%s RTL8139_Schedular: Setting device up\n", dev->rtl8139u_name));

    reply_port = CreateMsgPort();
    input = CreateMsgPort();

    dev->rtl8139u_input_port = input; 

    /* Randomize the generator with current time */
    BattClockBase =  OpenResource("battclock.resource");
    srandom(ReadBattClock());

    dev->rtl8139u_TimerSlowPort = CreateMsgPort();

    if (dev->rtl8139u_TimerSlowPort)
    {
        dev->rtl8139u_TimerSlowReq = (struct timerequest *)
            CreateIORequest((struct MsgPort *)dev->rtl8139u_TimerSlowPort, sizeof(struct timerequest));

        if (dev->rtl8139u_TimerSlowReq)
        {
            if (!OpenDevice("timer.device", UNIT_VBLANK,
                (struct IORequest *)dev->rtl8139u_TimerSlowReq, 0))
            {
                struct Message *msg = AllocVec(sizeof(struct Message), MEMF_PUBLIC|MEMF_CLEAR);
                ULONG sigset;

D(bug("%s RTL8139_Schedular: Got VBLANK unit of timer.device\n", dev->rtl8139u_name));

                dev->initialize(dev);

                msg->mn_ReplyPort = reply_port;
                msg->mn_Length = sizeof(struct Message);

D(bug("%s RTL8139_Schedular: Setup complete. Sending handshake\n", dev->rtl8139u_name));
                PutMsg(LIBBASE->rtl8139b_syncport, msg);
                WaitPort(reply_port);
                GetMsg(reply_port);

                FreeVec(msg);

D(bug("%s RTL8139_Schedular: entering forever loop ... \n", dev->rtl8139u_name));

                dev->rtl8139u_signal_0 = AllocSignal(-1);
                dev->rtl8139u_signal_1 = AllocSignal(-1);
                dev->rtl8139u_signal_2 = AllocSignal(-1);
                dev->rtl8139u_signal_3 = AllocSignal(-1);

                sigset = 1 << input->mp_SigBit  |
                         1 << dev->rtl8139u_signal_0  |
                         1 << dev->rtl8139u_signal_1  |
                         1 << dev->rtl8139u_signal_2  |
                         1 << dev->rtl8139u_signal_3;
                for(;;)
                {	
                    ULONG recvd = Wait(sigset);
                    if (recvd & dev->rtl8139u_signal_0)
                    {
                        /*
                         * Shutdown process. Driver should close everything 
                         * already and waits for our process to complete. Free
                         * memory allocared here and kindly return.
                         */
                        dev->deinitialize(dev);
                        CloseDevice((struct IORequest *)dev->rtl8139u_TimerSlowReq);
                        DeleteIORequest((struct IORequest *)dev->rtl8139u_TimerSlowReq);
                        DeleteMsgPort(dev->rtl8139u_TimerSlowPort);
                        DeleteMsgPort(input);
                        DeleteMsgPort(reply_port);

D(bug("%s RTL8139_Schedular: Process shutdown.\n", dev->rtl8139u_name));
                        return;
                    }
                    else if (recvd & (1 << input->mp_SigBit))
                    {
                        struct IOSana2Req *io;

                        /* Handle incoming transactions */
                        while ((io = (struct IOSana2Req *)GetMsg(input))!= NULL);
                        {
D(bug("%s RTL8139_Schedular: Handle incomming transaction.\n", dev->rtl8139u_name));
                            ObtainSemaphore(&dev->rtl8139u_unit_lock);
                            handle_request(LIBBASE, io);
                        }
                    }
                    else
                    {
D(bug("%s RTL8139_Schedular: Handle incomming signal.\n", dev->rtl8139u_name));
                        /* Handle incoming signals */
                    }
                }
            }
        }
    }

    AROS_USERFUNC_EXIT
}

/*
 * Create new RTL8139 ethernet device unit
 */
struct RTL8139Unit *CreateUnit(struct RTL8139Base *RTL8139DeviceBase, OOP_Object *pciDevice, ULONG CardCapabilities, char * CardName, char * CardChipset)
{
    BOOL success = TRUE;
    int i;

D(bug("[RTL8139] CreateUnit()\n"));

    struct RTL8139Unit *unit = AllocMem(sizeof(struct RTL8139Unit), MEMF_PUBLIC | MEMF_CLEAR);

    if (unit != NULL)
    {
        IPTR        DeviceID, base, len;
        OOP_Object  *driver;

D(bug("[RTL8139] CreateUnit: Unit allocated @ %x\n", unit));
		
        OOP_GetAttr(pciDevice, aHidd_PCIDevice_ProductID, &DeviceID);
        OOP_GetAttr(pciDevice, aHidd_PCIDevice_Driver, (APTR)&driver);

        unit->rtl8139u_rtl_cardname = CardName;
        unit->rtl8139u_rtl_chipname = CardChipset;
        unit->rtl8139u_rtl_chipcapabilities = CardCapabilities;
		
        unit->rtl8139u_device     = RTL8139DeviceBase;
        unit->rtl8139u_DeviceID   = DeviceID;
        unit->rtl8139u_mtu        = ETH_MTU;
        unit->rtl8139u_PCIDevice  = pciDevice;
        unit->rtl8139u_PCIDriver  = driver;
		unit->rtl8139u_UnitNum = RTL8139DeviceBase->rtl8139b_UnitCount++;
        
		int unitname_len = 11 + ((unit->rtl8139u_UnitNum/10)+1);

		unit->rtl8139u_name = AllocVec(unitname_len, MEMF_CLEAR|MEMF_PUBLIC);
D(bug("[RTL8139] CreateUnit: Allocated %d bytes for Unit %d's Name @ %x\n", unitname_len, unit->rtl8139u_UnitNum, unit->rtl8139u_name));
		sprintf(unit->rtl8139u_name, "[RTL8139.%d]", unit->rtl8139u_UnitNum);
		
        InitSemaphore(&unit->rtl8139u_unit_lock);
        NEWLIST(&unit->rtl8139u_Openers);
        NEWLIST(&unit->rtl8139u_multicast_ranges);
        NEWLIST(&unit->rtl8139u_type_trackers);

        OOP_GetAttr(pciDevice, aHidd_PCIDevice_INTLine, &unit->rtl8139u_IRQ);
        OOP_GetAttr(pciDevice, aHidd_PCIDevice_Base1,   &unit->rtl8139u_BaseIO);
        OOP_GetAttr(pciDevice, aHidd_PCIDevice_Base0,   &base);
        OOP_GetAttr(pciDevice, aHidd_PCIDevice_Size0,   &len);

D(bug("%s CreateUnit:   INT:%d, base1:%x, base0:%x, size0:%d\n", unit->rtl8139u_name,
																									   unit->rtl8139u_IRQ, unit->rtl8139u_BaseIO,
																									   base, len));

        unit->rtl8139u_BaseMem = (IPTR)HIDD_PCIDriver_MapPCI(driver, (APTR)base, len);
        unit->rtl8139u_SizeMem = len;

        if (unit->rtl8139u_BaseMem)
        {
            struct TagItem attrs[] = {
                { aHidd_PCIDevice_isIO,     TRUE },
                { aHidd_PCIDevice_isMEM,    TRUE },
                { aHidd_PCIDevice_isMaster, TRUE },
                { TAG_DONE,                 0    },
            };
            OOP_SetAttrs(pciDevice, (struct TagItem *)&attrs);

D(bug("%s CreateUnit:   PCI_BaseMem @ %x\n", unit->rtl8139u_name, unit->rtl8139u_BaseMem));
			
            unit->rtl8139u_fe_priv = AllocMem(sizeof(struct fe_priv), MEMF_PUBLIC|MEMF_CLEAR);

            rtl8139nic_get_functions(unit);

            if (unit->rtl8139u_fe_priv)
            {
                unit->rtl8139u_fe_priv->pci_dev = unit;
                InitSemaphore(&unit->rtl8139u_fe_priv->lock);

                unit->rtl8139u_irqhandler = AllocMem(sizeof(HIDDT_IRQ_Handler), MEMF_PUBLIC|MEMF_CLEAR);
                unit->rtl8139u_touthandler = AllocMem(sizeof(HIDDT_IRQ_Handler), MEMF_PUBLIC|MEMF_CLEAR);

                if (unit->rtl8139u_irqhandler && unit->rtl8139u_touthandler)
                {
                    struct Message *msg;

                    unit->rtl8139u_irqhandler->h_Node.ln_Pri = 100;
                    unit->rtl8139u_irqhandler->h_Node.ln_Name = LIBBASE->rtl8139b_Device.dd_Library.lib_Node.ln_Name;
                    unit->rtl8139u_irqhandler->h_Code = RTL8139_IntHandlerF;
                    unit->rtl8139u_irqhandler->h_Data = unit;

                    unit->rtl8139u_touthandler->h_Node.ln_Pri = 100;
                    unit->rtl8139u_touthandler->h_Node.ln_Name = LIBBASE->rtl8139b_Device.dd_Library.lib_Node.ln_Name;
                    unit->rtl8139u_touthandler->h_Code = RTL8139_TimeoutHandlerF;
                    unit->rtl8139u_touthandler->h_Data = unit;

                    unit->rtl8139u_rx_int.is_Node.ln_Name = unit->rtl8139u_name;
                    unit->rtl8139u_rx_int.is_Code = RTL8139_RX_IntF;
                    unit->rtl8139u_rx_int.is_Data = unit;

                    unit->rtl8139u_tx_int.is_Node.ln_Name = unit->rtl8139u_name;
                    unit->rtl8139u_tx_int.is_Code = RTL8139_TX_IntF;
                    unit->rtl8139u_tx_int.is_Data = unit;

                    for (i=0; i < REQUEST_QUEUE_COUNT; i++)
                    {
                        struct MsgPort *port = AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
                        unit->rtl8139u_request_ports[i] = port;

                        if (port == NULL) success = FALSE;

                        if (success)
                        {
                            NEWLIST(&port->mp_MsgList);
                            port->mp_Flags = PA_IGNORE;
                            port->mp_SigTask = &unit->rtl8139u_tx_int;
                        }
                    }

                    unit->rtl8139u_request_ports[WRITE_QUEUE]->mp_Flags = PA_SOFTINT;

                    if (success)
                    {
                        LIBBASE->rtl8139b_syncport = CreateMsgPort();

                        unit->rtl8139u_Process = CreateNewProcTags(
                                                NP_Entry, (IPTR)RTL8139_Schedular,
                                                NP_Name, RTL8139_TASK_NAME,
                                                NP_Priority, 0,
                                                NP_UserData, (IPTR)unit,
                                                NP_StackSize, 140960,
                                                TAG_DONE);

                        WaitPort(LIBBASE->rtl8139b_syncport);
                        msg = GetMsg(LIBBASE->rtl8139b_syncport);
                        ReplyMsg(msg);
                        DeleteMsgPort(LIBBASE->rtl8139b_syncport);

D(bug("[RTL8139] Unit up and running\n"));

                        return unit;
                    }
                    else
                    {
D(bug("%s: ERRORS occured during Device setup - ABORTING\n", unit->rtl8139u_name));
                    }
                }
            }
        }
        else
D(bug("[RTL8139] PANIC! Couldn't get MMIO area. Aborting\n"));
    }
    DeleteUnit(RTL8139DeviceBase, unit);	
    return NULL;
}

/*
 * DeleteUnit - removes selected unit. Frees all resources and structures.
 * 
 * The caller should be sure, that given unit is really ready to be freed.
 */

void DeleteUnit(struct RTL8139Base *RTL8139DeviceBase, struct RTL8139Unit *Unit)
{
    int i;
    if (Unit)
    {
        if (Unit->rtl8139u_Process)
        {
            Signal(&Unit->rtl8139u_Process->pr_Task, Unit->rtl8139u_signal_0);
        }

        for (i=0; i < REQUEST_QUEUE_COUNT; i++)
        {
            if (Unit->rtl8139u_request_ports[i] != NULL) 
                FreeMem(Unit->rtl8139u_request_ports[i],	sizeof(struct MsgPort));

            Unit->rtl8139u_request_ports[i] = NULL;
        }

        if (Unit->rtl8139u_irqhandler)
        {
            FreeMem(Unit->rtl8139u_irqhandler, sizeof(HIDDT_IRQ_Handler));
            LIBBASE->rtl8139b_irq = NULL;
        }

        if (Unit->rtl8139u_fe_priv)
        {
            FreeMem(Unit->rtl8139u_fe_priv, sizeof(struct fe_priv));
            Unit->rtl8139u_fe_priv = NULL;
        }

        if (Unit->rtl8139u_BaseMem)
        {
            HIDD_PCIDriver_UnmapPCI(Unit->rtl8139u_PCIDriver, 
                                    (APTR)Unit->rtl8139u_BaseMem,
                                    Unit->rtl8139u_SizeMem);
        }

        FreeMem(Unit, sizeof(struct RTL8139Unit));
    }
}

static struct AddressRange *FindMulticastRange(LIBBASETYPEPTR LIBBASE, struct RTL8139Unit *unit,
   ULONG lower_bound_left, UWORD lower_bound_right, ULONG upper_bound_left, UWORD upper_bound_right)
{
    struct AddressRange *range, *tail;
    BOOL found = FALSE;

    range = (APTR)unit->rtl8139u_multicast_ranges.mlh_Head;
    tail = (APTR)&unit->rtl8139u_multicast_ranges.mlh_Tail;

    while((range != tail) && !found)
    {
        if((lower_bound_left == range->lower_bound_left) &&
            (lower_bound_right == range->lower_bound_right) &&
            (upper_bound_left == range->upper_bound_left) &&
            (upper_bound_right == range->upper_bound_right))
            found = TRUE;
        else
            range = (APTR)range->node.mln_Succ;
    }

    if(!found)
        range = NULL;

    return range;
}

BOOL AddMulticastRange(LIBBASETYPEPTR LIBBASE, struct RTL8139Unit *unit, const UBYTE *lower_bound,
   const UBYTE *upper_bound)
{
    struct AddressRange *range;
    ULONG lower_bound_left, upper_bound_left;
    UWORD lower_bound_right, upper_bound_right;

    lower_bound_left = AROS_BE2LONG(*((ULONG *)lower_bound));
    lower_bound_right = AROS_BE2WORD(*((UWORD *)(lower_bound + 4)));
    upper_bound_left = AROS_BE2LONG(*((ULONG *)upper_bound));
    upper_bound_right = AROS_BE2WORD(*((UWORD *)(upper_bound + 4)));

    range = FindMulticastRange(LIBBASE, unit, lower_bound_left, lower_bound_right,
        upper_bound_left, upper_bound_right);

    if(range != NULL)
        range->add_count++;
    else
    {
        range = AllocMem(sizeof(struct AddressRange), MEMF_PUBLIC);
        if(range != NULL)
        {
            range->lower_bound_left = lower_bound_left;
            range->lower_bound_right = lower_bound_right;
            range->upper_bound_left = upper_bound_left;
            range->upper_bound_right = upper_bound_right;
            range->add_count = 1;

            Disable();
            AddTail((APTR)&unit->rtl8139u_multicast_ranges, (APTR)range);
            Enable();

            if (unit->rtl8139u_range_count++ == 0)
            {
                unit->rtl8139u_flags |= IFF_ALLMULTI;
                unit->set_multicast(unit);
            }
        }
    }

    return range != NULL;
}

BOOL RemMulticastRange(LIBBASETYPEPTR LIBBASE, struct RTL8139Unit *unit, const UBYTE *lower_bound, const UBYTE *upper_bound)
{
    struct AddressRange *range;
    ULONG lower_bound_left, upper_bound_left;
    UWORD lower_bound_right, upper_bound_right;

    lower_bound_left = AROS_BE2LONG(*((ULONG *)lower_bound));
    lower_bound_right = AROS_BE2WORD(*((UWORD *)(lower_bound + 4)));
    upper_bound_left = AROS_BE2LONG(*((ULONG *)upper_bound));
    upper_bound_right = AROS_BE2WORD(*((UWORD *)(upper_bound + 4)));

    range = FindMulticastRange(LIBBASE, unit, lower_bound_left, lower_bound_right,
        upper_bound_left, upper_bound_right);

    if(range != NULL)
    {
        if(--range->add_count == 0)
        {
            Disable();
            Remove((APTR)range);
            Enable();
            FreeMem(range, sizeof(struct AddressRange));

            if (--unit->rtl8139u_range_count == 0)
            {
                unit->rtl8139u_flags &= ~IFF_ALLMULTI;
                unit->set_multicast(unit);
            }
        }
    }
    return range != NULL;
}

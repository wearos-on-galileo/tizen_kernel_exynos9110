/*
 * Copyright (C) 1999-2018, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: wlfc_proto.h 735359 2017-12-08 10:56:04Z $
 *
 */

/** WL flow control for PROP_TXSTATUS. Related to host AMPDU reordering. */


#ifndef __wlfc_proto_definitions_h__
#define __wlfc_proto_definitions_h__

	/* Use TLV to convey WLFC information.
	 ---------------------------------------------------------------------------
	| Type |  Len | value                    | Description
	 ---------------------------------------------------------------------------
	|  1   |   1  | (handle)                 | MAC OPEN
	 ---------------------------------------------------------------------------
	|  2   |   1  | (handle)                 | MAC CLOSE
	 ---------------------------------------------------------------------------
	|  3   |   2  | (count, handle, prec_bmp)| Set the credit depth for a MAC dstn
	 ---------------------------------------------------------------------------
	|  4   |   4+ | see pkttag comments      | TXSTATUS
	|      |   12 | TX status & timestamps   | Present only when pkt timestamp is enabled
	 ---------------------------------------------------------------------------
	|  5   |   4  | see pkttag comments      | PKKTTAG [host->firmware]
	 ---------------------------------------------------------------------------
	|  6   |   8  | (handle, ifid, MAC)      | MAC ADD
	 ---------------------------------------------------------------------------
	|  7   |   8  | (handle, ifid, MAC)      | MAC DEL
	 ---------------------------------------------------------------------------
	|  8   |   1  | (rssi)                   | RSSI - RSSI value for the packet.
	 ---------------------------------------------------------------------------
	|  9   |   1  | (interface ID)           | Interface OPEN
	 ---------------------------------------------------------------------------
	|  10  |   1  | (interface ID)           | Interface CLOSE
	 ---------------------------------------------------------------------------
	|  11  |   8  | fifo credit returns map  | FIFO credits back to the host
	|      |      |                          |
	|      |      |                          | --------------------------------------
	|      |      |                          | | ac0 | ac1 | ac2 | ac3 | bcmc | atim |
	|      |      |                          | --------------------------------------
	|      |      |                          |
	 ---------------------------------------------------------------------------
	|  12  |   2  | MAC handle,              | Host provides a bitmap of pending
	|      |      | AC[0-3] traffic bitmap   | unicast traffic for MAC-handle dstn.
	|      |      |                          | [host->firmware]
	 ---------------------------------------------------------------------------
	|  13  |   3  | (count, handle, prec_bmp)| One time request for packet to a specific
	|      |      |                          | MAC destination.
	 ---------------------------------------------------------------------------
	|  15  |  12  | (pkttag, timestamps)     | Send TX timestamp at reception from host
	 ---------------------------------------------------------------------------
	|  16  |  12  | (pkttag, timestamps)     | Send WLAN RX timestamp along with RX frame
	 ---------------------------------------------------------------------------
	| 255  |  N/A |  N/A                     | FILLER - This is a special type
	|      |      |                          | that has no length or value.
	|      |      |                          | Typically used for padding.
	 ---------------------------------------------------------------------------
	*/

#define WLFC_CTL_TYPE_MAC_OPEN			1
#define WLFC_CTL_TYPE_MAC_CLOSE			2
#define WLFC_CTL_TYPE_MAC_REQUEST_CREDIT	3
#define WLFC_CTL_TYPE_TXSTATUS			4
#define WLFC_CTL_TYPE_PKTTAG			5	/** host<->dongle */

#define WLFC_CTL_TYPE_MACDESC_ADD		6
#define WLFC_CTL_TYPE_MACDESC_DEL		7
#define WLFC_CTL_TYPE_RSSI			8

#define WLFC_CTL_TYPE_INTERFACE_OPEN		9
#define WLFC_CTL_TYPE_INTERFACE_CLOSE		10

#define WLFC_CTL_TYPE_FIFO_CREDITBACK		11

#define WLFC_CTL_TYPE_PENDING_TRAFFIC_BMP	12	/** host->dongle */
#define WLFC_CTL_TYPE_MAC_REQUEST_PACKET	13
#define WLFC_CTL_TYPE_HOST_REORDER_RXPKTS	14

#define WLFC_CTL_TYPE_TX_ENTRY_STAMP		15
#define WLFC_CTL_TYPE_RX_STAMP			16
#define WLFC_CTL_TYPE_TX_STATUS_STAMP		17	/** obsolete */

#define WLFC_CTL_TYPE_TRANS_ID			18
#define WLFC_CTL_TYPE_COMP_TXSTATUS		19

#define WLFC_CTL_TYPE_TID_OPEN			20
#define WLFC_CTL_TYPE_TID_CLOSE			21


#define WLFC_CTL_TYPE_FILLER			255

#define WLFC_CTL_VALUE_LEN_MACDESC		8	/** handle, interface, MAC */

#define WLFC_CTL_VALUE_LEN_MAC			1	/** MAC-handle */
#define WLFC_CTL_VALUE_LEN_RSSI			1

#define WLFC_CTL_VALUE_LEN_INTERFACE		1
#define WLFC_CTL_VALUE_LEN_PENDING_TRAFFIC_BMP	2

#define WLFC_CTL_VALUE_LEN_TXSTATUS		4
#define WLFC_CTL_VALUE_LEN_PKTTAG		4
#define WLFC_CTL_VALUE_LEN_TIMESTAMP		12	/** 4-byte rate info + 2 TSF */

#define WLFC_CTL_VALUE_LEN_SEQ			2

/* The high bits of ratespec report in timestamp are used for various status */
#define WLFC_TSFLAGS_RX_RETRY		(1 << 31)
#define WLFC_TSFLAGS_PM_ENABLED		(1 << 30)
#define WLFC_TSFLAGS_MASK		(WLFC_TSFLAGS_RX_RETRY | WLFC_TSFLAGS_PM_ENABLED)

/* enough space to host all 4 ACs, bc/mc and atim fifo credit */
#define WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK	6

#define WLFC_CTL_VALUE_LEN_REQUEST_CREDIT	3	/* credit, MAC-handle, prec_bitmap */
#define WLFC_CTL_VALUE_LEN_REQUEST_PACKET	3	/* credit, MAC-handle, prec_bitmap */


#define WLFC_PKTFLAG_PKTFROMHOST	0x01
#define WLFC_PKTFLAG_PKT_REQUESTED	0x02

#define WL_TXSTATUS_STATUS_MASK			0xff /* allow 8 bits */
#define WL_TXSTATUS_STATUS_SHIFT		24

#define WL_TXSTATUS_SET_STATUS(x, status)	((x)  = \
	((x) & ~(WL_TXSTATUS_STATUS_MASK << WL_TXSTATUS_STATUS_SHIFT)) | \
	(((status) & WL_TXSTATUS_STATUS_MASK) << WL_TXSTATUS_STATUS_SHIFT))
#define WL_TXSTATUS_GET_STATUS(x)	(((x) >> WL_TXSTATUS_STATUS_SHIFT) & \
	WL_TXSTATUS_STATUS_MASK)

/**
 * Bit 31 of the 32-bit packet tag is defined as 'generation ID'. It is set by the host to the
 * "current" generation, and by the firmware to the "expected" generation, toggling on suppress. The
 * firmware accepts a packet when the generation matches; on reset (startup) both "current" and
 * "expected" are set to 0.
 */
#define WL_TXSTATUS_GENERATION_MASK		1 /* allow 1 bit */
#define WL_TXSTATUS_GENERATION_SHIFT		31

#define WL_TXSTATUS_SET_GENERATION(x, gen)	((x) = \
	((x) & ~(WL_TXSTATUS_GENERATION_MASK << WL_TXSTATUS_GENERATION_SHIFT)) | \
	(((gen) & WL_TXSTATUS_GENERATION_MASK) << WL_TXSTATUS_GENERATION_SHIFT))

#define WL_TXSTATUS_GET_GENERATION(x)	(((x) >> WL_TXSTATUS_GENERATION_SHIFT) & \
	WL_TXSTATUS_GENERATION_MASK)

#define WL_TXSTATUS_FLAGS_MASK			0xf /* allow 4 bits only */
#define WL_TXSTATUS_FLAGS_SHIFT			27

#define WL_TXSTATUS_SET_FLAGS(x, flags)	((x)  = \
	((x) & ~(WL_TXSTATUS_FLAGS_MASK << WL_TXSTATUS_FLAGS_SHIFT)) | \
	(((flags) & WL_TXSTATUS_FLAGS_MASK) << WL_TXSTATUS_FLAGS_SHIFT))
#define WL_TXSTATUS_GET_FLAGS(x)		(((x) >> WL_TXSTATUS_FLAGS_SHIFT) & \
	WL_TXSTATUS_FLAGS_MASK)

#define WL_TXSTATUS_FIFO_MASK			0x7 /* allow 3 bits for FIFO ID */
#define WL_TXSTATUS_FIFO_SHIFT			24

#define WL_TXSTATUS_SET_FIFO(x, flags)	((x)  = \
	((x) & ~(WL_TXSTATUS_FIFO_MASK << WL_TXSTATUS_FIFO_SHIFT)) | \
	(((flags) & WL_TXSTATUS_FIFO_MASK) << WL_TXSTATUS_FIFO_SHIFT))
#define WL_TXSTATUS_GET_FIFO(x)		(((x) >> WL_TXSTATUS_FIFO_SHIFT) & WL_TXSTATUS_FIFO_MASK)

#define WL_TXSTATUS_PKTID_MASK			0xffffff /* allow 24 bits */
#define WL_TXSTATUS_SET_PKTID(x, num)	((x) = \
	((x) & ~WL_TXSTATUS_PKTID_MASK) | (num))
#define WL_TXSTATUS_GET_PKTID(x)		((x) & WL_TXSTATUS_PKTID_MASK)

#define WL_TXSTATUS_HSLOT_MASK			0xffff /* allow 16 bits */
#define WL_TXSTATUS_HSLOT_SHIFT			8

#define WL_TXSTATUS_SET_HSLOT(x, hslot)	((x)  = \
	((x) & ~(WL_TXSTATUS_HSLOT_MASK << WL_TXSTATUS_HSLOT_SHIFT)) | \
	(((hslot) & WL_TXSTATUS_HSLOT_MASK) << WL_TXSTATUS_HSLOT_SHIFT))
#define WL_TXSTATUS_GET_HSLOT(x)	(((x) >> WL_TXSTATUS_HSLOT_SHIFT)& \
	WL_TXSTATUS_HSLOT_MASK)

#define WL_TXSTATUS_FREERUNCTR_MASK		0xff /* allow 8 bits */

#define WL_TXSTATUS_SET_FREERUNCTR(x, ctr)	((x)  = \
	((x) & ~(WL_TXSTATUS_FREERUNCTR_MASK)) | \
	((ctr) & WL_TXSTATUS_FREERUNCTR_MASK))
#define WL_TXSTATUS_GET_FREERUNCTR(x)		((x)& WL_TXSTATUS_FREERUNCTR_MASK)

/* AMSDU part of d11 seq number */
#define WL_SEQ_AMSDU_MASK             0x1 /* allow 1 bit */
#define WL_SEQ_AMSDU_SHIFT            14
#define WL_SEQ_SET_AMSDU(x, val)      ((x) = \
	((x) & ~(WL_SEQ_AMSDU_MASK << WL_SEQ_AMSDU_SHIFT)) | \
	(((val) & WL_SEQ_AMSDU_MASK) << WL_SEQ_AMSDU_SHIFT)) /**< sets a single AMSDU bit */
/** returns TRUE if ring item is AMSDU (seq = d11 seq nr) */
#define WL_SEQ_IS_AMSDU(x)   (((x) >> WL_SEQ_AMSDU_SHIFT) & \
	WL_SEQ_AMSDU_MASK)

#define WL_SEQ_FROMFW_MASK		0x1 /* allow 1 bit */
#define WL_SEQ_FROMFW_SHIFT		13
#define WL_SEQ_SET_FROMFW(x, val)	((x) = \
	((x) & ~(WL_SEQ_FROMFW_MASK << WL_SEQ_FROMFW_SHIFT)) | \
	(((val) & WL_SEQ_FROMFW_MASK) << WL_SEQ_FROMFW_SHIFT))
/** Set when firmware assigns D11 sequence number to packet */
#define SET_WL_HAS_ASSIGNED_SEQ(x)	WL_SEQ_SET_FROMFW((x), 1)

/** returns TRUE if packet has been assigned a d11 seq number by the WL firmware layer */
#define GET_WL_HAS_ASSIGNED_SEQ(x)	(((x) >> WL_SEQ_FROMFW_SHIFT) & WL_SEQ_FROMFW_MASK)

/**
 * Proptxstatus related.
 *
 * When a packet is suppressed by WL or the D11 core, the packet has to be retried. Assigning
 * a new d11 sequence number for the packet when retrying would cause the peer to be unable to
 * reorder the packets within an AMPDU. So, suppressed packet from bus layer (DHD for SDIO and
 * pciedev for PCIE) is re-using d11 seq number, so FW should not assign a new one.
 */
#define WL_SEQ_FROMDRV_MASK		0x1 /* allow 1 bit */
#define WL_SEQ_FROMDRV_SHIFT		12

/**
 * Proptxstatus, host or fw PCIe layer requests WL layer to reuse d11 seq no. Bit is reset by WL
 * subsystem when it reuses the seq number.
 */
#define WL_SEQ_SET_REUSE(x, val)	((x) = \
	((x) & ~(WL_SEQ_FROMDRV_MASK << WL_SEQ_FROMDRV_SHIFT)) | \
	(((val) & WL_SEQ_FROMDRV_MASK) << WL_SEQ_FROMDRV_SHIFT))
#define SET_WL_TO_REUSE_SEQ(x)   WL_SEQ_SET_REUSE((x), 1)
#define RESET_WL_TO_REUSE_SEQ(x) WL_SEQ_SET_REUSE((x), 0)

/** Proptxstatus, related to reuse of d11 seq numbers when retransmitting */
#define IS_WL_TO_REUSE_SEQ(x)	(((x) >> WL_SEQ_FROMDRV_SHIFT) & \
	WL_SEQ_FROMDRV_MASK)

#define WL_SEQ_NUM_MASK			0xfff /* allow 12 bit */
#define WL_SEQ_NUM_SHIFT		0
/** Proptxstatus, sets d11seq no in pkt tag, related to reuse of d11seq no when retransmitting */
#define WL_SEQ_SET_NUM(x, val)	((x) = \
	((x) & ~(WL_SEQ_NUM_MASK << WL_SEQ_NUM_SHIFT)) | \
	(((val) & WL_SEQ_NUM_MASK) << WL_SEQ_NUM_SHIFT))
/** Proptxstatus, gets d11seq no from pkt tag, related to reuse of d11seq no when retransmitting */
#define WL_SEQ_GET_NUM(x)	(((x) >> WL_SEQ_NUM_SHIFT) & \
	WL_SEQ_NUM_MASK)

#define WL_SEQ_AMSDU_SUPPR_MASK	((WL_SEQ_FROMDRV_MASK << WL_SEQ_FROMDRV_SHIFT) | \
				(WL_SEQ_AMSDU_MASK << WL_SEQ_AMSDU_SHIFT) | \
				(WL_SEQ_NUM_MASK << WL_SEQ_NUM_SHIFT))

/* 32 STA should be enough??, 6 bits; Must be power of 2 */
#define WLFC_MAC_DESC_TABLE_SIZE	32
#define WLFC_MAX_IFNUM				16
#define WLFC_MAC_DESC_ID_INVALID	0xff

/* b[7:5] -reuse guard, b[4:0] -value */
#define WLFC_MAC_DESC_GET_LOOKUP_INDEX(x) ((x) & 0x1f)

#define WLFC_PKTFLAG_SET_PKTREQUESTED(x)	(x) |= \
	(WLFC_PKTFLAG_PKT_REQUESTED << WL_TXSTATUS_FLAGS_SHIFT)

#define WLFC_PKTFLAG_CLR_PKTREQUESTED(x)	(x) &= \
	~(WLFC_PKTFLAG_PKT_REQUESTED << WL_TXSTATUS_FLAGS_SHIFT)


#define WLFC_MAX_PENDING_DATALEN	120

/* host is free to discard the packet */
#define WLFC_CTL_PKTFLAG_DISCARD	0
/* D11 suppressed a packet */
#define WLFC_CTL_PKTFLAG_D11SUPPRESS	1
/* WL firmware suppressed a packet because MAC is
	already in PSMode (short time window)
*/
#define WLFC_CTL_PKTFLAG_WLSUPPRESS	2
/* Firmware tossed this packet */
#define WLFC_CTL_PKTFLAG_TOSSED_BYWLC	3
/* Firmware tossed after retries */
#define WLFC_CTL_PKTFLAG_DISCARD_NOACK	4
/* Firmware wrongly reported suppressed previously,now fixing to acked */
#define WLFC_CTL_PKTFLAG_SUPPRESS_ACKED	5
#define WLFC_CTL_PKTFLAG_MASK		(0x0f)	/* For 4-bit mask with one extra bit */

#ifdef PROP_TXSTATUS_DEBUG
#define WLFC_DBGMESG(x) printf x
/* wlfc-breadcrumb */
#define WLFC_BREADCRUMB(x) do {if ((x) == NULL) \
	{printf("WLFC: %s():%d:caller:%p\n", \
	__FUNCTION__, __LINE__, CALL_SITE);}} while (0)
#define WLFC_WHEREIS(s) printf("WLFC: at %s():%d, %s\n", __FUNCTION__, __LINE__, (s))
#else
#define WLFC_DBGMESG(x)
#define WLFC_BREADCRUMB(x)
#define WLFC_WHEREIS(s)
#endif /* PROP_TXSTATUS_DEBUG */

/* AMPDU host reorder packet flags */
#define WLHOST_REORDERDATA_MAXFLOWS		256
#define WLHOST_REORDERDATA_LEN		 10
#define WLHOST_REORDERDATA_TOTLEN	(WLHOST_REORDERDATA_LEN + 1 + 1) /* +tag +len */

#define WLHOST_REORDERDATA_FLOWID_OFFSET		0
#define WLHOST_REORDERDATA_MAXIDX_OFFSET		2
#define WLHOST_REORDERDATA_FLAGS_OFFSET			4
#define WLHOST_REORDERDATA_CURIDX_OFFSET		6
#define WLHOST_REORDERDATA_EXPIDX_OFFSET		8

#define WLHOST_REORDERDATA_DEL_FLOW		0x01
#define WLHOST_REORDERDATA_FLUSH_ALL		0x02
#define WLHOST_REORDERDATA_CURIDX_VALID		0x04
#define WLHOST_REORDERDATA_EXPIDX_VALID		0x08
#define WLHOST_REORDERDATA_NEW_HOLE		0x10

/* transaction id data len byte 0: rsvd, byte 1: seqnumber, byte 2-5 will be used for timestampe */
#define WLFC_CTL_TRANS_ID_LEN			6
#define WLFC_TYPE_TRANS_ID_LEN			6

#define WLFC_MODE_HANGER	1 /* use hanger */
#define WLFC_MODE_AFQ		2 /* use afq (At Firmware Queue) */
#define WLFC_IS_OLD_DEF(x) ((x & 1) || (x & 2))

#define WLFC_MODE_AFQ_SHIFT		2	/* afq bit */
#define WLFC_SET_AFQ(x, val)	((x) = \
	((x) & ~(1 << WLFC_MODE_AFQ_SHIFT)) | \
	(((val) & 1) << WLFC_MODE_AFQ_SHIFT))
/** returns TRUE if firmware supports 'at firmware queue' feature */
#define WLFC_GET_AFQ(x)	(((x) >> WLFC_MODE_AFQ_SHIFT) & 1)

#define WLFC_MODE_REUSESEQ_SHIFT	3	/* seq reuse bit */
#define WLFC_SET_REUSESEQ(x, val)	((x) = \
	((x) & ~(1 << WLFC_MODE_REUSESEQ_SHIFT)) | \
	(((val) & 1) << WLFC_MODE_REUSESEQ_SHIFT))
/** returns TRUE if 'd11 sequence reuse' has been agreed upon between host and dongle */
#define WLFC_GET_REUSESEQ(x)	(((x) >> WLFC_MODE_REUSESEQ_SHIFT) & 1)

#define WLFC_MODE_REORDERSUPP_SHIFT	4	/* host reorder suppress pkt bit */
#define WLFC_SET_REORDERSUPP(x, val)	((x) = \
	((x) & ~(1 << WLFC_MODE_REORDERSUPP_SHIFT)) | \
	(((val) & 1) << WLFC_MODE_REORDERSUPP_SHIFT))
/** returns TRUE if 'reorder suppress' has been agreed upon between host and dongle */
#define WLFC_GET_REORDERSUPP(x)	(((x) >> WLFC_MODE_REORDERSUPP_SHIFT) & 1)

#define FLOW_RING_CREATE	1
#define FLOW_RING_DELETE	2
#define FLOW_RING_FLUSH		3
#define FLOW_RING_OPEN		4
#define FLOW_RING_CLOSED	5
#define FLOW_RING_FLUSHED	6
#define FLOW_RING_TIM_SET	7
#define FLOW_RING_TIM_RESET	8
#define FLOW_RING_FLUSH_TXFIFO	9

/* bit 7, indicating if is TID(1) or AC(0) mapped info in tid field) */
#define PCIEDEV_IS_AC_TID_MAP_MASK	0x80

#endif /* __wlfc_proto_definitions_h__ */
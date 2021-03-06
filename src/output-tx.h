/* Copyright (C) 2007-2013 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 *
 * AppLayer TX Logger Output registration functions
 */

#ifndef __OUTPUT_TX_H__
#define __OUTPUT_TX_H__

#include "decode.h"

/** packet logger function pointer type */
typedef int (*TxLogger)(ThreadVars *, void *thread_data, const Packet *, Flow *f, void *state, void *tx, uint64_t tx_id);

/** packet logger condition function pointer type,
 *  must return true for packets that should be logged
 */
//typedef int (*TxLogCondition)(ThreadVars *, const Packet *);

int OutputRegisterTxLogger(const char *name, AppProto alproto, TxLogger LogFunc, OutputCtx *);

void TmModuleTxLoggerRegister (void);

#endif /* __OUTPUT_PACKET_H__ */

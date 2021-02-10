/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef UTILS_CHECKLIST_H
#define UTILS_CHECKLIST_H

#include "../simtypes.h"


class memory_rw_t;


#define CHK_RANDS 32
#define CHK_DEBUG_SUMS 10

struct checklist_t
{
#if HEAVY_MODE
private:
	uint32 hash;
#else
	uint32 random_seed;
	uint16 halt_entry;
	uint16 line_entry;
	uint16 convoy_entry;

	uint32 ss;
	uint32 st;
	uint8 nfc;

	uint32 rand[CHK_RANDS];
	uint32 debug_sum[CHK_DEBUG_SUMS];

#endif


public:
	checklist_t();

#if HEAVY_MODE
	explicit checklist_t(const uint32 &hash);
#else
	checklist_t(uint32 _ss, uint32 _st, uint8 _nfc, uint32 _random_seed, uint16 _halt_entry, uint16 _line_entry, uint16 _convoy_entry, uint32 *_rands, uint32 *_debug_sums);
#endif

	bool operator == (const checklist_t &other) const;
	bool operator != (const checklist_t &other) const { return !( (*this)==other ); }

	void rdwr(memory_rw_t *buffer);
	int print(char *buffer, const char *entity) const;
};


#endif

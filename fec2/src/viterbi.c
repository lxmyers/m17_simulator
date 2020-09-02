/* Based on r=1/2 Viterbi decoder in portable C
 * Copyright Feb 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>

#include "fec2.h"

/* !! Matching Rate and Contraint required in convolutional.c !! */
#define K 5
#define RATE 2			// Only tested with 1/2 Rate. BFLY needs modification for use with different rates.
#define NUMSTATES 16	// NUMSTATES = 2^(K-1) = (2 to the power (CONTRAINT minus 1))

typedef union { unsigned int w[NUMSTATES]; } metric_t;
typedef union { unsigned long w[2];} decision_t;

static union branchtab { unsigned char c[NUMSTATES/2]; } Branchtab[RATE] __attribute__ ((aligned(16)));

static int Init = 0;

/* State info for instance of Viterbi decoder */
struct vd {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};


unsigned char Partab[256];
int P_init;

/* Create 256-entry odd-parity lookup table
 * Needed only on non-ia32 machines
 */
void partab_init(void){
  int i,cnt,ti;

  /* Initialize parity lookup table */
  for(i=0;i<256;i++){
    cnt = 0;
    ti = i;
    while(ti){
      if(ti & 1)
	cnt++;
      ti >>= 1;
    }
    Partab[i] = cnt & 1;
  }
  P_init=1;
}


/* Initialize Viterbi decoder for start of new frame */
int init_viterbi(void *p, int starting_state)
{
	struct vd *vp = p;

	if(p == NULL)
		return -1;

	for(int i = 0; i < NUMSTATES; i++)				// 256 instead of 64 used on v29
		vp->metrics1.w[i] = 63;

	vp->old_metrics = &vp->metrics1;
	vp->new_metrics = &vp->metrics2;
	vp->dp = vp->decisions;
	vp->old_metrics->w[starting_state & (NUMSTATES-1)] = 0; /* Bias known start state */    // 255 instead of 63 used on v29
	
	return 0;
}

void set_viterbi_polynomial(int polys[2])
{
	for(int state = 0; state < (NUMSTATES/2); state++)  // 128 instead of 32 used on v29
	{
		for (int i=0; i<RATE; i++)
		{
			Branchtab[i].c[state] = (polys[i] < 0) ^ parity((2*state) & abs(polys[i])) ? 255 : 0;
		}
	}

	Init++;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi(int len)
{
	struct vd *vp;

	if(!Init){
		int polys[2] = { V25POLYA, V25POLYB };
		set_viterbi_polynomial(polys);
	}
	if((vp = malloc(sizeof(struct vd))) == NULL)
		return NULL;

	if((vp->decisions = malloc((len+(K-1))*sizeof(decision_t))) == NULL){ //len+8 Used on v29    = K-1. (1/2 K=5) = 4
		free(vp);
		return NULL;
	}
	init_viterbi(vp,0);

	return vp;
}



/* Viterbi chainback */
int chainback_viterbi(void *p, unsigned char *data, /* Decoded output data */ unsigned int nbits, /* Number of data bits */ unsigned int endstate) /* Terminal encoder state */
{
	struct vd *vp = p;
	decision_t *d;

	if(p == NULL)
		return -1;

	d = vp->decisions;

	/* ADDSHIFT and SUBSHIFT make sure that the thing returned is a byte. */
	unsigned int addshift = 0;
	unsigned int subshift = 0;

	if (K-1<8)
		addshift = (8-(K-1));
	if (K-1>8)
		subshift = ((K-1)-8);

	/* Make room beyond the end of the encoder register so we can
	* accumulate a full byte of decoded data */
	endstate = (endstate % NUMSTATES) << addshift;

	/* The store into data[] only needs to be done every 8 bits.
	* But this avoids a conditional branch, and the writes will
	* combine in the cache anyway. */
	
	d += (K-1); /* Look past tail */
	while (nbits-- != 0)
	{
		int k = (d[nbits].w[(endstate >> addshift)/32] >> ((endstate >> addshift)%32)) & 1;
		endstate = (endstate >> 1) | (k << (K-2+addshift));
		data[nbits >> 3] = endstate >> subshift;
	}

	return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi(void *p)
{
	struct vd *vp = p;

	if(vp != NULL){
	free(vp->decisions);
	free(vp);
	}
}


/* C-language butterfly */	// 128 used instead of 32 in v29
#define BFLY(i) {\
unsigned int metric,m0,m1,decision;\
	metric = (Branchtab[0].c[i] ^ sym0) + (Branchtab[1].c[i] ^ sym1);\
	m0 = vp->old_metrics->w[i] + metric;\
	m1 = vp->old_metrics->w[i+(NUMSTATES/2)] + (510 - metric);\
	decision = (signed int)(m0-m1) > 0;\
	vp->new_metrics->w[2*i] = decision ? m1 : m0;\
	d->w[i/16] |= decision << ((2*i)&31);\
	m0 -= (metric+metric-510);\
	m1 += (metric+metric-510);\
	decision = (signed int)(m0-m1) > 0;\
	vp->new_metrics->w[2*i+1] = decision ? m1 : m0;\
	d->w[i/16] |= decision << ((2*i+1)&31);\
}

/* Update decoder with a block of demodulated symbols
 * Note that nbits is the number of decoded data bits, not the number
 * of symbols!
 */
int update_viterbi_blk(void *p,unsigned char syms[],int nbits)
{
	struct vd *vp = p;
	void *tmp;
	decision_t *d;

	if(p == NULL)
		return -1;
	
	d = (decision_t *)vp->dp;

	while(nbits--){
		unsigned char sym0,sym1;

		// 0 - 7 instead of 0 - 1
		d->w[0] = d->w[1] = 0;
		sym0 = *syms++;
		sym1 = *syms++;

		// 128 loop instead of 32
		for(int i = 0; i < (NUMSTATES/2); i++)
			BFLY(i);

		d++;
		/* Swap pointers to old and new metrics */
		tmp = vp->old_metrics;
		vp->old_metrics = vp->new_metrics;
		vp->new_metrics = tmp;
	}

	vp->dp = d;

	return 0;
}
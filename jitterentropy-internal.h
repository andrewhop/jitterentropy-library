/*
 * Non-physical true random number generator based on timing jitter.
 *
 * Copyright Stephan Mueller <smueller@chronox.de>, 2013 - 2021
 *
 * License
 * =======
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU General Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */


#ifndef JITTERENTROPY_JITTERENTROPY_INTERNAL_H
#define JITTERENTROPY_JITTERENTROPY_INTERNAL_H


#if defined(_MSC_VER)
#include "jitterentropy-base-x86-windows.h"
#else
#include "../arch/jitterentropy-base-user.h"
#endif

#define SHA3_256_SIZE_DIGEST_BITS	256
#define SHA3_256_SIZE_DIGEST		(SHA3_256_SIZE_DIGEST_BITS >> 3)

/*
 * The output 256 bits can receive more than 256 bits of min entropy,
 * of course, but the 256-bit output of SHA3-256(M) can only asymptotically
 * approach 256 bits of min entropy, not attain that bound. Random maps will
 * tend to have output collisions, which reduces the creditable output entropy
 * (that is what SP 800-90B Section 3.1.5.1.2 attempts to bound).
 *
 * The value "64" is justified in Appendix A.4 of the current 90C draft,
 * and aligns with NIST's in "epsilon" definition in this document, which is
 * that a string can be considered "full entropy" if you can bound the min
 * entropy in each bit of output to at least 1-epsilon, where epsilon is
 * required to be <= 2^(-32).
 */
#define ENTROPY_SAFETY_FACTOR		64

/**
 * Function pointer data structure to register an external thread handler
 * used for the timer-less mode of the Jitter RNG.
 *
 * The external caller provides these function pointers to handle the
 * management of the timer thread that is spawned by the Jitter RNG.
 *
 * @var jent_notime_init This function is intended to initialze the threading
 *	support. All data that is required by the threading code must be
 *	held in the data structure @param ctx. The Jitter RNG maintains the
 *	data structure and uses it for every invocation of the following calls.
 *
 * @var jent_notime_fini This function shall terminate the threading support.
 *	The function must dispose of all memory and resources used for the
 *	threading operation. It must also dispose of the @param ctx memory.
 *
 * @var jent_notime_start This function is called when the Jitter RNG wants
 *	to start a thread. Besides providing a pointer to the @param ctx
 *	allocated during initialization time, the Jitter RNG provides a
 *	pointer to the function the thread shall execute and the argument
 *	the function shall be invoked with. These two parameters have the
 *	same purpose as the trailing two parameters of pthread_create(3).
 *
 * @var jent_notime_stop This function is invoked by the Jitter RNG when the
 *	thread should be stopped. Note, the Jitter RNG intends to start/stop
 *	the thread frequently.
 *
 * An example implementation is found in the Jitter RNG itself with its
 * default thread handler of jent_notime_thread_builtin.
 *
 * If the caller wants to register its own thread handler, it must be done
 * with the API call jent_entropy_switch_notime_impl as the first
 * call to interact with the Jitter RNG, even before jent_entropy_init.
 * After jent_entropy_init is called, changing of the threading implementation
 * is not allowed.
 */
struct jent_notime_thread {
    int (*jent_notime_init)(void **ctx);
    void (*jent_notime_fini)(void *ctx);
    int (*jent_notime_start)(void *ctx,
                             void *(*start_routine) (void *), void *arg);
    void (*jent_notime_stop)(void *ctx);
};

/* The entropy pool */
struct rand_data
{
    /* all data values that are vital to maintain the security
     * of the RNG are marked as SENSITIVE. A user must not
     * access that information while the RNG executes its loops to
     * calculate the next random value. */
    void *hash_state;		/* SENSITIVE hash state entropy pool */
    uint64_t prev_time;		/* SENSITIVE Previous time stamp */
#define DATA_SIZE_BITS (SHA3_256_SIZE_DIGEST_BITS)

#ifndef JENT_HEALTH_LAG_PREDICTOR
    uint64_t last_delta;		/* SENSITIVE stuck test */
    uint64_t last_delta2;		/* SENSITIVE stuck test */
#endif /* JENT_HEALTH_LAG_PREDICTOR */

    unsigned int flags;		/* Flags used to initialize */
    unsigned int osr;		/* Oversampling rate */

#ifdef JENT_RANDOM_MEMACCESS
    /* The step size should be larger than the cacheline size. */
# ifndef JENT_MEMORY_BITS
#  define JENT_MEMORY_BITS 17
# endif
# ifndef JENT_MEMORY_SIZE
#  define JENT_MEMORY_SIZE (UINT32_C(1)<<JENT_MEMORY_BITS)
# endif
#else /* JENT_RANDOM_MEMACCESS */
# ifndef JENT_MEMORY_BLOCKS
#  define JENT_MEMORY_BLOCKS 512
# endif
# ifndef JENT_MEMORY_BLOCKSIZE
#  define JENT_MEMORY_BLOCKSIZE 128
# endif
# ifndef JENT_MEMORY_SIZE
#  define JENT_MEMORY_SIZE (JENT_MEMORY_BLOCKS*JENT_MEMORY_BLOCKSIZE)
# endif
#endif /* JENT_RANDOM_MEMACCESS */

#define JENT_MEMORY_ACCESSLOOPS 128
    unsigned char *mem;		/* Memory access location with size of
					 * JENT_MEMORY_SIZE or memsize */
#ifdef JENT_RANDOM_MEMACCESS
    uint32_t memmask;		/* Memory mask (size of memory - 1) */
#else
    unsigned int memlocation; 	/* Pointer to byte in *mem */
    unsigned int memblocks;		/* Number of memory blocks in *mem */
    unsigned int memblocksize; 	/* Size of one memory block in bytes */
#endif
    unsigned int memaccessloops;	/* Number of memory accesses per random
					 * bit generation */

    /* Repetition Count Test */
    int rct_count;			/* Number of stuck values */

    /* Adaptive Proportion Test for a significance level of 2^-30 */
    unsigned int apt_cutoff;	/* Calculated using a corrected version
					 * of the SP800-90B sec 4.4.2 formula */
#define JENT_APT_WINDOW_SIZE	512	/* Data window size */
    unsigned int apt_observations;	/* Number of collected observations in
					 * current window. */
    unsigned int apt_count;		/* The number of times the reference
					 * symbol been encountered in the
					 * window. */
    uint64_t apt_base;		/* APT base reference */
    unsigned int health_failure;	/* Permanent health failure */

    unsigned int apt_base_set:1;	/* APT base reference set? */
    unsigned int fips_enabled:1;
    unsigned int enable_notime:1;	/* Use internal high-res timer */
    unsigned int max_mem_set:1;	/* Maximum memory configured by user */

#ifdef JENT_CONF_ENABLE_INTERNAL_TIMER
    volatile uint8_t notime_interrupt;	/* indicator to interrupt ctr */
	volatile uint64_t notime_timer;		/* high-res timer mock-up */
	uint64_t notime_prev_timer;		/* previous timer value */
	void *notime_thread_ctx;		/* register thread data */
#endif /* JENT_CONF_ENABLE_INTERNAL_TIMER */

    uint64_t jent_common_timer_gcd;	/* Common divisor for all time deltas */

#ifdef JENT_HEALTH_LAG_PREDICTOR
    /* Lag predictor test to look for re-occurring patterns. */

	/* The lag global cutoff selected based on the selection of osr. */
	unsigned int lag_global_cutoff;

	/* The lag local cutoff selected based on the selection of osr. */
	unsigned int lag_local_cutoff;

	/*
	 * The number of times the lag predictor was correct. Compared to the
	 * global cutoff.
	 */
	unsigned int lag_prediction_success_count;

	/*
	 * The size of the current run of successes. Compared to the local
	 * cutoff.
	 */
	unsigned int lag_prediction_success_run;

	/*
	 * The total number of collected observations since the health test was
	 * last reset.
	 */
	unsigned int lag_best_predictor;

	/*
	 * The total number of collected observations since the health test was
	 * last reset.
	 */
	unsigned int lag_observations;

	/*
	 * This is the size of the window used by the predictor. The predictor
	 * is reset between windows.
	 */
#define JENT_LAG_WINDOW_SIZE (1U<<17)

	/*
	 * The amount of history to base predictions on. This must be a power
	 * of 2. Must be 4 or greater.
	 */
#define JENT_LAG_HISTORY_SIZE 8
#define JENT_LAG_MASK (JENT_LAG_HISTORY_SIZE - 1)

	/* The delta history for the lag predictor. */
	uint64_t lag_delta_history[JENT_LAG_HISTORY_SIZE];

	/* The scoreboard that tracks how successful each predictor lag is. */
	unsigned int lag_scoreboard[JENT_LAG_HISTORY_SIZE];
#endif /* JENT_HEALTH_LAG_PREDICTOR */
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#endif //JITTERENTROPY_JITTERENTROPY_INTERNAL_H

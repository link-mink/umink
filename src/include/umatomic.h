/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef UMATOMIC_H
#define UMATOMIC_H

/**
 * Atomic GET operation (FETCH and ADD 0)
 * @param[in]   v       Pointer to value
 * @return      Current value
 */
#define UM_ATOMIC_GET(v) __sync_fetch_and_add(v, 0)

/**
 * Atomic FETCH and ADD
 * @param[in]   v       Pointer to value
 * @param[in]   inc     Increment value
 * @return      Value before this operation
 */
#define UM_ATOMIC_F_ADD(v, inc) __sync_fetch_and_add(v, inc)

/**
 * Atomic ADD and FETCH
 * @param[in]   v       Pointer to value
 * @param[in]   inc     Increment value
 * @return      Value after this operation
 */
#define UM_ATOMIC_ADD_F(v, inc) __sync_add_and_fetch(v, inc)

/**
 * Atomic FETCH and bitwise AND
 * @param[in]   v       Pointer to value
 * @param[in]   inc     Value for bitwise op
 * @return      Value before this operation
 */
#define UM_ATOMIC_F_AND(v, inc) __sync_fetch_and_and(v, inc)

/**
 * Atomic bitwise AND and FETCH
 * @param[in]   v       Pointer to value
 * @param[in]   inc     Value for bitwise op
 * @return      Value after this operation
 */
#define UM_ATOMIC_AND_F(v, inc) __sync_and_and_fetch(v, inc)

/**
 * Atomic FETCH and SUB
 * @param[in]   v       Pointer to value
 * @param[in]   dc      Decrement value
 * @return      Value before this operation
 */
#define UM_ATOMIC_F_SUB(v, dc) __sync_fetch_and_sub(v, dc)

/**
 * Atomic SUB and FETCH
 * @param[in]   v       Pointer to value
 * @param[in]   dc      Decrement value
 * @return      Value after this operation
 */
#define UM_ATOMIC_SUB_F(v, dc) __sync_sub_and_fetch(v, dc)

/*
 * @brief Atomic exchange operation
 *
 * If the current value (v) is old value (o), then write
 * new value (n) into value (v)
 *
 * @param[in]	new_val		New value
 * @return	Value before this operation
 */
#define UM_ATOMIC_COMP_SWAP(v, o, n) __sync_val_compare_and_swap(v, o, n)
#endif /* ifndef UMATOMIC_H */

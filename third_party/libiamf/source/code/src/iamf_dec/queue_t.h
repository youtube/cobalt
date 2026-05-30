/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

/**
 * @file queue_t.h
 * @brief Queue APIs.
 * @version 0.1
 * @date Created 23/05/2023
 **/


#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef struct _queue_t queue_t;

queue_t *queue_new();
int queue_push(queue_t *, void *);
void *queue_pop(queue_t *);
void *queue_take(queue_t *, int);
int queue_is_empty(queue_t *);
int queue_length(queue_t *);
void queue_free(queue_t *);

#endif /* _QUEUE_H_ */

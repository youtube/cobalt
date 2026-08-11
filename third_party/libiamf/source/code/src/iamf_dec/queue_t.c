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
 * @file queue_t.c
 * @brief Queue APIs.
 * @version 0.1
 * @date Created 23/05/2023
 **/

#include "queue_t.h"

#include <stdlib.h>

typedef struct _node_t node_t;
struct _node_t {
  void *entity;
  node_t *next;
};

struct _queue_t {
  node_t *front;
  node_t *rear;
  int count;
};

queue_t *queue_new() {
  queue_t *q = 0;
  q = (queue_t *)calloc(1, sizeof(queue_t));
  return q;
}

int queue_push(queue_t *q, void *e) {
  if (!q || !e) return -1;
  node_t *n = 0;
  n = (node_t *)calloc(1, sizeof(node_t));
  if (!n) return -2;
  n->entity = e;
  if (!q->rear)
    q->rear = n;
  else {
    q->rear->next = n;
    q->rear = n;
  }
  ++q->count;
  if (!q->front) q->front = q->rear;
  return q->count - 1;
}

void *queue_pop(queue_t *q) {
  void *e = 0;
  node_t *n = 0;

  if (!q || queue_is_empty(q)) return 0;

  n = q->front;
  e = n->entity;
  q->front = n->next;
  if (!q->front) q->rear = 0;
  --q->count;
  free(n);
  return e;
}

void *queue_take(queue_t *q, int i) {
  void *e = 0;
  node_t *n = 0;

  if (!q || queue_length(q) <= i) return 0;

  n = q->front;
  for (int k = 0; k < i; ++k) {
    n = n->next;
  }
  e = n->entity;
  return e;
}

int queue_is_empty(queue_t *q) {
  if (!q) return 1;
  return !q->count;
}

int queue_length(queue_t *q) {
  if (!q) return 0;
  return q->count;
}

void queue_free(queue_t *q) {
  if (q) free(q);
}

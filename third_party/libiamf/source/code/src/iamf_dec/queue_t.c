/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

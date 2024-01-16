// Author Woohyuk Jang, Date Nov 7 2016
// Fifo Queue functions via a singly linked list

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "queue.h" // queue and other helper functions

// create a queue node that holds the job and a link to next
job_node *createJobNode(job *p) {
    job_node *node = (job_node*) malloc(sizeof(job_node));
    if (node == NULL)
    {
      fprintf(stderr, "malloc error: out of memory");
      exit(EXIT_FAILURE);
    }
    node->data = p;
    node->next = NULL;
    return node;
}

// initialize global job queue with this function
void initializeJobQueue(job_queue *q) {
    q->front = NULL;
    q->back = NULL;
    q->size = 0;
}

// add a job to the queue to the tail end
void enqueueJob(job_queue *q, job *p) {
    job_node *node = createJobNode(p);

    // queue is empty case
    if (q->front == NULL)
    {
      assert(q->back == NULL);
      q->front = node;
      q->back = node;
    }
    // queue not empty case
    else
    {
      assert(q->back != NULL);
      q->back->next = node;
      q->back = node;
    }
    q->size++;
}

// dequeues a job from the front of the queue if not empty
// returns the arrival_time of the job that was just dequeued
int dequeueJob(job_queue *q) {
    job_node *deleted = q->front;
    int arrival_time = q->front->data->arrival_time;

    // check that queue is not empty
    assert(q->size > 0);
    if (q->size == 1)
    {
        q->front = NULL;
        q->back = NULL;
    }
    // queue is empty case
    else
    {
        assert(q->front->next != NULL);
        q->front = q->front->next;
    }
    // house keeping
    free(deleted);
    q->size--;

    return arrival_time;
}

// check if queue is empty
// 0 means empty, 1 means NOT empty
int queueIsEmpty(job_queue *target)
{
  if ((target->front) == NULL)
    return 0;
  else
    return 1;
}

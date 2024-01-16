// Author Woohyuk Jang, Date Nov 7 2016
// Fifo queue API, and structures for client-server simulation

#ifndef _QUEUE_H
#define _QUEUE_H

#define QUEUE_EMPTY 0

// containers
typedef struct job job;
typedef struct job_node job_node;
typedef struct job_queue job_queue;

struct job
{
  int arrival_time;
};

// a node in a linked list of jobs
struct job_node
{
  job *data;
  job_node *next;
};

// FIFO queue implemented as a singly-linked list
struct job_queue {
    int size;
    job_node *front;
    job_node *back;
};

// Queue management functions
job_node *createJobNode(job *);
void initializeJobQueue(job_queue *);
void enqueueJob(job_queue *, job *);
int dequeueJob(job_queue *);
int queueIsEmpty(job_queue *);

#endif

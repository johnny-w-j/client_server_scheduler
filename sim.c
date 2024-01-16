// Author Woohyuk Jang, Date Nov 7 2016
// Client server scheduling simulation with pthreads
// Refer to make man on command line for usage

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "queue.h"
#include "check.h"

#define ITERATIONS 1000
// maximum threads for safety
#define MAX_THREADS 256
// server state definitions
typedef enum state {FREE,BUSY} State;

// thread argument passing
struct worker_data
{
  int thread_id;
  int num_workers;
  double constant;
};

// accounting structure to hold total values
struct accounting
{
  int number_of_jobs;

  int total_execution_time;
  int total_turnaround_time;
  int total_waiting_time;
  int total_queue_length;
  int total_interarrival_time;
} global_accounting = {0,0,0,0,0,0}; // init all to 0
job_queue global_queue;
// queue_mutex for using global_accounting, global_queue
pthread_mutex_t queue_mutex;

// time_mutex and cv for using global_time
// starts at -1 to assume that simulation starts at time 0
// clients that generate jobs are recorded at global_time
// servers that deal with jobs are recorded at global_time + 1 for logic
int global_time = -1;
pthread_mutex_t time_mutex;
pthread_cond_t time_cv;

// worker_mutex and cv for notifying clock thread that client,server threads are done current iteration
int worker_status = 0;
pthread_mutex_t worker_mutex;
pthread_cond_t worker_cv;

// time_barrier synchronization used to smoothen exit procedure
pthread_barrier_t time_barrier;

// server thread routine
void *server_function (void *thread_arg)
{
  // extract arguments with proper typing
  struct worker_data *my_data;
  my_data = (struct worker_data*) thread_arg;
  long tid = (long) my_data->thread_id;
  double mu = (double) my_data->constant;

  // server simulation init variables
  int prev_time = -1;
  int global_time_copy;
  unsigned int seed = time(NULL)+tid+(long)getpid();
  double mu_gamma;
  State server_state = FREE;
  int arrival_time = -1;
  int start_time = -1;
  int rc;

  // server simulation cycle
  int i;
  for(i=0; i<ITERATIONS; i++)
  {
    // mu_gamma of this current iteration compared to constant gamma
    mu_gamma = (double) rand_r(&seed) / (double) RAND_MAX;

    // wait for a change in time (i.e. clock thread signals for start of next cycle
    rc = pthread_mutex_lock(&time_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    while ( global_time <= prev_time )
    {
      rc = pthread_cond_wait(&time_cv, &time_mutex);
      errcheck("pthread_cond_wait()\n",rc);
    }
    prev_time = global_time;
    global_time_copy = global_time;
    fprintf(stderr, "ServerWorkerTime: %d\n", prev_time);
    rc = pthread_mutex_unlock(&time_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);

    // start of server work with global_accounting, and global_queue
    // queue_mutex lock and unlock block
    rc = pthread_mutex_lock(&queue_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    // if server state is free then check for work from global_queue
    if (server_state == FREE)
    {
      if (queueIsEmpty(&global_queue) == 1)
      {
        arrival_time = dequeueJob(&global_queue);
        server_state = BUSY;
        start_time = global_time_copy;
      }
    }

    // if mu_gamma satisfies probability condition, and server has a job being worked on...
    // then we terminate this job and record results
    if (mu_gamma < mu && server_state == BUSY)
    {
      fprintf(stderr, "Mu_Gamma Executed: %f\n", mu_gamma);
      // then terminate job
      if (start_time >= 0 && arrival_time >= 0)
      {
        // +1 is for server logic. i.e. time cycle starts at 0. A server can't finish a job at 0 immediately.
        // We assume job is finished at start of next cycle
        global_accounting.total_turnaround_time += (global_time_copy+1)-arrival_time;
        global_accounting.total_execution_time += (global_time_copy+1)-start_time;
        // (completion time - arrival time) - (completion time - start time) = waiting time
        global_accounting.total_waiting_time += ((global_time_copy+1)-arrival_time) - ((global_time_copy+1)-start_time);
        server_state = FREE;
      }
      else
      {
        // this should not happen
        fprintf(stderr, "QUEUE ERROR IN SERVER THREAD\n");\
        exit(EXIT_FAILURE);
      }
    }
    rc = pthread_mutex_unlock(&queue_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);

    // update clock thread that this particular server thread is done and waiting for next cycle
    rc = pthread_mutex_lock(&worker_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    worker_status--;
    rc = pthread_cond_signal(&worker_cv);
    errcheck("pthread_cond_signal()\n",rc);
    rc = pthread_mutex_unlock(&worker_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);
  }

  // for exit procedure synchronization
  pthread_barrier_wait(&time_barrier); // no EINTR
  printf("server thread %ld done. TIME: %d.\n", tid, prev_time);
  pthread_exit((void*) tid);
}

// client thread function
void *client_function (void *thread_arg)
{
  // unpack data
  struct worker_data *my_data;
  my_data = (struct worker_data*) thread_arg;
  long tid;
  tid = (long) my_data->thread_id;
  double lambda;
  lambda = (double) my_data->constant;

  // init client simulation variables
  int prev_time = -1;
  int potential_arrival_time;
  int previous_arrival_time = 0;
  unsigned int seed = time(NULL)+tid+(long)getpid();
  double lambda_gamma;
  int rc;

  // client simulation cycle
  int i;
  for(i=0; i<ITERATIONS; i++)
  {
    // current cycle's lambda gamma calculation
    lambda_gamma = (double) rand_r(&seed) / (double) RAND_MAX;

    // wait for time cycle start signal from clock thread
    rc = pthread_mutex_lock(&time_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    while ( global_time <= prev_time )
    {
      rc = pthread_cond_wait(&time_cv, &time_mutex);
      errcheck("pthread_cond_wait()\n",rc);
    }
    prev_time = global_time;
    potential_arrival_time = global_time;
    fprintf(stderr, "ClientWorkerTime: %d\n", prev_time);
    rc = pthread_mutex_unlock(&time_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);

    // queue_mutex block to do client's work and update global_accounting, global_queue
    rc = pthread_mutex_lock(&queue_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    // if probability condition satisfied, then we create a job
    if (lambda_gamma < lambda)
    {
      fprintf(stderr, "Lambda_Gamma Creation: %f\n", lambda_gamma);
      // then create new job, enqueue, and do some book keeping
      job new_job;
      new_job.arrival_time = potential_arrival_time;
      enqueueJob(&global_queue, &new_job);
      global_accounting.number_of_jobs++;
      global_accounting.total_interarrival_time += (new_job.arrival_time - previous_arrival_time);
      previous_arrival_time = new_job.arrival_time;
    }
    rc = pthread_mutex_unlock(&queue_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);

    // signal to clock that this particular client thread is ready for next cycle
    rc = pthread_mutex_lock(&worker_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    worker_status--;
    rc = pthread_cond_signal(&worker_cv);
    errcheck("pthread_cond_signal()\n",rc);
    rc = pthread_mutex_unlock(&worker_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);
  }

  // client exit procedure synchronization
  pthread_barrier_wait(&time_barrier); // no EINTR
  printf("client thread %ld done. TIME: %d.\n", tid, prev_time);
  pthread_exit((void*) tid);
}

// timekeeper thread functionality
void *clock_function (void *thread_arg)
{
  // unpack data
  struct worker_data *my_data;
  my_data = (struct worker_data*) thread_arg;
  long tid;
  tid = (long) my_data->thread_id;
  int worker_count = (long) my_data->num_workers;
  int rc;

  // time cycles
  int i;
  for (i=0; i<ITERATIONS; i++)
  {
    // increase global_time and broadcast
    rc = pthread_mutex_lock(&time_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    global_time++;
    rc = pthread_cond_broadcast(&time_cv);
    errcheck("pthread_cond_broadcast()\n",rc);
    rc = pthread_mutex_unlock(&time_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);

    // clock thread waits for clients and servers to finish their work for current cycle
    rc = pthread_mutex_lock(&worker_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    while (worker_status > 0)
    {
      rc = pthread_cond_wait(&worker_cv, &worker_mutex);
      errcheck("pthread_cond_wait()\n",rc);
    }
    worker_status = worker_count;
    rc = pthread_mutex_unlock(&worker_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);

    // uses queue_mutex to do queue_length book keeping
    rc = pthread_mutex_lock(&queue_mutex);
    errcheck("pthread_mutex_lock()\n",rc);
    global_accounting.total_queue_length += global_queue.size;
    rc = pthread_mutex_unlock(&queue_mutex);
    errcheck("pthread_mutex_unlock()\n",rc);
  }

  // exit procedure synchronization
  pthread_barrier_wait(&time_barrier); // no EINTR
  printf("Clock Thread %ld done.\n", tid);
  pthread_exit((void*) tid);
}

// main thread which waits for clock, clients, servers threads
int main (int argc, char *argv[])
{
  // argument count must be odd, otherwise invalid input from user
  if ( (argc%2) != 1 )
  {
    fprintf(stderr, "%s: Invalid number of arguments.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // setup default parameters for now
  int servers = 2, clients = 2, clocks = 1;
  double lambda = 0.005, mu = 0.01;

  // decipher user input for user's wanted parameters
  int i;
  for (i=1; i<argc && argc!=1; i++) {
    if ( strncmp("--Servers", argv[i], strlen("--Servers"))==0 || strncmp("--servers", argv[i], strlen("--servers"))==0 )
    {
      servers = atoi(argv[i+1]);
      i++;
    }
    else if ( strncmp("--Clients", argv[i], strlen("--Clients"))==0 || strncmp("--clients", argv[i], strlen("--clients"))==0 )
     {
      clients = atoi(argv[i+1]);
      i++;
    }
    else if ( strncmp("--Lambda", argv[i], strlen("--Lambda"))==0 || strncmp("--lambda", argv[i], strlen("--lambda"))==0 )
    {
      lambda = atof(argv[i+1]);
      i++;
    }
    else if ( strncmp("--Mu", argv[i], strlen("--Mu"))==0 || strncmp("--mu", argv[i], strlen("--mu"))==0 )
    {
      mu = atof(argv[i+1]);
      i++;
    }
  }

  // precaution from creating too many threads and crashing
  if ((servers+clients) > MAX_THREADS)
  {
    fprintf(stderr, "%s: SERVER+CLIENT THREAD COUNT CANNOT EXCEED 256.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  // print out parameters
  printf("servers: %d\n", servers);
  printf("clients: %d\n", clients);
  printf("lambda: %f\n", lambda);
  printf("mu: %f\n", mu);

  // bookkeeping and init for simulation start
  int total_num_threads = servers+clients+clocks;
  worker_status = servers+clients;
  pthread_t thread[total_num_threads];
  pthread_attr_t attr;
  int rc;
  long t;
  void *status;
  struct worker_data worker_data_array[total_num_threads];
  initializeJobQueue(&global_queue);

  // Initialize and set thread detached attribute explicitly as joinable (for portability)
  rc = pthread_attr_init(&attr);
  errcheck("pthread_attr_init()\n",rc);
  rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  errcheck("pthread_attr_setdetachstate()\n",rc);

  // Initialize mutex, condition variable, and barrier objects
  rc = pthread_mutex_init(&time_mutex, NULL);
  errcheck("pthread_mutex_init()\n",rc);
  rc = pthread_cond_init(&time_cv, NULL);
  errcheck("pthread_cond_init()\n",rc);
  rc = pthread_mutex_init(&worker_mutex, NULL);
  errcheck("pthread_mutex_init()\n",rc);
  rc = pthread_cond_init(&worker_cv, NULL);
  errcheck("pthread_cond_init()\n",rc);
  rc = pthread_mutex_init(&queue_mutex, NULL);
  errcheck("pthread_mutex_init()\n",rc);
  rc = pthread_barrier_init(&time_barrier,NULL,total_num_threads);
  errcheck("pthread_barrier_init()\n",rc);

  // Create the three types of threads: clock, servers, clients
  for(t=0; t<total_num_threads; t++)
  {
    if (t<servers)
    {
      worker_data_array[t].thread_id = t;
      worker_data_array[t].num_workers = clients+servers;
      worker_data_array[t].constant = mu;
      rc = pthread_create(&thread[t], &attr, server_function, (void*) &worker_data_array[t]);
    }
    else if (t<(servers+clients))
    {
      worker_data_array[t].thread_id = t;
      worker_data_array[t].num_workers = clients+servers;
      worker_data_array[t].constant = lambda;
      rc = pthread_create(&thread[t], &attr, client_function, (void*) &worker_data_array[t]);
    }
    else
    {
      worker_data_array[t].thread_id = t;
      worker_data_array[t].num_workers = clients+servers;
      worker_data_array[t].constant = t;
      rc = pthread_create(&thread[t], &attr, clock_function, (void*) &worker_data_array[t]);
    }
    errcheck("pthread_create()\n",rc);
  }

  // wait for threads to finish simulation
  for(t=0; t<total_num_threads; t++)
  {
    rc = pthread_join(thread[t], &status);
    errcheck("pthread_join()\n",rc);
    printf("Main: completed join with thread %ld having a status of %ld\n",t,(long)status);
  }

  // free attributes
  rc = pthread_attr_destroy(&attr);
  errcheck("pthread_attr_destroy(&attr)\n",rc);
  rc = pthread_mutex_destroy(&time_mutex);
  errcheck("pthread_mutex_destroy(&time_mutex)\n",rc);
  rc = pthread_cond_destroy(&time_cv);
  errcheck("pthread_cond_destroy(&time_cv)\n",rc);
  rc = pthread_mutex_destroy(&worker_mutex);
  errcheck("pthread_mutex_destroy(&worker_mutex)\n",rc);
  rc = pthread_cond_destroy(&worker_cv);
  errcheck("pthread_cond_destroy(&worker_cv)\n",rc);
  rc = pthread_mutex_destroy(&queue_mutex);
  errcheck("pthread_mutex_destroy(&queue_mutex)\n",rc);
  rc = pthread_barrier_destroy(&time_barrier);
  errcheck("pthread_barrier_destroy(&time_barrier)\n",rc);

  // time to print simulation results
  double awt = (double) global_accounting.total_waiting_time / (double) global_accounting.number_of_jobs;
  double axt = (double) global_accounting.total_execution_time / (double) global_accounting.number_of_jobs;
  double ata = (double) global_accounting.total_turnaround_time / (double) global_accounting.number_of_jobs;
  double aql = (double) global_accounting.total_queue_length / (double) ITERATIONS;
  double aia = (double) global_accounting.total_interarrival_time / (double) (global_accounting.number_of_jobs-1);
  printf("**********************************************************\n");
  printf("(1) Average Waiting Time (AWT): %f\n", awt);
  printf("(2) Average Execution Time (AXT): %f\n", axt);
  printf("(3) Average Turnaround Time (ATA): %f\n", ata);
  printf("(4) Average Queue Length (AQL): %f\n", aql);
  printf("(5) Average Interarrival Time (AIA): %f\n", aia);
  printf("**********************************************************\n");
  printf("MainSpawnerThread: Program Complete. Exit.\n");
  pthread_exit(NULL);
}

#include <pthread.h>
#include <semaphore.h>

#define perr(msg) { perror(msg); exit(1); }

extern pthread_mutex_t mutex;
extern sem_t sem;

/* Individual job struct. Accept function, arguments and job size. */
typedef struct JobStruct {
  void *(*function)(void *arg);
  void *arg;
  struct JobStruct *next;
  struct JobStruct *prev;
  int size;
}Job;

/* Queue struct. */
typedef struct QueueStruct {
  Job *front;
  Job *rear;
  int currentSize;
}Queue;

/* Thread pool struct. */
typedef struct PoolStruct {
  pthread_t *threads;
  int threadsNum;
  Queue *queue;
}ThreadPool;

/* Arguments struct. Store the args which will passed into every job. */
typedef struct ArgStruct {
  int sock_id;
  int sock_fd;
  int threadsNum;
  Queue *queue;
  char fullPath[256];
  char log[256];
  int flg;
  char *sched;
  char dir[256];
  char *logFile;
  int debugFlg;
}Args;

/* start the server */
int initServer();

/* init the thread pool */
void *InitThreadPool(void *);

/* the things each thread should do */
void executeThread(ThreadPool *);

/* init the job queue */
Queue *InitQueue();

/* enqueue the job by First Come First Serve */
void EnqueueFCFS(Queue *, Job *);

/* enqueue the job by Shorest Job First */
void EnqueueSJF(Queue *, Job *);

/* dequeue the job */
void Dequeue(Queue *);

/* get the rear job of the queue */
Job *GetRear(Queue *);

/* init the job */
Job *InitJob(void *(*function)(void *), void *, int);

/* add the job into the queue */
void addJob(void *);

/* get first line of the request info */
int getFirLine(int, char *);

/* insert some additional log info into the log file */
char *inserIntoString(char *, char *);

/* analyze the request action and request file path, return the request file size */
unsigned int getFileSize(int, char *, char *, char *);

/* send the content to the client */
void sendContent(void *);

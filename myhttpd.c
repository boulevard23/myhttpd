#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "myhttpd.h"

pthread_mutex_t mutex;
sem_t sem;

/*
 *  start the server, and listen to the given port
 */
int initServer(int port) {
  int sock_id;
  struct sockaddr_in saddr;
  sock_id = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_id == -1 ) {
    perr("Create socket error!");
  }
  bzero(&saddr, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);
  saddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock_id, (struct sockaddr *) &saddr, sizeof(saddr)) != 0) {
    perr("Server binding error!");
  }
  if (listen(sock_id, 5)) {
    perr("Server listen error!");
  }
  printf("Listening to the port: %d \n", port);
  return sock_id;
}

/*
 * initial the threadpopl
 */
void *InitThreadPool(void *arguments) {
  printf("123\n");
  ThreadPool *threadPool;

  Args *args = arguments;
  int threadsNum;
  Queue *queue;
  threadsNum = args->threadsNum;
  queue = args->queue;

  // default threadsNum is 4
  if(!threadsNum || threadsNum <= 0) {
    threadsNum = 4;
  }

  threadPool = (ThreadPool *)malloc(sizeof(ThreadPool));
  if (threadPool == NULL) {
    perr("Cannot allocate the memory for the thread pool!\n");
  }

  threadPool->threads = (pthread_t *)malloc(sizeof(pthread_t));
  if (threadPool->threads == NULL) {
    perr("Cannot allocate the memory for the thread!\n");
  }

  threadPool->queue = (Queue *)malloc(sizeof(queue));
  if (threadPool->queue == NULL) {
    perr("Cannot allocate the memory for the thread job queue!\n");
  }

  threadPool->threadsNum = threadsNum;
  threadPool->queue = queue;

  // create threads in the thread pool
  int i;
  for (i = 0; i < threadsNum; i++) {
    pthread_create(&(threadPool->threads[i]), NULL, (void *)executeThread, (void *)threadPool);
  }

  int j;
  for (j = 0; j < threadsNum; j++) {
    pthread_join(threadPool->threads[j], NULL);
  }
  printf("init thread pool successs\n");
}

/*
 * define the job each thread should do
 */
void executeThread(ThreadPool *threadPool) {
  while(1) {
    void *(*func_buff)(void *arg);
    void *arg_buff;
    Job *e;
    sem_wait(&sem);
    pthread_mutex_lock(&mutex);
    /*printf("thread: %d start working \n", (int)pthread_self());*/

    e = GetRear(threadPool->queue);
    func_buff = e->function;
    arg_buff = e->arg;
    func_buff(arg_buff);
    Dequeue(threadPool->queue);
    pthread_mutex_unlock(&mutex);
    free(e);
  }
}

Queue *InitQueue() {
  Queue *Q;
  Q = (Queue *)malloc(sizeof(Queue));
  if (Q == NULL) {
    perr("Cannot allocate the memory for the queue!\n");
  }

  // init the mutex
  pthread_mutex_init(&mutex, NULL);

  // init the sem, set the sem value to 0
  sem_init(&sem, 0, 0);

  Q->currentSize = 0;
  Q->front = NULL;
  Q->rear = NULL;
  return Q;
}

/*
void DisposeQueue(Queue *Q) {
  if (Q != NULL) {
    free(Q);
  }
}

void EmptyQueue(Queue *Q) {
  Q->front = NULL;
  Q->rear = NULL;
  Q->currentSize = 0;
}

int IsEmpty(Queue *Q) {
  return Q->currentSize == 0;
}
*/

/*
 * enqueue the job by FCFS
 */
void EnqueueFCFS(Queue *Q, Job *new) {
  Job *old;
  old = Q->front;
  if (Q->currentSize == 0) {
    Q->front = new;
    Q->rear = new;
  } else {
    old->prev = new;
    new->next = old;
    Q->front = new;
  }
  Q->currentSize++;
  /*printf("Enqueue Size: %d \n", Q->currentSize);*/
}

/*
 * enqueue the job by SJF
 */
void EnqueueSJF(Queue *Q, Job *new) {
  Job *old;
  old = Q->front;
  if(Q->currentSize == 0) {
      Q->front = new;
      Q->rear = new;
  } else if (new->size >= old->size) {
    new->next = old;
    old->prev = new;
    Q->front = new;
  } else {
    while(new->size < old->size) {
      if(old->next == NULL) {
        old->next = new;
        new->prev = old;
        Q->rear = new;
        Q->currentSize++;
        return;
      } else {
        old = old->next;
      }
    }
    old->prev->next = new;
    new->prev = old->prev;
    new->next = old;
    old->prev = new;
  }
  Q->currentSize++;
}

Job *GetRear(Queue *Q) {
  return Q->rear;
}

void Dequeue(Queue *Q) {
  Q->rear = Q->rear->prev;
  Q->currentSize--;
}

/*
 * init every job's function, args, and size
 */
Job *InitJob(void *(*function)(void *), void *arg, int size) {
  Job *j;
  j = (Job *)malloc(sizeof(Job));
  if (j == NULL) {
    perr("Cannot allocate memory for the job!\n");
  }

  j->next = NULL;
  j->prev = NULL;
  j->function = function;
  j->arg = arg;
  j->size = size;
  return j;
}

void addJob(void *arguments) {
  Args *args = arguments;
  Queue *Q = args->queue;
  int sock_id = args->sock_id;
  char *sched = args->sched;
  int sock_fd;
  unsigned int fileSize;
  char action[128];
  char path[256];
  char log[256];
  time_t t;
  int contentFlg = 1;

  while(1) {
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    bzero(&caddr, sizeof(caddr));
    sock_fd = accept(sock_id, (struct sockaddr *) &caddr, &clen);

    // log: receive time
    time(&t);
    char *recvTime = asctime(gmtime(&t));
    recvTime[strlen(recvTime) - 1] = 0;    // delete last \n character

    // log: remote ip
    strcpy(log, inet_ntoa(caddr.sin_addr));
    strcat(log, " [");
    strcat(log, recvTime);
    strcat(log, "] ");

    if( sock_fd == -1) {
      perr("Server accept error!");
    }

    strcpy(path, args->dir);

    Args *argsSendContent;
    argsSendContent = (Args *)malloc(sizeof(Args));
    strcpy(argsSendContent->dir, path);

    fileSize = getFileSize(sock_fd, action, path, log);

    if (strcasecmp(action, "HEAD") == 0) {
      fileSize = 0;
      contentFlg = 0;
    } else if (strcasecmp(action, "GET") != 0) {
      perr("Unknown request type! \n");
    }

    strcpy(argsSendContent->fullPath, path);
    strcpy(argsSendContent->log, log);
    argsSendContent->sock_fd = sock_fd;
    argsSendContent->flg = contentFlg;
    argsSendContent->logFile = args->logFile;
    argsSendContent->debugFlg = args->debugFlg;

    Job *new;
    new = InitJob((void *)sendContent, argsSendContent, fileSize);

    // judge which schedule it will choose
    if (sched == "FCFS") {
      pthread_mutex_lock(&mutex);
      EnqueueFCFS(Q, new);
      pthread_mutex_unlock(&mutex);
    } else {
      pthread_mutex_lock(&mutex);
      EnqueueSJF(Q, new);
      pthread_mutex_unlock(&mutex);
    }
    sem_post(&sem);
  }
}

/*
 *  get the first line of the request
 */
int getFirLine(int client, char *buffer) {
  int i = 0;
  int n = 0;
  char c = '\0';
  while (c != '\n') {
    n = recv(client, &c, 1, 0);
    if (n > 0) {
      buffer[i] = c;
      i++;
    }
  }
  buffer[i] = '\0';
  return i;
}

// get the request action, request file path, and return the file size.
unsigned int getFileSize(int client, char *action, char *path, char *log) {
  char buffer[128];
  char url[256];
  char size[10];
  char logPath[256];
  bzero(buffer, 128);
  bzero(url, 256);
  int i = 0, j = 0;
  int fileSize;
  FILE *file;
  getFirLine(client, buffer);

  // log: first line
  strcat(log, "\"");
  buffer[strlen(buffer) - 2] = 0;    // delete last \n character
  strcat(log, buffer);
  strcat(log, "\" ");
  // log: status line
  strcat(log, "200");

  // get the request action
  while (!isspace(buffer[j])) {
    action[i] = buffer[j];
    i++;
    j++;
  }
  i = 0;

  while(isspace(buffer[j])) {
    j++;
  }

  // get the request file path
  while(!isspace(buffer[j])) {
    url[i] = buffer[j];
    i++;
    j++;
  }


  if (*url == '~') {
    bzero(path, sizeof(path));
    strcat(path, "/home/");
    char *ext = strchr(url, '/');
    if (ext != NULL) {
      size_t pos = ext - url -1;
      char tmp[128];
      bzero(tmp, sizeof(tmp));
      memcpy(tmp, &url[1], pos);
      strcat(path, tmp);
      strcat(path, "/myhttpd");
      strcat(path, ext);
      /*printf("path index: %s \n", path);*/
    } else {
      strcat(path, &url[1]);
      strcat(path, "/myhttpd");
      /*printf("path no index: %s \n", path);*/
    }
  } else {
    strcat(path, url);
  }

  file = fopen(path, "r");
  if (file != NULL) {
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);
  } else {
    fileSize = -1;
  }

  // log: content length
  strcat(log, " ");
  sprintf(size, "%d", fileSize);
  strcat(log, size);
  strcat(log, "\n");

  return fileSize;
}

// manipulate the log buffer and get the final log info
char *insertIntoString(char *p1, char *p2) {
  size_t pos;
  char *lastPart = strchr(p1, ']');
  pos = lastPart - p1 + 1;
  char tmp[256];
  bzero(tmp, sizeof(tmp));
  memcpy(tmp, p1, pos);
  strcat(tmp, " [");
  strcat(tmp, p2);
  strcat(tmp, "]");
  strcat(tmp, &p1[pos]);
  return tmp;
}

/*
 *  write the log into the file
 *  send the formated headers and the data of the requested file to the
 *  client.
 */
void sendContent(void *arguments) {
  Args *args = arguments;
  int client = args->sock_fd;
  char *path = args->fullPath;
  char *log = args->log;
  int flg = args->flg;
  char *dir = args->dir;
  char buffer[1024];
  char logFinal[1024];
  char logPath[1024];
  char tmp[256];
  bzero(logFinal, sizeof(logFinal));
  bzero(logPath, sizeof(logPath));
  bzero(buffer, sizeof(buffer));
  bzero(tmp, sizeof(tmp));
  char *ext;
  time_t t;
  FILE *file, *logFile;

  time(&t);
  // status line
  strcat(buffer, "HTTP/1.0 200 OK \n");
  // GMT time
  strcat(buffer, "Date: ");
  char *assignTime = asctime(gmtime(&t));
  strcat(buffer, assignTime);
  assignTime[strlen(assignTime) - 1] = 0;    // delete last \n character

  // get the final log info
  strcpy(logFinal, insertIntoString(log, assignTime));

  if (args->debugFlg == 1) {
    printf("debug log: \n %s \n", logFinal);
  } else if (args->logFile != NULL) {
    // write the log info into the file
    strcpy(logPath, dir);
    strcat(logPath, args->logFile);
    logFile = fopen(logPath, "a+");
    fprintf(logFile, "%s", logFinal);
    fclose(logFile);
  }

  // server
  strcat(buffer, "Server: myhttpd/0.1 \n");

  // last modified time
  struct stat file_status;
  file = fopen(path, "r");
  if (file != NULL) {
    stat(path, &file_status);
    strcat(buffer, "Last-Modified: ");
    strcat(buffer, asctime(gmtime(&file_status.st_mtime)));

    // content type
    ext = strrchr(path, '.');
    if (ext != NULL) {
      if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".txt") == 0) {
        strcat(buffer, "Content-Type: text/html \n");
      } else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, "jpeg") == 0 || strcasecmp(ext, "gif")) {
        strcat(buffer, "Content-Type: image/gif \n");
      } else {
        perr("Content type not support. \n");
      }
    } else {
      printf("Content type input error! \n");
    }

    // content length
    strcat(buffer, "Content-Length: ");
    bzero(tmp, sizeof(tmp));
    sprintf(tmp, "%llu", file_status.st_size);
    strcat(buffer, tmp);
    strcat(buffer, "\n\n");

    // the request action is "GET", return the request file to the client.
    if (flg == 1) {
      // read file
      bzero(tmp, sizeof(tmp));
      fgets(tmp, sizeof(tmp), file);
      while(!feof(file)) {
        strcat(buffer, tmp);
        bzero(tmp, sizeof(tmp));
        fgets(tmp, sizeof(tmp), file);
      }
      fclose(file);
    }
  } else if (flg == 1) {
    // request file does not exist, output all the files under this directory.
    DIR *dir;
    ext = strrchr(path, '/');
    int pos = ext - path + 1;
    bzero(tmp, sizeof(tmp));
    memcpy(tmp, path, pos);
    strcat(buffer, "\nThe file you request does not exist.\n");
    strcat(buffer, "Dir: ");
    strcat(buffer, tmp);
    strcat(buffer, " contains following files \n\n");
    /*printf("The file you request does not exist.\n dir: %s \n", tmp);*/
    struct dirent *ent;
    dir = opendir(tmp);
    if (dir != NULL) {
      while ((ent = readdir (dir)) != NULL) {
        if (*ent->d_name != '.') {
          /*printf ("%s\n", ent->d_name);*/
          strcat(buffer, ent->d_name);
          strcat(buffer, "\n");
        }
      }
      closedir (dir);
    } else {
      perr("Directory does not found. \n");
    }
  }

  printf("%s", buffer);
  send(client, buffer, sizeof(buffer), 0);
  close(client);
}

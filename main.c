#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "myhttpd.h"

int main(int argc, char *argv[]) {
  static const char help[] =
    "\nMyhttpd Usage Summary:\n\n"
    "   -d              - Enter debugging mode. The server will only accept one connection at a time and enable logging to stdout.\n\n"
    "   -h              - Print this usage summary with all options.\n\n"
    "   -l file         - Log all requests to the given file.\n\n"
    "   -p port         - Listen on the given port. The default port is 8080.\n\n"
    "   -r dir          - Set the root directory for the http server to dir. The default dir is the directory where the server is running.\n\n"
    "   -t time         - Set the queuing time to time seconds. The default time is 60 seconds.\n\n"
    "   -n threadnum    - Set number of threads waiting ready in the execution thread pool to threadnum.\n\n"
    "   -s sched        - Set the scheduling policy. It can be either FCFS or SJF. The default is FCFS.\n\n";

  int c;
  char *logFile = NULL;
  int portNo = 8080;
  char *dir = NULL;
  int waitTime = 60;
  int threads = 4;
  char *sched = NULL;
  int debugFlg = 0;

  opterr = 0;

  while((c = getopt(argc, argv, "dhl:p:r:t:n:s:")) != -1) {
    switch(c) {
      case 'd':
        debugFlg = 1;
        break;
      case 'h':
        printf("%s", help);
        exit(0);
        break;
      case 'l':
        logFile = optarg;
        break;
      case 'p':
        portNo = atoi(optarg);
        if (portNo == 0 && optarg != '0') {
          perr("option -p argument type error! \n");
        }
        break;
      case 'r':
        dir = optarg;
        break;
      case 't':
        waitTime = atoi(optarg);
        if (waitTime == 0 && optarg != '0') {
          perr("option -t argument type error! \n");
        }
        break;
      case 'n':
        threads = atoi(optarg);
        if (threads == 0 && optarg != '0') {
          perr("option -n argument type error! \n");
        }
        break;
      case 's':
        sched = optarg;
        if (strcasecmp(sched, "FCFS") != 0 && strcasecmp(sched, "SJF") != 0) {
          perr("option -s argument can only be \"FCFS\" or \"SJF\" \n");
        }
        break;
      case '?':
        if (optopt == 'l') {
          perr("option -l needs an argument. \n");
        } else if (optopt == 'p') {
          perr("option -p needs an argument. \n");
        } else if (optopt == 'r') {
          perr("option -r needs an argument. \n");
        } else if (optopt == 't') {
          perr("option -t needs an argument. \n");
        } else if (optopt == 'n') {
          perr("option -n needs an argument. \n");
        } else if (optopt == 's') {
          perr("option -s needs an argument. \n");
        }
        return 1;
    }
  }

  Queue *Q;
  Args *argsAddJob, *argsInitThreadPool;
  pthread_t tPool, tEnqueue;
  pid_t pid;
  int sock_id, sock_fd, clen;

  sock_id = initServer(portNo);

  Q = InitQueue();

  // set the default scheduling policy to "FCFS"
  if (sched == NULL) {
    sched = "FCFS";
  }

  // set the default dir to the current working directory
  if (dir == NULL) {
    dir = (char *)malloc(256);
    getcwd(dir, 256);
  }

  // set the args which will be passed into InitThreadPool()
  argsInitThreadPool = (Args *)malloc(sizeof(Args));
  if (debugFlg == 0) {
    argsInitThreadPool->threadsNum = threads;
  } else {
    argsInitThreadPool->threadsNum = 1;
  }
  argsInitThreadPool->queue = Q;

  // set the args which will be passed into AddJob()
  argsAddJob = (Args *)malloc(sizeof(Args));
  argsAddJob->queue = Q;
  argsAddJob->sock_id = sock_id;
  strcpy(argsAddJob->dir, dir);
  argsAddJob->sched = sched;
  argsAddJob->logFile = logFile;
  argsAddJob->debugFlg = debugFlg;

  if (debugFlg == 0) {
    pid = fork();

    if (pid < 0) {
      perr("Fork new process error! \n");
    } else if (pid > 0) {
      // kill the parent process
      exit(0);
    }

    // set the child thread to daemon thread
    setsid();
    chdir("/");
    umask(0);
  }

  // create a thread to listening the port and add request to the queue
  pthread_create(&tEnqueue, NULL, (void *) addJob, (void *) argsAddJob);

  if (debugFlg == 0) {
    sleep(waitTime);    // thread pool will init after this time.
  }

  // create a thread init the thread pool
  pthread_create(&tPool, NULL, (void *) InitThreadPool, (void *) argsInitThreadPool);
  pthread_join(tEnqueue, NULL);
  pthread_join(tPool, NULL);

  return 0;
}

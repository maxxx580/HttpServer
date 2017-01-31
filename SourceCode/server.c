/* csci4061 S2016 Assignment 4
* section: 9
* date: 12/10/2016
* Name:	Trevor Lecrone, 	Al Swenson, 		Zixiang Ma
* Id: 	4968137(lecro007),	4962291(swens893), 	4644999(maxxx580)
*/
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "util.h"
#include <sys/syscall.h>
#define MAX_THREADS 100
#define MAX_QUEUE_SIZE 100
#define MAX_REQUEST_LENGTH 1024

// static int key = 4061; //msg queue key
// static int msqid = -1; // msg queue id

//Structure for queue.
typedef struct request_queue {
  int m_socket;
  char  m_szRequest[MAX_REQUEST_LENGTH];
} request_queue_t;

// Structure for passing information to logger
typedef struct log_info {
  int thread_number;
  int request_number;
  int target_fd;
  char  m_szRequest[MAX_REQUEST_LENGTH];
  int bytes;
  char* error;
} log_info_t;

// Structure for passing information to worker and dispatch
typedef struct input_info {
  int thread_num;
  char * path;
  int port;
  int queue_length;
  int cache_size;
} input_info_t;

// request queue
static request_queue_t buffer[MAX_QUEUE_SIZE];
static pthread_cond_t buffer_full = PTHREAD_COND_INITIALIZER;
static pthread_cond_t buffer_empty = PTHREAD_COND_INITIALIZER;

// locks
static pthread_mutex_t buffer_access = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_access = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t id_access = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t malloc_access = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t dispatch_cnt_lock = PTHREAD_MUTEX_INITIALIZER;

// request queue index
static int buff_front = 0;
static int buff_rear = 0;
static int num_workers;
static int queue_length;
static int cache_size;
static char * root_path;
static int dispatch_cnt = 0;        // dispatch thread counter
static int num_dispatchers;         // input info
static int current_thread_id = 0;   // used to get next availible thread ID
static int req_cnt = 0;             // count the number of request in the queue


// used to find the minimum between specified input and MAX.
int min(int a, int b) {
  if (a <= b) {
    return a;
  } else {
    return b;
  }
}

request_queue_t buffer_get() {
  request_queue_t temp = buffer[buff_rear%queue_length];
  req_cnt--;
  buff_rear = (buff_rear+1)%queue_length;
  return temp;
}

void buffer_add(request_queue_t temp) {
  buffer[buff_front%queue_length] = temp;
  req_cnt++;
  buff_front = (buff_front+1)%queue_length;
  return;
}

int getAnId() {
  int temp;
  pthread_mutex_lock(&id_access);
  if(current_thread_id < MAX_THREADS) {
    temp = current_thread_id;
    current_thread_id++;
  } else {
    temp = -1;
  }
  pthread_mutex_unlock(&id_access);
  return temp;
}

void logger(log_info_t log_info) {
  pthread_mutex_lock(&log_access);  // CRITICAL SECTION: write to end of log file
  FILE *fp;
  if ((fp = fopen("web_server_log","a")) == NULL) {
  	perror("Failed to open web_server_log");
  	return;
  }
  fprintf(fp, "[%d][%d][%d][%s]", log_info.thread_number,
          log_info.request_number, log_info.target_fd, log_info.m_szRequest);
  if (log_info.error != 0) {
  	fprintf(fp,"[%s]\n",log_info.error);
  } else {
  	fprintf(fp,"[%d]\n",log_info.bytes);
  }
  fclose(fp);
  pthread_mutex_unlock(&log_access);
}

char * get_format(char * filename, int len) {
  char * temp = filename;
  int i = 0;
  while (*temp != '.' && i++ < len)
    temp++;
  if (strcmp(".html", temp) == 0) {
    return "txt/html";
  } else if (strcmp(".hml", temp) == 0) {
    return "txt/html";
  } else if (strcmp(".jpg", temp) == 0) {
    return "image/jpeg";
  } else if (strcmp(".gif", temp) == 0) {
    return "image/gif";
  } else {
    return "text/plain";
  }
}

void * dispatch(void * arg) {
  while (1) {
    pthread_mutex_lock(&malloc_access);
    char * filename = malloc(MAX_REQUEST_LENGTH * sizeof(char)+1);
    pthread_mutex_unlock(&malloc_access);
    int fd = -1;
    // connect and get request
    if ((fd = accept_connection()) < 0) {
      printf("Dispatch failed to accept connections\n");
      // pthread_mutex_lock(&dispatch_cnt_lock);
      // dispatch_cnt--;
      // pthread_mutex_unlock(&dispatch_cnt_lock);
      // pthread_exit(NULL);
      continue;
    } else if (get_request(fd, filename) != 0) {
      printf("Dispatch failed to get request\n");
      continue;
    } else {
      request_queue_t request;
      request.m_socket = fd;
      strcpy(request.m_szRequest, filename);
      pthread_mutex_lock(&buffer_access);  //CRITICAL SECTION: put a request into the queue
      while (req_cnt == queue_length)
        pthread_cond_wait(&buffer_empty, &buffer_access);
      buffer_add(request);
      pthread_cond_signal(&buffer_full);
      pthread_mutex_unlock(&buffer_access);
    }
  }
  return NULL;
}

void * worker(void * arg) {
  log_info_t log_info;
  log_info.thread_number = getAnId();
  int request_cnt = 0;
  while (1) {                                   // this loop never ends
    pthread_mutex_lock(&buffer_access);         //CRITICAL SECTION: get a request from the queue
    while (req_cnt == 0) {
      if (dispatch_cnt == 0){
        pthread_exit(NULL);
      }
      pthread_cond_wait(&buffer_full, &buffer_access);
    }

    request_queue_t request =  buffer_get();
    request_cnt++;
    log_info.request_number = request_cnt;
    log_info.target_fd = request.m_socket;
    log_info.error = 0;
    strcpy(log_info.m_szRequest,request.m_szRequest);

    // concat path to target file
    char *target_path = malloc(strlen(root_path)+strlen(request.m_szRequest)+1);
    strcpy(target_path, root_path);
    strcat(target_path, request.m_szRequest);

    // access target file
    FILE *f = NULL;
    if ((f = fopen(target_path, "r")) == NULL) {
      log_info.error = strerror(errno);
      logger(log_info);
      errno = 0;
      if (0 != return_error(request.m_socket, request.m_szRequest)) {
        printf("Worker failed to return error\n");
      } else {
        printf("Worker return error to client\n");
      }
      pthread_cond_signal(&buffer_empty);
      pthread_mutex_unlock(&buffer_access);
      continue;
    }
    pthread_cond_signal(&buffer_empty);
    pthread_mutex_unlock(&buffer_access);

    char format_text[64] = "text/plain";
    char * format = format_text;
    format = get_format(request.m_szRequest, strlen(request.m_szRequest));
    // get the size of file and allocate buff_in
    fseek(f, 0, SEEK_END);                      //go to end of file
    long fsize = ftell(f);                      //save offset from start of file (size)
    char * buff_in = malloc(fsize + 1);         // allocate buff_in
    rewind(f);                                  //go back to start of file

    // read file into buff_in
    fread(buff_in, fsize, 1, f);
    fclose(f);
    log_info.bytes = fsize;
    buff_in[fsize] = 0;
    logger(log_info);
    return_result(request.m_socket, format, buff_in, fsize);
    free(buff_in);
  }
  pthread_exit(NULL);
}

int main(int argc, char **argv) {
        //Error check first.
        if(argc != 6 && argc != 7) {
          printf("usage: %s port path num_dispatcher num_workers queue_length [cache_size]\n", argv[0]);
          return -1;
        }
        printf("Call init() first and make a dispather and worker threads\n");
        int port = atoi(argv[1]);
        root_path = argv[2];
        printf("Root path is %s\n", root_path);
        num_dispatchers = min(atoi(argv[3]), MAX_THREADS);
        num_workers = min(atoi(argv[4]), MAX_THREADS);
        queue_length = min(atoi(argv[5]), MAX_REQUEST_LENGTH);
        cache_size = atoi(argv[6]);
        dispatch_cnt = num_dispatchers;

        // re-root server
        // char cwd[1024];
        // if (getcwd(cwd, sizeof(cwd)) == NULL) {
        //   printf("Failed to redirect to %s\n", path);
        // } else {
        //   fprintf(stdout, "Redirect and now current working dir: %s\n", cwd);
        // }
        // change directory to server root and initialize server
        // // initialize msg queue
        // if ((msqid = msgget(key, 0666)) == -1) {
        //   perror("msg queue initialization failed");
        //   return -1;
        // } else {
        //   printf("msg queue created\n");-
        // }


        // pthread_t * workers = malloc(num_workers * sizeof(pthread_t));
        // pthread_t * dispatchers = malloc(num_dispatchers * sizeof(pthread_t));
        chdir(root_path);
        init(port);
        pthread_t workers[MAX_THREADS];
        pthread_t dispatchers[MAX_THREADS];
        int cnt;
        // create dispatcher and worker threads
        for (cnt = 0;
             cnt < num_workers && num_workers <= MAX_THREADS;
             cnt++) {
          pthread_create(&workers[cnt], NULL, worker, NULL);
        }
        for (cnt = 0;
             cnt < num_dispatchers && num_dispatchers <= MAX_THREADS;
             cnt++) {
          pthread_create(&dispatchers[cnt], NULL, dispatch, NULL);
        }
        // join dispatch and worker thrads
        for (cnt = 0;
             cnt < num_dispatchers && num_dispatchers <= MAX_THREADS;
             cnt++) {
          if(pthread_join(dispatchers[cnt],NULL)!=0)
             fprintf(stderr,"Error joining consumer thread\n");
        }
        for (cnt = 0;
             cnt < num_workers && num_workers <= MAX_THREADS;
             cnt++) {
          if(pthread_join(workers[cnt],NULL)!=0)
             fprintf(stderr,"Error joining consumer thread\n");
        }
        return 0;
}

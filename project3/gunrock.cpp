#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
size_t BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "goober1.txt";
pthread_mutex_t gooberLock;
pthread_cond_t buffer_cond;
pthread_cond_t thread_cond;

// https://en.cppreference.com/w/cpp/container/deque
deque<MySocket*> req_buffer; // FIFO: push_back and pop_front
vector<pthread_t> thread_pool;

vector<HttpService *> services;

HttpService *find_service(HTTPRequest *request) {
   // find a service that is registered for this path prefix
  cout << "find_service called" << endl;

    // if the pathname has "..", abort
  string pathname = (*request).getPath();
  if (pathname.find("..") != string::npos) {
    return NULL;
  }
  for (unsigned int idx = 0; idx < services.size(); idx++) {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}

void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
  cout << "invoke_service_method called" << endl;
  stringstream payload;

  // invoke the service if we found one
  if (service == NULL) {
    // not found status
    response->setStatus(404);
  } else if (request->isHead()) {
    service->head(request, response);
  } else if (request->isGet()) {
    service->get(request, response);
  } else {
    // The server doesn't know about this method
    response->setStatus(501);
  }
}

void handle_request(MySocket *client) {
  cout << "handle_request called" << endl;
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;
  
  // read in the request
  bool readResult = false;
  try {
    payload << "client: " << (void *) client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  } catch (...) {
    // swallow it
  }    
    
  if (!readResult) {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }
  
  HttpService *service = find_service(request);
  if (service == NULL) goto clean_up;
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str(""); payload.clear();
  payload << " RES  PONSE " << response->getStatus() << " client: " << (void *) client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());

  clean_up: 
  delete response;
  delete request;

  payload.str(""); payload.clear();
  payload << " client: " << (void *) client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
}

void bufferThread(MyServerSocket *server, MySocket *client) {
  while (true) {
    dthread_mutex_lock(&gooberLock);
    sync_print("waiting_to_accept", "");
    while (req_buffer.size() >= BUFFER_SIZE) {
      dthread_cond_wait(&buffer_cond, &gooberLock); // allow other threads to run during this time
    }
    client = server->accept();
    sync_print("client_accepted", "");
    req_buffer.push_back(client);
    dthread_cond_broadcast(&thread_cond); // wake up all threads if they are asleep
    cout << "broadcasted thread_cond" << endl;
    dthread_mutex_unlock(&gooberLock);
  }
  return;
}

void *workerThread(void* arg) {
  // This is an individual thread, so we just need to check that there are requests to process
  // The number of available threads is inherently handled by this!
  while (true) {
    dthread_mutex_lock(&gooberLock);
    MySocket *thisClient = NULL;
    cout << "size of buffer right now: " << req_buffer.size() << endl;
    while (req_buffer.size() == 0) {
      //cout << "thread sleep" << endl;
      dthread_cond_wait(&thread_cond, &gooberLock); // wait until there are things to process
    }
    cout << "thread woke up" << endl;
    thisClient = req_buffer[0];
    req_buffer.pop_front(); // decrease size
    dthread_cond_signal(&buffer_cond); // signal that the buffer isn't full anymore
    dthread_mutex_unlock(&gooberLock);
    
    handle_request(thisClient); // thisClient is not shared among threads
  }
}

// Key modifications:
// - TODO: Make a fixed-size pool of threads
// - TODO: Schedule such that higher prio threads run first
int main(int argc, char *argv[]) {
  cout << "buffer cond addr: " << (void*) &buffer_cond << endl;
  cout << "thread cond addr: " << (void*) &thread_cond << endl;
  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
    switch (option) {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr<< "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client = NULL;

  // The order that you push services dictates the search order
  // for path prefix matching
  services.push_back(new FileService(BASEDIR));
  
  // The program should be able to take in multiple clients at a time and handle their requests.
  // instantiate request handlers
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_t newThread;
    pthread_attr_t *attr = NULL; // from docs: If attr is NULL, then the thread is created with default attributes.
    void *arg = NULL; // this is passed to start_routine

    int threadCreateErr = dthread_create(&newThread, attr, &workerThread, arg);
    if (threadCreateErr) {
      cout << "issue creating new thread" << endl;
      return 1;
    } else {
      cout << "new thread created" << endl;
    }
  }

  // manage the request buffer in the main thread
  bufferThread(server, client);
}

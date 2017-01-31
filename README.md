# Multi-thread HTTP Server
## Description
This project is a multi thread web server which is able to handing GET request from clients.
The server allows up to 200 threads in total and maintain a request queue up to 100 request in size. The server will log activity and errors.
## How to use it
The project should be unzipped into a folder containing the following items.
SourceCode/ includes all source code and makefile. testing/ contains testing resources and server log.
```
HttpServer
  - SourceCode/
    - makefile
    - server.c
    - util.h
    - util.o
  - testing/
    - image/
    - text/
    - how_to_test
    - web_server_log
    - urls
  - README.md
```
To compile the program, do the following and web_server should be generated.
```
$ cd SourceCode
$ make
```
To test the program on port 9000 with 100 worker, 100 dispatch, and 100-size request queue:
```
$ /web_server 9999 /home/<path to project directory>/csci4061-project4/testing 100 100 100 10
```
Start a different terminal window and run:
```
$ wget -i /home/<path to project directory>/testing/urls -O myres
```
The program will be tested with 4800 requests contained in /testing/urls and server response s will be stored in /testing/myres files.
Log will be saved in /csci4061-project4/testing/web_server_log

## Error handling
number of worker should be between 0 - 100   
number of dispatch should be between 0 - 100   
size of the request queue should be between 0 - 100   
server will log error when fail to return the resources    
server will be killed when receiving SIGINT, unprocessed request in request queue will be lost     
dispatch thread will exit upon failure and other thread will continue   

## Author
Trevor Lecrone   
Al Swenson   
Zixiang Ma    

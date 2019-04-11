#ifndef MESSAGE_H__
#define MESSAGE_H__

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>
#include <sys/select.h>
#include "list.h"

// struct for SBCP_Header
struct SBCP_Header {
   short Vrsn: 9;
   short Type: 7;
   short Length;
   char Payload[0];
};

// struct for SBCP_Attribute
struct SBCP_Attribute {
   short Type;
   short Length;
   char Payload[0];
};

// message type
enum Header_Type {
   JOIN = 2,
   FWD = 3,
   SEND = 4
};

// attr type
enum Attr_Type {
   Reason = 1,
   Username = 2,
   Client_Count = 3,
   Message = 4
};

#define Version 3

// default message size
#define Reason_max_size 32
#define Username_max_size 16
#define Message_max_size 512
#define Client_Count_size 2

// struct for list
typedef struct _connection_t {
   struct list_head list;
   int sock;
   struct sockaddr_in6 addr;
   size_t addrLen;
   char username[Username_max_size];
} connection_t;

#endif

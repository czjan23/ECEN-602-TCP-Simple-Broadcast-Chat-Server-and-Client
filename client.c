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
#include "utils.h"
#include "message.h"

static void print_err_and_exit(char *err_message) {
   fprintf(stdout, "Error: %s\n", err_message);
   exit(1);
}

// construct & send JOIN message
void join_server(int socket_fd, const char *username) {
   // configure attr
   int attr_len = sizeof(struct SBCP_Attribute) + strlen(username);
   struct SBCP_Attribute *attr = (struct SBCP_Attribute *)malloc(attr_len);
   attr->Type = Username;
   attr->Length = attr_len;
   memcpy(attr->Payload, username, strlen(username));

   // condigure header
   int header_len = sizeof(struct SBCP_Header) + attr_len;
   struct SBCP_Header *header = (struct SBCP_Header *)malloc(header_len);
   header->Vrsn = Version;
   header->Type = JOIN;
   header->Length = header_len;
   memcpy(header->Payload, attr, attr_len);

   // send JOIN message to server
   writen(socket_fd, header, header_len);
}

// construct & send SEND message
void send_msg(int socket_fd, const char *message) {
   // confiure attr
   int attr_len = sizeof(struct SBCP_Attribute) + strlen(message);
   struct SBCP_Attribute *attr = (struct SBCP_Attribute *)malloc(attr_len);
   attr->Type = Message;
   attr->Length = attr_len;
   memcpy(attr->Payload, message, strlen(message));

   // configure header
   int header_len = sizeof(struct SBCP_Header) + attr_len;
   struct SBCP_Header *header = (struct SBCP_Header *)malloc(header_len);
   header->Vrsn = Version;
   header->Type = SEND;
   header->Length = header_len;
   memcpy(header->Payload, attr, attr_len);

   // send message to server
   writen(socket_fd, header, header_len);
}

// resolve reason message
void reason_attr(const char *Payload, short Length) {
   if(Length <= 0 || Length > Reason_max_size) {
      printf("reason_attr Length is err %d\n", Length);
      return;
   }
   fprintf(stdout, "Received reason from server: %s\n", Payload);
}

// resolve message
void message_attr(const char *Payload, short Length) {
   if(Length <= 0 || Length > Message_max_size) {
      printf("message_attr Length is err %d\n", Length);
      return;
   }
   fprintf(stdout, "%s\n", Payload);
}

// resolve FWD message
void fwd_action(const char *Payload, short Length) {
   if(Length <= 0) {
      printf("fwd_action Length is err %d\n", Length);
      return;
   }

   struct SBCP_Attribute attr = {0};
   memcpy(&attr, Payload, sizeof(attr));

   // handle different type of message
   switch(attr.Type) {
      case Reason:
         reason_attr(Payload + sizeof(attr), attr.Length - sizeof(attr));
         break;
      case Message:
         message_attr(Payload + sizeof(attr), attr.Length - sizeof(attr));
         break;
      default:
         printf("fwd_action Type is err %d\n", attr.Type);
         break;
   }
}

// receiving message from server handler
static void socket_fd_isset(int socket_fd) {
   char buf[Message_max_size] = {0};
   memset(buf, 0, sizeof(buf));

   // read message from server
   ssize_t len = read(socket_fd, buf, Message_max_size - 1);

   if(len < 0) {
      print_err_and_exit("readline error");
   } else if(len > 0) {
      // resolve header
      struct SBCP_Header header = {0};
      memcpy(&header, buf, sizeof(header));

      // check version
      if(header.Vrsn != Version) {
         printf("socket_fd_isset Vrsn is err %d\n", header.Vrsn);
         return;
      }
      // handle FWD message
      if(header.Type == FWD) {
         fwd_action(buf + sizeof(header), header.Length - sizeof(header));
      } else {
         printf("socket_fd_isset Type is err %d\n", header.Type);
         return;
      }
   } else if(len == 0) {
      print_err_and_exit("closed");
   }
}

// receiving command line message handler
void stdin_fd_isset(int socket_fd) {
   char buf[Message_max_size] = {0};
   // read message from terminal
   char *str = fgets(buf, Message_max_size - 1, stdin);

   if(str != NULL) {
      // send message
      send_msg(socket_fd, str);
   }
}

int main(int argc, char *argv[]) {
   // check command line input format
   if(argc != 4) {
      print_err_and_exit("usage: echos username ip_address port_number\n");
   }

   fd_set readfds;
   int result;

   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if(socket_fd < 0) {
      print_err_and_exit("socket error");
   }

   struct sockaddr_in client_addr;
   client_addr.sin_family = AF_INET;
   inet_pton(AF_INET, argv[2], &client_addr.sin_addr);
   client_addr.sin_port = htons(atoi(argv[3]));

   // connect to server
   int connection_fd = connect(socket_fd, (struct sockaddr *)&client_addr, sizeof client_addr);
   if(connection_fd < 0) {
      print_err_and_exit("connect error");
   }

   fprintf(stdout, "Connecting to: %s:%s\n", argv[2], argv[3]);

   // send JOIN to server
   join_server(socket_fd, argv[1]);

   while(1) {
      FD_ZERO(&readfds);
      FD_SET(STDIN_FILENO, &readfds);
      FD_SET(socket_fd, &readfds);

      // check new message
      result = select(FD_SETSIZE, &readfds, 0, 0, NULL);

      if(result < 0) {
         if(errno != EINTR) {
            fprintf(stderr, "Error in select(): %d\n", errno);
         }
      } else if(result > 0) {
         if(FD_ISSET(STDIN_FILENO, &readfds)) { // receiving new message from terminal
            stdin_fd_isset(socket_fd);
         } else if(FD_ISSET(socket_fd, &readfds)) { // receiving message fro server
            socket_fd_isset(socket_fd);
         }
      }
   }

   close(socket_fd);
}

#include "utils.h"
#include "message.h"

static int max_conn_num = 0; //Maximum connection allowed, set by command line
static short conn_num = 0; //number of current connections
static connection_t connList_head; //head of list

// error handler
static void print_err_and_exit(char *err_message) {
   fprintf(stdout, "Error: %s\n", err_message);
   exit(1);
}

//handle new client connection
void new_connection_hanlder(int socket, struct sockaddr *addr, size_t length) {
   connection_t *connection;
   connection = (connection_t *)malloc(sizeof(connection_t));

   if(connection != NULL) {
      // add new client to list
      connection->sock = socket;
      memcpy(&(connection->addr), addr, length);
      connection->addrLen = length;
      memset(connection->username, 0, sizeof(connection->username));
      list_add_tail(&(connection->list), &(connList_head.list));
   }
}

// release list's space
void release_connection() {
   struct list_head *pos, *next;
   connection_t *connP;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);
      list_del_init(pos);
      free(connP);
      connP = NULL;
   }
}

// delete a node from list
void delete_connection_from_list(int sock) {
   struct list_head *pos, *next;
   connection_t *connP;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);
      // remove the node from list
      if(connP->sock == sock) {
         list_del_init(pos);
         free(connP);
         connP = NULL;
      }
   }
}

// find username in list
int find_username(const char *username, short Length) {
   struct list_head *pos;
   connection_t *connP;
   size_t n = 0;

   if(!list_empty(&(connList_head.list))) {
      list_for_each(pos, &(connList_head.list)) {
         connP = list_entry(pos, connection_t, list);
		 n = strlen(connP->username) > Length ? strlen(connP->username) : Length;
         // check whether the username is in the list
         if(memcmp(connP->username, username, n) == 0) {
            return 1;
         }
      }
   }

   return 0;
}

// construct and send reason message
void construct_reason_message(int socket_fd, const char *message) {
   // configure attr
   int attr_len = sizeof(struct SBCP_Attribute) + strlen(message);
   struct SBCP_Attribute *attr = (struct SBCP_Attribute *)malloc(attr_len);
   attr->Type = Reason;
   attr->Length = attr_len;
   memcpy(attr->Payload, message, strlen(message));

   // configure header
   int header_len = sizeof(struct SBCP_Header) + attr_len;
   struct SBCP_Header *header = (struct SBCP_Header *)malloc(header_len);
   header->Vrsn = Version;
   header->Type = FWD;
   header->Length = header_len;
   memcpy(header->Payload, attr, attr_len);

   // send to client
   writen(socket_fd, header, header_len);
}

// reveive username from client
void username_attr(connection_t *connP, const char *Payload, short Length) {
   if(Length <= 0 || Length > Username_max_size) {
      printf("username_attr Length is err %d\n", Length);
      return;
   }

   char reason[Reason_max_size] = {0};

   // lookup username in list
   if(find_username(Payload, Length) == 1) {
      // connection already there
      memset(reason, 0, sizeof(reason));
      sprintf(reason, "username is exist: %s\n", Payload);
      printf("%s", reason);
      construct_reason_message(connP->sock, reason);
      close(connP->sock);
      delete_connection_from_list(connP->sock);
   } else {
      printf("username is: %s\n", Payload);

      // check whether the current number of connections reaches maximum connection
      if(conn_num >= max_conn_num) {
         memset(reason, 0, sizeof(reason));
         sprintf(reason, "Too many connections %d\n", conn_num);
         construct_reason_message(connP->sock, reason);
         close(connP->sock);
         delete_connection_from_list(connP->sock);
      } else {
		conn_num++;
	  }

	  printf("Client Count is: %d\n", conn_num);

      // add new username to list
      memcpy(connP->username, Payload, Length);
   }
}

// broacast the usernmae list
void broadcast_username() {
   struct list_head *pos;
   connection_t *connP;
   char usr_list[Message_max_size] = {0};

   if(!list_empty(&(connList_head.list))) {
      // get all usernames from list
      list_for_each(pos, &(connList_head.list)) {
         connP = list_entry(pos, connection_t, list);
         if(strlen(connP->username) != 0) {
            if(strlen(usr_list) == 0) {
               sprintf(usr_list, "user list is: %s", connP->username);
            } else {
               sprintf(usr_list + strlen(usr_list), ",%s", connP->username);
            }
         }
      }
   }

   int usr_list_len = strlen(usr_list);

   // configure attr
   int attr_len = sizeof(struct SBCP_Attribute) + usr_list_len;
   struct SBCP_Attribute *attr = (struct SBCP_Attribute *)malloc(attr_len);
   attr->Type = Message;
   attr->Length = attr_len;
   memcpy(attr->Payload, usr_list, usr_list_len);

   // configure header
   int header_len = sizeof(struct SBCP_Header) + attr_len;
   struct SBCP_Header *header = (struct SBCP_Header *)malloc(header_len);
   header->Vrsn = Version;
   header->Type = FWD;
   header->Length = header_len;
   memcpy(header->Payload, attr, attr_len);

   if(!list_empty(&(connList_head.list))) {
      list_for_each(pos, &(connList_head.list)) {
         connP = list_entry(pos, connection_t, list);
         if(strlen(connP->username) != 0) {
            // send user lists to connected clients
            writen(connP->sock, header, header_len);
         }
      }
   }
}

// resolve JOIN message
void join_action(connection_t *connP, const char *Payload, short Length) {
   if(Length <= 0 || Length > Username_max_size + sizeof(struct SBCP_Attribute)) {
      printf("join_action Length is err %d\n", Length);
      return;
   }

   // configure attr
   struct SBCP_Attribute attr = {0};

   memcpy(&attr, Payload, sizeof(attr));

   if(attr.Type != Username) {
      printf("join_action Type is err %d\n", attr.Type);
      return;
   } else {
      // resolve username message
      username_attr(connP, Payload + sizeof(attr), attr.Length - sizeof(attr));
   }

   usleep(100);
   // broadcast username list
   broadcast_username();
}

// boradcase message to all connected clients
void broadcast_msg(connection_t *c, const char *Payload, short Length) {
   // add usernmae to message header
   char msg_header[Username_max_size + 3] = {0};
   sprintf(msg_header, "%s: ", c->username);
   int msg_header_len = strlen(msg_header);

   // configure attr
   int attr_len = sizeof(struct SBCP_Attribute) + msg_header_len + Length;
   struct SBCP_Attribute *attr = (struct SBCP_Attribute *)malloc(attr_len);
   attr->Type = Message;
   attr->Length = attr_len;
   memcpy(attr->Payload, msg_header, msg_header_len);
   memcpy(attr->Payload + msg_header_len, Payload, Length);

   // configure header
   int header_len = sizeof(struct SBCP_Header) + attr_len;
   struct SBCP_Header *header = (struct SBCP_Header *)malloc(header_len);
   header->Vrsn = Version;
   header->Type = FWD;
   header->Length = header_len;
   memcpy(header->Payload, attr, attr_len);

   struct list_head *pos;
   connection_t *connP;

   if(!list_empty(&(connList_head.list))) {
      list_for_each(pos, &(connList_head.list)) {
         connP = list_entry(pos, connection_t, list);

		 if(connP == c)
		 {
			continue;
		 }

         if(strlen(connP->username) != 0) {
            // boradcase message to all connected clients
            writen(connP->sock, header, header_len);
         }
      }
   }
}

// resolve message
static void message_attr(connection_t *connP, const char *Payload, short Length) {
   if(Length <= 0 || Length > Message_max_size) {
      printf("message_attr Length is err %d\n", Length);
      return;
   }
   fprintf(stdout, "%s\n", Payload);
   // boradcase message to all connected clients
   broadcast_msg(connP, Payload, Length);
}

// resolve SEND message
void send_action(connection_t *connP, const char *Payload, short Length) {
   if(Length <= 0 || Length > Message_max_size + sizeof(struct SBCP_Attribute)) {
      printf("send_action Length is err %d\n", Length);
      return;
   }

   // resolve attr
   struct SBCP_Attribute attr = {0};
   memcpy(&attr, Payload, sizeof(attr));
   if(attr.Type != Message) {
      printf("send_action Type is err %d\n", attr.Type);
      return;
   } else {
      // resolve message
      message_attr(connP, Payload + sizeof(attr), attr.Length - sizeof(attr));
   }
}

void fd_set_all(fd_set *readfds) {
   struct list_head *pos;
   connection_t *connP;
   if(!list_empty(&(connList_head.list))) {
      list_for_each(pos, &(connList_head.list)) {
         connP = list_entry(pos, connection_t, list);
         if(connP->sock > 0) {
            FD_SET(connP->sock, readfds);
         }
      }
   }
}

// receiving message hanlder
void connect_fd_isset(fd_set *readfds) {
   struct list_head *pos, *next;
   connection_t *connP;
   char buffer[Message_max_size] = {0};
   int numBytes = 0;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);

      memset(buffer, 0, sizeof(buffer));
      numBytes = 0;

      if(FD_ISSET(connP->sock, readfds)) {
         // read client's message
         numBytes = read(connP->sock, buffer, Message_max_size - 1);

         if(numBytes > 0) {
            buffer[numBytes] = 0;
            struct SBCP_Header header = {0};
            memcpy(&header, buffer, sizeof(header));

            if(header.Vrsn != Version) {
               // check version
               printf("connect_fd_isset Vrsn is err %d\n", header.Vrsn);
            } else {
               // check header message type
               switch(header.Type) {
                  case JOIN: //JOIN
                     join_action(connP, buffer + sizeof(header), header.Length - sizeof(header));
                     break;
                  case SEND: //SEND
                     send_action(connP, buffer + sizeof(header), header.Length - sizeof(header));
                     break;
                  default:
                     printf("connect_fd_isset Type is err %d\n", header.Type);
                     break;
               }
            }
         } else if(numBytes == 0) {
            // client disconnect
            close(connP->sock);

            char s[INET6_ADDRSTRLEN] = {0};
            s[0] = 0;
            struct sockaddr_in *saddr = (struct sockaddr_in *)&connP->addr;
            inet_ntop(AF_INET, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
            fprintf(stdout, "disconnect from [%d][%s][%s]\n", connP->sock, connP->username, s);

            // remove this connection from list
            list_del_init(pos);
            free(connP);
            connP = NULL;
            conn_num--;
            break;
         }
      }
   }
}

// receiving new connecion
static void socket_fd_isset(int socket_fd) {
   struct sockaddr_in connection_addr = {0};
   socklen_t connection_addr_len = INET6_ADDRSTRLEN;

   // accept connection
   int connection_fd = accept(socket_fd, (struct sockaddr *)&connection_addr, &connection_addr_len);
   if(connection_fd < 0) {
      print_err_and_exit("accept error!");
   }

   char s[INET6_ADDRSTRLEN] = {0};
   in_port_t port = 0;

   s[0] = 0;
   struct sockaddr_in *saddr = (struct sockaddr_in *)&connection_addr;
   inet_ntop(AF_INET, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
   port = saddr->sin_port;

   fprintf(stdout, "accept from [%d]-[%s]:%hu\n", connection_fd, s, ntohs(port));

   // add new username to list
   new_connection_hanlder(connection_fd, (struct sockaddr *)&connection_addr, connection_addr_len);
}

int main(int argc, char **argv) {
   // check parameters
   if(argc != 4) {
      print_err_and_exit("usage: echos server_ip server_port max_clients!");
   }

   fd_set readfds;
   int result;

   // init list
   INIT_LIST_HEAD(&(connList_head.list));

   // set maximum connection
   max_conn_num = atoi(argv[3]);

   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if(socket_fd < 0) {
      print_err_and_exit("socket error!");
   }

   // init father socket sockaddr_in
   struct sockaddr_in server_addr;
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(atoi(argv[2]));
   inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

   // bind socket
   if(bind(socket_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
      print_err_and_exit("bind error!");
   }

   // scoket start listening
   if(listen(socket_fd, max_conn_num) < 0) {
      print_err_and_exit("listen error!");
   }

   while(1) {
      // confiure monitor
      FD_ZERO(&readfds);
      FD_SET(socket_fd, &readfds);
      fd_set_all(&readfds);

      // check new message
      result = select(FD_SETSIZE, &readfds, 0, 0, NULL);

      if(result < 0) {
         if(errno != EINTR) {
            fprintf(stderr, "Error in select(): %d\n", errno);
         }
      } else if(result > 0) {
         // handle new message
         if(FD_ISSET(socket_fd, &readfds)) {
            socket_fd_isset(socket_fd);
         } else {
            connect_fd_isset(&readfds);
         }
      }
   }

   close(socket_fd);
   release_connection();
}

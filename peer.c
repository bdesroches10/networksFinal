/* peer.c */

#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*----------------------------------------------------------------------
 * Peer Client for P2P File Download
 *----------------------------------------------------------------------
 */
 
/* Protocol Data Unit */
typedef struct{
  char type;
  char data[100];
}pdu;

/* PDU for Content Download */
typedef struct{
  char type;
  char data[1024];
}cpdu;

/* TCP sd's LinkedList */
struct node{
  char pname[10];
  char cname[10];
  int sd;
  struct node *next;
};

struct node *head = NULL;
struct node *current = NULL;

void insert(int, char*, char*);
struct node* delete(int);
int findSD(char*, char*);

int tcpSock(char*, char*);
void sendFile(int, char*);

int main(int argc, char**argv){

  char* host;
  int port;
  struct hostent *phe;
  struct sockaddr_in usin, my_addr;
  int uslen = sizeof(usin);
  int s, type, nSD;
  
  fd_set rfds;
  struct node *ptr;
  
  pdu spdu;
  pdu * rpdu = malloc(sizeof(pdu));
  
  char buf[50] = {0};
  char pname[10];
  char cname[10]; 
  char nHost[16];
  char mHost[16];
  int prt;

  switch(argc){
    case 3:
      host = argv[1];
      port = atoi(argv[2]);
      break;
    default:
      fprintf(stderr, "Usage: %s [address] [port]\n", argv[0]);
      exit(1); 
  }

  memset(&usin, 0, sizeof(usin));
  usin.sin_family = AF_INET;
  usin.sin_port = htons(port);
  
  /* Map host name to IP address */
  if(phe = gethostbyname(host)){
    memcpy(&usin.sin_addr, phe->h_addr, phe->h_length);
  }
  else if((usin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
    fprintf(stderr, "Can't get host entry \n");
  
  /* Allocate Socket */
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s < 0)
    fprintf(stderr, "Can't create socket \n");
  
  /* Connect Socket */
  if(connect(s, (struct sockaddr *)&usin, sizeof(usin)) < 0)
    fprintf(stderr, "Can't connect to %s \n", host);
    
  socklen_t len = sizeof(my_addr);
  getsockname(s, (struct sockaddr *) &my_addr, &len);
  inet_ntop(AF_INET, &my_addr.sin_addr, mHost, sizeof(mHost));
    
  printf("\nOptions: Register, Download, List, Deregister, Quit.\nPlease enter your action: \n");
  
  /* Peer Requests */
  while(1){
    /* Set Up FD Set */
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    ptr = head;
    while(ptr != NULL){
      /* Add Open TCP SDs */
      FD_SET(ptr->sd, &rfds);
      ptr = ptr->next;
    }

    /* Check for I/O on the SDs */
    select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    
    /* If I/O is on Terminal */
    if(FD_ISSET(0, &rfds)){
      char action[15];
      char lower_action[15];
      
      scanf("%s", action);
    
      /* Ignore Casing */
      for(size_t i = 0; i < sizeof(action); i++){
        char low = tolower((unsigned char) action[i]);
        lower_action[i] = low;
      }
  
      if(strcmp(lower_action, "register") == 0){ //REGISTER
        spdu.type = 'R';
      
        printf("Enter the name of the content to register: ");
        scanf("%s", buf);
      
        while(buf[9] != '\0'){
          printf("%s\n", buf);
          printf("File name too long. Must be max. 9 characters including file extension.\n Please try again.\n");
          bzero(buf, 50);
          scanf("%s", buf);
        }
        strncpy(cname, buf, 10);
        bzero(buf, 50);
      
        /*Check that this peer has this data to share */
        FILE *fp;
        if((fp= fopen(cname, "r")) == NULL){
          fclose(fp);
          fprintf(stderr, "Content not found.\n");
        }else{  
          fclose(fp);
      
          /* Get peer name */
          printf("Enter peer name: ");
          scanf("%s", buf);
          strncpy(pname, buf, 9);
          bzero(buf, 50);
          
          prt = tcpSock(pname, cname);
        
          /* Write to UDP Socket */
          snprintf(spdu.data, sizeof(spdu.data), "%s:%s:%s:%d", pname, cname, mHost, prt);
          printf("Registering %s for peer %s\n", cname, pname);
      
          (void) sendto(s, &spdu, sizeof(spdu.data)+1, 0, (struct sockaddr *)&usin, sizeof(usin));
      
          if(recvfrom(s, rpdu, sizeof(*rpdu), 0, (struct sockaddr *)&usin, &uslen) < 0)
            fprintf(stderr, "recvfrom error.\n");
      
          printf("%s\n", rpdu->data);
        }
      }else if(strcmp(lower_action, "download") == 0){ //DOWNLOAD
        printf("What content do you need to download?\n");
        scanf("%s", buf);
        strncpy(cname, buf, 10);
        bzero(buf, 50);
        
        /* Check if this content is already downloaded. */
        FILE *fp;
        if((fp= fopen(cname, "r")) != NULL){
          fclose(fp);
          fprintf(stderr, "Content already downloaded.\n");
        }else{  
        
          /* Get peer name */
          printf("Enter peer name: ");
          scanf("%s", buf);
          strncpy(pname, buf, 9);
          bzero(buf, 50);
        
          spdu.type = 'S';
          snprintf(spdu.data, sizeof(spdu.data), "%s:%s", pname, cname);
          (void) sendto(s, &spdu, sizeof(spdu.data)+1, 0, (struct sockaddr *)&usin, sizeof(usin));
          
          if(recvfrom(s, rpdu, sizeof(*rpdu), 0, (struct sockaddr *)&usin, &uslen) < 0)
            fprintf(stderr, "recvfrom error.\n");
            
          if(rpdu->type == 'E')
            printf("%s\n", rpdu->data);
          else{
            strcpy(buf, strtok(rpdu->data, ":"));
            strcpy(nHost, strtok(NULL, ":"));
            prt = atoi(strtok(NULL, ":"));
            
            printf("Downloading from %s at %s:%d\n", buf, nHost, prt);  
            bzero(buf, 50);
            
            int sd;
            struct hostent *hp;
            struct sockaddr_in server;
            
            /* Create Client Socket */
            if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
              fprintf(stderr, "Cannot create socket.\n");
            }
            
            bzero((char *)&server, sizeof(struct sockaddr_in));
            server.sin_family = AF_INET;
            server.sin_port = htons(prt);
            
            if(hp = gethostbyname(nHost)){
              bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
            }else if(inet_aton(host, (struct in_addr *)&server.sin_addr)){
              fprintf(stderr, "Can't get Servers Address.\n");
            }
            
            /* Connect to Server */
            if(connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1){
              fprintf(stderr, "Can't connect.\n");
            }else{
              spdu.type = 'D';
              bzero(spdu.data, 100);
              snprintf(spdu.data, sizeof(spdu.data), "%s:%s", pname, cname);
              if(send(sd, &spdu, sizeof(spdu.data)+1, 0) == -1)
                fprintf(stderr, "Message Cannot be Sent.\n");
              else{
                cpdu * dpdu = malloc(sizeof(cpdu));
                FILE *fp = fopen(cname, "w");
                while(recv(sd, dpdu, sizeof(dpdu->data)+1, 0) > 0){
                  fprintf(fp, "%s", dpdu->data);
                }
                fclose(fp);
                printf("Content Downloaded\n");
              }             
            } 
            close(sd); 
         
            FILE *cfp;
            if((cfp= fopen(cname, "r")) != NULL){
              fclose(cfp);
              prt = tcpSock(pname, cname);  
            
              /* Register Downloaded Content */
              spdu.type = 'R'; 
              snprintf(spdu.data, sizeof(spdu.data), "%s:%s:%s:%d", pname, cname, mHost, prt);
              printf("Registering %s for peer %s\n", cname, pname);
            
              (void) sendto(s, &spdu, sizeof(spdu.data)+1, 0, (struct sockaddr *)&usin, sizeof(usin));
      
              if(recvfrom(s, rpdu, sizeof(*rpdu), 0, (struct sockaddr *)&usin, &uslen) < 0)
                fprintf(stderr, "recvfrom error.\n");
      
              if(rpdu->type == 'E'){
                int dsd = findSD(pname, cname);
                close(dsd);
                delete(dsd);
              }
              printf("%s\n", rpdu->data);  
            }   
          }
        }
      
      }else if(strcmp(lower_action, "list") == 0){ //LIST
        spdu.type = 'O';
        strcpy(spdu.data, "List Options."); // data field doesn't matter here.
        (void) sendto(s, &spdu, sizeof(spdu.data)+1, 0, (struct sockaddr *)&usin, sizeof(usin));
        
        while(recvfrom(s, rpdu, sizeof(*rpdu), 0, (struct sockaddr *)&usin, &uslen) > 0){
          if(rpdu->type == 'E'){
            printf("%s\n", rpdu->data);
            break;
          }        
          strcpy(pname, strtok(rpdu->data, ":"));
          strcpy(cname, strtok(NULL, ":"));
          printf("Peer: %s, Content: %s\n", pname, cname);
          if(rpdu->type == 'F')
            break;
        }
        
      }else if(strcmp(lower_action, "deregister") == 0){ //DEREGISTER
        int dsd;
        spdu.type = 'T';
        
        /* Get content title */
        printf("What is the content you want to deregister: ");
        scanf("%s", buf);
        strncpy(cname, buf, 10);
        bzero(buf, 50);
        
        /* Get peer name */
        printf("Enter peer name content is registered with: ");
        scanf("%s", buf);
        strncpy(pname, buf, 9);
        bzero(buf, 50);
        
        /* Delete Entry After Finding SD */
        dsd = findSD(pname, cname);
        if((dsd) == -1)
          printf("No such pair exists.\n");
        else{
          printf("Deleting entry %s of peer %s\n", cname, pname);
          snprintf(spdu.data, sizeof(spdu.data), "%s:%s", pname, cname);
          (void) sendto(s, &spdu, sizeof(spdu.data)+1, 0, (struct sockaddr *)&usin, sizeof(usin));
          
          if(recvfrom(s, rpdu, sizeof(*rpdu), 0, (struct sockaddr *)&usin, &uslen) < 0)
            fprintf(stderr, "recvfrom error.\n");
          
          if(rpdu->type == 'E')
            printf("%s\n", rpdu->data);
          else{
            close(dsd);
            delete(dsd);
            printf("%s\n", rpdu->data);
          }
        }
      
      }else if(strcmp(lower_action, "quit") == 0){ //QUIT
        spdu.type = 'T';
        ptr = head;
        struct node *previous = NULL;
        
        if(head = NULL){
          exit(0);
        }
        
        /* Deregister and Close All Servers */
        while(ptr != NULL){
          previous = ptr;
          ptr = ptr->next;
          
          printf("Deleting entry %s of peer %s\n", previous->cname, previous->pname);
          snprintf(spdu.data, sizeof(spdu.data), "%s:%s", previous->pname, previous->cname);
          (void) sendto(s, &spdu, sizeof(spdu.data)+1, 0, (struct sockaddr *)&usin, sizeof(usin));
          
          if(recvfrom(s, rpdu, sizeof(*rpdu), 0, (struct sockaddr *)&usin, &uslen) < 0)
            fprintf(stderr, "recvfrom error.\n");
          printf("%s\n", rpdu->data);
          
          close(previous->sd);
          delete(previous->sd);         
        }
        printf("All sockets closed. Exiting program.\n");
        close(s);
        exit(0);
      
      }else{
        printf("Invalid Action. Please try again.\n");     
      } 
      
      bzero(pname, 10);
      bzero(cname, 10);
      bzero(spdu.data, 100);
      printf("\nPlease enter your action: \n");
      
    }else{ //TCP INCOMING
      ptr = head;
      while(ptr != NULL){
        if(FD_ISSET(ptr->sd, &rfds)){
          nSD = ptr->sd;
          break;
        }
      } 
      
      /* Accept TCP Client */
      struct sockaddr_in client;
      int client_len = sizeof(client);
      int new_sd = accept(nSD, (struct sockaddr *)&client, &client_len);
     
      if(new_sd < 0){
        fprintf(stderr, "Can't accept client.\n");
      }else{
        if(recv(new_sd, rpdu, sizeof(*rpdu), 0) < 0)
          fprintf(stderr, "No message received.\n");
        else{
          strcpy(pname, strtok(rpdu->data, ":"));
          strcpy(cname, strtok(NULL, ":"));
          sendFile(new_sd, cname);
        }
        close(new_sd);
      }
    }  
  }
}

/* Create TCP Sockets */
int tcpSock(char* peer, char* content){
  /* TCP Socket for Content */
  struct sockaddr_in reg_addr;
  int sock, port;
  
  /* Create TCP Content Server */
  sock = socket(AF_INET, SOCK_STREAM, 0);
  reg_addr.sin_family = AF_INET;
  reg_addr.sin_port = htons(0); //Let TCP module choose port
  reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(sock, (struct sockaddr *)&reg_addr, sizeof(reg_addr));
  listen(sock, 5);
      
  socklen_t alen = sizeof(reg_addr);
  getsockname(sock, (struct sockaddr *)&reg_addr, &alen);
  port = ntohs(reg_addr.sin_port); 
  
  /* Add Server to Linked List */ 
  insert(sock, peer, content);
  
  return port;
}

/* Insert to List */
void insert(int sd, char* pname, char* cname){
  struct node *temp = (struct node*) malloc(sizeof(struct node));
  
  temp->sd = sd;
  strcpy(temp->pname, pname);
  strcpy(temp->cname, cname);
  
  temp->next = head;
  head = temp;
}

/* Delete from List */
struct node* delete(int sd){
  struct node* current = head;
  struct node* previous = NULL;
  
  if(head == NULL)
    return NULL;
  
  while(current->sd != sd){
    if(current->next == NULL)
      return NULL;
    else{
      previous = current;
      current = current->next;
    } 
  }
  
  if(current == head)
    head = head->next;
  else
    previous->next = current->next;
  
  return current;
}

/* Find Socket Descriptor to Close Socket */
int findSD(char* pname, char* cname){
  struct node* current = head;
  
  if(head == NULL){
    printf("No Content Registered.\n");
    return -1;
  }
  
  while(current != NULL){
    if((strcmp(current->cname, cname) == 0) && (strcmp(current->pname, pname) == 0)){
       return current->sd;   
    }
    current = current->next;
  }
  return -1;  
}

/* Send File to Request Client */
void sendFile(int sd, char* content){
  cpdu fpdu;
  FILE *fp = fopen(content, "r");
  
  if(fp == NULL){
    fprintf(stderr, "Cannot Open File.\n");
    fpdu.type = 'E';
    strcpy(fpdu.data, "Error: Cannot open this file.\n");
    if(send(sd, &fpdu, sizeof(fpdu.data)+1, 0) == -1)
      fprintf(stderr, "Message cannot be sent.\n");
  }else{
    fpdu.type = 'C';
    while(fgets(fpdu.data, sizeof(fpdu.data), fp) != NULL){
      if(send(sd, &fpdu, sizeof(fpdu.data)+1, 0) == -1)
        fprintf(stderr, "Message cannot be sent.\n");
    }
    fclose(fp);
  }
}






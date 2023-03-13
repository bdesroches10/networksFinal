/* index_server.c */

#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/*--------------------------------------------------------------------
 * Index Server for P2P File Download Service
 *--------------------------------------------------------------------
 */

/* Protocol Data Unit */
typedef struct{
  char type;		/* Types: R, D, S, T, C, O, A, E */
  char data[100];
}pdu;

/* Registration Info */
typedef struct{
  char peername[10];
  char contentname[10];
  char address[20];
  int port;
  int useCount;
}entry;

/* Registered Content Servers LinkedList */
struct node{
  entry * cServer;
  int key;
  struct node *next;
};

struct node *head = NULL;
struct node *current = NULL;

/* Linked List Functions */
void insert(char*, char*, char*, int);
struct node* delete(int);
int search(char*, char*);
struct node* getServ(char*);

int main(int argc, char *argv[]){

  struct sockaddr_in sin, psin;
  int plen = sizeof(psin);
  int s, type, port;
  
  pdu rpdu;
  pdu* ipdu = malloc(sizeof(pdu));
  char ackMsg[14] = "Acknowledged.";

  char pname[10];
  char cname[10];
  char addr[16];
  int prt;
  
  switch(argc){
    case 2:
      port = atoi(argv[1]);
      break;
    default:
      fprintf(stderr, "Usage: %s [port]\n", argv[0]);
      exit(1);
  }
  
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  
  /* Allocate the socket */
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s < 0)
    fprintf(stderr, "Can't create socket.\n");
    
  /* Bind the socket */
  if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    fprintf(stderr, "Can't bind socket.\n");
  
  listen(s, 5);
  
  /* Service to Peers */
  while(1){
  
    /* Receive Peer Request */
    if(recvfrom(s, ipdu, sizeof(*ipdu), 0, (struct sockaddr *)&psin, &plen) <0)
      fprintf(stderr, "recvfrom error.\n");
  
    /*Filter request by pdu type */
    if(ipdu->type == 'R'){
      /* Registration - Data: PeerName(10), ContentName(10), Address(80) */    
      strcpy(pname, strtok(ipdu->data, ":"));
      strcpy(cname, strtok(NULL, ":"));
      strcpy(addr, strtok(NULL, ":"));
      prt = atoi(strtok(NULL, ":"));
    
      /*Check if this content is already registered */
      if(search(pname, cname) > -1){
        /*Content is already registered, reply with error */
        rpdu.type = 'E';
        strcpy(rpdu.data, "Error: Content already registered from this peer.");
        (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
          (struct sockaddr *)&psin, sizeof(psin));
      }else{
        /*Content not registered under peer, add it */
        insert(pname, cname, addr, prt);
        rpdu.type = 'A';
        strcpy(rpdu.data, "Registered.");
        (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
          (struct sockaddr *)&psin, sizeof(psin));
      }
    
    }else if(ipdu->type == 'S'){
      /* Search - Data: PeerName(10), ContentName(10) */
      strcpy(pname, strtok(ipdu->data, ":"));
      strcpy(cname, strtok(NULL, ":"));
      
      struct node* info = (struct node*) malloc(sizeof(struct node));
      
      if((info = getServ(cname)) == NULL){
        /* No Entry Found to Service Request */
        rpdu.type = 'E';
        strcpy(rpdu.data, "Error: Cannot find content to service this request.");
        (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
          (struct sockaddr *)&psin, sizeof(psin));
      }else{
        /* Content Found, Send Info to Client Peer */
        rpdu.type = 'S';
        strcpy(pname, info->cServer->peername);
        strcpy(addr, info->cServer->address);
        prt = info->cServer->port;  
        snprintf(rpdu.data, sizeof(rpdu.data), "%s:%s:%d", pname, addr, prt);   
        
        (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
          (struct sockaddr *)&psin, sizeof(psin));
      }
    
    }else if(ipdu->type == 'T'){
      /* Deregister - Data: PeerName(10), ContentName(10) */
      strcpy(pname, strtok(ipdu->data, ":"));
      strcpy(cname, strtok(NULL, ":"));
      
      int key = search(pname, cname); 
      
      if(key > -1){
        /* Key Found for Content */
        delete(key);
        rpdu.type = 'A';
        strcpy(rpdu.data, "Content Deleted.");
        (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
          (struct sockaddr *)&psin, sizeof(psin)); 
      }else{
        /* Key Not Found for Content */
        rpdu.type = 'E';
        strcpy(rpdu.data, "Error: Content not found in Index Server.");
        (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
          (struct sockaddr *)&psin, sizeof(psin));
      }  
              
    }else{ 
      /* List */
      rpdu.type = 'O';
      struct node *ptr = head;
      
      if(head == NULL){
        /* If Head is Null, No Servers are Registered */
        rpdu.type = 'E';
        strcpy(rpdu.data, "No servers currently registered.");
        (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
          (struct sockaddr *)&psin, sizeof(psin));
      }else{
        /* Loop Through and Return Registered Servers */
        while(ptr != NULL){
          if(ptr->next == NULL){
            rpdu.type = 'F';
          }
          snprintf(rpdu.data, sizeof(rpdu.data), "%s:%s", ptr->cServer->peername, ptr->cServer->contentname); 
          (void) sendto(s, &rpdu, sizeof(rpdu)+1, 0,
            (struct sockaddr *)&psin, sizeof(psin));
          ptr = ptr->next;
        }  
      }        
    }

    /* Clear Commonly Used Variables */
    bzero(pname, 10);
    bzero(cname, 10);
    bzero(rpdu.data, 100);
  }
}

/* Insert to List */
void insert(char* pname, char* cname, char* addr, int prt){
  struct node *temp = (struct node*) malloc(sizeof(struct node));
  entry *tempServer = malloc(sizeof(entry));
  
  if(head == NULL)
    temp->key = 1;
  else
    temp->key = head->key + 1;
  
  strcpy(tempServer->peername, pname);
  strcpy(tempServer->contentname, cname);
  strcpy(tempServer->address, addr);
  tempServer->port = prt;
  tempServer->useCount = 0;
  
  temp->cServer = tempServer;
  temp->next = head;
  head = temp;
  printf("done");
}

/* Delete from List */
struct node* delete(int key){
  struct node* current = head;
  struct node* previous = NULL;
  
  if(head == NULL)
    return NULL;
  
  while(current->key != key){
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

/* Search if Peer has Already Registered this Content */
int search(char* peer, char*content){
  
  struct node* current = head;
  
  while(current != NULL){
    if(strcmp(current->cServer->contentname, content) == 0){
      if(strcmp(current->cServer->peername, peer) == 0){
        return current->key;
      }
    } 
    current = current->next;
  }
  return -1;
}

/* Get Content Server for Requesting Client */
struct node* getServ(char* content){
  int minEntry = INT_MAX;
  struct node* lowOp = NULL;
  struct node* current = head;
  
  while(current != NULL){
    if(strcmp(current->cServer->contentname, content) == 0){
      if(current->cServer->useCount < minEntry){
        minEntry = current->cServer->useCount;
        lowOp = current;
      }
    } 
    current = current->next;
  }
  
  //increase num of uses
  if(lowOp != NULL){
    current = head; 
    while(current != NULL){
      if(lowOp->key == current->key){
        current->cServer->useCount++;
        break;
      }
      current = current->next;
    }
  } 
  return lowOp;
}






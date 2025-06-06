/* 
 * stockserver.c - Thread-based concurrent stock server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

#define MAX_STOCK_NUM 128
#define MAX_CLIENTS 100

struct stock{ 
	int id;
	int amount;
	int price;
	sem_t mutex;
};

struct stock* tree[MAX_STOCK_NUM];		// tree that saves stock structs
int stock_num = 0;

void echo(int connfd);
void sigint_handler(int sig);

int left(int i) {return 2*i + 1;}
int right(int i) {return 2*i + 2;}

void insert_tree(struct stock* stock);
void print_tree(char* buf);
struct stock* search_tree(int id);

void stock_load(const char* filename);
void stock_save(const char* filename);

void process_request(int connfd, char* request);
void* thread(void* vargp);

void stock_load(const char* filename){
	stock_num = 0;
	FILE* fp = fopen(filename, "r");
	if (!fp){
		fprintf(stderr, "Error: Failed to open file: %s\n", filename);
		return;
	}
	
	// file read loop until EOF
	int id, amount, price;
	while (fscanf(fp, "%d %d %d\n", &id, &amount, &price) != EOF){
		struct stock* tmp = Malloc(sizeof(struct stock));
		tmp->id = id;
		tmp->amount = amount;
		tmp->price = price;
		Sem_init(&tmp->mutex, 0, 1);
		insert_tree(tmp);
	} 
	fclose(fp);
	return;
}

void stock_save(const char* filename){
	FILE* fp = fopen(filename, "w");
	if (!fp){
		fprintf(stderr, "Error: Failed to open file: %s\n", filename);
		return;
	}
	for (int i = 0; i < MAX_STOCK_NUM; i++){
		if(tree[i])
		fprintf(fp, "%d %d %d\n", tree[i]->id, tree[i]->amount, tree[i]->price);
	}	
	fclose(fp);
	return;
}

// inserts new stock struct to tree and updates stock_num
void insert_tree(struct stock* stock){
	int index = 0;

	if (stock_num >= MAX_STOCK_NUM){
		fprintf(stderr, "Error: Tree full\n");
		return;
	}
	
	while (index < MAX_STOCK_NUM){
		if (!tree[index]){
			tree[index] = stock;
			stock_num++;
			return;
		}
		if (stock->id < tree[index]->id){
			index = left(index);
		}
		else index = right(index);
		if (index > MAX_STOCK_NUM){
			fprintf(stderr, "Error: Tree full\n");
			return;
		}
	}
}

// print to buf instead of stdout directly (for multi-thread safety)
void print_tree(char* buf){
	static char stock_info[64];
	buf[0] = '\0';
	
	for (int i = 0; i < MAX_STOCK_NUM; i++){
		if (tree[i]){
		sprintf(stock_info, "%d %d %d\n", tree[i]->id, tree[i]->amount, tree[i]->price);
		strcat(buf, stock_info);
		}
	}
	return;
}

// searches tree by id. Returns NULL if not found
struct stock* search_tree(int id){
	int index = 0;
	
	if (stock_num == 0) return NULL;
	while(index < MAX_STOCK_NUM){
		if (!tree[index]) return NULL;
		if (tree[index]->id == id)
			return tree[index];
		else if (id < tree[index]->id){
			index = left(index);
		}
		else index = right(index);
		if (index >= MAX_STOCK_NUM)
			return NULL;
	}
	return NULL;	// shouldn't be here
}

// When receiving SIGINT(Ctrl-C), saves tree to file and exits.
void sigint_handler(int sig){
	stock_save("stock.txt");
	for (int i = 0; i < MAX_STOCK_NUM; i++){
		if (tree[i]) Free(tree[i]);
	}
	printf("Saved to stock.txt and exitting\n");
	exit(0);
}

void* thread(void* vargp){
	int connfd = *((int *)vargp);	// cast arg ptr as connfd to store connfd on thread's stack
	char buf[MAXLINE] = { '\0' };
	ssize_t n;
	rio_t rio;
	
	free(vargp);	// free allocated memory on heap
	
	Pthread_detach(Pthread_self());	// detach thread
	Rio_readinitb(&rio, connfd);	// init rio buffer for reading client request	

	while ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0){
		process_request(connfd, buf);		// if read from rio buffer, process request
		Rio_writen(connfd, buf, MAXLINE);	// write result to connfd (client-side fd)
	}
	// If control reaches here, connection has been closed

	Close(connfd);
	return NULL;
}

void process_request(int connfd, char* request){
	char response[MAXLINE];
	int id, amount;

	printf("Server received %zu bytes\n", strlen(request));
	if (strncmp(request, "show", 4) == 0){
		request[0] = '\0';
		print_tree(response);
		strcpy(request, response);
		//printf("handled show\n");
		return;
	}
	else if (strncmp(request, "buy", 3) == 0 && sscanf(request, "%*s %d %d\n", &id, &amount) == 2){
		struct stock* stock = search_tree(id);

		if (stock){
			P(&stock->mutex);		// critical section (handling shared resource)
			if (amount <= stock->amount){
				stock->amount -= amount;
				strcpy(response, "[buy] success\n");
			}
			else strcpy(response, "Not enough left stock\n");
			V(&stock->mutex);
			strcpy(request, response);
		//printf("handled buy\n");
		}
		return;
	}
	else if (strncmp(request, "sell", 4) == 0 && sscanf(request, "%*s %d %d\n", &id, &amount) == 2){
		struct stock* stock = search_tree(id);

		if (stock){
			P(&stock->mutex);		// critical section (handling shared resource)
			stock->amount += amount;
			strcpy(response, "[sell] success\n");
			V(&stock->mutex);
			strcpy(request, response);
		//printf("handled sell\n");
		}
		return;
	}
	else if (strncmp(request, "exit", 4) == 0){
		return;	// closing connfd is done on thread function (in case of abrupt client disconnection)
	}

	return;
}

int main(int argc, char **argv) 
{
    int listenfd;
	int* connfd;	//connfd must be stored on heap before being saved on thread's stack to avoid funky sync issues
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
	
	Signal(SIGINT, sigint_handler);

	// init tree with nullptr, then create tree by reading from filename
	for (int i = 0; i < MAX_STOCK_NUM; i++)
		tree[i] = NULL;
	stock_load("stock.txt");

    listenfd = Open_listenfd(argv[1]);
    while (1) {
		clientlen = sizeof(struct sockaddr_storage); 
		connfd = Malloc(sizeof(int));
		*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
					client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		
		Pthread_create(&tid, NULL, thread, connfd);		// create thread executing thread(&connfd) to handle each client's requests, while main thread continues to listen for new connections
    }
	Close(listenfd);
    return 0;
}
/* $end echoserverimain */

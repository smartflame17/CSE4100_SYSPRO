/* 
 * stockserver.c - Event-based concurrent stock server 
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

struct client_pool {
	int maxfd;	// highest file descriptor (for excluding redundant fd in check loop)
	fd_set read_set;	// set of fd to be read by select()
	fd_set ready_set;	// set of fd in read_set that actually have data / new connection
	int ready;		// ready descriptors from select()
	rio_t rio[MAX_CLIENTS];		// rio buffer
	int clientfd[MAX_CLIENTS];	// client fd
	int active_clients;
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

void init_pool(struct client_pool* pool);
void add_client(struct client_pool* pool, int connfd);
void check_clients(struct client_pool* pool, int listenfd);
void process_request(int connfd, char* request, struct client_pool* pool, int listenfd);


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
	printf("Saved to stock.txt and exiting\n");
	exit(0);
}

void init_pool(struct client_pool* pool){
	pool->maxfd = -1;	// fd are unsigned ints, so init to negative value
	pool->active_clients = 0;
	FD_ZERO(&pool->read_set);
	FD_ZERO(&pool->ready_set);
	for (int i = 0; i < MAX_CLIENTS; i++){
		pool->clientfd[i] = -1;
	}
	return;
}

// add client with connfd to pool
void add_client(struct client_pool* pool, int connfd){
	for (int i = 0; i < MAX_CLIENTS; i++){
		if (pool->clientfd[i] == -1){
			//new connection
			pool->active_clients++;
			pool->clientfd[i] = connfd;
			Rio_readinitb(&pool->rio[i], connfd);	// init rio buffer
			FD_SET(connfd, &pool->read_set);		// add connfd to read_set
			if (connfd > pool->maxfd) pool->maxfd = connfd;	// update maxfd
			return;
		}
	}
	// if control reaches here, pool is full (max clients reached)
	fprintf(stderr, "Error: Maximum number of clients reached.\n");
	Close(connfd);
	return;
}

// call select() and loop through pool to process requests
void check_clients(struct client_pool* pool, int listenfd){
	char buf[MAXLINE] = { '\0' };
	ssize_t n;
	
	// select fd to read from
//	pool->ready_set = pool->read_set;
//	pool->ready = Select(pool->maxfd + 1, &pool->ready_set, NULL, NULL, NULL);
	
	// loop if there are any ready clients
	for (int i = 0; i < MAX_CLIENTS && pool->ready > 0; i++){
		int connfd = pool->clientfd[i];
		if (connfd == -1) continue;
		if (FD_ISSET(connfd, &pool->ready_set)){
			pool->ready--;
			if ((n = Rio_readlineb(&pool->rio[i], buf, MAXLINE)) > 0){
				process_request(connfd, buf, pool, listenfd);	// if read from rio buffer, process request
				Rio_writen(connfd, buf, MAXLINE);
			}
			else {
				Close(connfd);
				FD_CLR(connfd, &pool->read_set);
				pool->clientfd[i] = -1;
				pool->active_clients--;
				if (connfd == pool->maxfd){
					pool->maxfd = listenfd;
					for (int j = 0; j < MAX_CLIENTS; j++){
						if (pool->clientfd[j] > pool->maxfd) pool->maxfd = pool->clientfd[j];
					}
				}
			}
		}
	}	
	return;
}

void process_request(int connfd, char* request, struct client_pool* pool, int listenfd){
	char response[MAXLINE];
	int id, amount, index;

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
			//P(&stock->mutex);
			if (amount <= stock->amount){
				stock->amount -= amount;
				strcpy(response, "[buy] success\n");
			}
			else strcpy(response, "Not enough left stock\n");
			//V(&stock->mutex);
			strcpy(request, response);
		//printf("handled buy\n");
		}
		return;
	}
	else if (strncmp(request, "sell", 4) == 0 && sscanf(request, "%*s %d %d\n", &id, &amount) == 2){
		struct stock* stock = search_tree(id);

		if (stock){
			//P(&stock->mutex);
			stock->amount += amount;
			strcpy(response, "[sell] success\n");
			//V(&stock->mutex);
			strcpy(request, response);
		//printf("handled sell\n");
		}
		return;
	}
	else if (strncmp(request, "exit", 4) == 0){
		for(index = 0; index < MAX_CLIENTS; index++){
			if (pool->clientfd[index] == connfd){
				Close(connfd);
				FD_CLR(connfd, &pool->read_set);
				pool->clientfd[index] = -1;
				pool->active_clients--;
				break;	// break after removing clientfd
			}
		}
		if (connfd == pool->maxfd){
			pool->maxfd = listenfd;
			for(int j = 0;  j < MAX_CLIENTS; j++){
				if (pool->clientfd[j] > pool->maxfd) pool->maxfd = pool->clientfd[j];
			}
		}
	}
	printf("received exit\n");
	return;
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	struct client_pool pool;	// struct holding client connfd and related info

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
	
	Signal(SIGINT, sigint_handler);

	// init tree with nullptr, then create tree by reading from filename
	for (int i = 0; i < MAX_STOCK_NUM; i++)
		tree[i] = NULL;
	stock_load("stock.txt");

	init_pool(&pool);
    listenfd = Open_listenfd(argv[1]);
	FD_SET(listenfd, &pool.read_set);
	pool.maxfd = listenfd;
    while (1) {
		pool.ready_set = pool.read_set;
		pool.ready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

		if (FD_ISSET(listenfd, &pool.ready_set)){
			clientlen = sizeof(struct sockaddr_storage); 
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        	Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    	client_port, MAXLINE, 0);
        	printf("Connected to (%s, %s)\n", client_hostname, client_port);
			add_client(&pool,connfd);
			pool.ready--;
		}
		if (pool.ready > 0)
			check_clients(&pool, listenfd);
    }
	Close(listenfd);
    return 0;
}
/* $end echoserverimain */

#include "list.h"
#include "hash.h"
#include "bitmap.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_STRUCTS 10
#define MAX_LINES 1024
#define MAX_ARGS 128
#define NAME_LEN 16

// For defining type of user created structs and handling struct-specific instructions
#define TYPE_LIST 0
#define TYPE_HASH 1
#define TYPE_BITMAP 2


// macro for string comaparison used in function calls
#define command(STRING) (strcmp(argv[0], STRING) == 0)

void eval(int argc, char* argv[]);
void eval_func(int argc, char* argv[]);
void list_func(int argc, char* argv[]);
void hash_func(int argc, char* argv[]);
void bitmap_func(int argc, char* argv[]);
int get_type(char* arg);
void parse_file(int mode, const char* filename);
struct data* find_data(const char* name);	// finds data based on name
void cleanup();

// prints to stdout all the data saved in struct data
void dumpdata(struct data* d);

// frees all the connected elements
void delete(struct data* d);

// initializes data struct inside
void create(int type, const char* name, int* aux);

/* user created data structure, run the init function to initialize the struct inside 
Searchable by name using find_data() function */
struct data {
	struct list list;
	struct hash hash;
	struct bitmap* bitmap;
	char name[NAME_LEN];
	int type;
};

struct data* ds[MAX_STRUCTS];

int quit_flag = 0;		// for quit command
int struct_cnt = 0;		// total # of user created structs

void eval(int argc, char* argv[]){
	if (command("create")){
		// Usage: create [type] [name]
		if (argc < 3){
			perror("Usage: create [type] [name]\n");
			return;
		}
		int struct_type = get_type(argv[1]);
		if (struct_type == TYPE_BITMAP) {		// if struct is bitmap, pass the size of the bitmap as aux
			int size = strtol(argv[3], NULL, 10);
			create(struct_type, argv[2], &size);
		}		
		else create(struct_type, argv[2], NULL);		

		//printf("created %s of name %s\n", argv[1], argv[2]);
		struct_cnt++;	// increment # of user created struct
	}
	else if (command("dumpdata")){
		//Usage: dumpdata [name]
		if (argc != 2){
			perror("Usage: dumpdata [name]\n");
			return;
		}
		struct data* d = find_data(argv[1]);
		
		dumpdata(d);

		//printf("dumping data of %s\n", d->name);
	}
	else if (command("delete")){		// move elements behind target
		//Usage: delete [name]
		if (argc != 2){
			perror("Usage: delete [name]\n");
			return;
		}
		for (int i = 0; i < struct_cnt; i++){
			if (strcmp(argv[1],ds[i]->name) == 0){
				//printf("deleting %s...\n", ds[i].name);
				delete(ds[i]);
				for(int j = i; j < struct_cnt-1; j++){
					ds[j] = ds[j+1];
					//memmove(ds[j], ds[j+1], sizeof(struct* data));
				}
				struct_cnt--;	// scoot up elements by one and decrease count
				return;
			}
		}
	}
	else if (command("quit")){
		quit_flag = 1;		
	}
	else eval_func(argc, argv);
	return;
}

// wrapper for different data structs
void eval_func(int argc, char* argv[]){
	switch(get_type(argv[0])){
		case TYPE_LIST:{
			list_func(argc, argv);
			break;
		}
		case TYPE_HASH:{
			hash_func(argc, argv);
			break;
		}
		case TYPE_BITMAP:{
			bitmap_func(argc, argv);
			break;
		}
		default:
			perror("Invalid instruction\n");
	}	
	return;
}

void list_func(int argc, char* argv[]){
	if (argc < 2) return;
	struct data* d = find_data(argv[1]);
	if (d == NULL) return;
	if (command("list_push_back")){
		if (argc != 3) return;
		struct list_item* item = malloc(sizeof(struct list_item));
		item->data = strtol(argv[2], NULL, 10);		// convert string to int
		list_push_back(&d->list, &item->elem);
	}
	else if (command("list_push_front")){
		if (argc != 3) return;
		struct list_item* item = malloc(sizeof(struct list_item));
		item->data = strtol(argv[2], NULL, 10);		// convert string to int
		list_push_front(&d->list, &item->elem);
	}
	else if (command("list_pop_back")){
		if (argc != 2) return;
		if (list_empty(&d->list)) return;
		list_pop_back(&d->list);
	}
	else if (command("list_pop_front")){
		if (argc != 2) return;
		if (list_empty(&d->list)) return;
		list_pop_front(&d->list);
	}
	else if (command("list_front")){
		if (argc != 2) return;
		if (list_empty(&d->list)) return;
		struct list_elem* e = list_front(&d->list);
		struct list_item* item = list_entry(e, struct list_item, elem);
		printf("%d\n", item->data);
	}	
	else if (command("list_back")){
		if (argc != 2) return;
		if (list_empty(&d->list)) return;
		struct list_elem* e = list_back(&d->list);
		struct list_item* item = list_entry(e, struct list_item, elem);
		printf("%d\n", item->data);
	}
	else if (command("list_empty")){
		if (argc != 2) return;
		if (list_empty(&d->list)) printf("true\n");
		else printf("false\n");
	}
	else if (command("list_size")){
		if (argc != 2) return;
		printf("%zu\n", list_size(&d->list));
	}
	else if (command("list_max")){
		if (argc != 2) return;
		struct list_elem* e = list_max(&d->list, cmp_list, NULL);
		struct list_item* item = list_entry(e, struct list_item, elem);
		printf("%d\n", item->data);
	}	
	else if (command("list_min")){
		if (argc != 2) return;
		struct list_elem* e = list_min(&d->list, cmp_list, NULL);
		struct list_item* item = list_entry(e, struct list_item, elem);
		printf("%d\n", item->data);
	}
	else if (command("list_insert")){
		if (argc != 4) return;
		struct list_item* item = malloc(sizeof(struct list_item));
		item->data = strtol(argv[3], NULL, 10);
		int idx = strtol(argv[2], NULL, 10);
		if (idx >= list_size(&d->list)) list_push_back(&d->list, &item->elem); 	// if inserting to end, call list_push_back
		else {
			struct list_elem* before = list_idx(&d->list, idx);
			list_insert(before, &item->elem);
		}
	}
	else if (command("list_remove")){		// remove by index from list and frees corresponding item
		if (argc != 3) return;
		int idx = strtol(argv[2], NULL, 10);
		struct list_elem* target = list_idx(&d->list, idx);
		if (target == NULL) return;
		list_remove(target);
		struct list_item* item = list_entry(target, struct list_item, elem);
		free(item);
	}	
	else if (command("list_swap")){			// swap two list elements by index
		if (argc != 4) return;
		int idx_a = strtol(argv[2], NULL, 10);
		int idx_b = strtol(argv[3], NULL, 10);
		struct list_elem* a = list_idx(&d->list, idx_a);
		struct list_elem* b = list_idx(&d->list, idx_b);
		list_swap(a, b);	
	}
	else if (command("list_shuffle")){		// shuffle order of elements randomly
		if (argc != 2) return;
		list_shuffle(&d->list);
	}
	else if (command("list_reverse")){
		if (argc != 2) return;
		list_reverse(&d->list);
	}
	else if (command("list_sort")){
		if (argc != 2) return;
		list_sort(&d->list, cmp_list, NULL);
	}
	else if (command("list_splice")){	
		if (argc != 6) return;
		struct data* d2 = find_data(argv[3]);
		struct list_elem* before = list_idx(&d->list, strtol(argv[2], NULL, 10));
		struct list_elem* first = list_idx(&d2->list, strtol(argv[4], NULL, 10));
		struct list_elem* last = list_idx(&d2->list, strtol(argv[5], NULL, 10));
		list_splice(before, first, last);
	}
	else if (command("list_unique")){		// if given auxiliary list, put it to argument
		if (argc == 3){
			 struct data* duplicate = find_data(argv[2]);
			 list_unique(&d->list, &duplicate->list, cmp_list, NULL);
		}
		else if(argc == 2) list_unique(&d->list, NULL, cmp_list, NULL);	
	}
	else if (command("list_insert_ordered")){
		if (argc != 3) return;
		struct list_item* item = malloc(sizeof(struct list_item));
		item->data = strtol(argv[2], NULL, 10);
		list_insert_ordered(&d->list, &item->elem, cmp_list, NULL);
	}
	return;
}

void hash_func(int argc, char* argv[]){
	if (argc < 2) return;
	struct data* d = find_data(argv[1]);
	if (d == NULL) return;

	if (command("hash_insert")){
		if (argc != 3) return;
		struct hash_item* item = malloc(sizeof(struct hash_item));
		item->key = strtol(argv[2], NULL, 10);
		item->value = item->key;
		if (hash_insert(&d->hash, &item->elem) != NULL) free(item);		// if same item is already in table, free the allocated item since it is not needed
	}
	else if (command("hash_delete")){
		if (argc != 3) return;			
		int key = strtol(argv[2], NULL, 10);
		struct hash_item temp;
		temp.key = key;
		temp.value = key;
		struct hash_elem* e = hash_delete(&d->hash, &temp.elem);
		if (e != NULL) {	// if item existed in hash table, free allocated memory
			struct hash_item* item = hash_entry(e, struct hash_item, elem);
			free(item);
		}
	}
	else if (command("hash_replace")){
		if (argc != 3) return;
		int key = strtol(argv[2], NULL, 10);
		struct hash_item* item = malloc(sizeof(struct hash_item));;
		item->key = key;
		item->value = key;
		struct hash_elem* old = hash_replace(&d->hash, &item->elem);
		if(old != NULL) free(hash_entry(old, struct hash_item, elem));	// if there was previous element replaced, free allocated memory
	}
	else if (command("hash_empty")){
		if (argc != 2) return;
		if (hash_empty(&d->hash)) printf("true\n");
		else printf("false\n");
	}
	else if (command("hash_size")){
		if (argc != 2) return;
		printf("%zu\n", hash_size(&d->hash));
	}
	else if (command("hash_find")){
		if (argc != 3) return;
		int key = strtol(argv[2], NULL, 10);
		struct hash_item temp;
		temp.key = key;
		temp.value = key;
		struct hash_elem* e = hash_find(&d->hash, &temp.elem);
		if (e != NULL){
			struct hash_item* item = hash_entry(e, struct hash_item, elem);
			printf("%d\n", item->value);
		}
	}
	else if (command("hash_clear")){
		if (argc != 2) return;
		hash_clear(&d->hash, hash_free);
	}
	else if (command("hash_apply")){
		if (argc != 3) return;
		if (strcmp(argv[2], "square") == 0) hash_apply(&d->hash, hash_square);
		else if (strcmp(argv[2], "triple") == 0) hash_apply(&d->hash, hash_triple);
	}
	return;
}

//TODO: implement bitmap
void bitmap_func(int argc, char* argv[]){
	if (argc < 2) return;
	struct data* d = find_data(argv[1]);
	if (d == NULL) return;

	if (command("bitmap_mark")){
		if (argc != 3) return;
		size_t idx = strtol(argv[2], NULL, 10);
		bitmap_mark(d->bitmap, idx); 
	}
	if (command("bitmap_reset")){
		if (argc != 3) return;
		size_t idx = strtol(argv[2], NULL, 10);
		bitmap_reset(d->bitmap, idx); 
	}
	else if (command("bitmap_flip")){
		if (argc != 3) return;
		size_t idx = strtol(argv[2], NULL, 10);
		bitmap_flip(d->bitmap, idx);
	}
	else if (command("bitmap_test")){
		if (argc != 3) return;
		size_t idx = strtol(argv[2], NULL, 10);
		if (bitmap_test(d->bitmap, idx)) printf("true\n");
		else printf("false\n");
	}
	else if (command("bitmap_size")){
		if (argc != 2) return;
		printf("%zu\n", bitmap_size(d->bitmap));
	}
	else if (command("bitmap_set")){
		if (argc != 4) return;
		size_t idx = strtol(argv[2], NULL, 10);
		bool value;
		if (strcmp(argv[3], "true") == 0) value = true;
		else value = false;
		bitmap_set(d->bitmap, idx, value);
	}
	else if (command("bitmap_set_all")){
		if (argc != 3) return;
		bool value;
		if (strcmp(argv[2], "true") == 0) value = true;
		else value = false;
		bitmap_set_all(d->bitmap, value);
	}
	else if (command("bitmap_set_multiple")){
		if (argc != 5) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		bool value;
		if (strcmp(argv[4], "true") == 0) value = true;
		else value = false;
		bitmap_set_multiple(d->bitmap, idx, cnt, value);
	}
	else if (command("bitmap_all")){
		if (argc != 4) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		if (bitmap_all(d->bitmap, idx, cnt)) printf("true\n");
		else printf("false\n");
	}
	else if (command("bitmap_none")){
		if (argc != 4) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		if (bitmap_none(d->bitmap, idx, cnt)) printf("true\n");
		else printf("false\n");
	}
	else if (command("bitmap_any")){
		if (argc != 4) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		if (bitmap_any(d->bitmap, idx, cnt)) printf("true\n");
		else printf("false\n");
	}
	else if(command("bitmap_contains")){
		if (argc != 5) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		bool value;
		if (strcmp(argv[4], "true") == 0) value = true;
		else value = false;
		if (bitmap_contains(d->bitmap, idx, cnt, value)) printf("true\n");
		else printf("false\n");
	}
	else if(command("bitmap_scan")){
		if (argc != 5) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		bool value;
		if (strcmp(argv[4], "true") == 0) value = true;
		else value = false;
		printf("%zu\n", bitmap_scan(d->bitmap, idx, cnt, value));
	}
	else if(command("bitmap_scan_and_flip")){
		if (argc != 5) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		bool value;
		if (strcmp(argv[4], "true") == 0) value = true;
		else value = false;
		printf("%zu\n", bitmap_scan_and_flip(d->bitmap, idx, cnt, value));
	}
	else if(command("bitmap_count")){
		if (argc != 5) return;
		size_t idx = strtol(argv[2], NULL, 10);
		size_t cnt = strtol(argv[3], NULL, 10);
		bool value;
		if (strcmp(argv[4], "true") == 0) value = true;
		else value = false;
		printf("%zu\n", bitmap_count(d->bitmap, idx, cnt, value));
	}
	else if (command("bitmap_dump")){
		if (argc != 2) return;
		bitmap_dump(d->bitmap);
	}
	else if (command("bitmap_expand")){
		if (argc != 3) return;
		int size = strtol(argv[2], NULL, 10);
		d->bitmap = bitmap_expand(d->bitmap, size); 
	}
	
	return;
}

// based on name, returns the data type we're dealing with
int get_type(char* arg){	
	// switch on first 4 letters : list, hash, bitm
	if(strncmp(arg, "list", 4) == 0){
		return TYPE_LIST;
	}
	else if (strncmp(arg, "hash", 4) == 0){
		return TYPE_HASH;
	}
	else if (strncmp(arg, "bitm", 4) == 0){
		return TYPE_BITMAP;;
	}
	else perror("Invalid data type\n");
	return -1;
}

// parses command from input. mode 0 is stdin, mode 1 is from file argument
void parse_file(int mode, const char* filename){
	FILE* fp = stdin;
	if (mode == 1){
		fp = fopen(filename, "r");
		if (fp == NULL){
			perror("Error opening file\n");
			return;
		}
	}
	char line[MAX_LINES];
	char* args[MAX_ARGS];
	int count = 0;

	while(fgets(line, sizeof(line), fp)){
		line[strcspn(line, "\n")] = 0;	// remove newline

		// duplicate string and save adress on args[]
		char* token = strtok(line, " ");	
		while(token != NULL && count < MAX_ARGS){
			args[count] = strdup(token);	
			count++;
			token = strtok(NULL, " ");
		}

		eval(count, args);

		count = 0;
		if (quit_flag) break;
	}
		
	if (fp != stdin) fclose(fp);
	cleanup();
	return;
}

struct data* find_data(const char* name){
	for(int i=0; i < struct_cnt; i++){
		if (strncmp(ds[i]->name, name, NAME_LEN) == 0)
			return ds[i];
	}
	return NULL;
}

// clean up any allocated structs in heap
void cleanup(){
	for(int i = 0; i < struct_cnt; i++){
		delete(ds[i]);
		free(ds[i]);	
	}
	return;
}

void create(int type, const char* name, int* size){
	struct data* d = malloc(sizeof(struct data));
	d->type = type;
	strcpy(d->name, name);
	switch(d->type){
		case TYPE_LIST:{
			list_init(&d->list);
			break;
		}
		case TYPE_HASH:{
			hash_init(&d->hash, my_hash_func, cmp_hash, NULL);		
			break;
		}
		case TYPE_BITMAP:{
			d->bitmap = bitmap_create(*size);
			break;
		}
	}
	ds[struct_cnt] = d;	//save allocated memory address of data d in ds[]
	return;
}

/* prints all elements to stdout. */
void dumpdata(struct data* d){

	switch(d->type){
		case TYPE_LIST:{
			if (list_empty(&d->list)) return;
			struct list_elem* e;
			for(e = list_begin(&d->list); e != list_end(&d->list); e = list_next(e)){
				struct list_item* item = list_entry(e, struct list_item, elem);
				printf("%d ", item->data);
			}
			printf("\n");
			break;
		}
		case TYPE_HASH:{
			if (hash_empty(&d->hash)) return;
			hash_apply(&d->hash, hash_print);
			printf("\n");
			break;
		}
		case TYPE_BITMAP:{
			if (!d->bitmap) return;
			size_t size = bitmap_size(d->bitmap);
			for (size_t i = 0; i < size; i++){
				printf("%d", bitmap_test(d->bitmap, i) ? 1 : 0);	//prints 1 if true, 0 if false
			}
			printf("\n");
			break;
		}
		default: perror("Invalid type.\n");
	}
	return;
}

void delete(struct data* d){
	switch(d->type){
		case TYPE_LIST:{
			list_cleanup(&d->list);		// free allocated memory in heap
			break;
		}
		case TYPE_HASH:{
			hash_destroy(&d->hash, hash_free);	// free buckets and entries in heap
			break;
		}
		case TYPE_BITMAP:{
			bitmap_destroy(d->bitmap);		// free bitmap struct in heap
		break;
		}
		default: perror("Invalid type.\n");
	}
	return;
}

int main(int argc, char* argv[]){
	int mode;
	srand(time(NULL));
	if (argc < 2) {		//reading from stdin
		mode = 0;
		parse_file(mode, NULL);
	}
	else {				//reading from file
		mode = 1;
		parse_file(mode, argv[1]);
	}
	return 0;
}


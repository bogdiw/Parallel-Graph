#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
	unsigned int idx;
} graph_task_arg_t;

static void process_node(unsigned int idx);

static void process_node_helper(void *arg)
{
	graph_task_arg_t *graph_task_arg = (graph_task_arg_t *)arg;
	unsigned int idx = graph_task_arg->idx;
	os_node_t *node;

	pthread_mutex_lock(&mutex);
	node = graph->nodes[idx];
	pthread_mutex_unlock(&mutex);

	pthread_mutex_lock(&mutex);
	sum += node->info;
	pthread_mutex_unlock(&mutex);

	// printf("Sum is now: %d\n", sum);

	// printf("Processing node %d\n", node->id);

	for (unsigned int i = 0; i < node->num_neighbours; i++)
	{
		pthread_mutex_lock(&mutex);
		unsigned int neighbour_idx = node->neighbours[i];
		if (graph->visited[neighbour_idx] == NOT_VISITED)
		{
			// printf("Enqueueing task for neighbour %d\n", neighbour_idx);
			process_node(neighbour_idx);
		}
		pthread_mutex_unlock(&mutex);
	}
	// printf("Finished processing node %d\n", node->id);

	pthread_mutex_lock(&mutex);
	graph->visited[idx] = DONE;
	pthread_mutex_unlock(&mutex);
}

static void process_node(unsigned int idx)
{
	graph_task_arg_t *graph_task_arg = malloc(sizeof(graph_task_arg_t));
	graph_task_arg->idx = idx;
	os_task_t *task = create_task(process_node_helper, graph_task_arg, NULL);

	graph->visited[idx] = PROCESSING;

	enqueue_task(tp, task);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	return 0;
}

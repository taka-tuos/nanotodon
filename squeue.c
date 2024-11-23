#include "squeue.h"

sbctx_t queue_data[QUEUE_SIZE];
pthread_mutex_t queue_mutex;
int queue_head;
int queue_num;

void squeue_init()
{
	pthread_mutex_init(&queue_mutex, NULL);
	queue_head = 0;
	queue_num = 0;
}

int squeue_enqueue(sbctx_t enq_data)
{
	int ret = 0;

	pthread_mutex_lock(&queue_mutex);
	//waddstr(scr, "en_lock");
	//wrefresh(scr);

	if (queue_num < QUEUE_SIZE) {
		queue_data[(queue_head + queue_num) % QUEUE_SIZE] = enq_data;
		queue_num++;
		ret = 0;
	} else {
		ret = 1;
	}

	//waddstr(scr, "en_unlock");
	//wrefresh(scr);
	pthread_mutex_unlock(&queue_mutex);

	return ret;
}

int squeue_dequeue(sbctx_t *deq_data)
{
	int ret = 0;

	pthread_mutex_lock(&queue_mutex);
	//waddstr(scr, "de_lock");
	//wrefresh(scr);

	if (queue_num > 0) {
		*deq_data = queue_data[queue_head];
		queue_head = (queue_head + 1) % QUEUE_SIZE;
		queue_num--;
		ret = 0;
	} else {
		ret = 1;
	}

	//waddstr(scr, "de_unlock");
	//wrefresh(scr);
	pthread_mutex_unlock(&queue_mutex);

	return ret;
}

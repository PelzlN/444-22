/*
 * elevator look
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct look_data {
	struct list_head queue;
};

// list_del_init removes an element and reinitializes the element as its own list
static void look_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static int look_dispatch(struct request_queue *q, int force)
{
	struct look_data *nd = q->elevator->elevator_data;

	if(!list_empty(&nd->queue)){
		struct request *rq;
		rq = list_entry(nd->queue.next, struct request, queuelist);
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);	
		return 1;
	}
	return 0;
}
//Appends the queuelist of a different request and appends it to our queue.
static void look_add_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;
	char dir;
	struct list_head *current_pos = NULL;

	list_for_each(current_pos, &nd->queue){
		if(rq_end_sector(list_entry(current_pos, struct request, queuelist)) > rq_end_sector(rq)){
			break;
		}
	}
	list_add_tail(&rq->queuelist, current_pos);

	/*
	if (list_empty(&nd->queue)) {
		printk(KERN_DEBUG "Queue empty, adding new item\n");
		list_add(&rq->queuelist, &nd->queue);
	} else {
		struct list_head *current_pos;
		struct request *current_req;
		list_for_each(current_pos, &nd->queue){
			current_req = list_entry(current_pos, struct request, queuelist);
			if (blk_rq_pos(current_req) < blk_rq_pos(rq)) {
				printk(KERN_DEBUG "Adding item via insertion sort\n");
				list_add_tail(&rq->queuelist, &current_req->queuelist);
				break;
			}
		}
	}
	*/

	//Debug
	if (rq_data_dir(rq) == READ) { //reading or writing
		dir = 'R';
	} else {
		dir = 'W';
	}
	//print out request added, reading/writing, and position
	printk(KERN_DEBUG "Request added: %c %lu\n", dir, (unsigned long)blk_rq_pos(rq));
	//End Debug
}

static struct request *
look_former_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	// if it's the only item in the queue
	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
look_latter_request(struct request_queue *q, struct request *rq)
{
	struct look_data *nd = q->elevator->elevator_data;

	// if it's the only item in the queue
	if (rq->queuelist.next == &nd->queue) 
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

// Shouldn't need to change this
static int look_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct look_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM; // error no memory, I assume

	// kmalloc_node does the same stuff as regular malloc
	// GFP_KERNEL means that the node being intitialized uses the kernel
	// and can be told to wait
	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);

	// I think these are kinda like a mutex for hardware
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void look_exit_queue(struct elevator_queue *e)
{
	struct look_data *nd = e->elevator_data;

	// BUG_ON dumps debug infor and kills the process
	// in this case, it triggers if the queue still has items in it when exited
	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_look = {
	.ops = {
		.elevator_merge_req_fn		= look_merged_requests,
		.elevator_dispatch_fn		= look_dispatch,
		.elevator_add_req_fn		= look_add_request,
		.elevator_former_req_fn		= look_former_request,
		.elevator_latter_req_fn		= look_latter_request,
		.elevator_init_fn		= look_init_queue,
		.elevator_exit_fn		= look_exit_queue,
	},
	.elevator_name = "look",
	.elevator_owner = THIS_MODULE,
};

static int __init look_init(void)
{
	return elv_register(&elevator_look);
}

static void __exit look_exit(void)
{
	elv_unregister(&elevator_look);
}

module_init(look_init);
module_exit(look_exit);


MODULE_AUTHOR("Alexander Nieber, Matthew Ruder, Nathaniel Pelzl");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LOOK IO scheduler");

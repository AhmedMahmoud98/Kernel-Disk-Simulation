#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>

int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, struct msgbuff item);
struct msgbuff dequeue(struct Queue* queue);
struct msgbuff front(struct Queue* queue);
struct msgbuff rear(struct Queue* queue);
struct Queue* createQueue(unsigned capacity);

key_t process_to_kernel_q, kernel_to_disk_q, disk_to_kernel_q;
struct Queue* requests_queue;  
unsigned int CLK = 0;
int disk_latency = 0;
int process_pid, disk_pid;
int disk_free_slots_number = 10;

struct msgbuff
{
   long mtype;
   char mtext[70];
};

struct Queue 
{ 
    int front, rear, size; 
    unsigned capacity; 
    struct msgbuff* array; 
}; 
  
void create_msg_queues();
void fork_process_and_disk();
void move_on(int SigNum);

int main()
{
    signal(SIGALRM, move_on);
    create_msg_queues();
    fork_process_and_disk();
    create_requests_queue(1000);    
    alarm(1);       //wait for one second till the process and disk initialise their data 

    while(1)
    {
        receive_all_process_requests();
        if(!isEmpty(requests_queue) && disk_latency == 0)
        {
            ask_disk_for_status();        //Send Signal SIGUSR1
            receive_status_from_disk();   //Number Of Free Slots
            check_request_type_and_send_it( dequeue(requests_queue) ); 
        }
    }
}
////////////////////////////////////////////////////////////////////////////
void create_msg_queues()
{
    process_to_kernel_q = msgget(IPC_PRIVATE, 0644);
    kernel_to_disk_q = msgget(IPC_PRIVATE, 0644);
    disk_to_kernel_q = msgget(IPC_PRIVATE, 0644);
}
////////////////////////////////////////////////////////////////////////////
void fork_process_and_disk()
{
    char Queue1_identifier[20], Queue2_identifier[20], Queue3_identifier[20];
    sprintf(Queue1_identifier, "%d", process_to_kernel_q);
    sprintf(Queue2_identifier, "%d", disk_to_kernel_q);
    sprintf(Queue3_identifier, "%d", kernel_to_disk_q);

    //Fork The Process
    process_pid = fork();
    if (process_pid == -1)
        perror("error in fork");

    
    if(process_pid == 0)
    {
        char *argv[] = { "./Process", Queue1_identifier , "Process_Input.txt", NULL };
        execve("./Process", argv, NULL); 
    }
  
    //Fork the disk process
    disk_pid = fork();
    if (disk_pid == -1)
        perror("error in fork");

    if(disk_pid == 0)
    {
        char *argv[] = { "./Disk", Queue2_identifier, Queue3_identifier, NULL };
        execve("./Disk", argv, NULL); 
    }

}
////////////////////////////////////////////////////////////////////////////
void move_on(int SigNum)
{
    CLK++;
    if(disk_latency > 0)
        disk_latency--;

    killpg(0, SIGUSR2);
    alarm(1);
}
////////////////////////////////////////////////////////////////////////////
void ask_disk_for_status()
{
    kill(disk_pid, SIGUSR1);
}
////////////////////////////////////////////////////////////////////////////
void receive_all_process_requests()
{
    //Take all requests into the queue
    struct msgbuff process_msg;
    int queue_is_not_empty = 1;
    while(queue_is_not_empty)
    {
        queue_is_not_empty = msgrcv(process_to_kernel_q, &process_msg, sizeof(process_msg.mtext), 0, IPC_NOWAIT);
        enqueue(requests_queue, process_msg);   
    }
}
////////////////////////////////////////////////////////////////////////////
void receive_status_from_disk()
{
    struct msgbuff disk_status_msg;
    int rec_val = msgrcv(disk_to_kernel_q, &disk_status_msg, sizeof(disk_status_msg.mtext), 0, !IPC_NOWAIT);
    disk_free_slots_number = atoi(disk_status_msg.mtext);
}
////////////////////////////////////////////////////////////////////////////
int send_add_request_to_disk(char *text)
{
    text++;             //neglect the 1st character 
    disk_latency = 3;

    struct msgbuff add_request;
    add_request.mtype = 1;    //Add
    strcpy(add_request.mtext , text);

    return msgsnd(kernel_to_disk_q, &add_request, sizeof(add_request.mtext), !IPC_NOWAIT);
}
////////////////////////////////////////////////////////////////////////////
int send_del_request_to_disk(char *slot_number)
{
    slot_number++;             //neglect the 1st character 
    disk_latency = 1;

    struct msgbuff del_request;
    del_request.mtype = 2;       //Delete
    strcpy(del_request.mtext , slot_number);
    
    return msgsnd(kernel_to_disk_q, &del_request, sizeof(del_request.mtext), !IPC_NOWAIT);
}
////////////////////////////////////////////////////////////////////////////
void check_request_type_and_send_it(struct msgbuff request)
{
    if(request.mtext[0] == "A")
        if(disk_free_slots_number > 0)
            send_add_request_to_disk(request.mtext);
            
    if(request.mtext[0] == "D")
        if(disk_free_slots_number < 10)
            send_del_request_to_disk(request.mtext)
}
























































////////////////////////////////////////////////////////////////////////////
// Queue is full when size becomes equal to the capacity  
int isFull(struct Queue* queue) 
{  return (queue->size == queue->capacity);  } 
  
// Queue is empty when size is 0 
int isEmpty(struct Queue* queue) 
{  return (queue->size == 0); } 
  
// Function to add an item to the queue.   
// It changes rear and size 
void enqueue(struct Queue* queue, struct msgbuff item) 
{ 
    if (isFull(queue)) 
        return; 
    queue->rear = (queue->rear + 1)%queue->capacity; 
    queue->array[queue->rear] = item; 
    queue->size = queue->size + 1; 
} 
  
// Function to remove an item from queue.  
// It changes front and size 
struct msgbuff dequeue(struct Queue* queue) 
{  
    struct msgbuff item = queue->array[queue->front]; 
    queue->front = (queue->front + 1) % queue->capacity; 
    queue->size = queue->size - 1; 
    return item; 
} 
  
// Function to get front of queue 
struct msgbuff front(struct Queue* queue) 
{ 
    return queue->array[queue->front]; 
} 
  
// Function to get rear of queue 
struct msgbuff rear(struct Queue* queue) 
{ 
    if (isEmpty(queue))  
    return queue->array[queue->rear]; 
} 
struct Queue* createQueue(unsigned capacity) 
{ 
    struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue)); 
    queue->capacity = capacity; 
    queue->front = queue->size = 0;  
    queue->rear = capacity - 1;  // This is important, see the enqueue 
    queue->array = (int*) malloc(queue->capacity * sizeof(struct msgbuff)); 
    requests_queue =  queue;
    return queue; 
} 
////////////////////////////////////////////////////////////////////////////
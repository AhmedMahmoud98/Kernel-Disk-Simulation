#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

struct msgbuff
{
   long mtype;
   char mtext[70];
};

unsigned int CLK = 0;
char Data[10][65];
int is_busy[10] = { 0 };
unsigned char number_of_free_slots = 10;
int kernel_to_disk_q, disk_to_kernel_q;
struct msgbuff kernel_operation_msg;

void move_on(int SigNum);
void send_kernel_disk_status(int SigNum);

int main(int argc, char *argv[])
{
    signal(SIGUSR2, move_on);
    signal(SIGUSR1, send_kernel_disk_status);
    disk_to_kernel_q = atoi(argv[1]);
    kernel_to_disk_q = atoi(argv[2]);

    while(1)
    {
        char request = receive_request_from_kernel();
        if(request == 'A')
            perform_add_operation();
        else if(request == 'D')
            perform_delete_operation();
    }
}

void move_on (int SigNum)
{
    CLK++;
}
////////////////////////////////////////////////////////////////////
void send_kernel_disk_status(int SigNum)
{
    struct msgbuff disk_status_msg;
    strcpy(disk_status_msg.mtext, itoa(number_of_free_slots));
    int send_val = msgsnd(disk_to_kernel_q, &disk_status_msg, sizeof(disk_status_msg.mtext), !IPC_NOWAIT);
}
/////////////////////////////////////////////////////////////////////
char receive_request_from_kernel()
{
    
    int kernel_has_request = msgrcv(kernel_to_disk_q, &kernel_operation_msg, sizeof(kernel_operation_msg.mtext), 0, !IPC_NOWAIT);
    if(kernel_has_request)
        return kernel_operation_msg.mtext[0];
    else
        return '-1';
}
////////////////////////////////////////////////////////////////////
void perform_add_operation()
{
    char* temp = kernel_operation_msg.mtext;
    temp++;   
    int i = 0;
    for(i = 0; i < 10; i++)
        if(is_busy[i] == 0)
            break;

    strcpy(Data[i], temp);
    number_of_free_slots--;
}
////////////////////////////////////////////////////////////////////
void perform_delete_operation()
{
    char* temp = kernel_operation_msg.mtext;
    temp++;
    int slot_number = atoi(temp);
    if(!is_busy[slot_number])
        return;
    else
    {
        is_busy[slot_number] = 1;
        number_of_free_slots++;
    }
    
}
////////////////////////////////////////////////////////////////////

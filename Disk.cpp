#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <bits/stdc++.h>

using namespace std;

struct msgbuff
{
   long mtype;
   char mtext[70];
};

int CLK = 0;
vector <string> Data(10 , "_");
int number_of_free_slots = 10;
int kernel_to_disk_q, disk_to_kernel_q;
msgbuff kernel_operation_msg;

void move_on(int SigNum);
void send_kernel_disk_status(int SigNum);
char receive_request_from_kernel();
void perform_add_operation();
void perform_delete_operation();

int main(int argc, char *argv[])
{
    signal(SIGUSR2, move_on);
    signal(SIGUSR1, send_kernel_disk_status);
    disk_to_kernel_q = atoi(argv[1]);
    kernel_to_disk_q = atoi(argv[2]);
    //cout << disk_to_kernel_q << " " << kernel_to_disk_q << endl;

    while(1)
    {
        char request = receive_request_from_kernel();
        if(request == 'A' && number_of_free_slots != 0)
            perform_add_operation();
        else if(request == 'D' && number_of_free_slots != 10)
            perform_delete_operation();
    }
}
////////////////////////////////////////////////////////////////////
void move_on (int SigNum)
{
    CLK++;
}
////////////////////////////////////////////////////////////////////
void send_kernel_disk_status(int SigNum)
{
    msgbuff disk_status_msg;
    strcpy(disk_status_msg.mtext, to_string(number_of_free_slots).c_str());
    disk_status_msg.mtype = 1;
    int send_val = msgsnd(disk_to_kernel_q, &disk_status_msg, 
                          sizeof(disk_status_msg.mtext), !IPC_NOWAIT);
    cout << send_val << endl;
}
/////////////////////////////////////////////////////////////////////
char receive_request_from_kernel()
{
    msqid_ds queue_status;
    int rc = msgctl(kernel_to_disk_q, IPC_STAT, &queue_status);
    int number_of_messages = queue_status.msg_qnum;

    if(number_of_messages != 0)
    {
        int rc2 = msgrcv(kernel_to_disk_q, &kernel_operation_msg, 
                        sizeof(kernel_operation_msg.mtext), 0, IPC_NOWAIT);

        return kernel_operation_msg.mtext[0];
    }
    return '0';
}
////////////////////////////////////////////////////////////////////
void perform_add_operation()
{
    int first_empty_slot_index;
    string message_text = kernel_operation_msg.mtext;
    message_text = message_text.substr(1, message_text.size() - 1);
    vector<string>::iterator it = find(Data.begin(), Data.end(), "_");

	if (it != Data.end())
        first_empty_slot_index = distance(Data.begin(), it);

    Data[first_empty_slot_index] = message_text;
    number_of_free_slots--;
}
////////////////////////////////////////////////////////////////////
void perform_delete_operation()
{
    string slot_number_str = kernel_operation_msg.mtext;
    slot_number_str = slot_number_str.substr(1, slot_number_str.size() - 1);

    int slot_number = stoi(slot_number_str);

    if(slot_number < 10)
    {
        if(Data[slot_number] != "_")
        {    
            Data[slot_number] = "_";
            number_of_free_slots++;
        }
    }
}
////////////////////////////////////////////////////////////////////

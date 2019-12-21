#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/wait.h> 
#include <bits/stdc++.h>
#include <fstream>

using namespace std;

struct msgbuff
{
   long mtype;
   char mtext[70];
};

enum event { process_request, 
             disk_response, 
             request_to_disk,
             process_terminated };

key_t process_to_kernel_q, kernel_to_disk_q, disk_to_kernel_q;
queue <msgbuff> requests_queue;
ofstream log_file;
vector<int> Processes_PIDs;
int Kernel_PID, Disk_PID;
int number_of_processes;
int number_of_alive_processes;
int disk_free_slots_number = 10;
int disk_latency = 0;
int CLK = 0;


void create_msg_queues();
void fork_process_and_disk();
void move_on(int SigNum);
void Process_terminated(int SigNum);
void ask_disk_for_status();
void receive_status_from_disk();
void check_request_type_and_send_it(msgbuff request);
void receive_all_process_requests();
int send_add_request_to_disk(string message_text);
int send_del_request_to_disk(string slot_number);
void write_to_log_file(event e, msgbuff message);
void End();


int main(int argc, char *argv[])
{
    signal(SIGALRM, move_on);
    signal(SIGCHLD, Process_terminated);
    signal(SIGUSR2, SIG_IGN);
    //number_of_alive_processes = number_of_processes = atoi(argv[1]);
    number_of_alive_processes = number_of_processes = 2;
    log_file.open("log_file.txt", ios::out | ios::trunc );

    create_msg_queues();
    fork_process_and_disk();   
    alarm(1);       //wait for one second till the process and disk initialise their data 

    while(1)
    {
        receive_all_process_requests();
        if(!requests_queue.empty() && disk_latency == 0)
        {
            ask_disk_for_status();        //Send Signal SIGUSR1
            receive_status_from_disk();   //Number Of Free Slots
            check_request_type_and_send_it( requests_queue.front());
            requests_queue.pop();
        }

        
        if(requests_queue.empty() && number_of_alive_processes == 0 && disk_latency == 0)
            End();
    }
}
////////////////////////////////////////////////////////////////////////////
void create_msg_queues()
{
    process_to_kernel_q = msgget(IPC_PRIVATE, 0644);
    kernel_to_disk_q = msgget(IPC_PRIVATE, 0644);
    disk_to_kernel_q = msgget(IPC_PRIVATE, 0644);
    //cout << int(process_to_kernel_q) << " " << 
    //        int(disk_to_kernel_q)<< " " << int(kernel_to_disk_q)  << endl;  
}
////////////////////////////////////////////////////////////////////////////
void fork_process_and_disk()
{
    Kernel_PID = getpid();
    string process_to_kernel_q_str = to_string(int(process_to_kernel_q));
    string disk_to_kernel_q_str = to_string(int(disk_to_kernel_q));
    string kernel_to_disk_q_str = to_string(int(kernel_to_disk_q));
    Processes_PIDs = vector<int>(number_of_processes, -1);
    
    //Fork The Processes
    for (int i = 0; i < number_of_processes; i++)
    {
        Processes_PIDs[i] = fork();
        if (Processes_PIDs[i] == -1)
            perror("error in fork");
        else if (Processes_PIDs[i] == 0)
            break;
    }

    if (getpid() != Kernel_PID)
    {
        int MyIndex = -1;
        for (int i = number_of_processes - 1; i >= 0; i--)
        {
            if (Processes_PIDs[i] == 0)
            {
                MyIndex = i;
                break;
            }
        }
        string input_file_name = "Process" + to_string(MyIndex) + "_Input.txt";
        vector<char*> commandVector = 
        {
            const_cast<char*>("./Process") 
           ,const_cast<char*>(process_to_kernel_q_str.c_str())
           ,const_cast<char*>(input_file_name.c_str())
           ,NULL
        };

        char ** argv = commandVector.data();
        execve("./Process", argv, NULL); 

    }


    //Fork the disk process
    Disk_PID = fork();
    if (Disk_PID == -1)
        perror("error in fork");

    if(Disk_PID == 0)
    {
         vector<char*> commandVector = 
        {
            const_cast<char*>("./Disk") 
           ,const_cast<char*>(disk_to_kernel_q_str.c_str())
           ,const_cast<char*>(kernel_to_disk_q_str.c_str())
           ,NULL
        };

        char ** argv = commandVector.data();
        execve("./Disk", argv, NULL); 
    }

}
////////////////////////////////////////////////////////////////////////////
void move_on(int SigNum)
{
    CLK++;
    if(disk_latency > 0)
        disk_latency--;

    //cout << requests_queue.size() << " " << number_of_alive_processes << " " <<  disk_latency << endl; 
    killpg(0, SIGUSR2);
    alarm(1);
}
////////////////////////////////////////////////////////////////////////////
void ask_disk_for_status()
{
    kill(Disk_PID, SIGUSR1);
}
////////////////////////////////////////////////////////////////////////////
void receive_all_process_requests()
{
    //Take all requests into the queue
    msgbuff process_msg;
    msqid_ds queue_status;
    int rc = msgctl(process_to_kernel_q, IPC_STAT, &queue_status);
    int number_of_messages = queue_status.msg_qnum;

    while(number_of_messages != 0)
    {
        int rc2 = msgrcv(process_to_kernel_q, &process_msg,
                     sizeof(process_msg.mtext), 0, IPC_NOWAIT);
        requests_queue.push(process_msg);
        write_to_log_file(process_request, process_msg);
        rc = msgctl(process_to_kernel_q, IPC_STAT, &queue_status);
        number_of_messages = queue_status.msg_qnum;
    }
}
////////////////////////////////////////////////////////////////////////////
void receive_status_from_disk()
{
    msqid_ds queue_status;
    int number_of_messages = 0;
    while(number_of_messages == 0)
    {
        int rc = msgctl(disk_to_kernel_q, IPC_STAT, &queue_status);
        number_of_messages = queue_status.msg_qnum;
    }

    msgbuff disk_status_msg;
    int rec_val = msgrcv(disk_to_kernel_q, &disk_status_msg, 
                sizeof(disk_status_msg.mtext), 0, !IPC_NOWAIT);
    disk_free_slots_number = atoi(disk_status_msg.mtext);
    write_to_log_file(disk_response, disk_status_msg);
}
////////////////////////////////////////////////////////////////////////////
void check_request_type_and_send_it(msgbuff request)
{
    if(request.mtext[0] == 'A')
        if(disk_free_slots_number > 0)
            send_add_request_to_disk(request.mtext);
            
    if(request.mtext[0] == 'D')
        if(disk_free_slots_number < 10)
            send_del_request_to_disk(request.mtext);

    write_to_log_file(request_to_disk, request);
}
////////////////////////////////////////////////////////////////////////////
int send_add_request_to_disk(string message_text)
{
    disk_latency = 3;

    msgbuff add_request;
    add_request.mtype = 1;    
    strcpy(add_request.mtext , message_text.c_str());

    return msgsnd(kernel_to_disk_q, &add_request, sizeof(add_request.mtext), !IPC_NOWAIT);
}
////////////////////////////////////////////////////////////////////////////
int send_del_request_to_disk(string slot_number)
{
    disk_latency = 1;

    msgbuff del_request;
    del_request.mtype = 1;       
    strcpy(del_request.mtext , slot_number.c_str());
    
    return msgsnd(kernel_to_disk_q, &del_request, sizeof(del_request.mtext), !IPC_NOWAIT);
}
////////////////////////////////////////////////////////////////////////////
void write_to_log_file(event event_type, msgbuff message)
{
    string message_text(message.mtext);
    string log_text;

    if(event_type == process_request)
    {
        message_text = message_text.substr(1, message_text.size() - 1);
        if(message.mtype == 1)
            log_text = "Process asked to ADD \"" + message_text 
                        + "\" to the Disk at Time Slot " + to_string(CLK);
        else if(message.mtype == 2)
            log_text = "Process asked to DEL slot #" + message_text 
                         + " from the Disk at Time Slot " + to_string(CLK);
    }

    else if(event_type == disk_response)
    {
        log_text = "Disk response that it has " + message_text 
                    +" free slots at Time Slot " + to_string(CLK); 
    }

    else if(event_type == request_to_disk)
    {
        message_text = message_text.substr(1, message_text.size() - 1);
        if(message.mtype == 1)
            log_text = "The Kernel asked Disk to ADD \"" + message_text 
                        + "\" at Time Slot " + to_string(CLK);
        else if(message.mtype == 2)
            log_text = "The Kernel asked Disk to DEL slot #" + message_text 
                        + " at Time Slot " + to_string(CLK);
    }

    else if(event_type == process_terminated)
    {
        log_text = "Process with PID " + message_text 
                 + " terminated with exit code " + to_string(message.mtype)
                 + " at Time Slot " + to_string(CLK);
    }

    log_file <<  log_text << endl;
}
////////////////////////////////////////////////////////////////////////////
void Process_terminated(int SigNum)
{
    int pid, stat_loc;
    while(waitpid(-1, NULL, WNOHANG))
    {
        number_of_alive_processes--;
    };
    //pid = wait(&stat_loc);
    //msgbuff process_data;
    //process_data.mtype = stat_loc;
    //strcpy(process_data.mtext , to_string(pid).c_str());
    //write_to_log_file(process_terminated, process_data);
}
////////////////////////////////////////////////////////////////////////////
void End()
{
    string log_text = "Kernel Terminated at Time Slot " + to_string(CLK);
    log_file <<  log_text << endl;
    log_file.close();
    msgctl(process_to_kernel_q, IPC_RMID, (struct msqid_ds *) 0);
    msgctl(disk_to_kernel_q, IPC_RMID, (struct msqid_ds *) 0);
    msgctl(kernel_to_disk_q, IPC_RMID, (struct msqid_ds *) 0);
    killpg(0, SIGKILL);
}
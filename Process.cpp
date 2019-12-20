#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <iostream>
#include <bits/stdc++.h>
using namespace std;

struct msgbuff
{
   long mtype;
   char mtext[70];
};

int CLK = 0;
int number_of_requests = 0;
int number_of_processed_requests = 0;
vector<int> requests_clk;
int process_to_kernel_q;
vector<msgbuff> Messages;

void move_on(int SigNum);

int main(int argc, char *argv[])
{
    signal(SIGUSR2, move_on);
    process_to_kernel_q = atoi(argv[1]);
    fstream requests_file; 
    requests_file.open(argv[2],ios::in);
    //requests_file.open("Process_Input.txt",ios::in);  
    string line, operation, message, temp;
    int request_clk;

    if(!requests_file)
        exit(1);

    while(getline (requests_file, line))
    {
        istringstream iss(line);
        iss >> request_clk;
        requests_clk.push_back(request_clk);
        iss >> operation;
        iss >> temp;
        message = temp;

        if(message[0] == '\"')
        {
            while(message[message.size() - 1] != '\"')
            {
                message += " ";
                iss >> temp;
                message += temp;                    
            }
        }

        message.erase(std::remove(message.begin(),message.end(),'\"'), message.end());
        message = operation[0] + message;
        Messages.push_back(msgbuff());
        strcpy(Messages[number_of_requests].mtext , message.c_str());

        if(operation == "ADD")
            Messages[number_of_requests].mtype = 1;
        else if(operation == "DEL") 
            Messages[number_of_requests].mtype = 2;
        number_of_requests++;
    }

    
    while(1)
    {
        if(number_of_processed_requests < number_of_requests)
        {
            if(requests_clk[number_of_processed_requests] == CLK)
            {
                struct msgbuff Msg = Messages[number_of_processed_requests];
                int send_val = msgsnd(process_to_kernel_q, &Msg, sizeof(Msg.mtext), !IPC_NOWAIT);
                number_of_processed_requests++;
            }
        }
        else
            exit(0);
    }

}

void move_on (int SigNum)
{ 
    CLK++;
}

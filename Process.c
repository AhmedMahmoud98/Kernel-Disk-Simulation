#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#define MAX_NUMBER_OF_REQUESTS 100

struct msgbuff
{
   long mtype;
   char mtext[70];
};

unsigned int CLK = 0;
unsigned int number_of_requests = 0;
unsigned char number_of_processed_requests = 0;
unsigned char requests_clk[MAX_NUMBER_OF_REQUESTS];
int process_to_kernel_q;
struct msgbuff Messages[MAX_NUMBER_OF_REQUESTS];

void move_on(int SigNum);

int main(int argc, char *argv[])
{
    signal(SIGUSR2, move_on);
    process_to_kernel_q = atoi(argv[1]);

    FILE* requests_file_ptr;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    if ((requests_file_ptr = fopen(argv[2], "r")) == NULL)
        exit(1);

    while ((read = getline(&line, &len, requests_file_ptr)) != -1)
    {
        unsigned char i = 0, j = 1;
        unsigned char request_clk_temp[3];
        unsigned char Msg_temp[70];

        while (line[i] != ' ') 
            request_clk_temp[i++] = line[i];

        requests_clk[number_of_requests] = atoi(request_clk_temp);

        i++;
        Msg_temp[0] = line[i];

        i+= 4;  
        while (line[i] != '\n') 
            Msg_temp[j++] = line[i++];
        
        Msg_temp[j] = '\0';
        strcpy(Messages[number_of_requests].mtext , Msg_temp);
        number_of_requests++;
    }

    while(1)
    {
        if(number_of_processed_requests < number_of_requests)
        {
            if(requests_clk[number_of_processed_requests] == CLK)
            {
                struct msgbuff Msg = Messages[number_of_processed_requests];
                Msg.mtype = 1;     //Process Request
                int send_val = msgsnd(process_to_kernel_q, &Msg, sizeof(Msg.mtext), !IPC_NOWAIT);
                number_of_processed_requests++;
            }
        }
    }

}

void move_on (int SigNum)
{
    CLK++;
}

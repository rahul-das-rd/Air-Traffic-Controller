#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

#define MSG_TYPE_TERMINATE 1000

// Define message structure for termination communication
struct msg_buffer {
    long msg_type;
    char msg_text[100];
};

int main() {
    //connect to the message queue of ATC
    key_t key = ftok("airtrafficcontroller.c", 'B');
    int msg_queue_id = msgget(key, 0666);
    if (msg_queue_id == -1) {
        perror("Error connecting to message queue");
        exit(EXIT_FAILURE);
    }

    // Loop for cleanup process
    while (1) {
        char response;
        printf("Do you want the Air Traffic Control System to terminate? (Y/N): ");
        scanf(" %c", &response);

        if (response == 'Y' || response == 'y') {
            // Inform Air Traffic Controller process to terminate
            struct msg_buffer terminate_msg;
            terminate_msg.msg_type = MSG_TYPE_TERMINATE;
            strcpy(terminate_msg.msg_text, "Termination request");
            if (msgsnd(msg_queue_id, &terminate_msg, sizeof(terminate_msg.msg_text), 0) == -1) {
                perror("Error sending termination message to ATC");
                exit(EXIT_FAILURE);
            }
            printf("Termination request sent to Air Traffic Control System.\n");

            // Terminate cleanup process
            exit(EXIT_SUCCESS);
        } else if (response == 'N' || response == 'n') {
            // Continue running cleanup process
            continue;
        } else {
            printf("Invalid input. Please enter 'Y' or 'N'.\n");
        }
    }

    return 0;
}

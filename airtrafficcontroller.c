#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>

#define MAX_AIRPORTS 10
#define MAX_FILENAME_LENGTH 256

// Define message structure for communication between processes
struct msg_buffer {
    long msg_type;
    char msg_text[100];
} message;

// Function prototypes
void setup_message_queue(int *msg_queue_id);
void send_message(int msg_queue_id, long msg_type, char *msg_text);
void receive_message(int msg_queue_id, long msg_type, char *msg_text);
void log_plane_journey(int plane_id, int departure_airport, int arrival_airport);

int main() {
    //DELETE BLOCK
    key_t dkey = ftok("airtrafficcontroller.c", 'B');
    int dmsg_queue_id = msgget(dkey, IPC_CREAT | 0666);
    msgctl(dmsg_queue_id, IPC_RMID, NULL);
    
    int num_airports, msg_queue_id;
    char log_filename[MAX_FILENAME_LENGTH];
    bool termination_requested = false;
    int active_planes = 0;
    int departureAirports[10] = {0};
    int arrivalAirports[10] = {0};

    // Get the number of airports to be managed
    printf("Enter the number of airports to be handled/managed: ");
    scanf("%d", &num_airports);

    // Input validation for number of airports
    if (num_airports < 2 || num_airports > MAX_AIRPORTS) {
        printf("Invalid number of airports. Must be between 2 and %d.\n", MAX_AIRPORTS);
        exit(EXIT_FAILURE);
    }

    
    // Set up message queue
    setup_message_queue(&msg_queue_id);

    // Open log file for appending
    snprintf(log_filename, sizeof(log_filename), "AirTrafficController.txt");
    int log_fd = open(log_filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    if (log_fd == -1) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    //Set up Airports
    for(int i=1; i<=num_airports; i++) {
        send_message(msg_queue_id, i*10+9, "Set up airport message"); //msg type=X9 for airport setup messages
    }

    // Main logic
    while (1) {

        if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), 1000, IPC_NOWAIT) != -1) {
            /*printf("\nReceived Termination Request");
            fflush(stdout);*/
            termination_requested = true;
        }

        if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), 999, IPC_NOWAIT) != -1) { //A plane has come into existence
            active_planes++; // Increment active planes counter
        }
         
        //Receive message from plane
        if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), 200, IPC_NOWAIT) != -1) {
            // Log plane journey
            char msg_copy[sizeof(message.msg_text)]; // Make a copy of the message
            strcpy(msg_copy, message.msg_text);
            char *token = strtok(msg_copy, ",");
            int plane_id = atoi(token);
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            int departure_airport = atoi(token);
            token = strtok(NULL, ",");
            int arrival_airport = atoi(token);

            //printf("\nMessage: %s: ", message.msg_text);
            // Inform departure airport to begin boarding/loading and takeoff process
            send_message(msg_queue_id, departure_airport*10+1, message.msg_text); //msg type=X1 for departure message from atc
            departureAirports[departure_airport-1]++;
            //printf("\ndepartureAirports[%d]=%d", departure_airport, departureAirports[departure_airport-1]);


            send_message(msg_queue_id, arrival_airport*10+3, message.msg_text); //msg type=X3 for prepare for arrival message from atc
            arrivalAirports[arrival_airport-1]++;
            //printf("\narrivalAirports[%d]=%d", arrival_airport, arrivalAirports[arrival_airport-1]);

        }
            // Receive confirmation from departure airport about takeoff process completion
        for(long int i=0; i<num_airports; i++) {
            //printf("\n DEPARTURE CONFIRMATINO");
            //if(departureAirports[i+1]!=0) {
                //printf("\nIn departure confirmation, i+1: %d", i+1);
                if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), (i+1)*10+5, IPC_NOWAIT) != -1) {
                    //receive_message(msg_queue_id, departure_airport*10+5, message.msg_text); //msg type=X5 for departure confirmation message from airport
                    char msg_copy[sizeof(message.msg_text)]; // Make a copy of the message
                    strcpy(msg_copy, message.msg_text);
                    char *token = strtok(msg_copy, ",");
                    int plane_id = atoi(token);
                    token = strtok(NULL, ",");
                    token = strtok(NULL, ",");
                    token = strtok(NULL, ",");
                    token = strtok(NULL, ",");
                    int departure_airport = atoi(token);
                    token = strtok(NULL, ",");
                    int arrival_airport = atoi(token);

                    departureAirports[i+1]--;
                    send_message(msg_queue_id, plane_id, "Takeoff Done");
                    log_plane_journey(plane_id, departure_airport, arrival_airport);
                    
                    //printf("--Taken off--\n");
                }
            //}
        }

        if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), 300, IPC_NOWAIT) != -1) {
            //receive_message(msg_queue_id, 300, message.msg_text); //msg_type=300 for landing clearance
            // Inform arrival airport about plane arrival
            char msg_copy[sizeof(message.msg_text)]; // Make a copy of the message
            strcpy(msg_copy, message.msg_text);
            char *token = strtok(msg_copy, ",");
            int plane_id = atoi(token);
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            int departure_airport = atoi(token);
            token = strtok(NULL, ",");
            int arrival_airport = atoi(token);

            send_message(msg_queue_id, arrival_airport*10+2, message.msg_text); //msg type=X2 for arrival message from atc
        }

        for(int i=0; i<num_airports; i++) {
            //printf("\n ARRIVAL CONFIRMATION");
            //if(arrivalAirports[i+1]!=0) {
                //printf("\nIn arrival confirmation, i+1: %d", i+1);
                if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), (i+1)*10+6, IPC_NOWAIT) != -1) {
                    // Receive confirmation from arrival airport about landing and deboarding process completion
                    //receive_message(msg_queue_id, arrival_airport*10+6, message.msg_text); //msg type=X6 for arrival confirmation message from airport
                    char msg_copy[sizeof(message.msg_text)]; // Make a copy of the message
                    strcpy(msg_copy, message.msg_text);
                    char *token = strtok(msg_copy, ",");
                    int plane_id = atoi(token);
                    token = strtok(NULL, ",");
                    token = strtok(NULL, ",");
                    token = strtok(NULL, ",");
                    token = strtok(NULL, ",");
                    int departure_airport = atoi(token);
                    token = strtok(NULL, ",");
                    int arrival_airport = atoi(token);
                    // Inform plane process about completion of journey
                    send_message(msg_queue_id, plane_id, "Journey completed");
                    arrivalAirports[i+1]--;
                    active_planes--;
                    // Log arrival message
                    //log_plane_journey(plane_id, departure_airport, arrival_airport);
                }
            //}
        }
        
        if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), 999, IPC_NOWAIT) != -1) {
            active_planes++; // Increment active planes counter
        }

        // Check if termination is requested and no planes are left
        if (termination_requested && active_planes==0) {

            printf("\nPerforming termination tasks...");
            for (int i = 1; i <= num_airports; i++) {
                send_message(msg_queue_id, i*10 + 8, "Terminate");
            }
            break;
        }
        

    }

    // Close log file
    close(log_fd);

    // Clean up message queue
    msgctl(msg_queue_id, IPC_RMID, NULL);

    return 0;
}

// Function to set up message queue
void setup_message_queue(int *msg_queue_id) {
    // Generate a key for the message queue
    key_t key = ftok("airtrafficcontroller.c", 'B');
    if (key == -1) {
        perror("Error generating key for message queue");
        exit(EXIT_FAILURE);
    }

    // Create a message queue
    *msg_queue_id = msgget(key, IPC_CREAT | 0666);
    if (*msg_queue_id == -1) {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }
}

// Function to send message via message queue
void send_message(int msg_queue_id, long msg_type, char *msg_text) {
    //printf("\nInside send_message with msg_type %ld\n", msg_type);
    strcpy(message.msg_text, msg_text); 
    message.msg_type = msg_type;
    //printf("\nSending message = %s with msg_type= %ld", message.msg_text, msg_type);
    //fflush(stdout);
    // Send the message
    if (msgsnd(msg_queue_id, &message, sizeof(message.msg_text), 0) == -1) {
        perror("Error sending message");
        exit(EXIT_FAILURE);
    }
}

// Function to receive message via message queue
void receive_message(int msg_queue_id, long msg_type, char *msg_text) {
    // Receive the message
    if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), msg_type, 0) == -1) {
        perror("Error receiving message");
        exit(EXIT_FAILURE);
    }
    
    // Print received message for checking
    //printf("\nReceived message: %s and type %ld", message.msg_text, message.msg_type);
    //fflush(stdout);

    // Copy received message text
    strcpy(msg_text, message.msg_text);
}


// Function to log plane journey to file
void log_plane_journey(int plane_id, int departure_airport, int arrival_airport) {
    char log_entry[100];
    snprintf(log_entry, sizeof(log_entry), "Plane %d has departed from Airport %d and will land at Airport %d.\n", plane_id, departure_airport, arrival_airport);

    // Open log file for appending
    char log_filename[MAX_FILENAME_LENGTH];
    snprintf(log_filename, sizeof(log_filename), "AirTrafficController.txt");
    int log_fd = open(log_filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    if (log_fd == -1) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    // Write log entry to file
    if (write(log_fd, log_entry, strlen(log_entry)) == -1) {
        perror("Error writing to log file");
        exit(EXIT_FAILURE);
    }

    // Close log file
    close(log_fd);
}

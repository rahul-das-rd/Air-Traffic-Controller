#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdbool.h>

#define MAX_RUNWAYS 10
#define BACKUP_LOAD_CAPACITY 15000
#define MAX_DEPARTURES 50
#define MAX_ARRIVALS 50

// Define message structure for communication between processes
struct msg_buffer {
    long msg_type;
    char msg_text[100];
} message;

// Structure to represent a runway
struct Runway {
    int id;
    int load_capacity;
    pthread_mutex_t mutex;
    int is_available; // Flag to indicate if the runway is available (0 for unavailable, 1 for available)
};

// Global variables for runways and airport information
int airport_number;
int num_runways;
struct Runway runways[MAX_RUNWAYS];
struct Runway backup_runway;
int msg_queue_id;

// Function prototypes
void* handle_departure(void* arg);
void* handle_arrival(void* arg);

int main() {
    // Connect to the message queue of ATC
    key_t key = ftok("airtrafficcontroller.c", 'B');
    msg_queue_id = msgget(key, 0666);
    if (msg_queue_id == -1) {
        perror("Error connecting to message queue of ATC");
        exit(EXIT_FAILURE);
    }

    // Display prompt to enter airport number
    printf("Enter Airport Number: ");
    scanf("%d", &airport_number);

    if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), airport_number*10+9, IPC_NOWAIT) == -1) {
        perror("Error receiving message/Airport does not exist/Airport already exists");
        
        return EXIT_FAILURE;
    }

    // Display prompt to enter number of runways
    printf("Enter number of Runways: ");
    scanf("%d", &num_runways);

    // Input validation for number of runways
    if (num_runways < 1 || num_runways > MAX_RUNWAYS || num_runways%2==1) {
        printf("Invalid number of runways. It must be even and between 1 and %d.\n", MAX_RUNWAYS);
        //send_message(msg_queue_id, airport_number*10+9, "Set up airport message");
        exit(EXIT_FAILURE);
    }

    int loadCapacities[num_runways]; // Array to store load capacities
    printf("Enter loadCapacity of Runways (give as a space separated list in a single line): ");
    for (int i = 0; i < num_runways; i++) {
        scanf("%d", &loadCapacities[i]); // Read each load capacity into the array
        if(loadCapacities[i]<1000 || loadCapacities[i]>12000) {
            printf("\nLoad Capacities invalid, try again.");
            exit(EXIT_FAILURE);
        } 
    }

    for (int i = 0; i < num_runways; i++) {
        runways[i].load_capacity = loadCapacities[i]; // Assign load capacity from array to each runway
        runways[i].id = i + 1;
        pthread_mutex_init(&runways[i].mutex, NULL);
        runways[i].is_available = 1;
    }

    // Additional backup runway
    backup_runway.id = num_runways + 1;
    backup_runway.load_capacity = BACKUP_LOAD_CAPACITY;
    pthread_mutex_init(&backup_runway.mutex, NULL);
    backup_runway.is_available = 1;

    pthread_t departure_threads[MAX_DEPARTURES];
    pthread_t arrival_threads[MAX_ARRIVALS];

    int departure_thread_count = 0;
    int arrival_thread_count = 0;


    // Main logic to receive messages from ATC and handle departures and arrivals
    while (1) {

        if(msgrcv(msg_queue_id, &message, sizeof(message.msg_text), airport_number*10+1, IPC_NOWAIT)!=-1) { //Handle Departure
            //printf("\nReceived Departure message from ATC");
            struct msg_buffer *departure_msg = malloc(sizeof(struct msg_buffer));
            if (departure_msg == NULL) {
                perror("Error allocating memory for departure message");
                continue;
            }
            memcpy(departure_msg, &message, sizeof(struct msg_buffer)); // Copy message to departure message buffer

            // Create new thread to handle departure
            if (pthread_create(&departure_threads[departure_thread_count], NULL, handle_departure, (void*)departure_msg) != 0) {
                perror("Error creating departure thread");
                free(departure_msg);
                continue;
            }
            departure_thread_count++;
        }

        else if(msgrcv(msg_queue_id, &message, sizeof(message.msg_text), airport_number*10+3, IPC_NOWAIT)!=-1) { //Handle prepare for arrival
            //printf("\nReceived Prepare for arrival message from ATC");
            struct msg_buffer *arrival_msg = malloc(sizeof(struct msg_buffer));
            if (arrival_msg == NULL) {
                perror("Error allocating memory for arrival message");
                continue;
            }
            memcpy(arrival_msg, &message, sizeof(struct msg_buffer)); // Copy message to arrival message buffer

            // Create new thread to handle arrival
            if (pthread_create(&arrival_threads[arrival_thread_count], NULL, handle_arrival, (void*)arrival_msg) != 0) {
                perror("Error creating arrival thread");
                free(arrival_msg);
                continue;
            }
            arrival_thread_count++;
        }

        else if(msgrcv(msg_queue_id, &message, sizeof(message.msg_text), airport_number*10+8, IPC_NOWAIT)!=-1) { //Handle Termination
            printf("\nTerminating.");
            for (int i = 0; i < departure_thread_count; i++) {
                pthread_join(departure_threads[i], NULL);
            }

            // Wait for arrival threads to finish
            for (int i = 0; i < arrival_thread_count; i++) {
                pthread_join(arrival_threads[i], NULL);
            }

            break;
        }

        else {
            continue;
        }

    }
    // Clean up (mutex destruction)
    for (int i = 0; i < num_runways; i++) {
        pthread_mutex_destroy(&runways[i].mutex);
    }
    pthread_mutex_destroy(&backup_runway.mutex);

    return 0;
}

// Function to handle departure of a plane
void* handle_departure(void* arg) {
    struct msg_buffer *departure_msg = (struct msg_buffer *)arg;

    int plane_id, total_weight;
    sscanf(departure_msg->msg_text, "%d,%d", &plane_id, &total_weight);

    // Find a suitable runway for departure
    // Use the backup runway if none of the runways have sufficient load capacity
    struct Runway* selected_runway = NULL;
    bool foundPossibleRunway=false;

    for (int i = 0; i < num_runways; i++) {  
        if (runways[i].load_capacity >= total_weight) {
            foundPossibleRunway=true;
            pthread_mutex_lock(&runways[i].mutex); // Lock the mutex for the runway
            if (runways[i].is_available) {
                selected_runway = &runways[i];
                runways[i].is_available = 0; // Set the runway as unavailable
                pthread_mutex_unlock(&runways[i].mutex);
                break;
            }
            pthread_mutex_unlock(&runways[i].mutex); // Unlock the mutex for the runway
        }
        
    }

    if (selected_runway == NULL) {
        if(foundPossibleRunway) {
            while(selected_runway==NULL) {
                for (int i = 0; i < num_runways; i++) {
                    pthread_mutex_lock(&runways[i].mutex);
                    if (runways[i].is_available) {
                        runways[i].is_available = 0;
                        selected_runway = &runways[i];
                        pthread_mutex_unlock(&runways[i].mutex);
                        break;
                    }
                    pthread_mutex_unlock(&runways[i].mutex);
                }
            }
        }


        else {
            while(selected_runway==NULL) {
                pthread_mutex_lock(&backup_runway.mutex); // Lock the mutex for the backup runway
                if (backup_runway.is_available) {
                    selected_runway = &backup_runway;
                    backup_runway.is_available = 0; // Set the backup runway as unavailable
                }
                pthread_mutex_unlock(&backup_runway.mutex); // Unlock the mutex for the backup runway
            }
        }
    }
    


    // Simulate boarding/loading process
    printf("\nPlane %d Boarding...", plane_id);
    fflush(stdout);
    sleep(3);

    

    // Simulate takeoff
    printf("\nPlane %d Taking off...", plane_id);
    fflush(stdout);
    sleep(2);
    
    printf("\nPlane %d has completed boarding/loading and has taken off from Runway No. %d of Airport No. %d.\n",plane_id, selected_runway->id, airport_number);
    pthread_mutex_lock(&selected_runway->mutex);
    selected_runway->is_available = 1;
    pthread_mutex_unlock(&selected_runway->mutex);
    
    
    // Inform ATC about successful takeoff
    departure_msg->msg_type = airport_number*10+5; // Update msg_type to inform ATC that takeoff has occurred successfully
    //printf("\nReceived departure message: %s", departure_msg->msg_text);
    // sprintf(departure_msg->msg_text, "Take off confirmation of plane %d", plane_id);
    if (msgsnd(msg_queue_id, departure_msg, sizeof(departure_msg->msg_text), 0) == -1) {
        perror("Error sending takeoff message to ATC");
    }
    fflush(stdout);

    free(departure_msg); // Free memory allocated for message buffer

    pthread_exit(NULL);
}

// Function to handle arrival of a plane
void* handle_arrival(void* arg) {
    struct msg_buffer *arrival_msg = (struct msg_buffer *)arg;

    int plane_id, total_weight;
    sscanf(arrival_msg->msg_text, "%d,%d", &plane_id, &total_weight);

    // Find a suitable runway for arrival
    // Use the backup runway if none of the runways have sufficient load capacity
    struct Runway* selected_runway = NULL;
    bool foundPossibleRunway=false;

    for (int i = 0; i < num_runways; i++) {  
        if (runways[i].load_capacity >= total_weight) {
            foundPossibleRunway=true;
            pthread_mutex_lock(&runways[i].mutex); // Lock the mutex for the runway
            if (runways[i].is_available) {
                selected_runway = &runways[i];
                runways[i].is_available = 0; // Set the runway as unavailable
                pthread_mutex_unlock(&runways[i].mutex);
                break;
            }
            pthread_mutex_unlock(&runways[i].mutex); // Unlock the mutex for the runway
        }
        
    }

    if (selected_runway == NULL) {
        if(foundPossibleRunway) {
            while(selected_runway==NULL) {
                for (int i = 0; i < num_runways; i++) {
                    pthread_mutex_lock(&runways[i].mutex);
                    if (runways[i].is_available) {
                        runways[i].is_available = 0;
                        selected_runway = &runways[i];
                        pthread_mutex_unlock(&runways[i].mutex);
                        break;
                    }
                    pthread_mutex_unlock(&runways[i].mutex);
                }
            }
        }


        else {
            while(selected_runway==NULL) {
                pthread_mutex_lock(&backup_runway.mutex); // Lock the mutex for the backup runway
                if (backup_runway.is_available) {
                    selected_runway = &backup_runway;
                    backup_runway.is_available = 0; // Set the backup runway as unavailable
                }
                pthread_mutex_unlock(&backup_runway.mutex); // Unlock the mutex for the backup runway
            }
        }
    }


    printf("\nWaiting for 'arrival of plane %d' message...", plane_id);
    fflush(stdout);
    if(msgrcv(msg_queue_id, &message, sizeof(message.msg_text), airport_number*10+2, 0)==-1) {
        perror("Error receiving arrival message from ATC");
        return NULL;
    }

    printf("\nPlane %d Landing...", plane_id);
    fflush(stdout);
    sleep(2);

    pthread_mutex_lock(&selected_runway->mutex);
    selected_runway->is_available = 1;
    pthread_mutex_unlock(&selected_runway->mutex);

    // Simulate deboarding/unloading process
    printf("\nPlane %d De-Boarding...", plane_id);
    fflush(stdout);
    sleep(3);

    printf("\nPlane %d has landed on Runway No. %d of Airport No. %d and has completed deboarding/unloading.\n", plane_id, selected_runway->id, airport_number);

    // Inform ATC about successful arrival and deboarding
    arrival_msg->msg_type = airport_number*10+6; // Update msg_type to inform ATC about successful landing and deboarding
    // Optionally, update the msg_text if needed to reflect the successful landing and deboarding
    // sprintf(arrival_msg->msg_text, "Landing and deboarding confirmation of plane %d", plane_id);
    if (msgsnd(msg_queue_id, arrival_msg, sizeof(arrival_msg->msg_text), 0) == -1) {
        perror("Error sending arrival acknowledgment message to ATC");
    }
    fflush(stdout);

    free(arrival_msg); // Free memory allocated for message buffer

    pthread_exit(NULL);
}

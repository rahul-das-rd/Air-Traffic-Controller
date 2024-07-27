#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

// Define constants
#define MAX_AIRPORTS 10

// Define message structure for communication between processes
struct msg_buffer {
    long msg_type;
    char msg_text[100];
} message;

// Define a structure to hold plane information
struct Plane {
    int plane_id;
    int total_weight;
    int type;
    int num_passengers;
    int departure_airport;
    int arrival_airport;
};

// Function prototypes
void create_passenger_processes(struct Plane *plane, int num_passengers);
void send_message_to_ATC(int msg_queue_id, struct Plane plane, long int type);
void receive_confirmation_from_ATC(int msg_queue_id, struct Plane plane);
void display_departure_message(struct Plane plane);
void display_arrival_message(struct Plane plane);
int connect_to_message_queue();

int main() {
    int plane_id, type, num_occupied_seats, num_cargo_items, avg_cargo_weight, departure_airport, arrival_airport;
    struct Plane plane;

    // Connect to message queue
    int msg_queue_id = connect_to_message_queue();

    
    // Get unique plane ID from user
    printf("Enter Plane ID: ");
    scanf("%d", &plane_id);

    // Get type of plane from user
    printf("Enter Type of Plane (1 for passenger, 0 for cargo): ");
    scanf("%d", &type);

    // Input validation for plane type
    if (type != 0 && type != 1) {
        printf("Invalid input for plane type.\n");
        exit(EXIT_FAILURE);
    }

    // Populate plane structure with common attributes
    plane.plane_id = plane_id;
    plane.type = type;

    if (type == 1) { // Passenger plane
        // Get number of occupied seats (passengers) from user
        printf("Enter Number of Occupied Seats: ");
        scanf("%d", &num_occupied_seats);
        plane.num_passengers = num_occupied_seats;

        // Create passenger plane process
        create_passenger_processes(&plane, num_occupied_seats);
        if(plane.total_weight>15000) {
            printf("\nPlane is too heavy.");
            exit(EXIT_FAILURE);
        }

    } else { // Cargo plane
        plane.num_passengers=0;
        // Get number of cargo items from user
        printf("Enter Number of Cargo Items: ");
        scanf("%d", &num_cargo_items);

        // Get average weight of cargo items from user
        printf("Enter Average Weight of Cargo Items: ");
        scanf("%d", &avg_cargo_weight);
        
        plane.total_weight = num_cargo_items * avg_cargo_weight + 75 * 2;
        printf("\nTotal weight of the plane with cargo and crew members: %d\n", plane.total_weight);
        if(plane.total_weight>15000) {
            printf("\nPlane is too heavy.");
            exit(EXIT_FAILURE);
        }
    }

    // Get departure airport from user
    printf("\nEnter Airport Number for Departure: ");
    scanf("%d", &departure_airport);

    // Get arrival airport from user
    printf("Enter Airport Number for Arrival: ");
    scanf("%d", &arrival_airport);
    if(departure_airport==arrival_airport) { 
        printf("\nDeparture and arrival airports cannot be the same.");
        exit(EXIT_FAILURE);
    }
    // Populate departure and arrival airports in plane structure
    plane.departure_airport = departure_airport;
    plane.arrival_airport = arrival_airport;

    message.msg_type = 999; // Set message type to 99 to indicate a new plane
    strcpy(message.msg_text, ""); // Empty message text

    // Send the message
    if (msgsnd(msg_queue_id, &message, sizeof(message.msg_text), 0) == -1) {
        perror("Error sending message");
        exit(EXIT_FAILURE);
    }
    //printf("\nSent the 99 message");
    

    // Send plane details to ATC
    send_message_to_ATC(msg_queue_id, plane, 200); //For takeoff clearance
    // Receive Takeoff confirmation from atc
    receive_confirmation_from_ATC(msg_queue_id, plane);
    printf("\nFlying to arrival airport(30 seconds)...");
    fflush(stdout);
    sleep(30);
    // Send arrival message to atc
    send_message_to_ATC(msg_queue_id, plane, 300); //For landing clearance
    // Receive confirmation from ATC
    receive_confirmation_from_ATC(msg_queue_id, plane);
    printf("\nPlane %d has successfully traveled from Airport %d to Airport %d!\n", plane.plane_id, plane.departure_airport, plane.arrival_airport);
    return 0;
}

// Function to create passenger processes for a passenger plane
void create_passenger_processes(struct Plane *plane, int num_passengers) {
    int i;
    pid_t pid;
    int pipe_fd[num_passengers][2];

    // Add weight of crew members (7 crew members with average weight of 75 kg)
    plane->total_weight += 7 * 75;

    // Create a child process for each passenger
    for (i = 0; i < num_passengers; i++) {
        // Create pipe for communication between plane process and passenger process
        if (pipe(pipe_fd[i]) == -1) {
            perror("Pipe creation failed");
            exit(EXIT_FAILURE);
        }

        // Fork a child process
        pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Child process (passenger)

            printf("\nPassenger %d's details: ", i+1); //Chnage getpid() to i later
            close(pipe_fd[i][0]); // Close read end of pipe

            // TODO: Implement passenger logic
            int luggage_weight, body_weight;
            printf("\nEnter Weight of Your Luggage: ");
            scanf("%d", &luggage_weight);
            printf("\nEnter Your Body Weight: ");
            scanf("%d", &body_weight);

            // Send luggage and body weight to plane process through pipe
            write(pipe_fd[i][1], &luggage_weight, sizeof(int));
            write(pipe_fd[i][1], &body_weight, sizeof(int));

            close(pipe_fd[i][1]); // Close write end of pipe
            exit(EXIT_SUCCESS);


        } else { // Parent process (plane)
            close(pipe_fd[i][1]); // Close write end of pipe

            // Receive data from passenger process through pipe
            int luggage_weight, body_weight;
            read(pipe_fd[i][0], &luggage_weight, sizeof(int));
            read(pipe_fd[i][0], &body_weight, sizeof(int));
            //printf("\nThe luggage weight: %d and body weight is: %d ",luggage_weight, body_weight);
            // Accumulate weights to total weight of the plane
            plane->total_weight += luggage_weight + body_weight;
            //printf("\nThe total weight of plane till now is: %d \n",plane->total_weight);
            close(pipe_fd[i][0]); // Close read end of pipe
            wait(NULL);
        }
    }

    printf("\nTotal weight of the plane with passengers and crew members: %d", plane->total_weight);
}

// Function to send message to Air Traffic Controller (ATC)
void send_message_to_ATC(int msg_queue_id, struct Plane plane, long int type) {
    //printf("\nSending message to ATC ");
    // Convert plane details to a string
    char msg_text[100];
    sprintf(msg_text, "%d,%d,%d,%d,%d,%d", plane.plane_id, plane.total_weight, plane.type, plane.num_passengers, plane.departure_airport, plane.arrival_airport);

    // Send message to ATC
    strcpy(message.msg_text, msg_text);
    message.msg_type = type; // For Plane->ATC messages, the message type is 200

    // Send the message
    if (msgsnd(msg_queue_id, &message, sizeof(message.msg_text), 0) == -1) {
        perror("Error sending message");
        exit(EXIT_FAILURE);
    }
}

// Function to receive confirmation from Air Traffic Controller (ATC)
void receive_confirmation_from_ATC(int msg_queue_id, struct Plane plane) {
    // Receive confirmation from ATC
    char confirmation_msg[100]; 
    if (msgrcv(msg_queue_id, &message, sizeof(message.msg_text), plane.plane_id, 0) == -1) { //For ATC->Plane type message, the message type is plane id
        perror("Error receiving message");
        exit(EXIT_FAILURE);
    }
    strcpy(confirmation_msg, message.msg_text);
    //printf("\nReceived confirmation from ATC: %s", confirmation_msg);
}

// Function to display departure message
void display_departure_message(struct Plane plane) {
    printf("\nPlane %d has successfully departed from Airport %d!\n", plane.plane_id, plane.departure_airport);
}

// Function to display arrival message
void display_arrival_message(struct Plane plane) {
    printf("\nPlane %d has successfully arrived at Airport %d!\n", plane.plane_id, plane.arrival_airport);
}

// Function to connect to the message queue created by ATC
int connect_to_message_queue() {
    // Get the message queue ID
    int msg_queue_id = msgget(ftok("airtrafficcontroller.c", 'B'), 0666);
    if (msg_queue_id == -1) {
        perror("Error connecting to message queue");
        exit(EXIT_FAILURE);
    }
    return msg_queue_id;
}

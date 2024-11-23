#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

// Constants
#define MAXR 1000 // Maximum number of resource types
#define MAXT 100  // Maximum number of threads

// Global Variables
int available[MAXR];           // Available resources
int max_claim[MAXT][MAXR];     // Maximum claims of threads
int allocation[MAXT][MAXR];    // Allocated resources
int request[MAXT][MAXR];       // Pending requests
int total_threads = 0;         // Total threads
int total_resources = 0;       // Total resource types
int deadlock_avoidance = 0;    // Deadlock avoidance flag

pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for synchronization
pthread_cond_t resource_cond = PTHREAD_COND_INITIALIZER;    // Condition variable

// Function Declarations
int reman_init(int t_count, int r_count, int avoid);
int reman_connect(int tid);
int reman_claim(int claim[]);
int reman_request(int request[]);
int reman_release(int release[]);
int reman_disconnect();
int reman_detect();
void reman_print(char titlemsg[]);

// Main Function (for demonstration purposes)
int main() {
    int t_count = 3; // Number of threads
    int r_count = 5; // Number of resource types
    int avoid = 1;   // Deadlock avoidance flag

    if (reman_init(t_count, r_count, avoid) == -1) {
        printf("Initialization failed.\n");
        return 1;
    }

    printf("Initialization successful.\n");

    // Example claims and requests
    int claim[] = {1, 1, 0, 0, 1};
    int request1[] = {1, 0, 0, 0, 1};
    int release[] = {1, 0, 0, 0, 1};

    if (reman_connect(0) == -1) {
        printf("Thread connection failed.\n");
        return 1;
    }

    if (reman_claim(claim) == -1) {
        printf("Claim failed.\n");
        return 1;
    }

    if (reman_request(request1) == -1) {
        printf("Resource request failed.\n");
        return 1;
    }

    reman_print("State after request:");

    if (reman_release(release) == -1) {
        printf("Resource release failed.\n");
        return 1;
    }

    reman_print("State after release:");
    reman_disconnect();
    return 0;
}

// Function Definitions
int reman_init(int t_count, int r_count, int avoid) {
    if (t_count > MAXT || r_count > MAXR || t_count <= 0 || r_count <= 0) {
        return -1; // Invalid parameters
    }

    pthread_mutex_lock(&resource_mutex);
    total_threads = t_count;
    total_resources = r_count;
    deadlock_avoidance = avoid;

    for (int i = 0; i < r_count; i++) {
        available[i] = 1; // Assuming one instance per resource
    }

    for (int i = 0; i < t_count; i++) {
        for (int j = 0; j < r_count; j++) {
            max_claim[i][j] = 0;
            allocation[i][j] = 0;
            request[i][j] = 0;
        }
    }
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_connect(int tid) {
    if (tid < 0 || tid >= total_threads) {
        return -1; // Invalid thread ID
    }
    return 0;
}

int reman_claim(int claim[]) {
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_resources; i++) {
        if (claim[i] < 0 || claim[i] > 1) {
            pthread_mutex_unlock(&resource_mutex);
            return -1; // Invalid claim array
        }
        max_claim[0][i] = claim[i]; // Store claim for the thread
    }

    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_request(int request[]) {
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_resources; i++) {
        if (request[i] < 0 || request[i] > 1) {
            pthread_mutex_unlock(&resource_mutex);
            return -1; // Invalid request array
        }
        if (request[i] > available[i]) {
            pthread_cond_wait(&resource_cond, &resource_mutex); // Wait for resources
        }
        allocation[0][i] += request[i];
        available[i] -= request[i];
    }

    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_release(int release[]) {
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_resources; i++) {
        if (release[i] < 0 || release[i] > allocation[0][i]) {
            pthread_mutex_unlock(&resource_mutex);
            return -1; // Invalid release array
        }
        allocation[0][i] -= release[i];
        available[i] += release[i];
    }

    pthread_cond_broadcast(&resource_cond); // Notify waiting threads
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int reman_disconnect() {
    pthread_mutex_lock(&resource_mutex);

    for (int i = 0; i < total_resources; i++) {
        if (allocation[0][i] > 0) { // Release all allocated resources for the thread
            available[i] += allocation[0][i];
            allocation[0][i] = 0;
        }
    }

    pthread_cond_broadcast(&resource_cond); // Notify waiting threads
    pthread_mutex_unlock(&resource_mutex);
    return 0; // Success
}

int reman_detect() {
    int work[MAXR];        // Copy of available resources
    int finish[MAXT] = {0}; // Tracks whether each thread can complete
    int deadlocked_count = 0;

    pthread_mutex_lock(&resource_mutex);

    // Initialize work with available resources
    for (int i = 0; i < total_resources; i++) {
        work[i] = available[i];
    }

    // Safety check using a simplified Banker's Algorithm approach
    for (int t = 0; t < total_threads; t++) {
        int can_finish = 1;
        for (int r = 0; r < total_resources; r++) {
            if (request[t][r] > work[r]) {
                can_finish = 0;
                break;
            }
        }
        if (can_finish) {
            finish[t] = 1; // Thread t can complete
            for (int r = 0; r < total_resources; r++) {
                work[r] += allocation[t][r]; // Release its resources
            }
        }
    }

    // Count deadlocked threads
    for (int t = 0; t < total_threads; t++) {
        if (!finish[t]) {
            deadlocked_count++;
        }
    }

    pthread_mutex_unlock(&resource_mutex);

    if (deadlocked_count > 0) {
        return deadlocked_count; // Return number of deadlocked threads
    } else {
        return 0; // No deadlock detected
    }
}


void reman_print(char titlemsg[]) {
    pthread_mutex_lock(&resource_mutex);
    printf("##########################\n");
    printf("%s\n", titlemsg);
    printf("###########################\n");
    printf("Resource Count: %d\n", total_resources);
    printf("Thread Count: %d\n", total_threads);

    printf("Available Resources:\n");
    for (int i = 0; i < total_resources; i++) {
        printf("R%d: %d ", i, available[i]);
    }
    printf("\n");

    printf("Allocation Matrix:\n");
    for (int i = 0; i < total_threads; i++) {
        for (int j = 0; j < total_resources; j++) {
            printf("%d ", allocation[i][j]);
        }
        printf("\n");
    }
    printf("###########################\n");

    pthread_mutex_unlock(&resource_mutex);
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>
#include <pthread.h>
#include "common.h"

#define MAXPENDING 5

const char *shared_object = "/posix-shared-object";
const char *sem_shared_object = "/posix-sem-shared-object";
const char *observers_shared_object = "/posix-observers-shared-object";

int pipe_fd[2];

int createServerSocket(in_addr_t sin_addr, int port) {
    int server_socket;
    struct sockaddr_in server_address;

    if ((server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Unable to create server socket");
        exit(-1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = sin_addr;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Unable to bind address");
        exit(-1);
    }

    if (listen(server_socket, MAXPENDING) < 0) {
        perror("Unable to listen server socket");
        exit(-1);
    }

    return server_socket;
}

int acceptClientConnection(int server_socket) {
    int client_socket;
    struct sockaddr_in client_address;
    unsigned int address_length;

    address_length = sizeof(client_address);

    if ((client_socket =
             accept(server_socket, (struct sockaddr *)&client_address, &address_length)) < 0) {
        perror("Unable to accept client connection");
        exit(-1);
    }

    printf("Handling client %s:%d\n", inet_ntoa(client_address.sin_addr), client_address.sin_port);

    return client_socket;
}

void printField(int *field, int columns, int rows) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
            if (field[i * columns + j] < 0) {
                printf("X ");
            } else {
                printf("%d ", field[i * columns + j]);
            }
        }
        printf("\n");
    }

    fflush(stdout);
}

void sprintField(char *buffer, int *field, int columns, int rows) {
    int offset = 0;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
            if (field[i * columns + j] < 0) {
                offset += sprintf(buffer + offset, "X ");
            } else {
                offset += sprintf(buffer + offset, "%d ", field[i * columns + j]);
            }
        }
        offset += sprintf(buffer + offset, "\n");
    }
}

void setEventWithCurrentTime(struct Event *event) {
    time_t timer;
    struct tm *tm_info;
    timer = time(NULL);
    tm_info = localtime(&timer);
    strftime(event->timestamp, sizeof(event->timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
}

void writeEventToPipe(struct Event *event) {
    if (write(pipe_fd[1], event, sizeof(*event)) < 0) {
        perror("Can't write to pipe");
        exit(-1);
    }
}

void handleGardenPlot(sem_t *semaphores, int *field, int columns, struct GardenerTask task) {
    sem_wait(semaphores + (task.plot_i / 2 * (columns / 2) + task.plot_j / 2));

    struct Event gardener_event;
    setEventWithCurrentTime(&gardener_event);
    gardener_event.type = ACTION;
    sprintf(gardener_event.buffer, "Gardener %d takes (row: %d, col: %d) plot\n", task.gardener_id,
            task.plot_i, task.plot_j);
    writeEventToPipe(&gardener_event);

    if (field[task.plot_i * columns + task.plot_j] == 0) {
        field[task.plot_i * columns + task.plot_j] = task.gardener_id;
        usleep(task.working_time * 1000);
    } else {
        usleep(task.working_time / EMPTY_PLOT_COEFFICIENT * 1000);
    }

    struct Event event;
    setEventWithCurrentTime(&event);
    sprintf(event.buffer, "\n");
    sprintField(event.buffer + 1, field, columns, columns);
    event.type = MAP;
    writeEventToPipe(&event);

    struct Event finish_event;
    setEventWithCurrentTime(&finish_event);
    finish_event.type = ACTION;
    sprintf(finish_event.buffer, "Gardener %d finish work\n", task.gardener_id);
    writeEventToPipe(&gardener_event);

    sem_post(semaphores + (task.plot_i / 2 * (columns / 2) + task.plot_j / 2));
}

void handle(int client_socket, sem_t *semaphores, int *field, struct FieldSize field_size) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    if (send(client_socket, (char *)(&field_size), sizeof(field_size), 0) != sizeof(field_size)) {
        perror("send() bad");
        exit(-1);
    }

    // Десериализация объекта
    struct GardenerTask task;
    const int plot_handle_status = 1;

    if ((bytes_received = recv(client_socket, buffer, sizeof(struct GardenerTask), 0)) < 0) {
        perror("recv() bad");
        exit(-1);
    }
    task = *((struct GardenerTask *)buffer);

    while (task.status != 1) {
        handleGardenPlot(semaphores, field, field_size.columns, task);

        if (send(client_socket, &plot_handle_status, sizeof(int), 0) != sizeof(int)) {
            perror("send() bad");
            exit(-1);
        }

        if ((bytes_received = recv(client_socket, buffer, sizeof(struct GardenerTask), 0)) < 0) {
            perror("recv() bad");
            exit(-1);
        }
        task = *((struct GardenerTask *)buffer);
    }

    if (send(client_socket, &plot_handle_status, sizeof(int), 0) != sizeof(int)) {
        perror("send() bad");
        exit(-1);
    }

    close(client_socket);
}

int *getField(int field_size) {
    int *field;
    int shmid;

    if ((shmid = shm_open(shared_object, O_CREAT | O_RDWR, 0666)) < 0) {
        perror("Can't connect to shared memory");
        exit(-1);
    } else {
        if (ftruncate(shmid, field_size * sizeof(int)) < 0) {
            perror("Can't resize shared memory");
            exit(-1);
        }
        if ((field = mmap(0, field_size * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, shmid,
                          0)) < 0) {
            printf("Can't connect to shared memory\n");
            exit(-1);
        };
        printf("Open shared Memory\n");
    }

    return field;
}

struct Observer *getObserversMemory() {
    struct Observer *observers;
    int shmid;

    if ((shmid = shm_open(observers_shared_object, O_CREAT | O_RDWR, 0666)) < 0) {
        perror("Can't connect to shared memory");
        exit(-1);
    } else {
        if (ftruncate(shmid, 100 * sizeof(struct Observer)) < 0) {
            perror("Can't resize shared memory");
            exit(-1);
        }
        if ((observers = mmap(0, 100 * sizeof(struct Observer), PROT_WRITE | PROT_READ, MAP_SHARED,
                              shmid, 0)) < 0) {
            printf("Can't connect to shared memory\n");
            exit(-1);
        };
        printf("Open shared Memory with observers\n");
    }

    return observers;
}

void initializeField(int *field, int rows, int columns) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
            field[i * columns + j] = 0;
        }
    }

    int percentage = 10 + random() % 20;
    int count_of_bad_plots = columns * rows * percentage / 100;
    for (int i = 0; i < count_of_bad_plots; ++i) {
        int row_index;
        int column_index;
        do {
            row_index = random() % rows;
            column_index = random() % columns;
        } while (field[row_index * columns + column_index] == -1);

        field[row_index * columns + column_index] = -1;
    }
}

void createSemaphores(sem_t *semaphores, int count) {
    for (int k = 0; k < count; ++k) {
        if (sem_init(semaphores + k, 1, 1) < 0) {
            perror("sem_init: can not create semaphore");
            exit(-1);
        };

        int val;
        sem_getvalue(semaphores + k, &val);
        if (val != 1) {
            printf(
                "Ooops, one of semaphores can't set initial value to 1. Please, restart server.\n");
            shm_unlink(shared_object);
            exit(-1);
        }
    }
}

sem_t *createSemaphoresSharedMemory(int sem_count) {
    int sem_main_shmid;
    sem_t *semaphores;

    if ((sem_main_shmid = shm_open(sem_shared_object, O_CREAT | O_RDWR, 0666)) < 0) {
        perror("Can't connect to shared memory");
        exit(-1);
    } else {
        if (ftruncate(sem_main_shmid, sem_count * sizeof(sem_t)) < 0) {
            perror("Can't rezie shm");
            exit(-1);
        }
        if ((semaphores = mmap(0, sem_count * sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED,
                               sem_main_shmid, 0)) < 0) {
            printf("Can\'t connect to shared memory for semaphores\n");
            exit(-1);
        };
        printf("Open shared Memory for semaphores\n");
    }

    return semaphores;
}

struct Observer *observers;

void *writeInfoToConsole(void *args) {
    sem_t *sem = (sem_t *)args;
    while (1) {
        struct Event event;
        if (read(pipe_fd[0], &event, sizeof(event)) < 0) {
            perror("Can't read from pipe");
            exit(-1);
        }
        if (event.type == MAP) {
            printf("%s | %s\n", event.timestamp, event.buffer);
        }
        char buffer[sizeof(event.timestamp) + sizeof(event.buffer) + 3];
        int size = sprintf(buffer, "%s | %s\n", event.timestamp, event.buffer);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        sem_wait(sem);
        for (int i = 0; i < 100; ++i) {
            if (observers[i].is_active == 1) {
                if (send(observers[i].socket, buffer, size, MSG_NOSIGNAL) < 0) {
                    observers[i].is_active = 0;
                    close(observers[i].socket);
                    printf("Observer disconnected\n");
                }
            }
        }
        sem_post(sem);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
}

void runWriter(sem_t *sem) {
    pthread_t thread;

    pthread_create(&thread, NULL, writeInfoToConsole, (void *)sem);
}

struct Args {
    int socket;
    sem_t *sem;
};

void *registerObservers(void *args) {
    struct Args data = *((struct Args *)args);
    while (1) {
        int client_socket = acceptClientConnection(data.socket);
        printf("Observer connected\n");
        struct Observer observer;
        observer.is_new = 1;
        observer.socket = client_socket;
        observer.is_active = 1;

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        sem_wait(data.sem);
        for (int i = 0; i < 100; ++i) {
            if (observers[i].is_active == 0) {
                observers[i] = observer;
                break;
            }
        }
        sem_post(data.sem);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
}

pid_t runObserverRegistrator(struct Args *args) {
    struct Observer observer;
    observer.is_active = 0;
    for (int i = 0; i < 100; ++i) {
        observers[i] = observer;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, registerObservers, (void *)args);
}

int server_socket;
int observer_socket;
int children_counter = 0;

void waitChildProcessess() {
    while (children_counter > 0) {
        int child_id = waitpid((pid_t)-1, NULL, 0);
        if (child_id < 0) {
            perror("Unable to wait child proccess");
            exit(-1);
        } else {
            children_counter--;
        }
    }
}

void sigint_handler(int signum) {
    printf("Server stopped\n");
    waitChildProcessess();
    shm_unlink(shared_object);
    shm_unlink(sem_shared_object);
    shm_unlink(observers_shared_object);
    close(server_socket);
    close(observer_socket);
    exit(0);
}

int main(int argc, char *argv[]) {

    if (argc != 5) {
        fprintf(stderr,
                "Usage:  %s <Server Address> <Server Port> <Observer Port> <Square side size>\n",
                argv[0]);
        exit(1);
    }

    in_addr_t server_address;
    if ((server_address = inet_addr(argv[1])) < 0) {
        perror("Invalid server address");
        exit(-1);
    }

    int server_port = atoi(argv[2]);
    if (server_port < 0) {
        perror("Invalid server port");
        exit(-1);
    }

    int observer_port = atoi(argv[3]);
    if (observer_port < 0) {
        perror("Invalid observer port");
        exit(-1);
    }

    if (pipe(pipe_fd) < 0) {
        perror("Can't open pipe");
        exit(-1);
    }

    // TODO: Создается поле, по которому ходят форки
    int square_side_size = atoi(argv[4]);
    int rows = 2 * square_side_size;
    int columns = 2 * square_side_size;
    // +1 под семафор для observers
    int sem_count = rows * columns / 4 + 1;

    int *field = getField(rows * columns);
    initializeField(field, rows, columns);

    sem_t *semaphores = createSemaphoresSharedMemory(sem_count);
    createSemaphores(semaphores, sem_count);

    observers = getObserversMemory();

    server_socket = createServerSocket(server_address, server_port);
    observer_socket = createServerSocket(server_address, observer_port);

    runWriter(semaphores + sem_count - 1);

    struct Args args;
    args.socket = observer_socket;
    args.sem = semaphores + sem_count - 1;
    runObserverRegistrator(&args);

    signal(SIGINT, sigint_handler);

    printField(field, columns, rows);
    while (1) {
        int client_socket = acceptClientConnection(server_socket);

        pid_t child_id;
        if ((child_id = fork()) < 0) {
            perror("Unable to create child for handling new connection");
            exit(-1);
        } else if (child_id == 0) {
            struct FieldSize field_size;
            field_size.columns = columns;
            field_size.rows = rows;

            signal(SIGINT, SIG_DFL);
            close(server_socket);
            handle(client_socket, semaphores, field, field_size);
            exit(0);
        }

        printf("with child process: %d\n", (int)child_id);
        close(client_socket);
        children_counter++;
    }

    return 0;
}

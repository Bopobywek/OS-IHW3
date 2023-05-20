
struct GardenerTask {
    int plot_i;
    int plot_j;
    int gardener_id;
    int working_time;
    int status;
};

struct FieldSize {
    int rows;
    int columns;
};

#define EMPTY_PLOT_COEFFICIENT 2
#define BUFFER_SIZE 256

int createClientSocket(char *server_ip, int server_port);
void sendHandleRequest(int client_socket, struct GardenerTask task);
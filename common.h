struct GardenerTask
{
    int plot_i;
    int plot_j;
    int gardener_id;
    int working_time;
    int status;
};

struct FieldSize
{
    int rows;
    int columns;
};

#define EMPTY_PLOT_COEFFICIENT 2
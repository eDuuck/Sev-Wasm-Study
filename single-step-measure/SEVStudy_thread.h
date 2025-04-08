typedef struct
{
    int apic_timer;
    char *format_prefix;
    bool debug_enabled;
    bool print_meas;
} single_step_measure_args;



bool is_measure_active();

void *mock_thread();
void *meas_thread();
void *inside_pingpong_measure();
void *track_pingpong();

void set_gpas(uint64_t i_gpa1, uint64_t i_gpa2);

void start_meas();
void stop_meas();

int init_CTX(single_step_measure_args *arguments);
int en_single_step();
int close_CTX();
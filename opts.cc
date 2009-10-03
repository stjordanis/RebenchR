
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "opts.hpp"

int HARDWARE_BLOCK_SIZE = 512;

void init_workload_config(workload_config_t *config) {
    bzero(config, sizeof(*config));
    config->silent = 0;
    config->threads = 1;
    config->block_size = HARDWARE_BLOCK_SIZE;
    config->duration = 10;
    config->stride = HARDWARE_BLOCK_SIZE;
    config->device[0] = NULL;
    config->direct_io = 1;
    config->local_fd = 0;
    config->buffered = 0;
    config->do_atime = 0;
    config->append_only = 0;
    config->workload = wl_rnd;
    config->io_type = iot_stateful;
    config->direction = opd_forward;
    config->operation = op_read;
    config->dist = rdt_uniform;
    config->sigma = -1;
}

void usage(const char *name) {
    printf("Usage:\n");
    printf("\t%s [OPTIONS] DEVICE\n", name);

    printf("\n\tAlternatively, if no arguments are passed on the command line, %s will read\n"\
           "\tstandard input and read its arguments from there. Multiple lines may be used\n"\
           "\tto specify multiple concurrent workloads.\n", name);
    
    printf("\nArguments:\n");
    printf("\tDEVICE - device or file name to perform operations on.\n");
    
    printf("\nOptions:\n");
    printf("\t-d, --duration\n\t\tDuration of the benchmark in seconds.\n");
    printf("\t-c, --threads\n\t\tNumbers of threads used to run the benchmark.\n");
    printf("\t-b, --block_size\n\t\tSize of blocks in bytes to use for IO operations.\n");
    printf("\t-s, --stride\n\t\tSize of stride in bytes (only applies to sequential workloads).\n");
    
    printf("\t-w, --workload\n\t\tDescription of a workload to perform.\n");
    printf("\t\tValid options are 'rnd' for a random load, and 'seq' for a sequential load.\n");

    printf("\t-t, --type\n\t\tType of IO calls to use.\n");
    printf("\t\tValid options are 'stateful' for read/write IO,\n" \
           "\t\t'stateless' for pread/pwrite type of IO, 'mmap' for\n" \
           "\t\tmemory mapping, and 'aio' for asynchronous IO.\n");
    
    printf("\t-r, --direction\n\t\tDirection in which the operations are performed.\n");
    printf("\t\tValid options are 'formward' and 'backward'.\n" \
           "\t\tThis option is only applicable to sequential workloads.\n");
    
    printf("\t-o, --operation\n\t\tThe operation to be performed.\n");
    printf("\t\tValid options are 'read' and 'write'.\n");
    
    printf("\t-p, --paged\n\t\tThis options turns off direct IO (which is on by default).\n");
    printf("\t-f, --buffered\n\t\tThis options turns off flushing (flushing is on by default).\n" \
           "\t\tThis option is only applicable for write operations.\n");
    printf("\t-m, --do-atime\n\t\tThis options turns on atime updates (off by default).\n");
    printf("\t-a, --append\n\t\tOpen the file in append-only mode (off by default).\n"\
           "\t\tThis option is only applicable for sequential writes.\n");
    printf("\t-u, --dist\n\t\tThe distribution used for random workloads (uniform by default).\n"\
           "\t\tValid options are 'uniform', 'normal', and 'pow' (for power law).\n"
           "\t\tThis option is only applicable for random workloads.\n");
    printf("\t-i, --sigma\n\t\tControls the random distribution.\n"\
           "\t\tFor normal distribution, it is the percent of the data one standard deviation away\n"\
           "\t\tfrom the mean (5 by default). If sigma = 5, roughly 68%% of the time only 10%%\n"\
           "\t\tof the data will be accessed. This option is only applicable when distribution is\n"\
           "\t\tset to 'normal'.\n"\
           "\t\tFor power distribution, it is the percentage of the size of the device that will\n"\
           "\t\tbe the mean accessed location (5 by default).\n");
    printf("\t-l, --local-fd\n\t\tUse a thread local file descriptor (by default file descriptors\n"\
           "\t\tare shared across threads).\n");
    printf("\t-n, --silent\n\t\tNon-interactive mode. Won't ask for write confirmation, and will\n"\
           "\t\tprint machine readable output.\n");
    exit(0);
}

void parse_options(int argc, char *argv[], workload_config_t *config) {
    init_workload_config(config);
    optind = 1; // reinit getopt
    while(1)
    {
        struct option long_options[] =
            {
                {"block_size",   required_argument, 0, 'b'},
                {"duration", required_argument, 0, 'd'},
                {"threads", required_argument, 0, 'c'},
                {"stride", required_argument, 0, 's'},
                {"workload", required_argument, 0, 'w'},
                {"type", required_argument, 0, 't'},
                {"direction", required_argument, 0, 'r'},
                {"operation", required_argument, 0, 'o'},
                {"dist", required_argument, 0, 'u'},
                {"sigma", required_argument, 0, 'i'},
                {"paged", no_argument, &config->direct_io, 0},
                {"buffered", no_argument, &config->buffered, 1},
                {"do-atime", no_argument, &config->do_atime, 1},
                {"append", no_argument, &config->append_only, 1},
                {"local-fd", no_argument, &config->local_fd, 1},
                {"silent", no_argument, &config->silent, 1},
                {0, 0, 0, 0}
            };

        int option_index = 0;
        int c = getopt_long(argc, argv, "b:d:c:w:t:r:s:o:u:i:mpfaln", long_options, &option_index);
     
        /* Detect the end of the options. */
        if (c == -1)
            break;
     
        switch (c)
        {
        case 0:
            break;
        case 'b':
            config->block_size = atoi(optarg);
            break;
            
        case 'd':
            config->duration = atoi(optarg);
            break;
     
        case 'c':
            config->threads = atoi(optarg);
            break;
     
        case 's':
            config->stride = atoi(optarg);
            break;
     
        case 'p':
            config->direct_io = 0;
            break;
     
        case 'l':
            config->local_fd = 1;
            break;
     
        case 'n':
            config->silent = 1;
            break;
     
        case 'f':
            config->buffered = 1;
            break;
     
        case 'm':
            config->do_atime = 1;
            break;
     
        case 'a':
            config->append_only = 1;
            break;
     
        case 'w':
            if(strcmp(optarg, "seq") == 0)
                config->workload = wl_seq;
            else if(strcmp(optarg, "rnd") == 0)
                config->workload = wl_rnd;
            else
                check("Invalid workload", 1);
            break;

        case 't':
            if(strcmp(optarg, "stateful") == 0)
                config->io_type = iot_stateful;
            else if(strcmp(optarg, "stateless") == 0)
                config->io_type = iot_stateless;
            else if(strcmp(optarg, "aio") == 0)
                config->io_type = iot_aio;
            else if(strcmp(optarg, "mmap") == 0)
                config->io_type = iot_mmap;
            else
                check("Invalid IO type", 1);
            break;
     
        case 'r':
            if(strcmp(optarg, "forward") == 0)
                config->direction = opd_forward;
            else if(strcmp(optarg, "backward") == 0)
                config->direction = opd_backward;
            else
                check("Invalid direction", 1);
            break;
     
        case 'o':
            if(strcmp(optarg, "read") == 0)
                config->operation = op_read;
            else if(strcmp(optarg, "write") == 0)
                config->operation = op_write;
            else
                check("Invalid operation", 1);
            break;

        case 'u':
            if(strcmp(optarg, "uniform") == 0)
                config->dist = rdt_uniform;
            else if(strcmp(optarg, "normal") == 0)
                config->dist = rdt_normal;
            else if(strcmp(optarg, "pow") == 0)
                config->dist = rdt_power;
            else
                check("Invalid distribution", 1);
            break;

        case 'i':
            config->sigma = atoi(optarg);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            usage(argv[0]);
            break;
     
        default:
            usage(argv[0]);
        }
    }

    if(config->workload == wl_rnd && config->direction == opd_backward)
        check("Direction can only be used for a sequential workload", 1);

    if(config->append_only && (config->workload == wl_rnd || config->operation == op_read))
        check("Append-only cannot be run with random or read workloads", 1);

    if(config->buffered == 1 && config->direct_io == 1)
        check("Can't turn on buffering with direct IO", 1);

    if (optind < argc) {
        strncpy(config->device, argv[optind++], DEVICE_NAME_LENGTH);
        config->device[DEVICE_NAME_LENGTH - 1] = 0;
    }
    else
        usage(argv[0]);

    if(config->sigma == -1) {
        if(config->dist == rdt_normal)
            config->sigma = 5;
        else if(config->dist == rdt_power)
            config->sigma = 5;
    }
    else if(config->dist != rdt_normal && config->dist != rdt_power)
        check("Sigma is only valid for normal and power distributions", 1);

    if(config->threads < 1)
        check("Please use at least one thread", 1);

    if(config->direct_io && config->io_type == iot_mmap)
        check("Can't use mmap with direct IO (use --paged)", 1);

    if(config->threads > 1 && config->io_type == iot_stateful && !config->local_fd)
        check("Can't use shared file descriptor with stateful IO on multiple threads"\
              " (use -t stateless or --local-fd)", 1);

    if(config->workload == wl_seq && config->operation == op_write && config->direction == opd_forward &&
        config->io_type == iot_mmap)
        check("Memory mapping isn't implemented where remapping might be required", 1);

    if(config->operation == op_write && !config->silent) {
        while(true) {
            printf("Are you sure you want to write to %s [y/N]? ", config->device);
            int response = getc(stdin);
            if(response == EOF) {
                printf("\n");
                exit(-1);
            } else if(response == '\n') {
                printf("\n");
                exit(-1);
            } else if(response == 'n' || response == 'N') {
                exit(-1);
            } else if(response == 'y' || response == 'Y') {
                break;
            } else {
                getc(stdin);
            }
        }
    }
}

void print_status(size_t length, workload_config_t *config) {
    if(config->silent)
        return;
    
    size_t hl = length / 1024 / 1024 / 1024;
    if(hl != 0)
        printf("Benchmarking results for [%s] (%ldGB)\n", config->device, hl);
    else {
        size_t hl = length / 1024 / 1024;
        if(hl != 0)
            printf("Benchmarking results for [%s] (%ldMB)\n", config->device, hl);
        else {
            size_t hl = length / 1024;
            if(hl != 0 )
                printf("Benchmarking results for [%s] (%ldKB)\n", config->device, hl);
            else
                printf("Benchmarking results for [%s] (%ldb)\n", config->device, length);
        }
    }
        
    printf("[");
    printf("duration: %ds, ", config->duration);

    printf("threads: %d, ", config->threads);

    if(config->threads > 1) {
        printf("fd: ");
        if(config->local_fd)
            printf("local, ");
        else
            printf("shared, ");
    }
    
    printf("operation: ");
    if(config->operation == op_write)
        printf("write, ");
    else
        printf("read, ");
    
    if(config->operation == op_write) {
        printf("buffering: ");
        if(config->buffered)
            printf("on, ");
        else
            printf("off, ");
    }
    printf("block size: %db, ", config->block_size);
    if(config->workload == wl_seq)
        printf("stride: %db, ", config->stride);

    printf("workload: ");
    if(config->workload == wl_rnd)
        printf("rnd, ");
    else if(config->workload == wl_seq)
        printf("seq, ");
    else
        check("Invalid workload", 1);

    if(config->workload == wl_rnd) {
        printf("dist: ");
        if(config->dist == rdt_uniform)
            printf("uniform, ");
        else if(config->dist == rdt_normal)
            printf("normal, ");
        else if(config->dist == rdt_power)
            printf("zipfian, ");
        else
            check("Invalid distribution", 1);
        if(config->dist == rdt_normal || config->dist == rdt_power)
            printf("sigma: %d, ", config->sigma);
    }
    
    printf("direct IO: ");
    if(config->direct_io)
        printf("on, ");
    else
        printf("off, ");
    
    printf("IO type: ");
    if(config->io_type == iot_stateful)
        printf("stateful, ");
    else if(config->io_type == iot_stateless)
        printf("stateless, ");
    else if(config->io_type == iot_aio)
        printf("aio, ");
    else if(config->io_type == iot_mmap)
        printf("mmap, ");
    else
        check("Invalid IO type", 1);
    
    if(config->workload == wl_seq) {
        printf("direction: ");
        if(config->direction == opd_forward)
            printf("forward, ");
        else if(config->direction == opd_backward)
            printf("backward, ");
        else
            check("Invalid direction", 1);
            
    }

    if(config->workload == wl_seq && config->operation == op_write) {
        printf("append-only: ");
        if(config->append_only)
            printf("on, ");
        else
            printf("off, ");
    }
    
    printf("time updates: ");
    if(config->do_atime)
        printf("on");
    else
        printf("off");
    printf("]\n");
}

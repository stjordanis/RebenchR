
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <strings.h>
#include <vector>
#include <algorithm>
#include <sys/mman.h>
#include "opts.hpp"
#include "utils.hpp"
#include "operations.hpp"

using namespace std;

void setup_io(int *fd, off64_t *length, void **map, workload_config_t *config) {
    int res;
    int flags = 0;
    
    if(!config->do_atime)
        flags |= O_NOATIME;
    
    if(config->io_type == iot_mmap) {
        if(config->operation == op_write)
            flags |= O_RDWR;
        else
            flags |= O_RDONLY;
    } else {
        if(config->operation == op_read)
            flags |= O_RDONLY;
        else if(config->operation == op_write)
            flags |= O_WRONLY;
        else
            check("Invalid operation", 1);
    }
        
    if(config->direct_io)
        flags |= O_DIRECT;
    
    if(config->append_only)
        flags |= O_APPEND;

    *length = get_device_length(config->device);
    
    *fd = open64(config->device, flags);
    check("Error opening device", *fd == -1);

    if(config->io_type == iot_mmap) {
        int prot = 0;
        if(config->operation == op_read)
            prot = PROT_READ;
        else
            prot = PROT_WRITE | PROT_READ;
        *map = mmap(NULL, *length, prot, MAP_SHARED, *fd, 0);
        check("Unable to mmap memory", *map == MAP_FAILED);
    }
}

void cleanup_io(int fd, void *map, off64_t length) {
    if(map) {
        check("Unable to unmap memory",
              munmap(map, length) != 0);
    }
    close(fd);
}

// Describes each thread in a workload
struct simulation_info_t {
    int *is_done;
    long ops;
    int fd;
    off64_t length;
    workload_config_t *config;
    void *mmap;
};

// Describes each workload simulation
struct workload_simulation_t {
    vector<simulation_info_t*> sim_infos;
    vector<pthread_t> threads;
    workload_config_t config;
    int is_done;
    int fd;
    off64_t length;
    ticks_t start_time, end_time;
    long long ops;
    void *mmap;
};
typedef vector<workload_simulation_t*> wsp_vector;

void* run_simulation(void *arg) {
    simulation_info_t *info = (simulation_info_t*)arg;
    rnd_gen_t rnd_gen;
    char *buf;

    int res = posix_memalign((void**)&buf,
                             max(getpagesize(), info->config->block_size),
                             info->config->block_size);
    check("Error allocating memory", res != 0);

    rnd_gen = init_rnd_gen();
    check("Error initializing random numbers", rnd_gen == NULL);

    char sum = 0;
    while(!(*info->is_done)) {
        long long ops = __sync_fetch_and_add(&(info->ops), 1);
        if(!perform_op(info->fd, info->mmap, buf, info->length, ops, rnd_gen, info->config))
            goto done;
        // Read from the buffer to make sure there is no optimization
        // shenanigans
	sum += buf[0];
    }

done:
    free_rnd_gen(rnd_gen);
    free(buf);

    return (void*)sum;
}

void print_stats(ticks_t start_time, ticks_t end_time, long long ops, workload_config_t *config) {
    float total_secs = ticks_to_secs(end_time - start_time);
    if(config->silent) {
        printf("%d %.2f\n",
               (int)((float)ops / total_secs),
               ((double)ops * config->block_size / 1024 / 1024) / total_secs);
    } else {
        printf("Operations/sec: %d (%.2f MB/sec) - %lld ops in %.2f secs\n",
               (int)((float)ops / total_secs),
               ((double)ops * config->block_size / 1024 / 1024) / total_secs,
               ops, ticks_to_secs(end_time - start_time));
    }
}

long long compute_total_ops(workload_simulation_t *ws) {
    long long ops = 0;
    for(int i = 0; i < ws->config.threads; i++) {
        ops += __sync_fetch_and_add(&(ws->sim_infos[i]->ops), 0);
    }
    return ops;
}

int main(int argc, char *argv[])
{
    int i = 0;
    wsp_vector workloads;

    // Parse the workloads
    if(argc < 2) {
        char buf[2048];
        char delims[] = " \t\n";
        while(fgets(buf, 2048, stdin)) {
            vector<char*> args;
            char *tok = strtok(buf, delims);
            while(tok) {
                args.push_back(tok);
                tok = strtok(NULL, delims);
            }
            if(!args.empty()) {
                args.insert(args.begin(), argv[0]);
                workload_simulation_t *ws = new workload_simulation_t();
                parse_options(args.size(), &args[0], &ws->config);
                workloads.push_back(ws);
            }
        }
    } else {
        workload_simulation_t *ws = new workload_simulation_t();
        parse_options(argc, argv, &ws->config);
        workloads.push_back(ws);
    }

    if(workloads.empty())
        usage(argv[0]);

    // Start the simulations
    int workload = 1;
    for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
        workload_simulation_t *ws = *it;

        if(!ws->config.silent) {
            if(workloads.size() > 1) {
                printf("Starting workload %d...\n", workload);
                if(workload == workloads.size())
                    printf("\n");
            }
            else
                printf("Benchmarking...\n\n");
        }
        workload++;

        ws->is_done = 0;
        ws->ops = 0;
        ws->mmap = NULL;
        if(!ws->config.local_fd) {
            // FD is shared for the workload
            setup_io(&ws->fd, &ws->length, &ws->mmap, &ws->config);
        }
        ws->start_time = get_ticks();
        for(i = 0; i < ws->config.threads; i++) {
            simulation_info_t *sim_info = new simulation_info_t();
            bzero(sim_info, sizeof(sim_info));
            sim_info->is_done = &ws->is_done;
            sim_info->config = &ws->config;
            sim_info->mmap = NULL;
            if(!ws->config.local_fd) {
                sim_info->fd = ws->fd;
                sim_info->length = ws->length;
                sim_info->mmap = ws->mmap;
            } else {
                setup_io(&(sim_info->fd), &(sim_info->length), &(sim_info->mmap), &ws->config);
            }
            pthread_t thread;
            check("Error creating thread",
                  pthread_create(&thread, NULL, &run_simulation, (void*)sim_info) != 0);
            ws->sim_infos.push_back(sim_info);
            ws->threads.push_back(thread);
        }
    }

    // Stop the simulations
    bool all_done = false;
    int total_slept = 0;
    while(!all_done) {
        all_done = true;
        for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
            workload_simulation_t *ws = *it;
            // See if the workload is done
            if(!ws->is_done) {
                if(ws->config.duration_unit == dut_space) {
                    long long total_bytes = compute_total_ops(ws) * ws->config.block_size;
                    if(total_bytes >= ws->config.duration) {
                        ws->is_done = 1;
                    } else {
                        all_done = false;
                    }
                } else {
                    if(total_slept / 200.0f >= ws->config.duration) {
                        ws->is_done = 1;
                    } else {
                        all_done = false;
                    }
                }
                // If the workload is done, wait for all the threads and grab the time
                if(ws->is_done) {
                    for(i = 0; i < ws->config.threads; i++) {
                        check("Error joining thread",
                              pthread_join(ws->threads[i], NULL) != 0);
                    }
                    ws->end_time = get_ticks();
                }
            }
        }
        
        if(!all_done) {
            usleep(5000);
            total_slept++;
        }
    }
    
    // Compute the stats
    for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
        workload_simulation_t *ws = *it;
        for(i = 0; i < ws->config.threads; i++) {
            if(ws->config.local_fd)
                cleanup_io(ws->sim_infos[i]->fd, ws->sim_infos[i]->mmap, ws->sim_infos[i]->length);
        }
        ws->ops = compute_total_ops(ws);
        
        if(!ws->config.local_fd)
            cleanup_io(ws->fd, ws->mmap, ws->length);

        // print results
        if(it != workloads.begin())
            printf("---\n");
        print_status(ws->sim_infos[0]->length, &ws->config);
        print_stats(ws->start_time, ws->end_time, ws->ops, &ws->config);

        // clean up
        for(i = 0; i < ws->sim_infos.size(); i++)
            delete ws->sim_infos[i];
        delete ws;
    }
}

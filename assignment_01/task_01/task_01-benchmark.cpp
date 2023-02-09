#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

struct timespec temp_timespec;

// struct to hold times for one thread
typedef struct thread_time {
    long wall_time;
    long burst_time;
} thread_time;

// to pass multiple arguments to the thread
typedef struct thread_arguments {
    thread_time *thread_time_struct;
    pthread_barrier_t *thread_starting_barrier;
    thread_arguments(thread_time *thread_time_struct, pthread_barrier_t *thread_starting_barrier) {
        this->thread_time_struct = thread_time_struct;
        this->thread_starting_barrier = thread_starting_barrier;
    }
} thread_arguments;

// struct to hold all thread_times
typedef struct timing_results {
    long wall_elapsed_time;
    unsigned int thread_count;
    thread_time *thread_times;
    timing_results(unsigned int thread_count) {
      this->thread_count = thread_count;
      thread_times = (thread_time *)malloc(sizeof(thread_time) * thread_count);
    }
    ~timing_results() {
        free(thread_times);
    }
} timing_results;

static void * benchmark(void *arg)
{
    thread_arguments *arguments = (thread_arguments *)arg;
    thread_time *thread_time_struct = arguments->thread_time_struct;
    pthread_barrier_t *starting_barrier = arguments->thread_starting_barrier;
    
    struct timespec thread_wall_timespec;
    struct timespec thread_burst_timespec;
    clockid_t thread_clock_id;
    int get_clock_error = pthread_getcpuclockid(pthread_self(), &thread_clock_id);
    if (get_clock_error) {
        fprintf(stderr, "Could not get thread clock id\n");
        fprintf(stderr, "Error: %s\n", strerror(get_clock_error));
        exit(EXIT_FAILURE);
    }
    
    // wait until all threads are created
    pthread_barrier_wait(starting_barrier);
    
    // get time before benchmark (wall clock ("normal") time and thread cpu time)
    clock_gettime(CLOCK_MONOTONIC, &thread_wall_timespec);
    clock_gettime(thread_clock_id, &thread_burst_timespec);
    thread_time_struct->wall_time = - (thread_wall_timespec.tv_nsec + thread_wall_timespec.tv_sec * 1e9);
    thread_time_struct->burst_time = - (thread_burst_timespec.tv_nsec + thread_burst_timespec.tv_sec * 1e9);
    

    // this was just a placeholder to do some calculations to test if calculating the times works. We still have to write the actual code we want to use as a benchmark
    int *test = (int *)malloc(sizeof(int) * 100000000);
    test[0] = 0;
    test[1] = 1;
    for (int j = 0; j < 10; j++) {
        for (int i = 2; i < 100000000; i++) {
            test[i] = test[i-2] + test[i-1];
        }
    }
    free(test);
    

    // calculate elapsed time by subtracting time before benchmark from time after benchmark
    clock_gettime(CLOCK_MONOTONIC, &thread_wall_timespec);
    clock_gettime(thread_clock_id, &thread_burst_timespec);
    thread_time_struct->wall_time += (thread_wall_timespec.tv_nsec + thread_wall_timespec.tv_sec * 1e9);
    thread_time_struct->burst_time += (thread_burst_timespec.tv_nsec + thread_burst_timespec.tv_sec * 1e9);
    
    return (NULL);
}


timing_results run_benchmark(unsigned int thread_count)
{
    timing_results return_results(thread_count);
    
    pthread_t *thread_ids = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    
    pthread_attr_t thread_attributes;
    
    if(pthread_attr_init(&thread_attributes)) {
        fprintf(stderr, "Unable to initalize thread attribute struct.\n");
    }
    if(pthread_attr_setschedpolicy(&thread_attributes, SCHED_RR)) {
        fprintf(stderr, "Unable to set policy.\n");
    }
    struct sched_param scheduling_parameter_priority;
    scheduling_parameter_priority.sched_priority = 99;
    if(pthread_attr_setschedparam(&thread_attributes, &scheduling_parameter_priority)) {
        fprintf(stderr, "Unable to set priority.\n");
    }
    if(pthread_attr_setinheritsched(&thread_attributes, PTHREAD_SCOPE_PROCESS)) {
        fprintf(stderr, "Unable to set inharitance.\n");
    }
    
    pthread_barrier_t thread_starting_barrier;
    pthread_barrierattr_t thread_starting_barrier_attributes;
    pthread_barrierattr_init(&(thread_starting_barrier_attributes)); 
    pthread_barrier_init(&(thread_starting_barrier), &(thread_starting_barrier_attributes), thread_count); 

    for (unsigned int i = 0; i < thread_count; i++) {
        thread_arguments argument(&(return_results.thread_times[i]), &(thread_starting_barrier));
        // start timer before last thread is created
        if (i == thread_count - 1) {
            clock_gettime(CLOCK_MONOTONIC, &temp_timespec);
            return_results.wall_elapsed_time = - (temp_timespec.tv_nsec + temp_timespec.tv_sec * 1e9);
        }
        if (int error = pthread_create(&(thread_ids[i]), &thread_attributes, benchmark, &argument)) {
            fprintf(stderr, "Could not create thread\n");
            fprintf(stderr, "Error: %s\n", strerror(error));
            exit(EXIT_FAILURE);
        }
        sleep(0.1);
        // so the 'argument' variable is still in scope while the values are copyed to thread
        thread_arguments trash = argument;
        printf("Thread %d created.\n", i);
    }
    
    // waits for all p-threads to finish execution
    for (unsigned int i = 0; i < thread_count; i++) {
        pthread_join(thread_ids[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &temp_timespec);
    return_results.wall_elapsed_time += (temp_timespec.tv_nsec + temp_timespec.tv_sec * 1e9);

    return (return_results);
}


int main(int argc, char *argv[])
{
    // the next 3 lines are copied from the presentation and illustrate how to calculate theses 3 metrics (just had them so i didn't have to look at the presentation)
    // Burst time: CLOCK_THREAD_CPUTIME_ID
    // Waiting time: CLOCK_REALTIME - CLOCK_THREAD_CPUTIME_ID
    // Throughput : (Burst_time_thread1 + ... + Burst_time_thread_N) / N
    
    // runs the benchmark with the specified amount of threads, which returns a timing_results struct which contains the executions times of the threads...
    timing_results timing_thing = run_benchmark(5);
    
    // calculate and output the burst time, waiting time and thoughput
    long sum_burst_time = 0;
    printf("Wall time: %ld ns\n", timing_thing.wall_elapsed_time);
    for (int i = 0; i < timing_thing.thread_count; i++) {
        sum_burst_time += timing_thing.thread_times[i].burst_time;
        printf("Thread %d - burst time: %ld ns, waiting time: %ld ns\n", i, timing_thing.thread_times[i].burst_time, timing_thing.thread_times[i].wall_time - timing_thing.thread_times[i].burst_time);
    }
    printf("Throughput: %ld\n", sum_burst_time / timing_thing.thread_count);

    return (0);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>


/*
struct timespec {
    time_t tv_sec;
    long tv_nsec;
} temp_timespec;
*/
struct timespec temp_timespec;

// struct to hold times for one thread
typedef struct thread_time {
    long wall_time;
    long burst_time;
} thread_time;

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
    thread_time *thread_time_struct = (thread_time *)arg;
    struct timespec thread_wall_timespec;
    struct timespec thread_burst_timespec;
    clockid_t thread_clock_id;
    int get_clock_error = pthread_getcpuclockid(pthread_self(), &thread_clock_id);
    if (get_clock_error) {
        fprintf(stderr, "Could not get thread clock id\n");
        fprintf(stderr, "Error: %s\n", strerror(get_clock_error));
        exit(1);
    }
    
    // get time before benchmark (wall clock ("normal") time and thread cpu time)
    clock_gettime(CLOCK_MONOTONIC, &thread_wall_timespec);
    clock_gettime(thread_clock_id, &thread_burst_timespec);
    thread_time_struct->wall_time = - (thread_wall_timespec.tv_nsec + thread_wall_timespec.tv_sec * 1e9);
    thread_time_struct->burst_time = - (thread_burst_timespec.tv_nsec + thread_burst_timespec.tv_sec * 1e9);
    

    // this was just a placeholder to do some calculations to test if calculating the times works. We still have to write the actual code we want to use as a benchmark
    int *test = (int *)malloc(sizeof(int) * 100000000);
    test[0] = 0;
    test[1] = 1;
    for (int i = 2; i < 100000000; i++) {
        test[i] = test[i-2] + test[i-1];
    }
    for (int i = 2; i < 100000000; i++) {
        test[i] = test[i-2] + test[i-1];
    }
    free(test);
    

    // calculate elapsed time by subtracting time before benchmark from time after benchmark
    clock_gettime(CLOCK_MONOTONIC, &thread_wall_timespec);
    clock_gettime(thread_clock_id, &thread_burst_timespec);
    thread_time_struct->wall_time += (thread_wall_timespec.tv_nsec + thread_wall_timespec.tv_sec * 1e9);
    thread_time_struct->burst_time += (thread_burst_timespec.tv_nsec + thread_burst_timespec.tv_sec * 1e9);
    
    return (NULL);
}


timing_results run_benchmark(unsigned thread_count)
{
    // instatiate the timing_results struct we use to get the thread timing values
    timing_results return_thing(thread_count);
    
    // create an array to hold the pthread references
    pthread_t *thread_ids = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    
    // I don't know if we need the programm execution time, I mainly wrote this to familiarize myself with the clock_gettime() function
    clock_gettime(CLOCK_MONOTONIC, &temp_timespec);
    return_thing.wall_elapsed_time = - (temp_timespec.tv_nsec + temp_timespec.tv_sec * 1e9);

    // creates the specified amount of p-threads and lets them execute the benchmark function
    for (unsigned int i = 0; i < thread_count; i++) {
        pthread_create(&(thread_ids[i]), NULL, benchmark, &(return_thing.thread_times[i])); // set scheduling here?
    }
    
    // waits for all p-threads to finish execution
    for (unsigned int i = 0; i < thread_count; i++) {
        pthread_join(thread_ids[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &temp_timespec);
    return_thing.wall_elapsed_time += (temp_timespec.tv_nsec + temp_timespec.tv_sec * 1e9);

    return (return_thing);
}


int main(int argc, char *argv[])
{
    // the next 3 lines are copied from the presentation and illustrate how to calculate theses 3 metrics (just had them so i didn't have to look at the presentation)
    // Burst time: CLOCK_THREAD_CPUTIME_ID
    // Waiting time: CLOCK_REALTIME - CLOCK_THREAD_CPUTIME_ID
    // Throughput : (Burst_time_thread1 + ... + Burst_time_thread_N) / N
    

    // runs the benchmark with the specified amount of threads, which returns a timing_results struct which contains the executions times of the threads...
    timing_results timing_thing = run_benchmark(24);
    
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

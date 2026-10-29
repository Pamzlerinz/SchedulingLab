/*
 * schedsim.c - CPU Scheduling Simulator
 *
 * Reads a list of processes from an input file and simulates four CPU
 * scheduling algorithms on it:
 *
 *   1. First Come First Serve (FCFS)
 *   2. Shortest Job First, preemptive (SRTF - Shortest Remaining Time First)
 *   3. Priority (non-preemptive, higher pri value runs first)
 *   4. Round Robin (RR) with a fixed time quantum
 *
 * SRTF and Round Robin both honor each process's arrival time: a process
 * is never scheduled before it arrives, and the CPU idles if nothing is ready.
 *
 * For each algorithm it prints every process's burst, waiting and turnaround
 * time, followed by the average waiting and turnaround time.
 *
 * Input file format (one process per line):
 *     pid  burst_time  arrival_time  waiting_time  turnaround_time  priority
 *
 * Usage:
 *     ./schedsim <input-file-path> [quantum]
 *     (quantum is optional and defaults to 2)
 *
 * Key formulas:
 *     Turnaround Time = Completion Time - Arrival Time
 *     Waiting Time    = Turnaround Time - Burst Time
 */
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "process.h"
#include "util.h"

#define DEFAULT_QUANTUM 2

/* ------------------------------------------------------------------ */
/*                    Waiting-time calculations                        */
/* ------------------------------------------------------------------ */

/*
 * findWaitingTimeRR - Round Robin waiting times, honoring arrival times.
 *
 * Processes wait in a FIFO ready queue. A process joins the queue only once
 * it has arrived (art <= t). The process at the front runs for at most
 * `quantum` time units; if it still has work left it goes to the back of
 * the queue, behind any processes that arrived while it was running.
 * If the queue is empty, the CPU idles until the next arrival.
 *
 * When a process finishes at time t:  wt = t - arrival_time - burst_time
 * (With every arrival time 0 this is the classic  wt = t - bt.)
 */
void findWaitingTimeRR(ProcessType plist[], int n, int quantum)
{
    int rem_bt[n];          /* remaining burst time of each process */
    int order[n];           /* process indices sorted by arrival time */
    int queue[n];           /* circular FIFO ready queue of process indices */
    int head = 0, count = 0;
    int next = 0;           /* next entry of order[] that has not arrived yet */
    int complete = 0;       /* number of processes finished so far */
    int t = 0;              /* current simulation time */

    for (int i = 0; i < n; i++) {
        rem_bt[i] = plist[i].bt;
        plist[i].wt = 0;
        order[i] = i;
    }

    /* Insertion sort by arrival time (stable, so ties keep input order) */
    for (int i = 1; i < n; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && plist[order[j]].art > plist[key].art) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    while (complete < n) {
        /* Add every process that has arrived by time t to the ready queue */
        while (next < n && plist[order[next]].art <= t) {
            queue[(head + count) % n] = order[next++];
            count++;
        }

        /* Nothing ready: CPU idles until the next process arrives */
        if (count == 0) {
            t = plist[order[next]].art;
            continue;
        }

        /* Take the process at the front of the queue */
        int i = queue[head];
        head = (head + 1) % n;
        count--;

        /* Run it for one quantum, or less if it finishes sooner */
        int slice = (rem_bt[i] < quantum) ? rem_bt[i] : quantum;
        t += slice;
        rem_bt[i] -= slice;

        /* Processes that arrived during this slice get in line first */
        while (next < n && plist[order[next]].art <= t) {
            queue[(head + count) % n] = order[next++];
            count++;
        }

        if (rem_bt[i] > 0) {
            /* Not done: back of the line */
            queue[(head + count) % n] = i;
            count++;
        } else {
            /* Finished at time t */
            complete++;
            plist[i].wt = t - plist[i].art - plist[i].bt;
        }
    }
}

/*
 * findWaitingTimeSJF - Preemptive Shortest Job First (SRTF) waiting times.
 *
 * The simulation advances one time unit at a time. At each step it picks,
 * among the processes that have already arrived, the one with the smallest
 * remaining burst time and runs it for one unit. A newly arrived process
 * with a shorter remaining time therefore preempts the current one.
 *
 * When a process finishes:  wt = completion_time - arrival_time - burst_time
 */
void findWaitingTimeSJF(ProcessType plist[], int n)
{
    int rem_bt[n];          /* remaining burst time of each process */
    int complete = 0;       /* number of processes finished so far */
    int t = 0;              /* current simulation time */

    for (int i = 0; i < n; i++)
        rem_bt[i] = plist[i].bt;

    while (complete != n) {
        /* Reset the search every time unit so no stale minimum carries over */
        int minm = INT_MAX;     /* smallest remaining time found this step */
        int shortest = -1;      /* index of that process, -1 = none ready */

        /* Find the arrived, unfinished process with the least remaining time */
        for (int j = 0; j < n; j++) {
            if (plist[j].art <= t && rem_bt[j] > 0 && rem_bt[j] < minm) {
                minm = rem_bt[j];
                shortest = j;
            }
        }

        /* No process has arrived yet: CPU is idle, move time forward */
        if (shortest == -1) {
            t++;
            continue;
        }

        /* Run the chosen process for one time unit */
        rem_bt[shortest]--;

        /* If it just finished, record its waiting time */
        if (rem_bt[shortest] == 0) {
            int finish_time = t + 1;
            complete++;
            plist[shortest].wt = finish_time - plist[shortest].bt - plist[shortest].art;
            if (plist[shortest].wt < 0)
                plist[shortest].wt = 0;
        }

        t++;
    }
}

/*
 * findWaitingTime - FCFS waiting times for processes in their current order.
 *
 * The first process starts immediately; every later process waits for all
 * the processes before it to finish:  wt[i] = wt[i-1] + bt[i-1].
 * Also used by Priority scheduling after the list has been sorted.
 */
void findWaitingTime(ProcessType plist[], int n)
{
    plist[0].wt = 0;

    for (int i = 1; i < n; i++)
        plist[i].wt = plist[i - 1].wt + plist[i - 1].bt;
}

/*
 * findTurnAroundTime - turnaround time = burst time + waiting time.
 * Also guards against negative waiting times from malformed input.
 */
void findTurnAroundTime(ProcessType plist[], int n)
{
    for (int i = 0; i < n; i++) {
        if (plist[i].wt < 0)
            plist[i].wt = 0;
        plist[i].tat = plist[i].bt + plist[i].wt;
    }
}

/*
 * my_comparer - qsort comparison function for Priority scheduling.
 *
 * Returns a positive value when `this` should come after `that`.
 *   - Higher pri value runs first.
 *   - Equal priorities fall back to first come, first served (pid order).
 */
int my_comparer(const void *this, const void *that)
{
    const ProcessType *p1 = (const ProcessType *) this;
    const ProcessType *p2 = (const ProcessType *) that;

    if (p1->pri < p2->pri)
        return 1;
    if (p1->pri > p2->pri)
        return -1;

    return p1->pid - p2->pid;
}

/* ------------------------------------------------------------------ */
/*              Per-algorithm drivers (compute + header)               */
/* ------------------------------------------------------------------ */

/* Prints a clearly separated heading for each algorithm's results */
static void printHeader(const char *title)
{
    printf("\n==================================================================\n");
    printf("  %s\n", title);
    printf("==================================================================\n");
}

void findavgTimeFCFS(ProcessType plist[], int n)
{
    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);
    printHeader("First Come First Serve (FCFS)");
}

void findavgTimeSJF(ProcessType plist[], int n)
{
    findWaitingTimeSJF(plist, n);
    findTurnAroundTime(plist, n);
    printHeader("Shortest Job First - Preemptive (SRTF)");
}

void findavgTimeRR(ProcessType plist[], int n, int quantum)
{
    char title[64];

    findWaitingTimeRR(plist, n, quantum);
    findTurnAroundTime(plist, n);

    snprintf(title, sizeof(title), "Round Robin (RR), Quantum = %d", quantum);
    printHeader(title);
}

/*
 * findavgTimePriority - non-preemptive Priority scheduling.
 * Sorts the processes by priority, then simply applies FCFS to that order.
 */
void findavgTimePriority(ProcessType plist[], int n)
{
    qsort(plist, n, sizeof(ProcessType), my_comparer);

    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);
    printHeader("Priority (non-preemptive, higher value = higher priority)");
}

/* ------------------------------------------------------------------ */
/*                           Output                                    */
/* ------------------------------------------------------------------ */

/*
 * printMetrics - prints an aligned table of every process followed by the
 * average waiting and turnaround times.
 */
void printMetrics(ProcessType plist[], int n)
{
    int total_wt = 0, total_tat = 0;

    printf("  %-10s %-12s %-14s %-16s\n",
           "Process", "Burst Time", "Waiting Time", "Turnaround Time");
    printf("  %-10s %-12s %-14s %-16s\n",
           "-------", "----------", "------------", "---------------");

    for (int i = 0; i < n; i++) {
        total_wt += plist[i].wt;
        total_tat += plist[i].tat;
        printf("  %-10d %-12d %-14d %-16d\n",
               plist[i].pid, plist[i].bt, plist[i].wt, plist[i].tat);
    }

    printf("\n  Average waiting time     = %.2f\n", (float) total_wt / (float) n);
    printf("  Average turn around time = %.2f\n", (float) total_tat / (float) n);
}

/* ------------------------------------------------------------------ */
/*                         Input & driver                              */
/* ------------------------------------------------------------------ */

/*
 * initProc - loads a fresh copy of the process list from the input file.
 * The caller owns the returned array and must free() it.
 */
ProcessType *initProc(char *filename, int *n)
{
    FILE *input_file = fopen(filename, "r");
    if (!input_file) {
        fprintf(stderr, "Error: Invalid filepath\n");
        exit(1);
    }

    ProcessType *plist = parse_file(input_file, n);
    fclose(input_file);

    return plist;
}

/* Algorithm IDs, so main() can run all four with one loop */
enum { ALG_FCFS, ALG_SJF, ALG_PRIORITY, ALG_RR, ALG_COUNT };

/*
 * runSimulation - runs one algorithm on its own freshly loaded copy of the
 * input. Each algorithm overwrites wt/tat and Priority reorders the array,
 * so every run starts from clean data. The memory is freed afterwards.
 */
static void runSimulation(int algorithm, char *filename, int quantum)
{
    int n = 0;
    ProcessType *proc_list = initProc(filename, &n);

    switch (algorithm) {
    case ALG_FCFS:     findavgTimeFCFS(proc_list, n);         break;
    case ALG_SJF:      findavgTimeSJF(proc_list, n);          break;
    case ALG_PRIORITY: findavgTimePriority(proc_list, n);     break;
    case ALG_RR:       findavgTimeRR(proc_list, n, quantum);  break;
    }

    printMetrics(proc_list, n);
    free(proc_list);
}

int main(int argc, char *argv[])
{
    int quantum = DEFAULT_QUANTUM;

    if (argc < 2) {
        fprintf(stderr, "Usage: ./schedsim <input-file-path> [quantum]\n");
        return 1;
    }

    /* Optional second argument sets the Round Robin quantum */
    if (argc >= 3) {
        quantum = atoi(argv[2]);
        if (quantum <= 0) {
            fprintf(stderr, "Error: quantum must be a positive integer\n");
            return 1;
        }
    }

    for (int alg = 0; alg < ALG_COUNT; alg++)
        runSimulation(alg, argv[1], quantum);

    return 0;
}
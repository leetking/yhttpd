#ifndef WORKER_H__
#define WORKER_H__

/**
 * run worker as a process
 * @return: EXIT_SUCCESS or EXIT_FAIL
 */
extern int run_worker(int id, int server_fd);

#endif

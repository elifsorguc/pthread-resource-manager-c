#ifndef REMAN_H
#define REMAN_H
#define MAXR 1000 // max num of resource types supported

#define MAXT 100 // max num of threads supported
int reman_init(int t_count, int r_count, int avoid);
int reman_connect(int tid);
int reman_disconnect();
int reman_claim(int claim[]); // only for avoidance
int reman_request(int request[]);
int reman_release(int release[]);
int reman_detect();
void reman_print(char titlemsg[]);
#endif /* REMAN_H */
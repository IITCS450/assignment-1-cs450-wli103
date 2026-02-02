#include "common.h"
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
static void usage(const char *a){
    fprintf(stderr,"Usage: %s <pid>\n",a); 
    exit(1);
}

static int isnum(const char*s){
    for(;*s;s++) if(!isdigit(*s)) return 0; 
    return 1;
}

int main(int c,char**v){
    if(c!=2||!isnum(v[1])) usage(v[0]);

    int pid = atoi(v[1]);

    char stat_path[64], status_path[64], cmdline_path[64];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
    snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);

    FILE *file = fopen(stat_path, "r");
    if(!file) {
        if(errno == EACCES) DIE_MSG("permission denied");
        if(errno == ENOENT) DIE_MSG("pid not found");
        DIE("fopen stat");
    }

    char line[4096];
    if(!fgets(line, sizeof(line), file)) DIE("fgets stat");
    fclose(file);

    char *p = strrchr(line, ')');
    if(!p) DIE_MSG("bad /proc/<pid>/stat format");
    char *x = p + 2;

    char state;
    int ppid;
    unsigned long utime, stime;

    int count =sscanf(x, "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &state, &ppid, &utime, &stime);
    if(count != 4) DIE_MSG("could not parse /proc/<pid>/stat");

    long hz = sysconf(_SC_CLK_TCK);
    double cpu = (double)(utime + stime) / (double)hz;

    FILE *fc = fopen(cmdline_path, "r");
    char cmdline[4096] = {0};
    size_t n = 0;
    if(fc){
        n = fread(cmdline, 1, sizeof(cmdline)-1, fc);
        fclose(fc);
    }
    for(size_t i = 0; i < n; i++){
        if(cmdline[i] == '\0') cmdline[i] = ' ';
    }
    while(n > 0 && cmdline[n-1] == ' ') cmdline[--n] = '\0';

    FILE *fs = fopen(status_path, "r");
    if(!fs) {
        if(errno == EACCES) DIE_MSG("permission denied");
        if(errno == ENOENT) DIE_MSG("pid not found");
        DIE("fopen status");
    }
    char vmrss_line[256];
    long vmrss = -1;

    while(fgets(vmrss_line, sizeof(vmrss_line), fs)){
        if(strncmp(vmrss_line, "VmRSS:", 6) == 0){
            sscanf(vmrss_line + 6, "%ld", &vmrss);
            break;
        }
    }
    fclose(fs);

    printf("PID:%d\n", pid);
    printf("State:%c\n", state);
    printf("PPID:%d\n", ppid);
    printf("Cmd:%s\n", cmdline);
    printf("CPU:%.3f\n", cpu);
    printf("VmRSS:%ld\n", vmrss);

    return 0;
}

#define _GNU_SOURCE
#include <sched.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "monitor_ioctl.h"

#define STACK_SIZE (1024 * 1024)
#define MAX_CONTAINERS 10
#define SOCKET_PATH "/tmp/container_socket"

char stack[STACK_SIZE];

// =======================
// Metadata
// =======================
typedef struct {
    char id[32];
    pid_t pid;
    char state[16];
} container_t;

container_t containers[MAX_CONTAINERS];
int container_count = 0;

// =======================
// Container
// =======================
typedef struct {
    char rootfs[128];
    char command[128];
} container_args;

int container_main(void *arg) {
    container_args *args = (container_args *)arg;

    chroot(args->rootfs);
    chdir("/");
    mount("proc", "/proc", "proc", 0, NULL);

    char *exec_args[] = {"/bin/sh", "-c", args->command, NULL};
    execvp(exec_args[0], exec_args);

    return 1;
}

// =======================
// Run Container
// =======================
void run_container(char *id, char *rootfs, char *cmd) {

    container_args *arg = malloc(sizeof(container_args));
    strcpy(arg->rootfs, rootfs);
    strcpy(arg->command, cmd);

    pid_t pid = clone(container_main,
                      stack + STACK_SIZE,
                      CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD,
                      arg);

    // Register with kernel
    int fd = open("/dev/container_monitor", O_RDWR);
    if (fd >= 0) {
        ioctl(fd, REGISTER_PID, &pid);
        close(fd);
    }

    // Store metadata
    strcpy(containers[container_count].id, id);
    containers[container_count].pid = pid;
    strcpy(containers[container_count].state, "running");
    container_count++;

    waitpid(pid, NULL, 0);

    for (int i = 0; i < container_count; i++) {
        if (containers[i].pid == pid)
            strcpy(containers[i].state, "stopped");
    }

    printf("[Supervisor] Container %s exited\n", id);
}

// =======================
// Print Containers
// =======================
void print_containers() {
    printf("\n[Supervisor] Containers:\n");

    if (container_count == 0) {
        printf("  (no containers)\n");
        return;
    }

    for (int i = 0; i < container_count; i++) {
        printf("  ID: %s | PID: %d | State: %s\n",
               containers[i].id,
               containers[i].pid,
               containers[i].state);
    }
}

// =======================
// Stop Container
// =======================
void stop_container(char *id) {
    for (int i = 0; i < container_count; i++) {
        if (strcmp(containers[i].id, id) == 0) {
            kill(containers[i].pid, SIGKILL);
            strcpy(containers[i].state, "stopped");
            printf("[Supervisor] Stopped container %s\n", id);
            return;
        }
    }

    printf("[Supervisor] Container %s not found\n", id);
}

// =======================
// Supervisor
// =======================
void start_supervisor() {

    int server_fd, client_fd;
    struct sockaddr_un addr;
    char buffer_cmd[256];

    unlink(SOCKET_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("[Supervisor] Running...\n");

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);

        memset(buffer_cmd, 0, sizeof(buffer_cmd));
        read(client_fd, buffer_cmd, sizeof(buffer_cmd));

        char cmd[32], id[32], rootfs[128], exec_cmd[128];

        int parts = sscanf(buffer_cmd, "%s %s %s %[^\n]", cmd, id, rootfs, exec_cmd);

        if (parts >= 1) {

            if (strcmp(cmd, "start") == 0 && parts == 4)
                run_container(id, rootfs, exec_cmd);

            else if (strcmp(cmd, "ps") == 0)
                print_containers();

            else if (strcmp(cmd, "stop") == 0 && parts >= 2)
                stop_container(id);
        }

        close(client_fd);
    }
}

// =======================
// Client
// =======================
void send_command(char *command) {

    int sock;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    write(sock, command, strlen(command));
    close(sock);
}

// =======================
// Main
// =======================
int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage: engine [supervisor|start|ps|stop]\n");
        return 0;
    }

    if (strcmp(argv[1], "supervisor") == 0) {
        start_supervisor();
    }

    else if (strcmp(argv[1], "start") == 0) {

        if (argc < 5) {
            printf("Usage: engine start <id> <rootfs> <cmd>\n");
            return 0;
        }

        char command[256];
        snprintf(command, sizeof(command),
                 "start %s %s %s",
                 argv[2], argv[3], argv[4]);

        send_command(command);
    }

    else if (strcmp(argv[1], "ps") == 0) {
        send_command("ps");
    }

    else if (strcmp(argv[1], "stop") == 0) {

        if (argc < 3) {
            printf("Usage: engine stop <id>\n");
            return 0;
        }

        char command[64];
        snprintf(command, sizeof(command),
                 "stop %s", argv[2]);

        send_command(command);
    }

    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

static void print_pretty_os_name(void) {
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp) {
        printf("Distribution    : Unavailable (/etc/os-release not found)\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *val = line + 12;
            /* Strip trailing newline and surrounding quotes */
            size_t len = strlen(val);
            while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r' || val[len - 1] == '"')) {
                val[--len] = '\0';
            }
            if (*val == '"') {
                val++;
            }
            printf("Distribution    : %s\n", val);
            fclose(fp);
            return;
        }
    }

    fclose(fp);
    printf("Distribution    : Unknown\n");
}

int main(void) {
    struct utsname uts;
    char hostname[256];

    printf("=== System Information Collector ===\n");

    /* 1. Hostname via POSIX gethostname() */
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hostname[sizeof(hostname) - 1] = '\0';
        printf("Hostname        : %s\n", hostname);
    } else {
        perror("gethostname");
    }

    /* 2. OS & Processor Architecture via uname() syscall */
    if (uname(&uts) == 0) {
        printf("OS Kernel       : %s\n", uts.sysname);
        printf("Kernel Release  : %s\n", uts.release);
        printf("Kernel Version  : %s\n", uts.version);
        printf("Architecture    : %s\n", uts.machine);
    } else {
        perror("uname");
        return EXIT_FAILURE;
    }

    /* 3. Human-readable Linux distro name from /etc/os-release */
    print_pretty_os_name();

    return EXIT_SUCCESS;
}

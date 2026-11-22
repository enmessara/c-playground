#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define BOX_WIDTH 64

static void print_divider(char left, char fill, char right) {
    putchar(left);
    for (int i = 0; i < BOX_WIDTH - 2; i++) {
        putchar(fill);
    }
    putchar(right);
    putchar('\n');
}

static void print_section(const char *title) {
    print_divider('+', '-', '+');
    printf("| %-60s |\n", title);
    print_divider('+', '-', '+');
}

static void print_row(const char *label, const char *value) {
    printf("| %-22s : %-35.35s |\n", label, value);
}

static void get_distro_name(char *buf, size_t size) {
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp) {
        snprintf(buf, size, "Unavailable");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *val = line + 12;
            size_t len = strlen(val);
            while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r' || val[len - 1] == '"')) {
                val[--len] = '\0';
            }
            if (*val == '"') {
                val++;
            }
            snprintf(buf, size, "%s", val);
            fclose(fp);
            return;
        }
    }

    fclose(fp);
    snprintf(buf, size, "Unknown Linux");
}

static unsigned long read_memavailable_kb(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return 0;
    }

    char key[64];
    unsigned long value = 0;
    char unit[32];

    while (fscanf(fp, "%63s %lu %31s", key, &value, unit) == 3) {
        if (strcmp(key, "MemAvailable:") == 0) {
            fclose(fp);
            return value;
        }
    }

    fclose(fp);
    return 0;
}

static void format_bytes(unsigned long long bytes, char *buf, size_t size) {
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double mib = 1024.0 * 1024.0;

    if (bytes >= (unsigned long long)gib) {
        snprintf(buf, size, "%.2f GiB (%llu MiB)", bytes / gib, bytes / (1024ULL * 1024ULL));
    } else {
        snprintf(buf, size, "%.2f MiB", bytes / mib);
    }
}

int main(int argc, char *argv[]) {
    struct utsname uts;
    struct sysinfo si;
    char hostname[256] = "Unavailable";
    char distro[128];
    char cwd[PATH_MAX] = "Unavailable";
    char timestamp[64] = "Unavailable";
    char val_buf[128];

    /* Timestamp for execution report */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S %Z", tm_info);
    }

    /* Header */
    print_divider('+', '=', '+');
    printf("| %-60s |\n", "SYSTEM INFORMATION COLLECTOR");
    print_divider('+', '=', '+');

    /* 1. Host & Operating System Information */
    print_section("1. HOST & OPERATING SYSTEM");

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hostname[sizeof(hostname) - 1] = '\0';
    }
    print_row("Hostname", hostname);

    get_distro_name(distro, sizeof(distro));
    print_row("Distribution", distro);

    if (uname(&uts) == 0) {
        print_row("OS Kernel", uts.sysname);
        print_row("Kernel Release", uts.release);

        /* 2. Processor Architecture */
        print_section("2. PROCESSOR & ARCHITECTURE");
        print_row("Architecture", uts.machine);
    } else {
        print_row("OS Kernel", strerror(errno));
        print_section("2. PROCESSOR & ARCHITECTURE");
        print_row("Architecture", "Unavailable");
    }

    long online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    long conf_cpus = sysconf(_SC_NPROCESSORS_CONF);
    if (online_cpus > 0 && conf_cpus > 0) {
        snprintf(val_buf, sizeof(val_buf), "%ld online / %ld configured", online_cpus, conf_cpus);
        print_row("Logical Cores", val_buf);
    }

    /* 3. Memory Information */
    print_section("3. MEMORY INFORMATION");

    if (sysinfo(&si) == 0) {
        unsigned long long unit = si.mem_unit ? si.mem_unit : 1;
        unsigned long long total_ram = (unsigned long long)si.totalram * unit;
        unsigned long long free_ram = (unsigned long long)si.freeram * unit;
        unsigned long long total_swap = (unsigned long long)si.totalswap * unit;
        unsigned long long free_swap = (unsigned long long)si.freeswap * unit;
        unsigned long avail_kb = read_memavailable_kb();

        format_bytes(total_ram, val_buf, sizeof(val_buf));
        print_row("Total RAM", val_buf);

        if (avail_kb > 0) {
            format_bytes((unsigned long long)avail_kb * 1024ULL, val_buf, sizeof(val_buf));
            print_row("Available RAM", val_buf);
        }

        format_bytes(free_ram, val_buf, sizeof(val_buf));
        print_row("Unallocated RAM", val_buf);

        format_bytes(total_swap, val_buf, sizeof(val_buf));
        print_row("Total Swap", val_buf);

        format_bytes(free_swap, val_buf, sizeof(val_buf));
        print_row("Free Swap", val_buf);
    } else {
        print_row("Memory Status", strerror(errno));
    }

    /* 4. Program Execution Information */
    print_section("4. PROGRAM EXECUTION DETAILS");

    print_row("Executed At", timestamp);
    print_row("Executable (argv[0])", (argc > 0 && argv[0]) ? argv[0] : "Unknown");

    snprintf(val_buf, sizeof(val_buf), "%d", argc);
    print_row("Argument Count", val_buf);

    snprintf(val_buf, sizeof(val_buf), "PID %d (Parent PPID %d)", getpid(), getppid());
    print_row("Process IDs", val_buf);

    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    snprintf(val_buf, sizeof(val_buf), "%s (UID %u, GID %u)",
             pw ? pw->pw_name : "unknown", uid, getgid());
    print_row("Effective User", val_buf);

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        print_row("Working Directory", cwd);
    } else {
        print_row("Working Directory", strerror(errno));
    }

    print_divider('+', '=', '+');
    return EXIT_SUCCESS;
}

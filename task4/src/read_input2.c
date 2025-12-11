#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string.h>
#include <sys/ioctl.h>

// Task 2

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s /dev/input/eventX\n", argv[0]);
        return 1;
    }

    const char *device_path = argv[1];

    // check permission for reading
    if (access(device_path, R_OK) != 0) {
        printf("You dont't have permission to read %s", device_path);
        return 1;
    }


    // Открыть файл устройства для чтения (O_RDONLY)
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    printf("Reading events from %s. Press Ctrl+C to exit.\n", device_path);

    char phys_name[64];
    char phys_path[64];

    //get phys_name and phys_path
    if (ioctl(fd, EVIOCGPHYS(sizeof(phys_path)), phys_path) == -1 || ioctl(fd, EVIOCGNAME(sizeof(phys_name)), phys_name) == -1) {
        printf("Error in ioctl!\n");
        close(fd);
        return 1;
    }
    
    printf("Phys name: %s \nPhys path: %s\n", phys_name, phys_path);

    close(fd);

    return 0;
}

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>

int main() {
    char path[] = "../jobs";
    DIR *dir = opendir(path);

    if(dir == NULL) {
        printf("Error! Unable to read directory\n");
        exit(1);
    }

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        // Ignore everything that is not a file
        if(entry->d_type != DT_REG) continue;
        char * name = entry->d_name;
        // Ignore everything that is not a .jobs file
        if(strstr(name, ".jobs") == NULL) continue;

        printf("\n%s\n", entry->d_name);

    }

    closedir(dir);
    return 0;
}

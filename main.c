
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char *STANDARD_LOGFILE_NAME = "projectMonitorOut.txt";


struct trackerEnum {

    char file_to_track[1024];
    char log_file_out[1024];

};

int main(int argc, char *argv[]) {

    FILE *newFile;

    char file_to_track[1024];
    int found = 0;
    
    struct trackerEnum newTracker;

    printf("Starting file\n");
    printf("Found %d arguments\n", argc);

    
    for (int i = 0 ; i < argc; i++) {
        if (strcmp("-f", argv[i]) == 0) {
            if (i + 1 == argc) {
                printf("-f argument is missing a filename after it. Exiting\n");
                return 1;
            }
            printf("Found -f, next arg is %s\n", argv[i+1]);
            strcpy(newTracker.file_to_track, argv[i+1]);
            found = 1; 
        }
    }

    if (found == 0) {
        printf("Program requires a -f file for input. Exiting.\n");
        return 1;
    }


    // checking the file specified on the 
    if (!(newFile = fopen(newTracker.file_to_track, "r"))) {

        printf("Failed to open the file to track : \"%s\". Exiting\n", newTracker.file_to_track);

        // closing the file on exit
        fclose(newFile);
        return 1;
        
    }
    
    
    /**
     * Main
     * 
     * Arguments:
     *  -f for new file to watch
     *  -o for log to output this too // defaults to the standard log out 
     */
    
    
    // closing the file on exit
    fclose(newFile);

    return 0;
}
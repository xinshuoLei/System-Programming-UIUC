// author: xinshuo3

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

int check_file_exists(char* filename) {
    struct stat s;
    if (stat(filename, &s) == 0) {
        return 1;
    }
    return 0;
}

off_t get_file_size(char* filename) {
    struct stat s;
    if (stat(filename, &s) == 0) {
        return s.st_size;
    }
    // error
    return -1;
}

int main(int argc, char* argv[]) {
    if (strcmp(argv[0], "./encrypt") == 0) {
        // encrypt mode

        // check argument number
        if (argc != 4) {
            printf("Hello World\n");
            exit(0);
        }
        char* input = argv[1];
        char* output1 = argv[2];
        char* output2 = argv[3];
        // check if output files exists
        if (check_file_exists(output1) || check_file_exists(output2)) {
            printf("Hello World\n");
            exit(0);
        }
        if (check_file_exists(input) == 0) {
            // input file does not exist
            printf("Hello World\n");
            exit(0);
        } 

        // open input file with open and mmap
        int input_fd = open(input, O_RDWR);
        off_t input_size = get_file_size(input);
        if (input_fd == -1) {
            // error when opening input file
            exit(1);
        }
        char* input_buffer = mmap(NULL, input_size, PROT_READ | PROT_WRITE, MAP_SHARED, input_fd, 0);

        // create output files
        FILE* fp_out1 = fopen(output1, "w");
        FILE* fp_out2 = fopen(output2, "w");
        if (fp_out1 == NULL || fp_out2 == NULL) {
            // error when creating output files
            exit(1);
        }

        // get random bytes from /dev/urandom
        int random_fd = open("/dev/urandom", O_RDONLY);
        char random_buffer[input_size];
        if (read(random_fd, random_buffer, input_size) == -1) {
            // error when reading random bytes
            exit(1);
        }

        // modify output and input files
        int i = 0;
        for(; i < input_size; i++) {
            fputc(random_buffer[i], fp_out1);
            fputc(random_buffer[i] ^ input_buffer[i], fp_out2);
        }
        memset(input_buffer, 0, input_size);
        
        // clean up 
        close(input_fd);
        fclose(fp_out1);
        fclose(fp_out2);

    } else if (strcmp(argv[0], "./decrypt") == 0) {
        // decrypt mode

        // check argument numbers
        if (argc != 4) {
            printf("Hello World\n");
            exit(0);
        }

        char* input1 = argv[2];
        char* input2 = argv[3];
        if (check_file_exists(input1) == 0 || check_file_exists(input2) == 0) {
            // input files do not exist
            printf("Hello World\n");
            exit(0);
        }
        // check input files have same size
        if (get_file_size(input1) != get_file_size(input2)) {
            printf("Hello World\n");
            exit(0);
        }

        // process input files using open and mmap
        int input_fd1 = open(input1, O_RDONLY);
        int input_fd2 = open(input2, O_RDONLY);
        off_t input_file_size = get_file_size(input1);
        if (input_fd1 == -1 || input_fd2 == -1) {
            // error when open input files
            exit(1);
        }
        char* input_buffer1 = mmap(NULL, input_file_size, PROT_READ, MAP_SHARED, input_fd1, 0);
        char* input_buffer2 = mmap(NULL, input_file_size, PROT_READ, MAP_SHARED, input_fd2, 0);

        // create and write output file
        char* output = argv[1];
        FILE* fp_out = fopen(output, "w");
        if (fp_out == NULL) {
            // error when creating output file
            exit(1);
        }
        off_t j = 0;
        for(; j < input_file_size; j++) {
            fputc(input_buffer1[j] ^ input_buffer2[j], fp_out);
        }

        // clean up and exit
        close(input_fd1);
        close(input_fd2);
        fclose(fp_out);
        unlink(input1);
        unlink(input2);
        
    } else {
        printf("Hello World\n");
        exit(0);
    }
    return 0;
}
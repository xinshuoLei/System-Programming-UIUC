/**
 * perilous_pointers
 * CS 241 - Spring 2021
 */
#include "part2-functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * (Edit this function to print out the "Illinois" lines in
 * part2-functions.c in order.)
 */
int main() {
    // your code here
    first_step(81);

    int second = 132;
    second_step(&second);

    int double_ = 8942;
    int* double_arr[1];
    double_arr[0] = &double_;
    double_step(double_arr);

    int strange = 15;
    strange_step((char*) &strange - 5);

    char empty[4];
    empty[3] = 0;
    empty_step(empty);

    char s2[4];
    s2[3] = 'u';
    two_step(s2, s2);

    char three_[5];
    three_step(three_, three_+2, three_ +4);

    char first_arr[2];
    char second_arr[3];
    char third_arr[4];
    first_arr[1] = 'a';
    second_arr[2] = first_arr[1] + 8;
    third_arr[3] = second_arr[2] + 8;
    step_step_step(first_arr, second_arr, third_arr);

    int b = 5;
    char a_char = 0;
    char* a = &a_char;
    *a = b;
    it_may_be_odd(a, b);

    char tok[] = "x,CS241,";
    tok_step(tok);

    char blue[2];
    blue[0] = 1;
    blue[1] = 2; 
    the_end(blue, blue);

    return 0;
}

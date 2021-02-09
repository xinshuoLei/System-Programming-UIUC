/**
 * extreme_edge_cases
 * CS 241 - Spring 2021
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camelCaser.h"
#include "camelCaser_tests.h"

int test_camelCaser(char **(*camelCaser)(const char *),
                    void (*destroy)(char **)) {
    // TODO: Implement me!
    
    // test 0: test input is NULL
    char** result0 = camelCaser(NULL);
    if (result0 != NULL) {
        destroy(result0);
        return 0;
    }

    // test 1: test empty string
    char** result1 = camelCaser("");
    if (result1[0] != NULL) {
        destroy(result1);
        return 0;
    }
    destroy(result1);
    


    // test 2: test single punct
    // output should be "", NULL
    char** result2 = camelCaser(".");
    if (result2[0][0] != '\0' || result2[1] != NULL) {
        destroy(result2);
        return 0;
    }
    destroy(result2);


    // test 3: test continuous punct, punct separated by a space and non-sentence
    char** result3 = camelCaser(".! .,Hello World");  
    if (result3[4] != NULL) {
        destroy(result3);
        return 0;
    }
    char* expected3[5];
    expected3[0] = "";
    expected3[1] = "";
    expected3[2] = "";
    expected3[3] = "";
    expected3[4] = NULL;
    int i = 0;
    while(expected3[i]) {
        if(strcmp(expected3[i], result3[i]) != 0) {
            destroy(result3);
            return 0;
        }
        i++;
    }
    destroy(result3);
    

    // test 4: a short normal input with 1 sentence
    char** result4 = camelCaser("hello. welcome to cs241");
    if (result4[1] != NULL) {
        destroy(result4);
        return 0;
    }
    char* expected4[2];
    expected4[0] = "hello";
    expected4[1] = NULL;
    i = 0;
    while (expected4[i]) {
        if (strcmp(expected4[i], result4[i]) != 0) {
            destroy(result4);
            return 0;
        }
        i++;
    }
    destroy(result4);


    // test 5: test special character and number
    char** result5 = camelCaser("1\t23abc(8)");
    if (result5[2] != NULL) {
        destroy(result5);
        return 0;
    }
    char* expected5[3];
    expected5[0] = "123Abc";
    expected5[1] = "8";
    expected5[2] = NULL;
    i = 0;
    while(expected5[i]) {
        if (strcmp(expected5[i], result5[i]) != 0) {
            destroy(result5);
            return 0;
        }
        i++;
    }
    destroy(result5);

    // test 6: test continus punct and a single upper case letter
    char** result6 = camelCaser(".[3hello world]A(.?");
    if (result6[6] != NULL) {
        destroy(result6);
        return 0;
    }
    char* expected6[7];
    expected6[0] = "";
    expected6[1] = "";
    expected6[2] = "3helloWorld";
    expected6[3] = "a";
    expected6[4] = "";
    expected6[5] = "";
    expected6[6] = NULL;
    i = 0;
    while(expected6[i]) {
        if (strcmp(expected6[i], result6[i]) != 0) {
            destroy(result6);
            return 0;
        }
        i++;
    }
    destroy(result6);


    // test 7: a long normal input
    char** result7 = camelCaser("The Heisenbug is an incredible creature. Facenovel servers get their power from its indeterminism. Code smell can be ignored with INCREDIBLE use of air freshener. God objects are the new religion.");
    if (result7[4] != NULL) {
        destroy(result7);
        return 0;
    }
    char* expected7[5];
    expected7[0] = "theHeisenbugIsAnIncredibleCreature";
    expected7[1] = "facenovelServersGetTheirPowerFromItsIndeterminism";
    expected7[2] = "codeSmellCanBeIgnoredWithIncredibleUseOfAirFreshener";
    expected7[3] = "godObjectsAreTheNewReligion";
    expected7[4] = NULL;
    i = 0;
    while(expected7[i]) {
        if (strcmp(expected7[i], result7[i]) != 0) {
            destroy(result7);
            return 0;
        }
        i++;
    }
    destroy(result7);

    // test 8: random long string
    char** result8 = camelCaser("uVe1w no62c!1u 0A8J6eu2. 81JcqbV dxESUT KrBOD AQ(VXOaXP6AV1");
    if (result8[3] != NULL) {
        destroy(result8);
        return 0;
    }
    char* expected8[4];
    expected8[0] = "uve1wNo62c";
    expected8[1] = "1u0A8j6eu2";
    expected8[2] = "81jcqbvDxesutKrbodAq";
    expected8[3] = NULL;
    i = 0;
    while(expected8[i]) {
        if (strcmp(expected8[i], result8[i]) != 0) {
            destroy(result8);
            return 0;
        }
        i++;
    }
    destroy(result8);

    // test 9: a special test
    char** result9 = camelCaser("1 23Abc.");
    if ((strcmp(result9[0], "123Abc") != 0) && result9[1] != NULL) {
        destroy(result9);
        return 0;
    } 
    destroy(result9);
    return 1;
}

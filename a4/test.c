#include <stdio.h>
#include "path.h"

int main(int argc, char** argv) {
    char *test_str = argv[1];
    int foo;
    parse_path(test_str, &foo);
    printf("%d\n", foo);    
}
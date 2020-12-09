#pragma once
#include <stdio.h>
#include <stdlib.h>

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

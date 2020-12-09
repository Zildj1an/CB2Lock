#pragma once

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

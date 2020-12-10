#ifndef __MAP_WRAPPER_H
#define __MAP_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

void insert_if_new(int key);

int get_and_increase(int key);

void map_decrease(int key);

#ifdef __cplusplus
}
#endif
#endif

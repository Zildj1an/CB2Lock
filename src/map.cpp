#include "map.h"
#include <iostream>
#include <map>

static std::map<int,int> k_map;

void insert_if_new(int key)
{
	if (k_map.find(key) == k_map.end()){
		k_map[key] = 0;
	}
}

int get_and_increase(int key)
{
	return k_map[key]++;
}

void map_decrease(int key)
{
	if (k_map[key]){
		k_map[key]--;
	}
}


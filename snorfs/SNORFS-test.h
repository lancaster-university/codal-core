#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// min/max
#include <algorithm>
using namespace std;

//#define LOG(...) do{}while(0)
#define LOG printf

#define LOGV(...) do{}while(0)
//#define LOGV printf

#define target_panic(...) assert(false)

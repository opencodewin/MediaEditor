#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

double rand_float() {
    /* returns a random floating point number between 0.0 and 1.0 */
    static bool initialized = false;
    if(!initialized) {
        srand((uint32_t)time(NULL));
        initialized = true;
    }
    return (double)rand() / RAND_MAX;
}

double box_muller(double sigma, double mean) {
    /* Box-Muller algorithm:
     * generates a normal distributed random number between 0 and 2*mean.
     * useful default values: sigma=2.5, mean=50.0
    */
    double r1 = rand_float();
    double r2 = rand_float();
    double x = sigma * sqrt(-2 * log(r1)) * cos(2 * M_PI * r2) + mean;
    return fmin(fmax(x, 0), mean * 2);
}

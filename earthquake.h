#ifndef EARTHQUAKE_H
#define EARTHQUAKE_H

typedef enum {
    past_time_class1, past_time_class2, past_time_class3,
    past_time_class4, past_time_class5
} past_time_t;

#define NUM_PAST_TIME_CLASS 5

typedef struct {
    time_t time;
    double lat;
    double lon;
    double pos[3];
    float magnitude;
    past_time_t past_time_class;
    unsigned char radius_factor;
//    float depth;
} earthquake_info_t;

enum { radius_factor1 = 1,
    radius_factor2 = 2,
    radius_factor3 = 3,
    radius_factor4 = 4,
    radius_factor5 = 6,
    radius_factor6 = 8,
    radius_factor7 = 12,
    radius_factor8 = 18,
};

#define MAX_EARTHQUAKE_ENTRY    500
typedef struct {
    earthquake_info_t *item;
    int count;
} earthquake_list_t;

void get_earthquake_data (void);

earthquake_list_t *get_earthquake_list (void);

extern time_t cur_earth_dat_file_mtime;

#endif                          /* EARTHQUAKE_H */
/* vim: set sw=4 ts=8 sts=4 expandtab spell : */

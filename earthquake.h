#ifndef EARTHQUAKE_H
#define EARTHQUAKE_H

typedef enum {
   past_time_class1, past_time_class2, past_time_class3,
   past_time_class4, past_time_class5
} past_time_t;

typedef struct {
    time_t time;
    double lat;
    double lon;
    double pos[3];
    float magnitude;
    past_time_t past_time_class;
//    float depth;
} earthquake_info_t;

#define MAX_EARTHQUAKE_ENTRY    500
typedef struct {
    earthquake_info_t *item;
    int count;
} earthquake_list_t;

void get_earthquake_data (void);

earthquake_list_t *get_earthquake_list (void);

#endif /* EARTHQUAKE_H */
/* vim: set sw=4 ts=8 sts=4 expandtab spell : */

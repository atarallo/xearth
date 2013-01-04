/* define earthquake plot for xearth
 * It is assumed that ~/.config directory always exist */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <pwd.h>

#include "earthquake.h"

#define EARTHQUAKE_4_MONTH_URL	    "http://earthquake.usgs.gov/earthquakes" \
                                    "/feed/csv/4.5/month"

#define EARTHQUAKE_4_WEEK_URL	    "http://earthquake.usgs.gov/earthquakes" \
                                    "/feed/csv/2.5/week"

#define DEFAULT_EARTHQUAKE_DATA_DIR ".config/xearth"
#define EARTHQUAKE_MONTH_FILE	    DEFAULT_EARTHQUAKE_DATA_DIR "/month"
#define EARTHQUAKE_WEEK_FILE	    DEFAULT_EARTHQUAKE_DATA_DIR "/week"

#define EARTHQUAKE_INFO_URL EARTHQUAKE_4_WEEK_URL
#define EARTHQUAKE_FILE     EARTHQUAKE_WEEK_FILE

static void test_config_dir (void);
static void load_earthquake_marker (void);

static time_t pre_dat_file_mtime = 0;
static time_t last_check_time = 0;

time_t cur_earth_dat_file_mtime = 0;

// the week's data are updated every minute at earthquake.usgs.gov
// but we only query at most once per 15 minutes
static time_t earthquake_min_update_intrval = 15 * 60;

/* use wget to retrieve the earthquake information */
static earthquake_info_t earthquake_items[MAX_EARTHQUAKE_ENTRY];

static earthquake_list_t earthquake_lst = {
    .item = earthquake_items,
    .count = 0,
};

static void update_ele_past_time_class (earthquake_info_t *ele);
static void update_earthquake_past_time_class (void);

extern time_t current_time;

void
get_earthquake_data (void)
{
    pid_t pid;
    struct stat file_stat;

    // data file not updated yet, used old data
    if (last_check_time + earthquake_min_update_intrval > current_time)
        return;
    test_config_dir ();
    // get earthquake data with wget
    pid = fork ();
    if (pid == 0) {     // child
        chdir (DEFAULT_EARTHQUAKE_DATA_DIR);
        execlp ("wget", "wget", "-Nq", EARTHQUAKE_INFO_URL, NULL);
    } else if (pid > 0) {       // parent
        int status;

        waitpid (pid, &status, 0);
    }                   // end if
    if (stat (EARTHQUAKE_FILE, &file_stat) < 0) {
        // not a fatal error, just continue
        return;
    }                   // end if
    cur_earth_dat_file_mtime = file_stat.st_mtime;
    if (cur_earth_dat_file_mtime != pre_dat_file_mtime) {
        pre_dat_file_mtime = cur_earth_dat_file_mtime;
        load_earthquake_marker ();
    } else {
        update_earthquake_past_time_class ();
    }                   // end if
    last_check_time = current_time;
}                       // end get_earthquake_data

earthquake_list_t *
get_earthquake_list (void)
{
    return &earthquake_lst;
}                       // end get_earthquake_list

static void
test_config_dir (void)
{
    DIR *dir;
    struct passwd *pw = getpwuid (getuid ());

    if (pw == NULL) {
        fprintf (stderr, "Failed to get home directory: %s\n",
                 strerror (errno));
        exit (1);
    }
    chdir (pw->pw_dir);
    dir = opendir (DEFAULT_EARTHQUAKE_DATA_DIR);
    if (dir == NULL) {
        if (mkdir (DEFAULT_EARTHQUAKE_DATA_DIR, 0700) < 0) {
            fprintf (stderr, "Failed to create directory: %s\n",
                     strerror (errno));
            exit (1);
        }               // end if
    }                   // end if
}                       // end test_config_dir

#define PAST_2HRS   (2 * 60 * 60)
#define PAST_DAY    (12 * PAST_2HRS)
#define PAST_2DAYS  (2 * PAST_DAY)
#define PAST_4DAYS  (4 * PAST_DAY)

static void update_ele_data (earthquake_info_t *ele);

static void
load_earthquake_marker (void)
{
    char buf[512];
    char *save_ptr;
    char *token;
    struct tm tm;
    double lat;
    double lon;
    float mag;

    FILE *fp = fopen (EARTHQUAKE_FILE, "r");

    if (!fp) {
        fprintf (stderr, "Failed to open ~/%s: %s\n", EARTHQUAKE_FILE,
                 strerror (errno));
        exit (1);
    }                   // end if
    fgets (buf, sizeof (buf), fp);      // skip the first title line
    earthquake_lst.count = 0;
    printf ("Loading earthquake data file ...\n");
    while (earthquake_lst.count < MAX_EARTHQUAKE_ENTRY
           && fgets (buf, sizeof (buf), fp)) {
        // Datetime
        token = strtok_r (buf, ",", &save_ptr);
        memset (&tm, 0, sizeof (tm));
        strptime (token, "%Y-%m-%dT%H:%M:%S", &tm);

        earthquake_lst.item[earthquake_lst.count].time = timegm (&tm);
        update_ele_past_time_class (earthquake_lst.item +
                                    earthquake_lst.count);

        // lat
        token = strtok_r (NULL, ",", &save_ptr);
        lat = strtod (token, NULL) * M_PI / 180;
        earthquake_lst.item[earthquake_lst.count].lat = lat;

        // lon
        token = strtok_r (NULL, ",", &save_ptr);
        lon = strtod (token, NULL) * M_PI / 180;
        earthquake_lst.item[earthquake_lst.count].lon = lon;

        // depth
        token = strtok_r (NULL, ",", &save_ptr);
#if 0
        earthquake_list[earthquake_count].depth = strtof (token, NULL);
#endif
        // magnitude
        token = strtok_r (NULL, ",", &save_ptr);
        mag = strtof (token, NULL);
        earthquake_lst.item[earthquake_lst.count].magnitude = mag;

        update_ele_data (earthquake_lst.item + earthquake_lst.count);

        ++earthquake_lst.count;
    }                   // end while
    fclose (fp);
    printf ("Total of %d earthquake entries loaded.\n", earthquake_lst.count);
}                       // end load_earthquake_marker

static void
update_ele_data (earthquake_info_t *ele)
{
    // find radius factor
    if (ele->magnitude >= 8.0)
        ele->radius_factor = radius_factor8;
    else if (ele->magnitude >= 7.0)
        ele->radius_factor = radius_factor7;
    else if (ele->magnitude >= 6.0)
        ele->radius_factor = radius_factor6;
    else if (ele->magnitude >= 5.0)
        ele->radius_factor = radius_factor5;
    else if (ele->magnitude >= 4.0)
        ele->radius_factor = radius_factor4;
    else if (ele->magnitude >= 3.0)
        ele->radius_factor = radius_factor3;
    else if (ele->magnitude >= 2.0)
        ele->radius_factor = radius_factor2;
    else
        ele->radius_factor = radius_factor1;

    // pe-calculated 3 positions
    ele->pos[0] = sin (ele->lon) * cos (ele->lat);
    ele->pos[1] = sin (ele->lat);
    ele->pos[2] = cos (ele->lon) * cos (ele->lat);
}                       // end update_ele_data

static void
update_ele_past_time_class (earthquake_info_t *ele)
{
    double time_diff;

    time_diff = difftime (current_time, ele->time);

    if (time_diff <= PAST_2HRS) {
        ele->past_time_class = past_time_class1;
    } else if (time_diff <= PAST_DAY) {
        ele->past_time_class = past_time_class2;
    } else if (time_diff <= PAST_2DAYS) {
        ele->past_time_class = past_time_class3;
    } else if (time_diff <= PAST_4DAYS) {
        ele->past_time_class = past_time_class4;
    } else {
        ele->past_time_class = past_time_class5;
    }
}                       // end update_ele_past_time_class

static void
update_earthquake_past_time_class (void)
{
    int i;

    for (i = 0; i < earthquake_lst.count; ++i) {
        update_ele_past_time_class (earthquake_lst.item + i);
    }                   // end for
}                       // end update_earthquake_past_time_class

/* vim: set sw=4 ts=8 sts=4 expandtab spell : */

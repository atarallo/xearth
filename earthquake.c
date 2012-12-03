/* define earthquake plot for xearth
 * It is assumed that ~/.config directory always exist */

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
static time_t cur_dat_file_mtime = 0;
static time_t last_check_time = 0;

// the week's data are updated every minute at earthquake.usgs.gov
// but we only query at most once per 15 minutes
static time_t earthquake_min_update_intrval = 15 *60;
/* use wget to retrieve the earthquake information */
static earthquake_info_t earthquake_items[MAX_EARTHQUAKE_ENTRY];

static earthquake_list_t earthquake_lst = {
    .item  = earthquake_items,
    .count = 0,
};

extern time_t current_time;

void
get_earthquake_data (void)
{
    pid_t pid;
    struct stat file_stat;

    // data file not updated yet, used old data
    if (last_check_time + earthquake_min_update_intrval > current_time)
        return;
    test_config_dir();
    // get earthquake data with wget
    pid = fork ();
    if (pid == 0) { // child
	chdir (DEFAULT_EARTHQUAKE_DATA_DIR); 
	execlp ("wget", "wget", "-Nq", EARTHQUAKE_INFO_URL, NULL);
    } else if (pid > 0) { // parent
	int status;
	waitpid (pid, &status, 0);
	if (WEXITSTATUS(status)) {
	    exit (1);
	} // end if
    } else {
	fprintf (stderr, "failed to fork(): %s\n", strerror (errno));
	exit (1);
    } // end if
    if (stat (EARTHQUAKE_FILE, &file_stat) < 0) {
        fprintf (stderr, "stat earth quake dat file failed: %s\n",
                 strerror (errno));
        exit (1);
    } // end if
    cur_dat_file_mtime = file_stat.st_mtime;
    if (cur_dat_file_mtime != pre_dat_file_mtime) {
        pre_dat_file_mtime = cur_dat_file_mtime;
        load_earthquake_marker ();
    } // end if
    last_check_time = current_time;
} // end get_earthquake_data

earthquake_list_t *
get_earthquake_list (void)
{
    return &earthquake_lst;
} // end get_earthquake_list

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
	} // end if
    } // end if
} // end test_config_dir

static int
get_week_of_day (const char *day)
{
    switch (day[0]) {
    case 'F': // Friday
        return 5;
    case 'M': // Monday
        return 1;
    case 'T':
        if (day[1] == 'u') // Tuesday
            return 2;
        else // Thursday
            return 4;
    case 'W': // Wednesday
        return 3;
    case 'S':
        if (day[1] =='u') // Sunday
            return 0;
        else // Saturday
            return 6;
    default:
        return 0;
    } // end switch
} // end get_week_of_day

static int
get_month (const char *mon)
{
    switch (mon[0]) {
    case 'J':
        if (mon[1] == 'a') // January
            return 0;
        if (mon[2] == 'n') // June
            return 5;
        // July
        return 6;
    case 'F': // February
        return 1;
    case 'M':
        if (mon[2] == 'r') // March
            return 2;
        else // May
            return 4;
    case 'A':
        if (mon[1] == 'p') // April
            return 3;
        else // August
            return 7;
    case 'S': // September
        return 8;
    case 'O': // October
        return 9;
    case 'N': // November
        return 10;
    case 'D': // December
        return 11;
    default:
        return 0;
    } // end switch
} // end get_month

#define PAST_2HRS   (2 * 60 * 60)
#define PAST_DAY    (12 * PAST_2HRS)
#define PAST_2DAYS  (2 * PAST_DAY)
#define PAST_4DAYS  (4 * PAST_DAY)

static void
load_earthquake_marker (void)
{
    char      buf[512];
    char      *save_ptr;
    char      *token;
    struct tm tm;
    time_t    tm_diff;
    double    lat;
    double    lon;
    float     mag;

    FILE *fp = fopen (EARTHQUAKE_FILE, "r");

    if (!fp) {
        fprintf(stderr, "Failed to open ~/%s: %s\n", EARTHQUAKE_FILE,
                strerror (errno));
        exit (1);
    } // end if
    fgets (buf, sizeof (buf), fp); // skip the first title line
    earthquake_lst.count = 0;
    printf ("Loading earthquake data file ...\n");
    while (earthquake_lst.count < MAX_EARTHQUAKE_ENTRY
           && fgets (buf, sizeof (buf), fp)) {
        // skip src
        token = strtok_r (buf, ",", &save_ptr);
        // skip EqID
        token = strtok_r (NULL, ",", &save_ptr);
        // skip Version
        token = strtok_r (NULL, ",", &save_ptr);
        // Datetime
        ++save_ptr; // skip the opening "
        token = save_ptr;
        memset (&tm, 0, sizeof (tm));
        // day
        token = strtok_r (NULL, ",", &save_ptr);
        tm.tm_wday = get_week_of_day (token);
        // month
        ++save_ptr; // skip the first white space
        token = strtok_r (NULL, " ", &save_ptr);
        tm.tm_mon = get_month (token);
        // day
        token = strtok_r (NULL, ",", &save_ptr);
        tm.tm_mday = strtol (token, NULL, 10);
        // year
        ++save_ptr;
        token = strtok_r (NULL, " ", &save_ptr);
        tm.tm_year = strtol (token, NULL, 10) - 1900;
        // hour
        token = strtok_r (NULL, ":", &save_ptr);
        tm.tm_hour = strtol (token, NULL, 10);
        // min
        token = strtok_r (NULL, ":", &save_ptr);
        tm.tm_min = strtol (token, NULL, 10);
        // seconds
        token = strtok_r (NULL, " ", &save_ptr);
        tm.tm_sec = strtol (token, NULL, 10);

        earthquake_lst.item[earthquake_lst.count].time = timegm (&tm);
        tm_diff = current_time - earthquake_lst.item[earthquake_lst.count].time;
        if (tm_diff <= PAST_2HRS) {
            earthquake_lst.item[earthquake_lst.count].past_time_class
                = past_time_class1;
        } else if (tm_diff <= PAST_DAY) {
            earthquake_lst.item[earthquake_lst.count].past_time_class
                = past_time_class2;
        } else if (tm_diff <= PAST_2DAYS) {
            earthquake_lst.item[earthquake_lst.count].past_time_class
                = past_time_class3;
        } else if (tm_diff <= PAST_4DAYS) {
            earthquake_lst.item[earthquake_lst.count].past_time_class
                = past_time_class4;
        } else {
            earthquake_lst.item[earthquake_lst.count].past_time_class
                = past_time_class5;
        }
        // skip to next token start
        save_ptr += 5;
        // lat
        token = strtok_r (NULL, ",", &save_ptr);
        lat = strtod (token, NULL) * (M_PI/180);
        earthquake_lst.item[earthquake_lst.count].lat = lat;

        // lon
        token = strtok_r (NULL, ",", &save_ptr);
        lon = strtod (token, NULL) * (M_PI/180);
        earthquake_lst.item[earthquake_lst.count].lon = lon;
        // magnitude
        token = strtok_r (NULL, ",", &save_ptr);
        mag = strtof (token, NULL);
        earthquake_lst.item[earthquake_lst.count].magnitude = mag;
        // find radius factor
        if (mag >= 8.0)
            earthquake_lst.item[earthquake_lst.count].radius_factor = 18;
        else if (mag >= 7.0)
            earthquake_lst.item[earthquake_lst.count].radius_factor = 12;
        else if (mag >= 6.0)
            earthquake_lst.item[earthquake_lst.count].radius_factor = 8;
        else if (mag >= 5.0)
            earthquake_lst.item[earthquake_lst.count].radius_factor = 6;
        else if (mag >= 4.0)
            earthquake_lst.item[earthquake_lst.count].radius_factor = 4;
        else if (mag >= 3.0)
            earthquake_lst.item[earthquake_lst.count].radius_factor = 3;
        else if (mag >= 2.0)
            earthquake_lst.item[earthquake_lst.count].radius_factor = 2;
        else
            earthquake_lst.item[earthquake_lst.count].radius_factor = 1;

        // preculated 3 positions
        earthquake_lst.item[earthquake_lst.count].pos[0] = sin (lon)
                                                           * cos (lat);
        earthquake_lst.item[earthquake_lst.count].pos[1] = sin (lat);
        earthquake_lst.item[earthquake_lst.count].pos[2] = cos (lon)
                                                           * cos (lat);

#if 0
        // depth
        token = strtok_r (NULL, ",", &save_ptr);
        earthquake_list[earthquake_count].depth = strtof (token, NULL);
#endif
        ++earthquake_lst.count;
    } // end while
    fclose (fp);
    printf ("Total of %d earthquake entries loaded.\n", earthquake_lst.count);
} // end load_earthquake_marker

/* vim: set sw=4 ts=8 sts=4 expandtab spell : */

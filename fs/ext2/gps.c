/*
 * Created by Suhyun Lee.
 * GPS-related operations.
 */


#include <linux/gps.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>

#define POW(x) (x) * (x)
#define TO_METER(x) (x) / 9	// approximately 1m = 8.9524 * 10^-6(degree)

/* 
 * check if the given gps location is near current location.
 */
int is_near(struct gps_location* target) {
	struct gps_location diff;
	
	spin_lock(&gps_lock);
	diff.lat_integer = target->lat_integer - __curr_gps_loc.lat_integer;
	diff.lat_fractional = target->lat_fractional - __curr_gps_loc.lat_fractional;
	diff.lng_integer = target->lng_integer - __curr_gps_loc.lng_integer;
	diff.lng_fractional = target->lng_fractional - __curr_gps_loc.lng_fractional;
	diff.accuracy = target->accuracy + __curr_gps_loc.accuracy;
	spin_unlock(&gps_lock);

	// not near
	if (diff.lat_integer != 0 || diff.lng_integer != 0) {
		return 0;
	}

	if (POW(TO_METER((long)diff.lat_fractional)) 
		+ POW(TO_METER((long)diff.lng_fractional)) < POW((long)diff.accuracy)) {
		return 1;
	}

	return 0;
}

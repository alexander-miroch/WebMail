#include <stdio.h>
#include <stdlib.h>

#include "wmail.h"
#include "driver.h"

extern struct error_struct error;
struct drivers **main_drivers;
int inited = 0;

driver_t dummy_drv = {
	NULL,
	NULL
};

int register_driver(driver_t *dr, int on) {
	struct drivers *tdrv,*t0drv;
	int i=0;

	if (!inited) init_drivers();
	for (tdrv = *main_drivers; tdrv; tdrv = tdrv->next,i++) { 
			if (!tdrv->next) {
				t0drv = newdrvs();
				t0drv->next = 0;
				t0drv->state = on;
				t0drv->drv = dr;
				tdrv->next = t0drv;
				break;
			}
			if (i > MAX_DRIVERS) {
				SETERR(E_MAXDRV,NULL);
				return -1;
			}
	}
	
	return 0;
}

driver_t *select_driver(void) {
	struct drivers *tdrv;

	for(tdrv = *main_drivers; tdrv; tdrv = tdrv->next) {
		if (!tdrv->state) continue;
		if (tdrv->drv) return tdrv->drv;
	}
	return (driver_t *) 0;
}


void init_drivers(void) {
	main_drivers = (struct drivers **) malloc(sizeof(struct drivers *) * MAX_DRIVERS);
	if (!main_drivers) {
		SETERR(E_NOMEM,NULL);
		return;
	}

	*main_drivers = newdrvs();
	return;
}

struct drivers *newdrvs(void) {
	struct drivers *drvs;

	drvs = (struct drivers *) malloc (sizeof(struct drivers));
	if (!drvs) {
		SETERR(E_NOMEM,NULL);
		return (struct drivers *) 0;
	}

	drvs->state = 0;
	drvs->next = 0;
	drvs->drv = &dummy_drv;
	return drvs;
}



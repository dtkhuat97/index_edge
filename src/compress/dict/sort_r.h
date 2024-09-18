/**
 * @file sort_r.h
 * @author FR
 */

#ifndef SORT_R_H
#define SORT_R_H

#include <stddef.h>

struct sort_r_data {
	void* arg;
	int (*compar)(const void* a1, const void* a2, void* aarg);
};

int sort_r_arg_swap(void* s, const void* aa, const void* bb) {
	struct sort_r_data* ss = (struct sort_r_data*) s;
	return (ss->compar)(aa, bb, ss->arg);
}

void sort_r(void* base, size_t nel, size_t width, int (*compar)(const void* a1, const void* a2, void* aarg), void* arg) {
#if (defined __APPLE__ || defined __MACH__ || defined __DARWIN__ || \
		 defined __FREEBSD__ || defined __BSD__ || \
		 defined OpenBSD3_1 || defined OpenBSD3_9)

	struct sort_r_data tmp;
	tmp.arg = arg;
	tmp.compar = compar;
	qsort_r(base, nel, width, &tmp, &sort_r_arg_swap);
#elif (defined _GNU_SOURCE || defined __GNU__ || defined __linux__)
	qsort_r(base, nel, width, compar, arg);
#elif (defined _WIN32 || defined _WIN64 || defined __WINDOWS__)
	struct sort_r_data tmp = {arg, compar};
	qsort_s(*base, nel, width, &sort_r_arg_swap, &tmp);
#else
	#error Cannot detect operating system
#endif
}

#endif

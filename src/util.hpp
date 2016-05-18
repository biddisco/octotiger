/*
 * util.hpp
 *
 *  Created on: Apr 20, 2016
 *      Author: dmarce1
 */

#ifndef UTILAA_HPP_
#define UTILAA_HPP_

#include <stdio.h>

int file_copy(const char* fin, const char* fout);

template<class... Args>
int lprintf( const char* log, const char* str, Args&&...args) {
	FILE* fp = fopen (log, "at");
	if( fp == NULL) {
		return -1;
	}
	fprintf( fp, str, std::forward<Args>(args)...);
	printf( str, std::forward<Args>(args)...);
	fclose(fp);
	return 0;
}


bool find_root(std::function<real(real)>& func, real xmin, real xmax,
		real& root, real toler = 1.0e-10);

inline real  assert_positive(real r, const char* filename, int line) {
	if( r <= 0.0 ) {
		FILE* fp = fopen("assert.log", "at");
		printf( "ASSERT_POSITIVE FAILED\n");
		printf( "file %s line %i\n", filename, line);
		fprintf( fp, "ASSERT_POSITIVE FAILED\n");
		fprintf( fp, "file %s line %i\n", filename, line);
		fclose(fp);
		abort();
	}
	return r;
}

inline void  assert_nonan(real r, const char* filename, int line) {
	if( std::isnan(r) ) {
		FILE* fp = fopen("assert.log", "at");
		printf( "ASSERT_NONAN FAILED\n");
		printf( "file %s line %i\n", filename, line);
		fprintf( fp, "ASSERT_NONAN FAILED\n");
		fprintf( fp, "file %s line %i\n", filename, line);
		fclose(fp);
		abort();
	}
}

#define ASSERT_POSITIVE(r) assert_positive((r), __FILE__, __LINE__)
#define ASSERT_NONAN(r) assert_nonan((r), __FILE__, __LINE__)


#endif /* UTIL_HPP_ */

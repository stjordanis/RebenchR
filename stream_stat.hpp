#ifndef __STREAM_STAT_HPP__
#define __STREAM_STAT_HPP__

#include "utils.hpp"
#include <vector>
#include <map>

struct stat_counters_t {
	long *histogram;

	double sum_values;
	long count;
	ticks_t min_value;
	ticks_t max_value;
};

struct stat_data_t {
	std::map<double, ticks_t> percentiles;
	double mean;
	long count;
	ticks_t min_value;
	ticks_t max_value;
};

class stream_stat_t {
public:
	stream_stat_t(int _buckets, int _bucket_size_exp);
	~stream_stat_t();
	
	void add(ticks_t value);

	stat_data_t get_global_stat();
	stat_data_t get_global_stat(std::vector<double> &percentile_marks);

	stat_data_t get_snapshot_stat();
	stat_data_t get_snapshot_stat(std::vector<double> &percentile_marks);

	void snapshot_and_reset();
	
private:

	std::vector<double> default_percentile_marks;

	stat_counters_t *global_stat;
	stat_counters_t *active_stat;
	stat_counters_t *snapshot_stat;
	int buckets;
	int bucket_size_exp;
	int bucket_size;

	void init_stat_counters(stat_counters_t *stat_counters);
	void destroy_stat_counters(stat_counters_t *stat_counters);	

	stat_data_t get_stat(stat_counters_t *stat_counters, std::vector<double> &percentile_marks);
	void add(stat_counters_t *stat_counters, int b, int compressed_value, ticks_t value);
};

#endif // __STREAM_STAT_HPP__

#include "profiler.hpp"
#include <iostream>
#include <sstream>
double cal_time_elapsed(struct timeval &start, struct timeval &end) {
  double sec = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1000000.;
  return sec;
}
Timer::Timer(const std::string &name, int summary_cond_times)
    : name_(name), summary_cond_times_(summary_cond_times) {}

Timer::~Timer() {}

void Timer::Tic() { gettimeofday(&(start_), NULL); }

void Timer::Toc(int times) {
  gettimeofday(&(end_), NULL);
  total_time_ += end_.tv_sec - start_.tv_sec + (end_.tv_usec - start_.tv_usec) / 1000000.;
  times_ += times;
  if (summary_cond_times_ > 0 && times_ > summary_cond_times_) {
    Summary();
  }
}
void Timer::TicToc(int step, const std::string &str_step) {
#ifdef PERF_EVAL
  if (step == 0) {
    // start a new round to record timestamp
    int max_step = 0;
    if (step_time_.size() != 0) {
      for (auto &kv : step_time_) {
        int step = kv.first;
        if (step > max_step) {
          max_step = step;
        }
        int prev_step = step - 1;
        if (step_time_.count(prev_step)) {
          double ts = cal_time_elapsed(step_time_[prev_step], step_time_[step]);
          if (step_time_elpased_.count(step) == 0) {
            step_time_elpased_[step] = ts;
          } else {
            step_time_elpased_[step] += ts;
          }
        }
      }
      times_ += 1;
      step_time_.clear();
    }
    if (times_ == summary_cond_times_) {
      double total_ts = 0;
      std::stringstream ss;
      ss << "[Timer] " << name_ << " ";
      for (int i = 1; i <= max_step; i++) {
        if (step_time_elpased_.count(i)) {
          ss << step_names_[i] << ":" << step_time_elpased_[i] * 1000 / times_ << ",";
          total_ts += step_time_elpased_[i] * 1000 / times_;
        }
      }
      ss << ",total:" << total_ts;
      std::cout << ss.str() << std::endl;
      times_ = 0;
      step_time_elpased_.clear();
    }
  }
  struct timeval t;
  gettimeofday(&t, NULL);
  if (step_time_.count(step)) {
    std::cout << "error,has existed step:" << step << std::endl;
  }
  step_time_[step] = t;
  step_names_[step] = str_step;
#endif
}
void Timer::Config(const std::string &name, int summary_cond_times) {
  name_ = name;
  summary_cond_times_ = summary_cond_times;
}

void Timer::Summary() {
#ifdef PERF_EVAL
  std::cout << "[Timer] " << name_ << " " << 1000 * total_time_ / times_ << "ms" << std::endl;
#endif
  total_time_ = 0.;
  times_ = 0;
}

/* =========================================== */
/*                  FpsProfiler                */
/* =========================================== */

FpsProfiler::FpsProfiler(const std::string &name, int summary_cond_cnts)
    : name_(name), summary_cond_cnts_(summary_cond_cnts) {
  gettimeofday(&(start_), NULL);
}

FpsProfiler::~FpsProfiler() {}

void FpsProfiler::Add(int cnts) {
  int print = 0;
  pthread_mutex_lock(&lock_);
  cnts_ += cnts;
  if (cnts_ > summary_cond_cnts_) {
    Summary();
    print = 1;
  }
  pthread_mutex_unlock(&lock_);

  if (print) {
#ifdef PERF_EVAL
    std::cerr << "[" << name_ << "] temp_fps:" << tmp_fps_ << ",average_fps:" << average_fps_
              << std::endl;
#endif
  }
}

void FpsProfiler::Config(const std::string &name, int summary_cond_cnts) {
  name_ = name;
  summary_cond_cnts_ = summary_cond_cnts;
}

float FpsProfiler::Elapse() {
  gettimeofday(&(end_), NULL);
  return end_.tv_sec - start_.tv_sec + (end_.tv_usec - start_.tv_usec) / 1000000.;
}

void FpsProfiler::Summary() {
  tmp_fps_ = cnts_ / Elapse();
  // use moving average
  average_fps_ = average_fps_ * 0.9 + tmp_fps_ * 0.1;

  cnts_ = 0;
  gettimeofday(&(start_), NULL);
}

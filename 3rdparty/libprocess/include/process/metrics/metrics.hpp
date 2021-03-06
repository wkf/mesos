#ifndef __PROCESS_METRICS_METRICS_HPP__
#define __PROCESS_METRICS_METRICS_HPP__

#include <string>

#include <process/dispatch.hpp>
#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>

#include <process/metrics/metric.hpp>

#include <stout/hashmap.hpp>
#include <stout/nothing.hpp>

namespace process {
namespace metrics {

namespace internal {

class MetricsProcess : public Process<MetricsProcess>
{
public:
  static MetricsProcess* instance();

  Future<Nothing> add(Owned<Metric> metric);

  Future<Nothing> remove(const std::string& name);

protected:
  virtual void initialize();

private:
  static std::string help();

  MetricsProcess() : ProcessBase("metrics") {}

  // Non-copyable, non-assignable.
  MetricsProcess(const MetricsProcess&);
  MetricsProcess& operator = (const MetricsProcess&);

  Future<http::Response> snapshot(const http::Request& request);
  static Future<http::Response> _snapshot(
      const http::Request& request,
      const hashmap<std::string, Future<double> >& metrics);

  // The Owned<Metric> is an explicit copy of the Metric passed to 'add'.
  hashmap<std::string, Owned<Metric> > metrics;
};

}  // namespace internal {


template <typename T>
Future<Nothing> add(const T& metric)
{
  // There is an explicit copy in this call to ensure we end up owning
  // the last copy of a Metric when we remove it.
  return dispatch(
      internal::MetricsProcess::instance(),
      &internal::MetricsProcess::add,
      Owned<Metric>(new T(metric)));
}


inline Future<Nothing> remove(const Metric& metric)
{
  return dispatch(
      internal::MetricsProcess::instance(),
      &internal::MetricsProcess::remove,
      metric.name());
}

}  // namespace metrics {
}  // namespace process {

#endif  // __PROCESS_METRICS_METRICS_HPP__

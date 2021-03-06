/**
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __STOUT_INTERVAL_HPP__
#define __STOUT_INTERVAL_HPP__

#include <functional> // For std::less.

#include <boost/icl/interval.hpp>
#include <boost/icl/interval_set.hpp>


// Forward declarations.
template <typename T>
class Interval;


// Represents a bound (left or right) for an interval. A bound can
// either be open or closed.
template <typename T>
class Bound
{
public:
  // Creates an open bound.
  static Bound<T> open(const T& value)
  {
    return Bound<T>(OPEN, value);
  }

  // Creates an closed bound.
  static Bound<T> closed(const T& value)
  {
    return Bound<T>(CLOSED, value);
  }

  // Intervals can be created using the comma operator. For example:
  //   (2, 6):  (Bound<int>::open(2), Bound<int>::open(6))
  //   (3, 4]:  (Bound<int>::open(3), Bound<int>::closed(4))
  //   [0, 5):  (Bound<int>::closed(0), Bound<int>::open(5))
  //   [1, 2]:  (Bound<int>::closed(1), Bound<int>::closed(2))
  Interval<T> operator , (const Bound<T>& right) const;

private:
  enum Type
  {
    OPEN,
    CLOSED,
  };

  Bound(const Type _type, const T& _value)
    : type(_type), value(_value) {}

  const Type type;
  const T value;
};


// Represents an interval.
template <typename T>
class Interval
{
public:
  // We must provide a public default constructor as the boost
  // interval set expects that.
  Interval() {}

  // Returns the inclusive lower bound of this interval.
  T lower() const { return data.lower(); }

  // Returns the exclusive upper bound of this interval.
  T upper() const { return data.upper(); }

private:
  friend class Bound<T>;

  Interval(const boost::icl::right_open_interval<T, std::less>& _data)
    : data(_data) {}

  // We store the interval in right open format: [lower, upper).
  // Notice that we use composition here instead of inheritance to
  // prevent Interval from being passed to bare boost icl containers.
  boost::icl::right_open_interval<T, std::less> data;
};


template <typename T>
Interval<T> Bound<T>::operator , (const Bound<T>& right) const
{
  if (type == OPEN) {
    if (right.type == OPEN) {
      // For example: (1, 3).
      return boost::icl::static_interval<
          boost::icl::right_open_interval<T, std::less>, // Target.
          boost::icl::is_discrete<T>::value,
          boost::icl::interval_bounds::static_open,      // Input bounds.
          boost::icl::interval_bounds::static_right_open // Output bounds.
      >::construct(value, right.value);
    } else {
      // For example: (1, 3].
      return boost::icl::static_interval<
          boost::icl::right_open_interval<T, std::less>, // Target.
          boost::icl::is_discrete<T>::value,
          boost::icl::interval_bounds::static_left_open, // Input bounds.
          boost::icl::interval_bounds::static_right_open // Output bounds.
      >::construct(value, right.value);
    }
  } else {
    if (right.type == OPEN) {
      // For example: [1, 3).
      return boost::icl::static_interval<
          boost::icl::right_open_interval<T, std::less>,  // Target.
          boost::icl::is_discrete<T>::value,
          boost::icl::interval_bounds::static_right_open, // Input bounds.
          boost::icl::interval_bounds::static_right_open  // Output bounds.
      >::construct(value, right.value);
    } else {
      // For example: [1, 3].
      return boost::icl::static_interval<
          boost::icl::right_open_interval<T, std::less>,  // Target.
          boost::icl::is_discrete<T>::value,
          boost::icl::interval_bounds::static_closed,     // Input bounds.
          boost::icl::interval_bounds::static_right_open  // Output bounds.
      >::construct(value, right.value);
    }
  }
}


// Modeled after boost interval_set. Provides a compact representation
// of a set by merging adjacent elements into intervals.
template <typename T>
class IntervalSet : public boost::icl::interval_set<T, std::less, Interval<T> >
{
public:
  IntervalSet() {}

  explicit IntervalSet(const T& value)
  {
    Base::add(value);
  }

  explicit IntervalSet(const Interval<T>& interval)
  {
    Base::add(interval);
  }

  IntervalSet(const Bound<T>& lower, const Bound<T>& upper)
  {
    Base::add((lower, upper));
  }

  // Checks if the specified value is in this set.
  bool contains(const T& value) const
  {
    return boost::icl::contains(static_cast<const Base&>(*this), value);
  }

  // Returns the number of intervals in this set.
  size_t intervalCount() const
  {
    return boost::icl::interval_count(static_cast<const Base&>(*this));
  }

  // Overloaded operators.
  template <typename X>
  IntervalSet<T>& operator += (const X& x)
  {
    static_cast<Base&>(*this) += x;
    return *this;
  }

  template <typename X>
  IntervalSet<T>& operator -= (const X& x)
  {
    static_cast<Base&>(*this) -= x;
    return *this;
  }

  template <typename X>
  IntervalSet<T>& operator &= (const X& x)
  {
    static_cast<Base&>(*this) &= x;
    return *this;
  }

  // Special overloads for IntervalSet. According to C++ standard,
  // a non-template function always wins an ambiguous overload.
  IntervalSet<T>& operator += (const IntervalSet<T>& set)
  {
    static_cast<Base&>(*this) += static_cast<const Base&>(set);
    return *this;
  }

  IntervalSet<T>& operator -= (const IntervalSet<T>& set)
  {
    static_cast<Base&>(*this) -= static_cast<const Base&>(set);
    return *this;
  }

  IntervalSet<T>& operator &= (const IntervalSet<T>& set)
  {
    static_cast<Base&>(*this) &= static_cast<const Base&>(set);
    return *this;
  }

private:
  // We use typedef here to make the code less verbose.
  typedef boost::icl::interval_set<T, std::less, Interval<T> > Base;
};


// Defines type traits for the custom Interval above. These type
// traits are required by the boost interval set.
namespace boost {
namespace icl {

template <typename T>
struct interval_traits<Interval<T> >
{
  typedef interval_traits type;
  typedef T domain_type;
  typedef std::less<T> domain_compare;

  static Interval<T> construct(const T& lower, const T& upper)
  {
    return (Bound<T>::closed(lower), Bound<T>::open(upper));
  }

  static T lower(const Interval<T>& interval)
  {
    return interval.lower();
  }

  static T upper(const Interval<T>& interval)
  {
    return interval.upper();
  }
};


template <typename T>
struct interval_bound_type<Interval<T> >
  : public interval_bound_type<right_open_interval<T, std::less> >
{
  typedef interval_bound_type type;
};

} // namespace icl {
} // namespace boost {

#endif // __STOUT_INTERVAL_HPP__

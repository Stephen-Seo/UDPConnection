#ifndef SEODISPARATE_COM_UDPC_TEST_HELPERS_H_
#define SEODISPARATE_COM_UDPC_TEST_HELPERS_H_

#include <cstring>
#include <iostream>

extern int checks_checked;
extern int checks_passed;

// Macros for unit testing.

#define CHECK_TRUE(x)                                                     \
  do {                                                                    \
    ++checks_checked;                                                     \
    if (!(x)) {                                                           \
      std::cout << "CHECK_TRUE at line " << __LINE__ << " failed: " << #x \
                << '\n';                                                  \
    } else {                                                              \
      ++checks_passed;                                                    \
    }                                                                     \
  } while (false);
#define ASSERT_TRUE(x)                                                    \
  do {                                                                    \
    ++checks_checked;                                                     \
    if (!(x)) {                                                           \
      std::cout << "CHECK_TRUE at line " << __LINE__ << " failed: " << #x \
                << '\n';                                                  \
      return;                                                             \
    } else {                                                              \
      ++checks_passed;                                                    \
    }                                                                     \
  } while (false);
#define CHECK_FALSE(x)                                                     \
  do {                                                                     \
    ++checks_checked;                                                      \
    if (x) {                                                               \
      std::cout << "CHECK_FALSE at line " << __LINE__ << " failed: " << #x \
                << '\n';                                                   \
    } else {                                                               \
      ++checks_passed;                                                     \
    }                                                                      \
  } while (false);
#define ASSERT_FALSE(x)                                                    \
  do {                                                                     \
    ++checks_checked;                                                      \
    if (x) {                                                               \
      std::cout << "CHECK_FALSE at line " << __LINE__ << " failed: " << #x \
                << '\n';                                                   \
      return;                                                              \
    } else {                                                               \
      ++checks_passed;                                                     \
    }                                                                      \
  } while (false);

#define CHECK_FLOAT(var, value)                                              \
  do {                                                                       \
    ++checks_checked;                                                        \
    if ((var) > (value) - 0.0001F && (var) < (value) + 0.0001F) {            \
      ++checks_passed;                                                       \
    } else {                                                                 \
      std::cout << "CHECK_FLOAT at line " << __LINE__ << " failed: " << #var \
                << " != " << #value << '\n';                                 \
    }                                                                        \
  } while (false);

#define CHECK_EQ(var, value)                                              \
  do {                                                                    \
    ++checks_checked;                                                     \
    if ((var) == (value)) {                                               \
      ++checks_passed;                                                    \
    } else {                                                              \
      std::cout << "CHECK_EQ at line " << __LINE__ << " failed: " << #var \
                << " != " << #value << '\n';                              \
    }                                                                     \
  } while (false);
#define CHECK_GE(var, value)                                              \
  do {                                                                    \
    ++checks_checked;                                                     \
    if ((var) >= (value)) {                                               \
      ++checks_passed;                                                    \
    } else {                                                              \
      std::cout << "CHECK_GE at line " << __LINE__ << " failed: " << #var \
                << " < " << #value << '\n';                               \
    }                                                                     \
  } while (false);
#define CHECK_LE(var, value)                                              \
  do {                                                                    \
    ++checks_checked;                                                     \
    if ((var) <= (value)) {                                               \
      ++checks_passed;                                                    \
    } else {                                                              \
      std::cout << "CHECK_LE at line " << __LINE__ << " failed: " << #var \
                << " > " << #value << '\n';                               \
    }                                                                     \
  } while (false);

#define CHECK_STREQ(str_a, str_b)                                             \
  do {                                                                        \
    ++checks_checked;                                                         \
    if (std::strcmp((str_a), (str_b)) == 0) {                                 \
      ++checks_passed;                                                        \
    } else {                                                                  \
      std::cout << "CHECK_STREQ at line " << __LINE__ << "failed: " << #str_a \
                << " != " << #str_b << '\n';                                  \
    }                                                                         \
  } while (false);

#endif

/*
 * Copyright (C) 2016 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <chrono>
#include <cstdlib>

template <typename T>
class RandomValueGenerator
{
public:
    RandomValueGenerator(const T& min, const T& max)
        : minVal(min)
        , maxVal(max)
    {
        return;
    }

    const T operator()() const
    {
        return static_cast<T>(
            std::rand() % (maxVal + 1 - minVal) + minVal);
    }

private:
    T minVal;
    T maxVal;
};

class Timer
{
public:
    typedef typename std::chrono::microseconds us;
    typedef typename std::chrono::milliseconds ms;
    typedef typename std::chrono::seconds       s;

    Timer() { reset(); }

    template <typename T = std::chrono::microseconds>
    typename T::rep elapsed() const
    {
        return std::chrono::duration_cast<T>(
            std::chrono::steady_clock::now() - start).count();
    }

    void reset()
    {
        start = std::chrono::steady_clock::now();
    }

private:
    std::chrono::steady_clock::time_point start;
};

#endif // TEST_UTILS_H

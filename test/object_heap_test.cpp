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

#include "test.h"

extern "C" {
    #include "object_heap.h"
}

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <vector>

TEST(ObjectHeapTest, Init)
{
    struct test_object {
        struct object_base base;
        int i;
    };

    struct object_heap heap = {
        object_size: -1,
        id_offset: -1,
        next_free: -1,
        heap_size: -1,
        heap_increment: -1,
        mutex: {},
        bucket: NULL,
        num_buckets: -1,
    };

    EXPECT_EQ(0, object_heap_init(&heap, sizeof(test_object), 0xffffffff));

    EXPECT_EQ(sizeof(test_object), (size_t)heap.object_size);
    EXPECT_EQ(OBJECT_HEAP_OFFSET_MASK, heap.id_offset);
    EXPECT_EQ(0, heap.next_free);
    EXPECT_LE(1, heap.heap_increment);
    EXPECT_EQ(heap.heap_increment, heap.heap_size);
    EXPECT_PTR(heap.bucket);
    EXPECT_LE(1, heap.num_buckets);

    object_heap_destroy(&heap);

    EXPECT_PTR_NULL(heap.bucket);
    EXPECT_EQ(0, heap.heap_size);
    EXPECT_GT(0, heap.next_free);

    EXPECT_EQ(0, object_heap_init(&heap, sizeof(test_object), 0x0));

    EXPECT_EQ(sizeof(test_object), (size_t)heap.object_size);
    EXPECT_EQ(0, heap.id_offset);
    EXPECT_EQ(0, heap.next_free);
    EXPECT_LE(1, heap.heap_increment);
    EXPECT_EQ(heap.heap_increment, heap.heap_size);
    EXPECT_PTR(heap.bucket);
    EXPECT_LE(1, heap.num_buckets);

    object_heap_destroy(&heap);

    EXPECT_PTR_NULL(heap.bucket);
    EXPECT_EQ(0, heap.heap_size);
    EXPECT_GT(0, heap.next_free);
}

TEST(ObjectHeapTest, AllocateAndLookup)
{
    struct object_heap heap = {};

    ASSERT_EQ(0, object_heap_init(&heap, sizeof(object_base), 0));

    int nbuckets = heap.num_buckets;

    for (int i(0); i < heap.heap_increment * nbuckets * 2; ++i)
    {
        int id = object_heap_allocate(&heap);
        EXPECT_EQ(i, id);
        int expect(i+1);
        if (expect % heap.heap_increment == 0)
            expect = -1;
        EXPECT_EQ(expect, heap.next_free);
    }

    EXPECT_EQ(nbuckets * 2, heap.num_buckets);

    for (int i(0); i < heap.heap_increment * nbuckets * 2; ++i)
    {
        object_base_p obj = object_heap_lookup(&heap, i);
        EXPECT_PTR(obj);
        object_heap_free(&heap, obj);
        obj = object_heap_lookup(&heap, i);
        EXPECT_PTR_NULL(obj);
    }

    object_heap_destroy(&heap);
}

TEST(ObjectHeapTest, Iterate)
{
    struct object_heap heap = {};
    object_heap_iterator iter;

    ASSERT_EQ(0, object_heap_init(&heap, sizeof(object_base), 0));

    EXPECT_PTR_NULL(object_heap_first(&heap, &iter));
    EXPECT_PTR_NULL(object_heap_next(&heap, &iter));

    std::vector<object_base_p> objects(256, NULL);
    std::generate(objects.begin(), objects.end(),
        [&]{ return object_heap_lookup(&heap, object_heap_allocate(&heap)); });

    // iterate all objects starting at first
    object_base_p current = object_heap_first(&heap, &iter);
    EXPECT_EQ(0, current->id);
    EXPECT_TRUE(objects.front() == current);
    size_t i(1);
    while ((current = object_heap_next(&heap, &iter)) && i < objects.size())
    {
        EXPECT_EQ(i, (size_t)current->id);
        EXPECT_TRUE(objects[i++] == current);
    }
    EXPECT_PTR_NULL(current);
    EXPECT_EQ(i, objects.size());
    EXPECT_PTR_NULL(object_heap_next(&heap, &iter));

    // get "first" and free it
    current = object_heap_first(&heap, &iter);
    ASSERT_TRUE(objects[0] == current);
    object_heap_free(&heap, current);
    objects[0] = NULL;

    // get "first" again and ensure it's our second allocated object
    current = object_heap_first(&heap, &iter);
    EXPECT_TRUE(objects[1] == current);

    // free the object after "current" and ensure "next"
    // returns the one after it.
    object_heap_free(&heap, objects[2]);
    objects[2] = NULL;
    current = object_heap_next(&heap, &iter);
    EXPECT_TRUE(objects[3] == current);

    // free all objects
    std::for_each(objects.begin(), objects.end(),
        [&](object_base_p o){ object_heap_free(&heap, o); });

    object_heap_destroy(&heap);
}

TEST(ObjectHeapTest, DataIntegrity)
{
    struct test_object {
        struct object_base base;
        int i;
    };

    typedef test_object *test_object_p;
    struct object_heap heap = {};

    ASSERT_EQ(0, object_heap_init(&heap, sizeof(test_object), 0));

    std::vector<int> values;

    auto generator = [&]{
        int id = object_heap_allocate(&heap);
        object_base_p base = object_heap_lookup(&heap, id);
        test_object_p object = (test_object_p)base;
        object->i = std::rand();
        values.push_back(object->i);
        return object;
    };

    std::vector<test_object*> objects(71, NULL);
    std::generate(objects.begin(), objects.end(), generator);

    ASSERT_EQ(objects.size(), values.size());

    auto validator = [&](test_object_p object) {
        object_base_p base = object_heap_lookup(&heap, object->base.id);
        EXPECT_TRUE(&object->base == base);
        EXPECT_EQ(object->base.id, base->id);
        test_object_p lo = (test_object_p)base;
        ASSERT_GT(values.size(), (size_t)lo->base.id);
        EXPECT_EQ(values[lo->base.id], lo->i);
    };

    std::for_each(objects.begin(), objects.end(), validator);

    std::for_each(objects.begin(), objects.end(),
        [&](test_object_p o){ object_heap_free(&heap, &o->base); });
    object_heap_destroy(&heap);
}

TEST(ObjectHeapTest, OffsetID)
{
    ASSERT_LT(0, (OBJECT_HEAP_OFFSET_MASK >> 24));
    for (int i(0); i <= (OBJECT_HEAP_OFFSET_MASK >> 24); ++i)
    {
        struct object_heap heap = {};
        int offset = i << 24;

        SCOPED_TRACE(
            ::testing::Message()
            << "offset=0x" << std::hex << std::setfill('0')
            << std::setw(8) << offset << std::dec);

        ASSERT_EQ(0, object_heap_init(&heap, sizeof(object_base), offset));

        EXPECT_EQ(offset & OBJECT_HEAP_OFFSET_MASK, heap.id_offset);

        std::vector<object_base_p> objects(1024, NULL);
        std::generate(objects.begin(), objects.end(),
            [&]{ return object_heap_lookup(&heap, object_heap_allocate(&heap)); });

        for (int idx(0); (size_t)idx < objects.size(); ++idx)
            EXPECT_EQ(offset + idx, objects[idx]->id);

        std::for_each(objects.begin(), objects.end(),
            [&](object_base_p o){ object_heap_free(&heap, o); });
        object_heap_destroy(&heap);
    }
}

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

#include "i965_test_fixture.h"
const std::string I965TestFixture::getFullTestName() const
{
    const ::testing::TestInfo * const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return std::string(info->test_case_name())
        + std::string(".")
        + std::string(info->name());
}

Surfaces I965TestFixture::createSurfaces(int w, int h, int format, size_t count,
    const SurfaceAttribs& attributes)
{
    Surfaces surfaces(count, VA_INVALID_ID);
    if (attributes.empty()) {
        EXPECT_STATUS(
            i965_CreateSurfaces(
                *this, w, h, format, surfaces.size(), surfaces.data()));
    } else {
        VADriverContextP ctx(*this);
        EXPECT_PTR(ctx);
        if (ctx)
            EXPECT_STATUS(
                ctx->vtable->vaCreateSurfaces2(
                    *this, format, w, h, surfaces.data(), surfaces.size(),
                    const_cast<VASurfaceAttrib*>(attributes.data()),
                    attributes.size()));
    }

    for (size_t i(0); i < count; ++i) {
        EXPECT_ID(surfaces[i]);
    }

    return surfaces;
}

TEST_F(I965TestFixture,testImportedSurface){

    VAStatus va_status;
    VASurfaceAttrib surfaceAttrib,surfaceAttrib2;
    VASurfaceAttribExternalBuffers exbuffer;
    surfaceAttrib.type = VASurfaceAttribMemoryType;
    surfaceAttrib.flags = 2;
    surfaceAttrib.value.type = VAGenericValueTypeInteger;
    surfaceAttrib.value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_KERNEL_DRM;
    //surfaceAttrib.value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    surfaceAttrib2.flags = 2;
    surfaceAttrib2.type = VASurfaceAttribExternalBufferDescriptor;
    surfaceAttrib2.value.type = VAGenericValueTypePointer;
    surfaceAttrib2.value.value.p = &exbuffer;

    VASurfaceAttrib attributes[2] = {surfaceAttrib, surfaceAttrib2};
    exbuffer.pitches[0] = 4096;
    exbuffer.pitches[1] = 4096;
    exbuffer.pitches[2] = 2048;
    exbuffer.pixel_format = VA_FOURCC_P010;
    exbuffer.offsets[0] = 4456448;
    exbuffer.offsets[1] = 4456448;
    exbuffer.offsets[2] = 64;
    exbuffer.data_size = 6291456;
    exbuffer.num_planes = 2;
    exbuffer.num_buffers = 1;
    exbuffer.width = 2048;
    exbuffer.height = 1088;
    int region_width = 1024;
    int region_height = 3072;
    unsigned int tiling_mode = 1;
    long unsigned int pitches =  2048;
    VADriverContextP ctx(*this);
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface = (object_surface*)malloc(sizeof(object_surface));
    obj_surface->bo = drm_intel_bo_alloc_tiled(i965->intel.bufmgr,
                                                    "vaapi surface",
                                                    region_width,
                                                    region_height,
                                                    1,
                                                    &tiling_mode,
                                                    &pitches,
                                                  (unsigned long) 0);

    /*    int size = 6291456;
    obj_surface->bo = dri_bo_alloc(i965->intel.bufmgr,

                                     //"gem flinked vaapi surface",
                                       "vaapi surface",
                                        size,
                                        0x1000);
    */
    int external_memory_type = 1;

    exbuffer.buffers = new unsigned long[4];
    if (external_memory_type == I965_SURFACE_MEM_GEM_FLINK) {
        uint32_t name;
        drm_intel_bo_flink(obj_surface->bo, &name);
        exbuffer.buffers[0] = (long unsigned int)name;
    } else if (external_memory_type == I965_SURFACE_MEM_DRM_PRIME) {
        int name;
        drm_intel_bo_gem_export_to_prime(obj_surface->bo,&name) ;
        exbuffer.buffers[0] = (long unsigned int)name;
    }
    unsigned int num_surfaces = 1;
    unsigned int num_attributes  = 2;
    int w = 2048;
    int h = 1088;
    int format = VA_RT_FORMAT_YUV420;
    unsigned int *surface_id = new unsigned int[1];

    va_status = ctx->vtable->vaCreateSurfaces2(ctx, format, w, h,surface_id, num_surfaces, attributes, num_attributes);
    free(obj_surface);
    std::cout<<"va_status"<<va_status<<std::endl;

}


void I965TestFixture::destroySurfaces(Surfaces& surfaces)
{
    EXPECT_STATUS(
        i965_DestroySurfaces(*this, surfaces.data(), surfaces.size()));
}

VAConfigID I965TestFixture::createConfig(
    VAProfile profile, VAEntrypoint entrypoint, const ConfigAttribs& attribs)
{
    VAConfigID id = VA_INVALID_ID;
    EXPECT_STATUS(
        i965_CreateConfig(
            *this, profile, entrypoint,
            const_cast<VAConfigAttrib*>(attribs.data()), attribs.size(), &id));
    EXPECT_ID(id);

    return id;
}

void I965TestFixture::destroyConfig(VAConfigID id)
{
    EXPECT_STATUS(i965_DestroyConfig(*this, id));
}

VAContextID I965TestFixture::createContext(
    VAConfigID config, int w, int h, int flags, const Surfaces& targets)
{
    VAContextID id = VA_INVALID_ID;
    EXPECT_STATUS(
        i965_CreateContext(
            *this, config, w, h, flags,
            const_cast<VASurfaceID*>(targets.data()), targets.size(), &id));
    EXPECT_ID(id);

    return id;
}

void I965TestFixture::destroyContext(VAContextID id)
{
    EXPECT_STATUS(i965_DestroyContext(*this, id));
}

VABufferID I965TestFixture::createBuffer(
    VAContextID context, VABufferType type,
    unsigned size, unsigned num, const void *data)
{
    VABufferID id;
    EXPECT_STATUS(
        i965_CreateBuffer(*this, context, type, size, num, (void*)data, &id));
    EXPECT_ID(id);

    return id;
}

void I965TestFixture::beginPicture(VAContextID context, VASurfaceID target)
{
    EXPECT_STATUS(
        i965_BeginPicture(*this, context, target));
}

void I965TestFixture::renderPicture(
    VAContextID context, VABufferID *bufs, int num_bufs)
{
    EXPECT_STATUS(
        i965_RenderPicture(*this, context, bufs, num_bufs));
}

void I965TestFixture::endPicture(VAContextID context)
{
    EXPECT_STATUS(
        i965_EndPicture(*this, context));
}

void I965TestFixture::destroyBuffer(VABufferID id)
{
    EXPECT_STATUS(
        i965_DestroyBuffer(*this, id));
}

void I965TestFixture::deriveImage(VASurfaceID surface, VAImage &image)
{
    EXPECT_STATUS(
        i965_DeriveImage(*this, surface, &image));
}

void I965TestFixture::destroyImage(VAImage &image)
{
    EXPECT_STATUS(
        i965_DestroyImage(*this, image.image_id));
}

void I965TestFixture::syncSurface(VASurfaceID surface)
{
    EXPECT_STATUS(
        i965_SyncSurface(*this, surface));
}

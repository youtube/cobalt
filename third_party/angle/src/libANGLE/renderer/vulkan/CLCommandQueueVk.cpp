//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueVk.cpp: Implements the class methods for CLCommandQueueVk.

#include "common/PackedCLEnums_autogen.h"
#include "common/PackedEnums.h"

#include "libANGLE/renderer/vulkan/CLCommandQueueVk.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/vulkan/CLKernelVk.h"
#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/CLProgramVk.h"
#include "libANGLE/renderer/vulkan/CLSamplerVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLEvent.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLKernel.h"
#include "libANGLE/CLSampler.h"
#include "libANGLE/cl_utils.h"

#include "spirv/unified1/NonSemanticClspvReflection.h"
#include "vulkan/vulkan_core.h"

namespace rx
{

class CLAsyncFinishTask : public angle::Closure
{
  public:
    CLAsyncFinishTask(CLCommandQueueVk *queueVk) : mQueueVk(queueVk) {}

    void operator()() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::finish (async)");
        if (IsError(mQueueVk->finish()))
        {
            ERR() << "Async finish (clFlush) failed for queue (" << mQueueVk << ")!";
        }
    }

  private:
    CLCommandQueueVk *mQueueVk;
};

CLCommandQueueVk::CLCommandQueueVk(const cl::CommandQueue &commandQueue)
    : CLCommandQueueImpl(commandQueue),
      mContext(&commandQueue.getContext().getImpl<CLContextVk>()),
      mDevice(&commandQueue.getDevice().getImpl<CLDeviceVk>()),
      mPrintfBuffer(nullptr),
      mComputePassCommands(nullptr),
      mCurrentQueueSerialIndex(kInvalidQueueSerialIndex),
      mHasAnyCommandsPendingSubmission(false),
      mNeedPrintfHandling(false),
      mPrintfInfos(nullptr)
{}

angle::Result CLCommandQueueVk::init()
{
    ANGLE_CL_IMPL_TRY_ERROR(
        vk::OutsideRenderPassCommandBuffer::InitializeCommandPool(
            mContext, &mCommandPool.outsideRenderPassPool,
            mContext->getRenderer()->getQueueFamilyIndex(), getProtectionType()),
        CL_OUT_OF_RESOURCES);

    ANGLE_CL_IMPL_TRY_ERROR(mContext->getRenderer()->getOutsideRenderPassCommandBufferHelper(
                                mContext, &mCommandPool.outsideRenderPassPool,
                                &mOutsideRenderPassCommandsAllocator, &mComputePassCommands),
                            CL_OUT_OF_RESOURCES);

    // Generate initial QueueSerial for command buffer helper
    ANGLE_CL_IMPL_TRY_ERROR(
        mContext->getRenderer()->allocateQueueSerialIndex(&mCurrentQueueSerialIndex),
        CL_OUT_OF_RESOURCES);
    mComputePassCommands->setQueueSerial(
        mCurrentQueueSerialIndex,
        mContext->getRenderer()->generateQueueSerial(mCurrentQueueSerialIndex));

    // Initialize serials to be valid but appear submitted and finished.
    mLastFlushedQueueSerial   = QueueSerial(mCurrentQueueSerialIndex, Serial());
    mLastSubmittedQueueSerial = mLastFlushedQueueSerial;

    return angle::Result::Continue;
}

CLCommandQueueVk::~CLCommandQueueVk()
{
    ASSERT(mComputePassCommands->empty());
    ASSERT(!mNeedPrintfHandling);

    if (mPrintfBuffer)
    {
        mPrintfBuffer->release();
    }

    VkDevice vkDevice = mContext->getDevice();

    if (mCurrentQueueSerialIndex != kInvalidQueueSerialIndex)
    {
        mContext->getRenderer()->releaseQueueSerialIndex(mCurrentQueueSerialIndex);
        mCurrentQueueSerialIndex = kInvalidQueueSerialIndex;
    }

    // Recycle the current command buffers
    mContext->getRenderer()->recycleOutsideRenderPassCommandBufferHelper(&mComputePassCommands);
    mCommandPool.outsideRenderPassPool.destroy(vkDevice);
}

angle::Result CLCommandQueueVk::setProperty(cl::CommandQueueProperties properties, cl_bool enable)
{
    // NOTE: "clSetCommandQueueProperty" has been deprecated as of OpenCL 1.1
    // http://man.opencl.org/deprecated.html
    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueReadBuffer(const cl::Buffer &buffer,
                                                  bool blocking,
                                                  size_t offset,
                                                  size_t size,
                                                  void *ptr,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
        auto bufferVk = &buffer.getImpl<CLBufferVk>();
        ANGLE_TRY(bufferVk->copyTo(ptr, offset, size));

        ANGLE_TRY(createEvent(eventCreateFunc, true));
    }
    else
    {
        CLBufferVk &bufferVk = buffer.getImpl<CLBufferVk>();

        // Reached transfer buffer creation limit/heuristic, finish this current batch
        if (mHostBufferUpdateList.size() >= kMaxHostBufferUpdateListSize)
        {
            ANGLE_TRY(finishInternal());
        }

        // Create a transfer buffer and push it in update list
        mHostBufferUpdateList.emplace_back(
            cl::Buffer::Cast(this->mContext->getFrontendObject().createBuffer(
                nullptr, cl::MemFlags{buffer.getFlags().get() | CL_MEM_USE_HOST_PTR},
                buffer.getSize(), ptr)));
        if (mHostBufferUpdateList.back() == nullptr)
        {
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
        CLBufferVk &transferBufferVk = mHostBufferUpdateList.back()->getImpl<CLBufferVk>();
        // Release initialization reference, lifetime controlled by RefPointer.
        mHostBufferUpdateList.back()->release();

        const VkBufferCopy copyRegion = {offset, offset, size};

        // We need an execution barrier if buffer can be written to by kernel
        if (!mComputePassCommands->getCommandBuffer().empty() && bufferVk.isWritable())
        {
            VkMemoryBarrier memoryBarrier = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
            mComputePassCommands->getCommandBuffer().pipelineBarrier(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                &memoryBarrier, 0, nullptr, 0, nullptr);
        }

        mComputePassCommands->getCommandBuffer().copyBuffer(
            bufferVk.getBuffer().getBuffer(), transferBufferVk.getBuffer().getBuffer(), 1,
            &copyRegion);

        ANGLE_TRY(createEvent(eventCreateFunc, false));
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueWriteBuffer(const cl::Buffer &buffer,
                                                   bool blocking,
                                                   size_t offset,
                                                   size_t size,
                                                   const void *ptr,
                                                   const cl::EventPtrs &waitEvents,
                                                   CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    auto bufferVk = &buffer.getImpl<CLBufferVk>();
    ANGLE_TRY(bufferVk->copyFrom(ptr, offset, size));
    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    ANGLE_TRY(createEvent(eventCreateFunc, true));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueReadBufferRect(const cl::Buffer &buffer,
                                                      bool blocking,
                                                      const cl::MemOffsets &bufferOrigin,
                                                      const cl::MemOffsets &hostOrigin,
                                                      const cl::Coordinate &region,
                                                      size_t bufferRowPitch,
                                                      size_t bufferSlicePitch,
                                                      size_t hostRowPitch,
                                                      size_t hostSlicePitch,
                                                      void *ptr,
                                                      const cl::EventPtrs &waitEvents,
                                                      CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueWriteBufferRect(const cl::Buffer &buffer,
                                                       bool blocking,
                                                       const cl::MemOffsets &bufferOrigin,
                                                       const cl::MemOffsets &hostOrigin,
                                                       const cl::Coordinate &region,
                                                       size_t bufferRowPitch,
                                                       size_t bufferSlicePitch,
                                                       size_t hostRowPitch,
                                                       size_t hostSlicePitch,
                                                       const void *ptr,
                                                       const cl::EventPtrs &waitEvents,
                                                       CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueCopyBuffer(const cl::Buffer &srcBuffer,
                                                  const cl::Buffer &dstBuffer,
                                                  size_t srcOffset,
                                                  size_t dstOffset,
                                                  size_t size,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    CLBufferVk *srcBufferVk = &srcBuffer.getImpl<CLBufferVk>();
    CLBufferVk *dstBufferVk = &dstBuffer.getImpl<CLBufferVk>();

    vk::CommandBufferAccess access;
    if (srcBufferVk->isSubBuffer() && dstBufferVk->isSubBuffer() &&
        (srcBufferVk->getParent() == dstBufferVk->getParent()))
    {
        // this is a self copy
        access.onBufferSelfCopy(&srcBufferVk->getBuffer());
    }
    else
    {
        access.onBufferTransferRead(&srcBufferVk->getBuffer());
        access.onBufferTransferWrite(&dstBufferVk->getBuffer());
    }

    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(getCommandBuffer(access, &commandBuffer));

    VkBufferCopy copyRegion = {srcOffset, dstOffset, size};
    // update the offset in the case of sub-buffers
    if (srcBufferVk->getOffset())
    {
        copyRegion.srcOffset += srcBufferVk->getOffset();
    }
    if (dstBufferVk->getOffset())
    {
        copyRegion.dstOffset += dstBufferVk->getOffset();
    }
    commandBuffer->copyBuffer(srcBufferVk->getBuffer().getBuffer(),
                              dstBufferVk->getBuffer().getBuffer(), 1, &copyRegion);

    ANGLE_TRY(createEvent(eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueCopyBufferRect(const cl::Buffer &srcBuffer,
                                                      const cl::Buffer &dstBuffer,
                                                      const cl::MemOffsets &srcOrigin,
                                                      const cl::MemOffsets &dstOrigin,
                                                      const cl::Coordinate &region,
                                                      size_t srcRowPitch,
                                                      size_t srcSlicePitch,
                                                      size_t dstRowPitch,
                                                      size_t dstSlicePitch,
                                                      const cl::EventPtrs &waitEvents,
                                                      CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueFillBuffer(const cl::Buffer &buffer,
                                                  const void *pattern,
                                                  size_t patternSize,
                                                  size_t offset,
                                                  size_t size,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    CLBufferVk *bufferVk = &buffer.getImpl<CLBufferVk>();
    if (mComputePassCommands->usesBuffer(bufferVk->getBuffer()))
    {
        ANGLE_TRY(finishInternal());
    }

    ANGLE_TRY(bufferVk->fillWithPattern(pattern, patternSize, offset, size));

    ANGLE_TRY(createEvent(eventCreateFunc, true));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueMapBuffer(const cl::Buffer &buffer,
                                                 bool blocking,
                                                 cl::MapFlags mapFlags,
                                                 size_t offset,
                                                 size_t size,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc,
                                                 void *&mapPtr)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    bool eventComplete = false;
    if (blocking || !eventCreateFunc)
    {
        ANGLE_TRY(finishInternal());
        eventComplete = true;
    }

    CLBufferVk *bufferVk = &buffer.getImpl<CLBufferVk>();
    uint8_t *mapPointer  = nullptr;
    if (buffer.getFlags().intersects(CL_MEM_USE_HOST_PTR))
    {
        ANGLE_TRY(finishInternal());
        mapPointer = static_cast<uint8_t *>(buffer.getHostPtr()) + offset;
        ANGLE_TRY(bufferVk->copyTo(mapPointer, offset, size));
        eventComplete = true;
    }
    else
    {
        ANGLE_TRY(bufferVk->map(mapPointer, offset));
    }
    mapPtr = static_cast<void *>(mapPointer);

    if (bufferVk->isCurrentlyInUse())
    {
        eventComplete = false;
    }
    ANGLE_TRY(createEvent(eventCreateFunc, eventComplete));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::copyImageToFromBuffer(CLImageVk &imageVk,
                                                      vk::BufferHelper &buffer,
                                                      const cl::MemOffsets &origin,
                                                      const cl::Coordinate &region,
                                                      size_t bufferOffset,
                                                      ImageBufferCopyDirection direction)
{
    vk::CommandBufferAccess access;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    VkImageAspectFlags aspectFlags = imageVk.getImage().getAspectFlags();
    if (direction == ImageBufferCopyDirection::ToBuffer)
    {
        access.onImageTransferRead(aspectFlags, &imageVk.getImage());
        access.onBufferTransferWrite(&buffer);
    }
    else
    {
        access.onImageTransferWrite(gl::LevelIndex(0), 1, 0,
                                    static_cast<uint32_t>(imageVk.getArraySize()), aspectFlags,
                                    &imageVk.getImage());
        access.onBufferTransferRead(&buffer);
    }
    ANGLE_TRY(getCommandBuffer(access, &commandBuffer));

    VkBufferImageCopy copyRegion               = {};
    copyRegion.bufferOffset                    = bufferOffset;
    copyRegion.bufferRowLength                 = 0;
    copyRegion.bufferImageHeight               = 0;
    copyRegion.imageExtent.width               = static_cast<uint32_t>(region.x);
    copyRegion.imageExtent.height              = static_cast<uint32_t>(region.y);
    copyRegion.imageExtent.depth               = static_cast<uint32_t>(region.z);
    copyRegion.imageOffset.x                   = static_cast<int32_t>(origin.x);
    copyRegion.imageOffset.y                   = static_cast<int32_t>(origin.y);
    copyRegion.imageOffset.z                   = static_cast<int32_t>(origin.z);
    copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount     = 1;
    copyRegion.imageSubresource.mipLevel       = 0;
    if (imageVk.isWritable())
    {
        // We need an execution barrier if image can be written to by kernel
        VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
            &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    if (direction == ImageBufferCopyDirection::ToBuffer)
    {
        commandBuffer->copyImageToBuffer(imageVk.getImage().getImage(),
                                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         buffer.getBuffer().getHandle(), 1, &copyRegion);
    }
    else
    {
        commandBuffer->copyBufferToImage(buffer.getBuffer().getHandle(),
                                         imageVk.getImage().getImage(),
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueReadImage(const cl::Image &image,
                                                 bool blocking,
                                                 const cl::MemOffsets &origin,
                                                 const cl::Coordinate &region,
                                                 size_t rowPitch,
                                                 size_t slicePitch,
                                                 void *ptr,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);
    CLImageVk &imageVk = image.getImpl<CLImageVk>();
    size_t size        = (region.x * region.y * region.z * imageVk.getElementSize());

    ANGLE_TRY(processWaitlist(waitEvents));

    if (imageVk.isStagingBufferInitialized() == false)
    {
        ANGLE_TRY(imageVk.createStagingBuffer(imageVk.getSize()));
    }

    if (blocking)
    {
        ANGLE_TRY(copyImageToFromBuffer(imageVk, imageVk.getStagingBuffer(), origin, region, 0,
                                        ImageBufferCopyDirection::ToBuffer));
        ANGLE_TRY(finishInternal());
        ANGLE_TRY(imageVk.copyStagingTo(ptr, 0, size));
    }
    else
    {
        // Create a transfer buffer and push it in update list
        mHostBufferUpdateList.emplace_back(
            cl::Buffer::Cast(this->mContext->getFrontendObject().createBuffer(
                nullptr, cl::MemFlags{image.getFlags().get() | CL_MEM_USE_HOST_PTR},
                image.getSize(), ptr)));
        if (mHostBufferUpdateList.back() == nullptr)
        {
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
        CLBufferVk &transferBufferVk = mHostBufferUpdateList.back()->getImpl<CLBufferVk>();
        // Release initialization reference, lifetime controlled by RefPointer.
        mHostBufferUpdateList.back()->release();

        // We need an execution barrier if buffer can be written to by kernel
        if (!mComputePassCommands->getCommandBuffer().empty() && imageVk.isWritable())
        {
            VkMemoryBarrier memoryBarrier = {
                VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
            mComputePassCommands->getCommandBuffer().pipelineBarrier(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                &memoryBarrier, 0, nullptr, 0, nullptr);
        }

        ANGLE_TRY(copyImageToFromBuffer(imageVk, transferBufferVk.getBuffer(), origin, region, 0,
                                        ImageBufferCopyDirection::ToBuffer));
    }

    ANGLE_TRY(createEvent(eventCreateFunc, true));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueWriteImage(const cl::Image &image,
                                                  bool blocking,
                                                  const cl::MemOffsets &origin,
                                                  const cl::Coordinate &region,
                                                  size_t inputRowPitch,
                                                  size_t inputSlicePitch,
                                                  const void *ptr,
                                                  const cl::EventPtrs &waitEvents,
                                                  CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);
    ANGLE_TRY(processWaitlist(waitEvents));

    CLImageVk &imageVk = image.getImpl<CLImageVk>();
    size_t size        = (region.x * region.y * region.z * imageVk.getElementSize());
    if (imageVk.isStagingBufferInitialized() == false)
    {
        ANGLE_TRY(imageVk.createStagingBuffer(imageVk.getSize()));
    }
    ANGLE_TRY(imageVk.copyStagingFrom((void *)ptr, 0, size));

    ANGLE_TRY(copyImageToFromBuffer(imageVk, imageVk.getStagingBuffer(), origin, region, 0,
                                    ImageBufferCopyDirection::ToImage));

    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    ANGLE_TRY(createEvent(eventCreateFunc, blocking));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueCopyImage(const cl::Image &srcImage,
                                                 const cl::Image &dstImage,
                                                 const cl::MemOffsets &srcOrigin,
                                                 const cl::MemOffsets &dstOrigin,
                                                 const cl::Coordinate &region,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);
    ANGLE_TRY(processWaitlist(waitEvents));

    auto srcImageVk = &srcImage.getImpl<CLImageVk>();
    auto dstImageVk = &dstImage.getImpl<CLImageVk>();

    vk::CommandBufferAccess access;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    VkImageAspectFlags dstAspectFlags = srcImageVk->getImage().getAspectFlags();
    VkImageAspectFlags srcAspectFlags = dstImageVk->getImage().getAspectFlags();
    access.onImageTransferWrite(gl::LevelIndex(0), 1, 0, 1, dstAspectFlags,
                                &dstImageVk->getImage());
    access.onImageTransferRead(srcAspectFlags, &srcImageVk->getImage());
    ANGLE_TRY(getCommandBuffer(access, &commandBuffer));

    VkImageCopy copyRegion                   = {};
    copyRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel       = 0;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.layerCount     = 1;
    copyRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.mipLevel       = 0;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.layerCount     = 1;
    copyRegion.srcOffset.x                   = static_cast<int32_t>(srcOrigin.x);
    copyRegion.srcOffset.y                   = static_cast<int32_t>(srcOrigin.y);
    copyRegion.srcOffset.z                   = static_cast<int32_t>(srcOrigin.z);
    copyRegion.dstOffset.x                   = static_cast<int32_t>(dstOrigin.x);
    copyRegion.dstOffset.y                   = static_cast<int32_t>(dstOrigin.y);
    copyRegion.dstOffset.z                   = static_cast<int32_t>(dstOrigin.z);
    copyRegion.extent.width                  = static_cast<uint32_t>(region.x);
    copyRegion.extent.height                 = static_cast<uint32_t>(region.y);
    copyRegion.extent.depth                  = static_cast<uint32_t>(region.z);
    if (srcImageVk->isWritable() || dstImageVk->isWritable())
    {
        // We need an execution barrier if buffer can be written to by kernel
        VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
            &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    commandBuffer->copyImage(
        srcImageVk->getImage().getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImageVk->getImage().getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    ANGLE_TRY(createEvent(eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueFillImage(const cl::Image &image,
                                                 const void *fillColor,
                                                 const cl::MemOffsets &origin,
                                                 const cl::Coordinate &region,
                                                 const cl::EventPtrs &waitEvents,
                                                 CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueCopyImageToBuffer(const cl::Image &srcImage,
                                                         const cl::Buffer &dstBuffer,
                                                         const cl::MemOffsets &srcOrigin,
                                                         const cl::Coordinate &region,
                                                         size_t dstOffset,
                                                         const cl::EventPtrs &waitEvents,
                                                         CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);
    CLImageVk &srcImageVk   = srcImage.getImpl<CLImageVk>();
    CLBufferVk &dstBufferVk = dstBuffer.getImpl<CLBufferVk>();

    ANGLE_TRY(processWaitlist(waitEvents));

    ANGLE_TRY(copyImageToFromBuffer(srcImageVk, dstBufferVk.getBuffer(), srcOrigin, region,
                                    dstOffset, ImageBufferCopyDirection::ToBuffer));

    ANGLE_TRY(createEvent(eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueCopyBufferToImage(const cl::Buffer &srcBuffer,
                                                         const cl::Image &dstImage,
                                                         size_t srcOffset,
                                                         const cl::MemOffsets &dstOrigin,
                                                         const cl::Coordinate &region,
                                                         const cl::EventPtrs &waitEvents,
                                                         CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);
    CLBufferVk &srcBufferVk = srcBuffer.getImpl<CLBufferVk>();
    CLImageVk &dstImageVk   = dstImage.getImpl<CLImageVk>();

    ANGLE_TRY(processWaitlist(waitEvents));

    ANGLE_TRY(copyImageToFromBuffer(dstImageVk, srcBufferVk.getBuffer(), dstOrigin, region,
                                    srcOffset, ImageBufferCopyDirection::ToImage));

    ANGLE_TRY(createEvent(eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueMapImage(const cl::Image &image,
                                                bool blocking,
                                                cl::MapFlags mapFlags,
                                                const cl::MemOffsets &origin,
                                                const cl::Coordinate &region,
                                                size_t *imageRowPitch,
                                                size_t *imageSlicePitch,
                                                const cl::EventPtrs &waitEvents,
                                                CLEventImpl::CreateFunc *eventCreateFunc,
                                                void *&mapPtr)
{
    ANGLE_TRY(enqueueWaitForEvents(waitEvents));

    // TODO: Look into better enqueue handling of this map-op if non-blocking
    // https://anglebug.com/376722715
    CLImageVk *imageVk = &image.getImpl<CLImageVk>();
    VkExtent3D extent  = imageVk->getImageExtent();
    if (blocking)
    {
        ANGLE_TRY(finishInternal());
    }

    if (imageVk->isStagingBufferInitialized() == false)
    {
        ANGLE_TRY(imageVk->createStagingBuffer(imageVk->getSize()));
    }

    ANGLE_TRY(copyImageToFromBuffer(*imageVk, imageVk->getStagingBuffer(), {0, 0, 0},
                                    {extent.width, extent.height, extent.depth}, 0,
                                    ImageBufferCopyDirection::ToBuffer));
    ANGLE_TRY(finishInternal());

    uint8_t *mapPointer = nullptr;
    size_t offset       = (origin.x * origin.y * origin.z * imageVk->getElementSize());
    size_t size         = (region.x * region.y * region.z * imageVk->getElementSize());
    if (image.getFlags().intersects(CL_MEM_USE_HOST_PTR))
    {
        mapPointer = static_cast<uint8_t *>(image.getHostPtr()) + offset;
        ANGLE_TRY(imageVk->copyTo(mapPointer, offset, size));
    }
    else
    {
        ANGLE_TRY(imageVk->map(mapPointer, offset));
    }
    mapPtr = static_cast<void *>(mapPointer);

    *imageRowPitch = (region.x * imageVk->getElementSize());

    switch (imageVk->getDesc().type)
    {
        case cl::MemObjectType::Image1D:
        case cl::MemObjectType::Image1D_Buffer:
        case cl::MemObjectType::Image2D:
            if (imageSlicePitch != nullptr)
            {
                *imageSlicePitch = 0;
            }
            break;
        case cl::MemObjectType::Image2D_Array:
        case cl::MemObjectType::Image3D:
            *imageSlicePitch = (region.y * (*imageRowPitch));
            break;
        case cl::MemObjectType::Image1D_Array:
            *imageSlicePitch = *imageRowPitch;
            break;
        default:
            UNREACHABLE();
            break;
    }

    ANGLE_TRY(createEvent(eventCreateFunc, true));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueUnmapMemObject(const cl::Memory &memory,
                                                      void *mappedPtr,
                                                      const cl::EventPtrs &waitEvents,
                                                      CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    bool eventComplete = false;
    if (!eventCreateFunc)
    {
        ANGLE_TRY(finishInternal());
        eventComplete = true;
    }

    if (memory.getType() == cl::MemObjectType::Buffer)
    {
        CLBufferVk &bufferVk = memory.getImpl<CLBufferVk>();
        if (memory.getFlags().intersects(CL_MEM_USE_HOST_PTR))
        {
            ANGLE_TRY(finishInternal());
            ANGLE_TRY(bufferVk.copyFrom(memory.getHostPtr(), 0, bufferVk.getSize()));
            eventComplete = true;
        }
    }
    else if (memory.getType() != cl::MemObjectType::Pipe)
    {
        // of image type
        CLImageVk &imageVk = memory.getImpl<CLImageVk>();
        VkExtent3D extent  = imageVk.getImageExtent();
        ANGLE_TRY(copyImageToFromBuffer(imageVk, imageVk.getStagingBuffer(), {0, 0, 0},
                                        {extent.width, extent.height, extent.depth}, 0,
                                        ImageBufferCopyDirection::ToImage));
        ANGLE_TRY(finishInternal());
        eventComplete = true;
    }
    else
    {
        // mem object type pipe is not supported and creation of such an object should have failed
        UNREACHABLE();
    }

    memory.getImpl<CLMemoryVk>().unmap();
    ANGLE_TRY(createEvent(eventCreateFunc, eventComplete));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueMigrateMemObjects(const cl::MemoryPtrs &memObjects,
                                                         cl::MemMigrationFlags flags,
                                                         const cl::EventPtrs &waitEvents,
                                                         CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueNDRangeKernel(const cl::Kernel &kernel,
                                                     const cl::NDRange &ndrange,
                                                     const cl::EventPtrs &waitEvents,
                                                     CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));

    cl::WorkgroupCount workgroupCount;
    vk::PipelineCacheAccess pipelineCache;
    vk::PipelineHelper *pipelineHelper = nullptr;
    CLKernelVk &kernelImpl             = kernel.getImpl<CLKernelVk>();

    // Here, we create-update-bind the kernel's descriptor set, put push-constants in cmd
    // buffer, capture kernel resources, and handle kernel execution dependencies
    ANGLE_TRY(processKernelResources(kernelImpl, ndrange, workgroupCount));

    // Fetch or create compute pipeline (if we miss in cache)
    ANGLE_CL_IMPL_TRY_ERROR(mContext->getRenderer()->getPipelineCache(mContext, &pipelineCache),
                            CL_OUT_OF_RESOURCES);
    ANGLE_TRY(kernelImpl.getOrCreateComputePipeline(
        &pipelineCache, ndrange, mCommandQueue.getDevice(), &pipelineHelper, &workgroupCount));

    mComputePassCommands->retainResource(pipelineHelper);

    mComputePassCommands->getCommandBuffer().bindComputePipeline(pipelineHelper->getPipeline());
    mComputePassCommands->getCommandBuffer().dispatch(workgroupCount[0], workgroupCount[1],
                                                      workgroupCount[2]);

    ANGLE_TRY(createEvent(eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueTask(const cl::Kernel &kernel,
                                            const cl::EventPtrs &waitEvents,
                                            CLEventImpl::CreateFunc *eventCreateFunc)
{
    constexpr size_t globalWorkSize[3] = {1, 0, 0};
    constexpr size_t localWorkSize[3]  = {1, 0, 0};
    cl::NDRange ndrange(1, nullptr, globalWorkSize, localWorkSize);
    return enqueueNDRangeKernel(kernel, ndrange, waitEvents, eventCreateFunc);
}

angle::Result CLCommandQueueVk::enqueueNativeKernel(cl::UserFunc userFunc,
                                                    void *args,
                                                    size_t cbArgs,
                                                    const cl::BufferPtrs &buffers,
                                                    const std::vector<size_t> bufferPtrOffsets,
                                                    const cl::EventPtrs &waitEvents,
                                                    CLEventImpl::CreateFunc *eventCreateFunc)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLCommandQueueVk::enqueueMarkerWithWaitList(const cl::EventPtrs &waitEvents,
                                                          CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRY(processWaitlist(waitEvents));
    ANGLE_TRY(createEvent(eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueMarker(CLEventImpl::CreateFunc &eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // This deprecated API is essentially a super-set of clEnqueueBarrier, where we also return an
    // event object (i.e. marker) since clEnqueueBarrier does not provide this
    VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    mComputePassCommands->getCommandBuffer().pipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
        &memoryBarrier, 0, nullptr, 0, nullptr);

    ANGLE_TRY(createEvent(&eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueWaitForEvents(const cl::EventPtrs &events)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // Unlike clWaitForEvents, this routine is non-blocking
    ANGLE_TRY(processWaitlist(events));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueBarrierWithWaitList(const cl::EventPtrs &waitEvents,
                                                           CLEventImpl::CreateFunc *eventCreateFunc)
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    // The barrier command either waits for a list of events to complete, or if the list is empty it
    // waits for all commands previously enqueued in command_queue to complete before it completes
    if (waitEvents.empty())
    {
        VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
            &memoryBarrier, 0, nullptr, 0, nullptr);
    }
    else
    {
        ANGLE_TRY(processWaitlist(waitEvents));
    }

    ANGLE_TRY(createEvent(eventCreateFunc, false));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::enqueueBarrier()
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    mComputePassCommands->getCommandBuffer().pipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
        &memoryBarrier, 0, nullptr, 0, nullptr);

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::flush()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::flush");

    // Non-blocking finish
    // TODO: Ideally we should try to find better impl. to avoid spawning a submit-thread/Task here
    // https://anglebug.com/42267107
    std::shared_ptr<angle::WaitableEvent> asyncEvent =
        getPlatform()->postMultiThreadWorkerTask(std::make_shared<CLAsyncFinishTask>(this));
    ASSERT(asyncEvent != nullptr);

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::finish()
{
    std::scoped_lock<std::mutex> sl(mCommandQueueMutex);

    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::finish");

    // Blocking finish
    return finishInternal();
}

angle::Result CLCommandQueueVk::syncHostBuffers()
{
    for (const cl::MemoryPtr &memoryPtr : mHostBufferUpdateList)
    {
        ASSERT(memoryPtr->getHostPtr() != nullptr);
        CLBufferVk &bufferVk = memoryPtr->getImpl<CLBufferVk>();
        ANGLE_TRY(
            bufferVk.copyTo(memoryPtr->getHostPtr(), memoryPtr->getOffset(), memoryPtr->getSize()));
    }
    mHostBufferUpdateList.clear();

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::processKernelResources(CLKernelVk &kernelVk,
                                                       const cl::NDRange &ndrange,
                                                       const cl::WorkgroupCount &workgroupCount)
{
    bool needsBarrier = false;
    const CLProgramVk::DeviceProgramData *devProgramData =
        kernelVk.getProgram()->getDeviceProgramData(mCommandQueue.getDevice().getNative());
    ASSERT(devProgramData != nullptr);

    // Set the descriptor set layouts and allocate descriptor sets
    // The descriptor set layouts are setup in the order of their appearance, as Vulkan requires
    // them to point to valid handles.
    angle::EnumIterator<DescriptorSetIndex> layoutIndex(DescriptorSetIndex::LiteralSampler);
    for (DescriptorSetIndex index : angle::AllEnums<DescriptorSetIndex>())
    {
        if (!kernelVk.getDescriptorSetLayoutDesc(index).empty())
        {
            // Setup the descriptor layout
            ANGLE_CL_IMPL_TRY_ERROR(mContext->getDescriptorSetLayoutCache()->getDescriptorSetLayout(
                                        mContext, kernelVk.getDescriptorSetLayoutDesc(index),
                                        &kernelVk.getDescriptorSetLayouts()[*layoutIndex]),
                                    CL_INVALID_OPERATION);

            ANGLE_CL_IMPL_TRY_ERROR(
                kernelVk.getProgram()->getMetaDescriptorPool(index).bindCachedDescriptorPool(
                    mContext, kernelVk.getDescriptorSetLayoutDesc(index), 1,
                    mContext->getDescriptorSetLayoutCache(),
                    &kernelVk.getProgram()->getDynamicDescriptorPoolPointer(index)),
                CL_INVALID_OPERATION);

            // Allocate descriptor set
            ANGLE_TRY(kernelVk.allocateDescriptorSet(index, layoutIndex, mComputePassCommands));
            ++layoutIndex;
        }
    }

    // Setup the pipeline layout
    ANGLE_CL_IMPL_TRY_ERROR(mContext->getPipelineLayoutCache()->getPipelineLayout(
                                mContext, kernelVk.getPipelineLayoutDesc(),
                                kernelVk.getDescriptorSetLayouts(), &kernelVk.getPipelineLayout()),
                            CL_INVALID_OPERATION);

    // Push global offset data
    const VkPushConstantRange *globalOffsetRange = devProgramData->getGlobalOffsetRange();
    if (globalOffsetRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            globalOffsetRange->offset, globalOffsetRange->size, ndrange.globalWorkOffset.data());
    }

    // Push global size data
    const VkPushConstantRange *globalSizeRange = devProgramData->getGlobalSizeRange();
    if (globalSizeRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            globalSizeRange->offset, globalSizeRange->size, ndrange.globalWorkSize.data());
    }

    // Push region offset data.
    const VkPushConstantRange *regionOffsetRange = devProgramData->getRegionOffsetRange();
    if (regionOffsetRange != nullptr)
    {
        // We dont support non-uniform batches yet in ANGLE, this field also represents global
        // offset for NDR in uniform cases. Update this when non-uniform batches are supported.
        // https://github.com/google/clspv/blob/main/docs/OpenCLCOnVulkan.md#module-scope-push-constants
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            regionOffsetRange->offset, regionOffsetRange->size, ndrange.globalWorkOffset.data());
    }

    // Push region group offset data.
    const VkPushConstantRange *regionGroupOffsetRange = devProgramData->getRegionGroupOffsetRange();
    if (regionGroupOffsetRange != nullptr)
    {
        // We dont support non-uniform batches yet in ANGLE, and based on clspv doc/notes:
        // "only required when non-uniform NDRanges are supported"
        // For now, we set this field to zeros until we later support non-uniform.
        // https://github.com/google/clspv/blob/main/docs/OpenCLCOnVulkan.md#module-scope-push-constants
        uint32_t regionGroupOffsets[3] = {0, 0, 0};
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            regionGroupOffsetRange->offset, regionGroupOffsetRange->size, &regionGroupOffsets);
    }

    // Push enqueued local size
    const VkPushConstantRange *enqueuedLocalSizeRange = devProgramData->getEnqueuedLocalSizeRange();
    if (enqueuedLocalSizeRange != nullptr)
    {
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            enqueuedLocalSizeRange->offset, enqueuedLocalSizeRange->size,
            ndrange.localWorkSize.data());
    }

    // Push number of workgroups
    const VkPushConstantRange *numWorkgroupsRange = devProgramData->getNumWorkgroupsRange();
    if (devProgramData->reflectionData.pushConstants.contains(
            NonSemanticClspvReflectionPushConstantNumWorkgroups))
    {
        uint32_t numWorkgroups[3] = {workgroupCount[0], workgroupCount[1], workgroupCount[2]};
        mComputePassCommands->getCommandBuffer().pushConstants(
            kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT,
            numWorkgroupsRange->offset, numWorkgroupsRange->size, &numWorkgroups);
    }

    // Retain kernel object until we finish executing it later
    mKernelCaptures.push_back(cl::KernelPtr{&kernelVk.getFrontendObject()});

    // Process each kernel argument/resource
    vk::DescriptorSetArray<UpdateDescriptorSetsBuilder> updateDescriptorSetsBuilders;
    for (const auto &arg : kernelVk.getArgs())
    {
        UpdateDescriptorSetsBuilder &kernelArgDescSetBuilder =
            updateDescriptorSetsBuilders[DescriptorSetIndex::KernelArguments];
        switch (arg.type)
        {
            case NonSemanticClspvReflectionArgumentUniform:
            case NonSemanticClspvReflectionArgumentStorageBuffer:
            {
                cl::Memory *clMem = cl::Buffer::Cast(*static_cast<const cl_mem *>(arg.handle));
                CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();

                // Retain this resource until its associated dispatch completes
                mMemoryCaptures.emplace_back(clMem);

                // Handle possible resource RAW hazard
                if (arg.type != NonSemanticClspvReflectionArgumentUniform)
                {
                    if (mDependencyTracker.contains(clMem) ||
                        mDependencyTracker.size() == kMaxDependencyTrackerSize)
                    {
                        needsBarrier = true;
                        mDependencyTracker.clear();
                    }
                    mDependencyTracker.insert(clMem);
                }

                // Update buffer/descriptor info
                VkDescriptorBufferInfo &bufferInfo =
                    kernelArgDescSetBuilder.allocDescriptorBufferInfo();
                bufferInfo.range  = clMem->getSize();
                bufferInfo.offset = clMem->getOffset();
                bufferInfo.buffer = vkMem.getBuffer().getBuffer().getHandle();
                VkWriteDescriptorSet &writeDescriptorSet =
                    kernelArgDescSetBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType =
                    arg.type == NonSemanticClspvReflectionArgumentUniform
                        ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                        : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writeDescriptorSet.pBufferInfo = &bufferInfo;
                writeDescriptorSet.sType       = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet =
                    kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
                writeDescriptorSet.dstBinding = arg.descriptorBinding;
                break;
            }
            case NonSemanticClspvReflectionArgumentPodPushConstant:
            {
                // Spec requires the size and offset to be multiple of 4, round up for size and
                // round down for offset to ensure this
                uint32_t offset = roundDownPow2(arg.pushConstOffset, 4u);
                uint32_t size =
                    roundUpPow2(arg.pushConstOffset + arg.pushConstantSize, 4u) - offset;
                ASSERT(offset + size <= kernelVk.getPodArgumentsData().size());
                mComputePassCommands->getCommandBuffer().pushConstants(
                    kernelVk.getPipelineLayout().get(), VK_SHADER_STAGE_COMPUTE_BIT, offset, size,
                    &kernelVk.getPodArgumentsData()[offset]);
                break;
            }
            case NonSemanticClspvReflectionArgumentSampler:
            {
                cl::Sampler *clSampler =
                    cl::Sampler::Cast(*static_cast<const cl_sampler *>(arg.handle));
                CLSamplerVk &vkSampler = clSampler->getImpl<CLSamplerVk>();
                VkDescriptorImageInfo &samplerInfo =
                    kernelArgDescSetBuilder.allocDescriptorImageInfo();
                samplerInfo.sampler = vkSampler.getSamplerHelper().get().getHandle();
                VkWriteDescriptorSet &writeDescriptorSet =
                    kernelArgDescSetBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
                writeDescriptorSet.pImageInfo      = &samplerInfo;
                writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet =
                    kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
                writeDescriptorSet.dstBinding = arg.descriptorBinding;
                break;
            }
            case NonSemanticClspvReflectionArgumentStorageImage:
            case NonSemanticClspvReflectionArgumentSampledImage:
            {
                cl::Memory *clMem = cl::Image::Cast(*static_cast<const cl_mem *>(arg.handle));
                CLImageVk &vkMem  = clMem->getImpl<CLImageVk>();

                mMemoryCaptures.emplace_back(clMem);

                // Handle possible resource RAW hazard
                if (clMem->getFlags().intersects(CL_MEM_READ_WRITE))
                {
                    if (mDependencyTracker.contains(clMem) ||
                        mDependencyTracker.size() == kMaxDependencyTrackerSize)
                    {
                        ANGLE_TRY(enqueueBarrier());
                        mDependencyTracker.clear();
                    }
                    mDependencyTracker.insert(clMem);
                }

                // Update image/descriptor info
                VkDescriptorImageInfo &imageInfo =
                    kernelArgDescSetBuilder.allocDescriptorImageInfo();
                imageInfo.imageLayout =
                    arg.type == NonSemanticClspvReflectionArgumentStorageImage
                        ? VK_IMAGE_LAYOUT_GENERAL
                        : vkMem.getImage().getCurrentLayout(mContext->getRenderer());
                imageInfo.imageView = vkMem.getImageView().getHandle();
                imageInfo.sampler   = VK_NULL_HANDLE;
                VkWriteDescriptorSet &writeDescriptorSet =
                    kernelArgDescSetBuilder.allocWriteDescriptorSet();
                writeDescriptorSet.descriptorCount = 1;
                writeDescriptorSet.descriptorType =
                    arg.type == NonSemanticClspvReflectionArgumentStorageImage
                        ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                        : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writeDescriptorSet.pImageInfo = &imageInfo;
                writeDescriptorSet.sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.dstSet =
                    kernelVk.getDescriptorSet(DescriptorSetIndex::KernelArguments);
                writeDescriptorSet.dstBinding = arg.descriptorBinding;
                break;
            }
            case NonSemanticClspvReflectionArgumentPodUniform:
            case NonSemanticClspvReflectionArgumentPointerUniform:
            case NonSemanticClspvReflectionArgumentPodStorageBuffer:
            case NonSemanticClspvReflectionArgumentUniformTexelBuffer:
            case NonSemanticClspvReflectionArgumentStorageTexelBuffer:
            case NonSemanticClspvReflectionArgumentPointerPushConstant:
            default:
            {
                UNIMPLEMENTED();
                break;
            }
        }
    }

    // process the printf storage buffer
    if (kernelVk.usesPrintf())
    {
        UpdateDescriptorSetsBuilder &printfDescSetBuilder =
            updateDescriptorSetsBuilders[DescriptorSetIndex::Printf];

        cl::Memory *clMem   = cl::Buffer::Cast(getOrCreatePrintfBuffer());
        CLBufferVk &vkMem   = clMem->getImpl<CLBufferVk>();
        uint8_t *mapPointer = nullptr;
        ANGLE_TRY(vkMem.map(mapPointer, 0));
        // The spec calls out *The first 4 bytes of the buffer should be zero-initialized.*
        memset(mapPointer, 0, 4);

        auto &bufferInfo  = printfDescSetBuilder.allocDescriptorBufferInfo();
        bufferInfo.range  = clMem->getSize();
        bufferInfo.offset = clMem->getOffset();
        bufferInfo.buffer = vkMem.getBuffer().getBuffer().getHandle();

        auto &writeDescriptorSet           = printfDescSetBuilder.allocWriteDescriptorSet();
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDescriptorSet.pBufferInfo     = &bufferInfo;
        writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet          = kernelVk.getDescriptorSet(DescriptorSetIndex::Printf);
        writeDescriptorSet.dstBinding      = kernelVk.getProgram()
                                            ->getDeviceProgramData(kernelVk.getKernelName().c_str())
                                            ->reflectionData.printfBufferStorage.binding;

        mNeedPrintfHandling = true;
        mPrintfInfos        = kernelVk.getProgram()->getPrintfDescriptors(kernelVk.getKernelName());
    }

    angle::EnumIterator<DescriptorSetIndex> descriptorSetIndex(DescriptorSetIndex::LiteralSampler);
    for (DescriptorSetIndex index : angle::AllEnums<DescriptorSetIndex>())
    {
        if (!kernelVk.getDescriptorSetLayoutDesc(index).empty())
        {
            mContext->getPerfCounters().writeDescriptorSets =
                updateDescriptorSetsBuilders[index].flushDescriptorSetUpdates(
                    mContext->getRenderer()->getDevice());

            VkDescriptorSet descriptorSet = kernelVk.getDescriptorSet(index);
            mComputePassCommands->getCommandBuffer().bindDescriptorSets(
                kernelVk.getPipelineLayout().get(), VK_PIPELINE_BIND_POINT_COMPUTE,
                *descriptorSetIndex, 1, &descriptorSet, 0, nullptr);

            ++descriptorSetIndex;
        }
    }

    if (needsBarrier)
    {
        VkMemoryBarrier memoryBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
        mComputePassCommands->getCommandBuffer().pipelineBarrier(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
            &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::flushComputePassCommands()
{
    if (mComputePassCommands->empty())
    {
        return angle::Result::Continue;
    }

    // Flush any host visible buffers by adding appropriate barriers
    if (mComputePassCommands->getAndResetHasHostVisibleBufferWrite())
    {
        // Make sure all writes to host-visible buffers are flushed.
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT;
        memoryBarrier.dstAccessMask   = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

        mComputePassCommands->getCommandBuffer().memoryBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT, memoryBarrier);
    }

    mLastFlushedQueueSerial = mComputePassCommands->getQueueSerial();
    // Here, we flush our compute cmds to RendererVk's primary command buffer
    ANGLE_TRY(mContext->getRenderer()->flushOutsideRPCommands(
        mContext, getProtectionType(), egl::ContextPriority::Medium, &mComputePassCommands));

    mHasAnyCommandsPendingSubmission = true;

    mContext->getPerfCounters().flushedOutsideRenderPassCommandBuffers++;

    // Generate new serial for next batch of cmds
    mComputePassCommands->setQueueSerial(
        mCurrentQueueSerialIndex,
        mContext->getRenderer()->generateQueueSerial(mCurrentQueueSerialIndex));

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::processWaitlist(const cl::EventPtrs &waitEvents)
{
    if (!waitEvents.empty())
    {
        bool insertedBarrier = false;
        for (const cl::EventPtr &event : waitEvents)
        {
            if (event->getImpl<CLEventVk>().isUserEvent() ||
                event->getCommandQueue() != &mCommandQueue)
            {
                // We cannot use a barrier in these cases, therefore defer the event
                // handling till submission time
                // TODO: Perhaps we could utilize VkEvents here instead and have GPU wait(s)
                // https://anglebug.com/42267109
                mDependantEvents.push_back(event);
            }
            else if (event->getCommandQueue() == &mCommandQueue && !insertedBarrier)
            {
                // As long as there is at least one dependant command in same queue,
                // we just need to insert one execution barrier
                VkMemoryBarrier memoryBarrier = {
                    VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
                mComputePassCommands->getCommandBuffer().pipelineBarrier(
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                    1, &memoryBarrier, 0, nullptr, 0, nullptr);

                insertedBarrier = true;
            }
        }
    }
    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::submitCommands()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CLCommandQueueVk::submitCommands()");

    // Kick off renderer submit
    ANGLE_TRY(mContext->getRenderer()->submitCommands(mContext, getProtectionType(),
                                                      egl::ContextPriority::Medium, nullptr,
                                                      nullptr, mLastFlushedQueueSerial));

    mLastSubmittedQueueSerial = mLastFlushedQueueSerial;

    // Now that we have submitted commands, some of pending garbage may no longer pending
    // and should be moved to garbage list.
    mContext->getRenderer()->cleanupPendingSubmissionGarbage();

    mHasAnyCommandsPendingSubmission = false;

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::createEvent(CLEventImpl::CreateFunc *createFunc, bool blocking)
{
    if (createFunc != nullptr)
    {
        *createFunc = [this, blocking](const cl::Event &event) {
            auto eventVk = new (std::nothrow) CLEventVk(event);
            if (eventVk == nullptr)
            {
                ERR() << "Failed to create event obj!";
                ANGLE_CL_SET_ERROR(CL_OUT_OF_HOST_MEMORY);
                return CLEventImpl::Ptr(nullptr);
            }

            if (blocking)
            {
                if (IsError(eventVk->setStatusAndExecuteCallback(CL_COMPLETE)))
                {
                    ANGLE_CL_SET_ERROR(CL_OUT_OF_RESOURCES);
                }
            }
            else
            {
                eventVk->setQueueSerial(mComputePassCommands->getQueueSerial());
                // Save a reference to this event
                mAssociatedEvents.push_back(cl::EventPtr{&eventVk->getFrontendObject()});

                if (mCommandQueue.getProperties().intersects(CL_QUEUE_PROFILING_ENABLE))
                {
                    if (IsError(mCommandQueue.getImpl<CLCommandQueueVk>().flush()))
                    {
                        ANGLE_CL_SET_ERROR(CL_OUT_OF_RESOURCES);
                    }
                }
            }

            return CLEventImpl::Ptr(eventVk);
        };
    }
    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::finishInternal()
{
    for (cl::EventPtr event : mAssociatedEvents)
    {
        ANGLE_TRY(event->getImpl<CLEventVk>().setStatusAndExecuteCallback(CL_SUBMITTED));
    }

    if (!mComputePassCommands->empty())
    {
        // If we still have dependant events, handle them now
        if (!mDependantEvents.empty())
        {
            for (const auto &depEvent : mDependantEvents)
            {
                if (depEvent->getImpl<CLEventVk>().isUserEvent())
                {
                    // We just wait here for user to set the event object
                    cl_int status = CL_QUEUED;
                    ANGLE_TRY(depEvent->getImpl<CLEventVk>().waitForUserEventStatus());
                    ANGLE_TRY(depEvent->getImpl<CLEventVk>().getCommandExecutionStatus(status));
                    if (status < 0)
                    {
                        ERR() << "Invalid dependant user-event (" << depEvent.get()
                              << ") status encountered!";
                        mComputePassCommands->getCommandBuffer().reset();
                        ANGLE_CL_RETURN_ERROR(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
                    }
                }
                else
                {
                    // Otherwise, we just need to submit/finish for dependant event queues
                    // here that are not associated with this queue
                    ANGLE_TRY(depEvent->getCommandQueue()->finish());
                }
            }
            mDependantEvents.clear();
        }

        ANGLE_TRY(flushComputePassCommands());
    }

    for (cl::EventPtr event : mAssociatedEvents)
    {
        ANGLE_TRY(event->getImpl<CLEventVk>().setStatusAndExecuteCallback(CL_RUNNING));
    }

    if (mHasAnyCommandsPendingSubmission)
    {
        // Submit and wait for fence
        ANGLE_TRY(submitCommands());
        ANGLE_TRY(mContext->getRenderer()->finishQueueSerial(mContext, mLastSubmittedQueueSerial));

        // Ensure any resources are synced back to host on GPU completion
        ANGLE_TRY(syncHostBuffers());
    }

    if (mNeedPrintfHandling)
    {
        ANGLE_TRY(processPrintfBuffer());
        mNeedPrintfHandling = false;
    }

    for (cl::EventPtr event : mAssociatedEvents)
    {
        ANGLE_TRY(event->getImpl<CLEventVk>().setStatusAndExecuteCallback(CL_COMPLETE));
    }

    mMemoryCaptures.clear();
    mAssociatedEvents.clear();
    mDependencyTracker.clear();
    mKernelCaptures.clear();

    return angle::Result::Continue;
}

// Helper function to insert appropriate memory barriers before accessing the resources in the
// command buffer.
angle::Result CLCommandQueueVk::onResourceAccess(const vk::CommandBufferAccess &access)
{
    // Buffers
    for (const vk::CommandBufferBufferAccess &bufferAccess : access.getReadBuffers())
    {
        if (mComputePassCommands->usesBufferForWrite(*bufferAccess.buffer))
        {
            // read buffers only need a new command buffer if previously used for write
            ANGLE_TRY(flush());
        }

        mComputePassCommands->bufferRead(bufferAccess.accessType, bufferAccess.stage,
                                         bufferAccess.buffer);
    }

    for (const vk::CommandBufferBufferAccess &bufferAccess : access.getWriteBuffers())
    {
        if (mComputePassCommands->usesBuffer(*bufferAccess.buffer))
        {
            // write buffers always need a new command buffer
            ANGLE_TRY(flush());
        }

        mComputePassCommands->bufferWrite(bufferAccess.accessType, bufferAccess.stage,
                                          bufferAccess.buffer);
        if (bufferAccess.buffer->isHostVisible())
        {
            // currently all are host visible so nothing to do
        }
    }

    for (const vk::CommandBufferBufferExternalAcquireRelease &bufferAcquireRelease :
         access.getExternalAcquireReleaseBuffers())
    {
        mComputePassCommands->retainResourceForWrite(bufferAcquireRelease.buffer);
    }

    for (const vk::CommandBufferResourceAccess &resourceAccess : access.getAccessResources())
    {
        mComputePassCommands->retainResource(resourceAccess.resource);
    }

    return angle::Result::Continue;
}

angle::Result CLCommandQueueVk::processPrintfBuffer()
{
    ASSERT(mPrintfBuffer);
    ASSERT(mNeedPrintfHandling);
    ASSERT(mPrintfInfos);

    cl::Memory *clMem = cl::Buffer::Cast(getOrCreatePrintfBuffer());
    CLBufferVk &vkMem = clMem->getImpl<CLBufferVk>();

    unsigned char *data = nullptr;
    ANGLE_TRY(vkMem.map(data, 0));
    ANGLE_TRY(ClspvProcessPrintfBuffer(data, vkMem.getSize(), mPrintfInfos));
    vkMem.unmap();

    return angle::Result::Continue;
}

// A single CL buffer is setup for every command queue of size kPrintfBufferSize. This can be
// expanded later, if more storage is needed.
cl_mem CLCommandQueueVk::getOrCreatePrintfBuffer()
{
    if (!mPrintfBuffer)
    {
        mPrintfBuffer = cl::Buffer::Cast(mContext->getFrontendObject().createBuffer(
            nullptr, cl::MemFlags(CL_MEM_READ_WRITE), kPrintfBufferSize, nullptr));
    }
    return mPrintfBuffer;
}

}  // namespace rx

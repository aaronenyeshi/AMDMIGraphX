
#ifndef MIGRAPHX_GUARD_RTGLIB_DEVICE_ADD_RELU_HPP
#define MIGRAPHX_GUARD_RTGLIB_DEVICE_ADD_RELU_HPP

#include <migraphx/argument.hpp>
#include <migraphx/config.hpp>
#include <hip/hip_runtime_api.h>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {
namespace device {

void mul_add_relu(hipStream_t stream,
                  const argument& result,
                  const argument& arg1,
                  const argument& arg2,
                  const argument& arg3);

void add_relu(hipStream_t stream,
              const argument& result,
              const argument& arg1,
              const argument& arg2);

void add_relu(hipStream_t stream,
              const argument& result,
              const argument& arg1,
              const argument& arg2,
              const argument& arg3);

} // namespace device
} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif

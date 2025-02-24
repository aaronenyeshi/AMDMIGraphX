#include <migraphx/gpu/argmin.hpp>
#include <migraphx/gpu/device/argmin.hpp>
#include <migraphx/gpu/context.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {

shape hip_argmin::compute_shape(const std::vector<shape>& inputs) const
{
    check_shapes{inputs, *this}.has(2).standard();
    return op.compute_shape({inputs.at(0)});
}

argument hip_argmin::compute(context& ctx, const shape&, const std::vector<argument>& args) const
{
    device::argmin(ctx.get_stream().get(), args.back(), args.front(), op.axis);
    return args.back();
}

} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

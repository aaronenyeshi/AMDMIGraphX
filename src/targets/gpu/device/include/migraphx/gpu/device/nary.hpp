#ifndef MIGRAPHX_GUARD_RTGLIB_DEVICE_NARY_HPP
#define MIGRAPHX_GUARD_RTGLIB_DEVICE_NARY_HPP

#include <migraphx/gpu/device/launch.hpp>
#include <migraphx/gpu/device/visit.hpp>
#include <migraphx/functional.hpp>
#include <migraphx/ranges.hpp>
#include <migraphx/array.hpp>
#include <migraphx/config.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {
namespace device {

template <class... Ts>
auto pack(Ts... xs) __device__
{
    return [=](auto f) { return f(xs...); };
}

template <class F, class... Arguments>
auto nary_nonstandard_impl(hipStream_t stream, F f, argument result, Arguments... args)
{
    std::size_t nelements = result.get_shape().elements();
    hip_visit_all(result, args...)([&](auto output, auto... inputs) {
        gs_launch(stream, nelements)([=](auto i) {
            auto idx  = output.get_shape().multi(i);
            output[i] = f(inputs[idx]...);
        });
    });
}

template <class F, class... Arguments>
void nary_broadcast_vec_impl(
    hipStream_t stream, F f, argument result, argument barg, Arguments... args)
{
    const auto& output_shape = result.get_shape();
    const auto& b_shape      = barg.get_shape();
    auto bdim =
        std::distance(b_shape.strides().begin(),
                      std::find_if(b_shape.strides().begin(), b_shape.strides().end(), [](auto x) {
                          return x != 0;
                      }));
    auto bdim_len         = output_shape.lens()[bdim];
    auto bdim_stride      = output_shape.strides()[bdim];
    auto bdim_next_stride = bdim_stride * bdim_len;

    const std::size_t vec_size     = 4;
    const std::size_t nlocal       = 1024;
    const std::size_t nglobal      = 256 * nlocal;
    const std::size_t bdim_vec_len = bdim_len / vec_size;
    hip_vec_visit_all<vec_size>(result, barg, args...)(
        [&](auto output, auto binput, auto... inputs) {
            using type                  = typename decltype(output)::value_type;
            const std::size_t nelements = output.size() / vec_size;
            launch(stream, nglobal, nlocal)([=](auto idx) __device__ {

                MIGRAPHX_DEVICE_SHARED type buffer[2048 / vec_size];
                // Load bias into LDS
                for(size_t i = idx.local; i < bdim_vec_len; i += nlocal)
                {
                    buffer[i] = binput.data()[i];
                }
                __syncthreads();
                auto* bp = as_pointer(buffer);
                // Process the data
                for(size_t i = idx.global; i < nelements; i += nglobal)
                {
                    auto bidx = ((i * vec_size) % bdim_next_stride) / bdim_stride;
                    auto b    = bp[bidx];
                    auto out  = output.data()[i];
                    for(std::size_t j = 0; j < vec_size; j++)
                    {
                        out[j] = f(inputs.data()[i][j]..., b);
                    }
                    output.data()[i] = out;
                }
            });
        });
}

template <class F, class... Arguments>
void nary_broadcast_impl(hipStream_t stream, F f, argument result, argument barg, Arguments... args)
{
    const auto& output_shape = result.get_shape();
    const auto& b_shape      = barg.get_shape();
    auto bdim =
        std::distance(b_shape.strides().begin(),
                      std::find_if(b_shape.strides().begin(), b_shape.strides().end(), [](auto x) {
                          return x != 0;
                      }));
    auto bdim_len         = output_shape.lens()[bdim];
    auto bdim_stride      = output_shape.strides()[bdim];
    auto bdim_next_stride = bdim_stride * bdim_len;

    const std::size_t nlocal  = 1024;
    const std::size_t nglobal = 256 * nlocal;
    std::size_t nelements     = result.get_shape().elements();
    hip_visit_all(result, barg, args...)([&](auto output, auto binput, auto... inputs) {
        using type = typename decltype(output)::value_type;
        launch(stream, nglobal, nlocal)([=](auto idx) __device__ {
            MIGRAPHX_DEVICE_SHARED type buffer[2048];
            // Load bias into LDS
            for(size_t i = idx.local; i < bdim_len; i += nlocal)
            {
                buffer[i] = binput.data()[i];
            }
            __syncthreads();
            // Process the data
            for(size_t i = idx.global; i < nelements; i += nglobal)
            {
                auto bidx        = (i % bdim_next_stride) / bdim_stride;
                auto b           = buffer[bidx];
                output.data()[i] = f(inputs.data()[i]..., b);
            }
        });
    });
}

template <class F, class... Arguments>
void nary_double_broadcast_vec_impl(
    hipStream_t stream, F f, argument result, argument barg1, argument barg2, Arguments... args)
{
    assert(barg1.get_shape().broadcasted());
    assert(barg2.get_shape().broadcasted());
    assert(barg1.get_shape() == barg2.get_shape());
    const auto& output_shape = result.get_shape();
    const auto& b_shape      = barg1.get_shape();
    auto bdim =
        std::distance(b_shape.strides().begin(),
                      std::find_if(b_shape.strides().begin(), b_shape.strides().end(), [](auto x) {
                          return x != 0;
                      }));
    auto bdim_len         = output_shape.lens()[bdim];
    auto bdim_stride      = output_shape.strides()[bdim];
    auto bdim_next_stride = bdim_stride * bdim_len;

    const std::size_t vec_size     = 4;
    const std::size_t nlocal       = 1024;
    const std::size_t nglobal      = 256 * nlocal;
    const std::size_t bdim_vec_len = bdim_len / vec_size;
    hip_vec_visit_all<vec_size>(result, barg1, barg2, args...)(
        [&](auto output, auto binput1, auto binput2, auto... inputs) {
            using type                  = typename decltype(output)::value_type;
            const std::size_t nelements = output.size() / vec_size;
            launch(stream, nglobal, nlocal)([=](auto idx) __device__ {

                MIGRAPHX_DEVICE_SHARED type buffer[2048 / vec_size];
                // Load bias into LDS
                for(size_t i = idx.local; i < bdim_vec_len; i += nlocal)
                {
                    buffer[i] = binput1.data()[i];
                }
                for(size_t i = idx.local; i < bdim_vec_len; i += nlocal)
                {
                    buffer[i + bdim_vec_len] = binput2.data()[i];
                }
                __syncthreads();
                auto* bp = as_pointer(buffer);
                // Process the data
                for(size_t i = idx.global; i < nelements; i += nglobal)
                {
                    auto bidx = ((i * vec_size) % bdim_next_stride) / bdim_stride;
                    auto b1   = bp[bidx];
                    auto b2   = bp[bidx + bdim_len];
                    auto out  = output.data()[i];
                    for(std::size_t j = 0; j < vec_size; j++)
                    {
                        out[j] = f(inputs.data()[i][j]..., b2, b1);
                    }
                    output.data()[i] = out;
                }
            });
        });
}

template <class F, class... Arguments>
void nary_double_broadcast_impl(
    hipStream_t stream, F f, argument result, argument barg1, argument barg2, Arguments... args)
{
    assert(barg1.get_shape().broadcasted());
    assert(barg2.get_shape().broadcasted());
    assert(barg1.get_shape() == barg2.get_shape());
    const auto& output_shape = result.get_shape();
    const auto& b_shape      = barg1.get_shape();
    auto bdim =
        std::distance(b_shape.strides().begin(),
                      std::find_if(b_shape.strides().begin(), b_shape.strides().end(), [](auto x) {
                          return x != 0;
                      }));
    auto bdim_len         = output_shape.lens()[bdim];
    auto bdim_stride      = output_shape.strides()[bdim];
    auto bdim_next_stride = bdim_stride * bdim_len;

    const std::size_t nlocal  = 1024;
    const std::size_t nglobal = 256 * nlocal;
    std::size_t nelements     = result.get_shape().elements();
    hip_visit_all(result, barg1, barg2, args...)(
        [&](auto output, auto binput1, auto binput2, auto... inputs) {
            using type = typename decltype(output)::value_type;
            launch(stream, nglobal, nlocal)([=](auto idx) __device__ {
                MIGRAPHX_DEVICE_SHARED type buffer[2048];
                // Load bias into LDS
                for(size_t i = idx.local; i < bdim_len; i += nlocal)
                {
                    buffer[i] = binput1.data()[i];
                }
                for(size_t i = idx.local; i < bdim_len; i += nlocal)
                {
                    buffer[i + bdim_len] = binput2.data()[i];
                }
                __syncthreads();
                // Process the data
                for(size_t i = idx.global; i < nelements; i += nglobal)
                {
                    auto bidx        = (i % bdim_next_stride) / bdim_stride;
                    auto b1          = buffer[bidx];
                    auto b2          = buffer[bidx + bdim_len];
                    output.data()[i] = f(inputs.data()[i]..., b2, b1);
                }
            });
        });
}

template <class F, class... Arguments>
void nary_standard_vec_impl(hipStream_t stream, F f, argument result, Arguments... args)
{
    const auto& output_shape = result.get_shape();
    visit_all(result, args...)([&](auto output, auto... inputs) {
        using type = device_type<std::remove_cv_t<typename decltype(output)::value_type>>;
        const std::size_t vec_size = 4;
        auto data                  = pack_vec<4>(device_cast(inputs.data())...);
        auto* outp                 = as_vec<4>(device_cast(output.data()));
        gs_launch(stream, output_shape.elements() / vec_size)([=](auto i) {
            vec<type, 4> out = outp[i];
            data(
                [&](auto... xs) {
                    for(std::size_t j = 0; j < vec_size; j++)
                    {
                        out[j] = f(xs[j]...);
                    }
                },
                i);
            outp[i] = out;
        });
    });
}

template <class F, class... Arguments>
void nary_standard_impl(hipStream_t stream, F f, argument result, Arguments... args)
{
    std::size_t nelements = result.get_shape().elements();
    hip_pointer_visit_all(result, args...)([&](auto output, auto... inputs) {
        gs_launch(stream, nelements)([=](auto i) { output[i] = f(inputs[i]...); });
    });
}

template <class F, class... Arguments>
void nary_impl(hipStream_t stream, F f, argument result, Arguments... args)
{
    bool standard = all_of({args.get_shape()...}, [](const shape& s) { return s.standard(); });
    bool packed   = all_of({args.get_shape()...}, [](const shape& s) { return s.packed(); });
    bool same_shapes =
        all_of({args.get_shape()...}, [&](const shape& s) { return s == result.get_shape(); });
    if(standard or (packed and same_shapes))
        nary_standard_impl(stream, f, result, args...);
    else
        nary_nonstandard_impl(stream, f, result, args...);
}

template <class... Arguments>
auto nary_nonstandard(hipStream_t stream, argument result, Arguments... args)
{
    return [=](auto f) { nary_nonstandard_impl(stream, f, result, args...); };
}

template <class... Arguments>
auto nary_standard(hipStream_t stream, argument result, Arguments... args)
{
    return [=](auto f) { nary_standard_impl(stream, f, result, args...); };
}

template <class... Arguments>
bool broadcastable(bool& divisible_by_4,
                   std::size_t max_size,
                   const argument& result,
                   const argument& barg,
                   const Arguments&... args)
{
    divisible_by_4 = false;
    auto bshape    = barg.get_shape();
    const bool standard =
        all_of({args.get_shape()...}, [](const shape& s) { return s.standard(); });
    const bool same_shapes =
        all_of({args.get_shape()...}, [&](const shape& s) { return s == result.get_shape(); });
    // TODO: Check result and args shape is the same
    if(standard and same_shapes and bshape.broadcasted() and not bshape.scalar())
    {
        auto not_zero       = [](auto x) { return x != 0; };
        const auto& strides = bshape.strides();
        auto b_it           = std::find_if(strides.begin(), strides.end(), not_zero);
        auto b_idx          = std::distance(strides.begin(), b_it);
        auto b_len          = result.get_shape().lens()[b_idx];
        auto b_stride       = result.get_shape().strides()[b_idx];
        assert(bshape.lens()[b_idx] == b_len);
        if(b_len <= max_size and std::none_of(std::next(b_it), strides.end(), not_zero))
        {

            divisible_by_4 = (b_len % 4 == 0) and (b_stride % 4 == 0) and
                             (front_args(args...).get_shape().elements() % 4 == 0);
            return true;
        }
    }
    return false;
}

inline bool broadcastable(bool& divisible_by_4, std::size_t, const argument&, const argument&)
{
    divisible_by_4 = false;
    return false;
}

// Nullary
inline auto nary(hipStream_t stream, argument result)
{
    return [=](auto f) { nary_standard_impl(stream, f, result); };
}

// Unary
inline auto nary(hipStream_t stream, argument result, argument arg)
{
    return [=](auto f) { nary_impl(stream, f, result, arg); };
}

// Binary
inline auto nary(hipStream_t stream, argument result, argument arg, argument barg)
{
    return [=](auto f) {
        bool divisible_by_4 = false;
        if(broadcastable(divisible_by_4, 2048, result, barg, arg))
        {
            if(divisible_by_4)
                nary_broadcast_vec_impl(stream, f, result, barg, arg);
            else
                nary_broadcast_impl(stream, f, result, barg, arg);
        }
        else
        {
            nary_impl(stream, f, result, arg, barg);
        }
    };
}

template <class... Arguments>
auto nary(hipStream_t stream, argument result, Arguments... args)
{
    static_assert(sizeof...(args) > 2, "Args needs to be greater than 2");
    return [=](auto f) {
        auto barg1     = back_args(args...);
        bool fallback1 = pop_back_args(args...)([&](auto&&... args2) {
            auto barg2 = back_args(args2...);
            bool fallback2 =
                barg2.get_shape() != barg1.get_shape() or not barg2.get_shape().broadcasted() or
                pop_back_args(args2...)([&](auto&&... args3) {
                    bool divisible_by_4 = false;
                    if(broadcastable(divisible_by_4, 1024, result, barg2, args3...))
                    {
                        if(divisible_by_4)
                            nary_double_broadcast_vec_impl(
                                stream, f, result, barg1, barg2, args3...);
                        else
                            nary_double_broadcast_impl(stream, f, result, barg1, barg2, args3...);
                        return false;
                    }
                    return true;
                });
            if(not fallback2)
                return false;
            bool divisible_by_4 = false;
            if(broadcastable(divisible_by_4, 2048, result, barg1, args2...))
            {
                if(divisible_by_4)
                    nary_broadcast_vec_impl(stream, f, result, barg1, args2...);
                else
                    nary_broadcast_impl(stream, f, result, barg1, args2...);
                return false;
            }
            return true;
        });
        if(fallback1)
            nary_impl(stream, f, result, args...);
    };
}

} // namespace device
} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif

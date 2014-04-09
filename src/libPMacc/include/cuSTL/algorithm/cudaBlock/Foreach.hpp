/**
 * Copyright 2013-2014 Heiko Burau, Rene Widera, Axel Huebl
 *
 * This file is part of libPMacc.
 *
 * libPMacc is free software: you can redistribute it and/or modify
 * it under the terms of of either the GNU General Public License or
 * the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libPMacc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License and the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and the GNU Lesser General Public License along with libPMacc.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <types.h>
#include <math/vector/Int.hpp>
#include <math/vector/compile-time/Int.hpp>
#include <forward.hpp>

namespace PMacc
{
namespace algorithm
{
namespace cudaBlock
{
namespace detail
{
    template<uint32_t dim>
    struct getPos;

    template<>
    struct getPos<3>
    {
        template<typename Zone>
        DINLINE math::Int<3> operator()( Zone, const int linearThreadIdx )
        {
            return math::Int<3>(
                Zone::Offset::x::value + (linearThreadIdx  % Zone::Size::x::value),
                Zone::Offset::y::value + ((linearThreadIdx % (Zone::Size::x::value * Zone::Size::y::value)) / Zone::Size::x::value),
                Zone::Offset::z::value + (linearThreadIdx  / (Zone::Size::x::value * Zone::Size::y::value)));
        }
    };

    template<>
    struct getPos<2>
    {
        template<typename Zone>
        DINLINE math::Int<2> operator()( Zone, const int linearThreadIdx )
        {
            return math::Int<2>(
                Zone::Offset::x::value + (linearThreadIdx % Zone::Size::x::value),
                Zone::Offset::y::value + (linearThreadIdx / Zone::Size::x::value));
        }
    };

    template<>
    struct getPos<1>
    {
        template<typename Zone>
        DINLINE math::Int<1> operator()( Zone, const int linearThreadIdx )
        {
            return math::Int<1>( Zone::Offset::x::value + linearThreadIdx );
        }
    };
} // namespace detail

#ifndef FOREACH_KERNEL_MAX_PARAMS
#define FOREACH_KERNEL_MAX_PARAMS 4
#endif

#define SHIFTACCESS_CURSOR(Z, N, _) forward(c ## N [pos])

#define FOREACH_OPERATOR(Z, N, _)                                                  \
    /*      <             , typename C0, ..., typename C(N-1)  ,              > */ \
    template<typename Zone, BOOST_PP_ENUM_PARAMS(N, typename C), typename Functor> \
    /*                     (      C0 c0, ..., C(N-1) c(N-1)           ,       ) */ \
    DINLINE void operator()(Zone, BOOST_PP_ENUM_BINARY_PARAMS(N, C, c), const Functor& functor) \
    {                                                                              \
        BOOST_AUTO(functor_, lambda::make_Functor(functor));                       \
        const int dataVolume = math::CT::volume<typename Zone::Size>::type::value; \
        const int blockVolume = math::CT::volume<BlockDim>::type::value;           \
        for(int i = this->linearThreadIdx; i < dataVolume; i += blockVolume)       \
        {                                                                          \
            math::Int<Zone::dim> pos = detail::getPos<Zone::dim>()( Zone(), i );   \
            functor_(BOOST_PP_ENUM(N, SHIFTACCESS_CURSOR, _));                     \
        }                                                                          \
    }

/** Foreach algorithm that is executed by one cuda thread block
 * 
 * \tparam BlockDim 3D compile-time vector (PMacc::math::CT::Int) of the size of the cuda blockDim.
 *
 * BlockDim could also be obtained from cuda itself at runtime but
 * it is faster to know it at compile-time.
 */
template<typename BlockDim>
struct Foreach
{
private:
    const int linearThreadIdx;
public:
    DINLINE Foreach() 
     : linearThreadIdx(
        threadIdx.z * BlockDim::x::value * BlockDim::y::value +
        threadIdx.y * BlockDim::x::value +
        threadIdx.x) {}
    DINLINE Foreach(int linearThreadIdx) : linearThreadIdx(linearThreadIdx) {}

    /* operator()(zone, cursor0, cursor1, ..., cursorN-1, functor or lambdaFun)
     * 
     * \param zone compile-time zone object, see zone::CT::SphericZone. (e.g. ContainerType::Zone())
     * \param cursorN cursor for the N-th data source (e.g. containerObj.origin())
     * \param functor or lambdaFun either a functor with N arguments or a N-ary lambda function (e.g. _1 = _2)
     * 
     * The functor or lambdaFun is called for each cell within the zone.
     * It is called like functor(*cursor0(cellId), ..., *cursorN(cellId))
     * 
     */
    BOOST_PP_REPEAT_FROM_TO(1, BOOST_PP_INC(FOREACH_KERNEL_MAX_PARAMS), FOREACH_OPERATOR, _)
};

#undef SHIFTACCESS_CURSOR
#undef FOREACH_OPERATOR

} // cudaBlock
} // algorithm
} // PMacc

/*! \file */
/* ************************************************************************
* Copyright (C) 2021-2024 Advanced Micro Devices, Inc. All rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
* ************************************************************************ */

#include "rocsparse_coomm.hpp"
#include "common.h"
#include "control.h"
#include "utility.h"

namespace rocsparse
{
    // Scale kernel for beta != 1.0
    template <unsigned int BLOCKSIZE, typename I, typename T>
    ROCSPARSE_DEVICE_ILF void coommnn_scale_device(
        I m, I n, T beta, T* __restrict__ data, int64_t ld, int64_t stride, rocsparse_order order)
    {
        const I gid   = hipBlockIdx_x * BLOCKSIZE + hipThreadIdx_x;
        const I batch = hipBlockIdx_y;

        if(gid >= m * n)
        {
            return;
        }

        const I wid = (order == rocsparse_order_column) ? gid / m : gid / n;
        const I lid = (order == rocsparse_order_column) ? gid % m : gid % n;

        if(beta == static_cast<T>(0))
        {
            data[lid + ld * wid + stride * batch] = static_cast<T>(0);
        }
        else
        {
            data[lid + ld * wid + stride * batch] *= beta;
        }
    }

    template <unsigned int BLOCKSIZE, typename I, typename T, typename U>
    ROCSPARSE_KERNEL(BLOCKSIZE)
    void coommnn_scale_kernel(I m,
                              I n,
                              U beta_device_host,
                              T* __restrict__ data,
                              int64_t         ld,
                              int64_t         stride,
                              rocsparse_order order)
    {

        const auto beta = rocsparse::load_scalar_device_host(beta_device_host);
        if(beta != static_cast<T>(1))
        {
            rocsparse::coommnn_scale_device<BLOCKSIZE>(m, n, beta, data, ld, stride, order);
        }
    }

    template <typename T, typename I, typename A, typename B, typename C, typename U>
    rocsparse_status coomm_template_atomic(rocsparse_handle          handle,
                                           rocsparse_operation       trans_A,
                                           rocsparse_operation       trans_B,
                                           I                         m,
                                           I                         n,
                                           I                         k,
                                           int64_t                   nnz,
                                           I                         batch_count_A,
                                           int64_t                   batch_stride_A,
                                           U                         alpha_device_host,
                                           const rocsparse_mat_descr descr,
                                           const A*                  coo_val,
                                           const I*                  coo_row_ind,
                                           const I*                  coo_col_ind,
                                           const B*                  dense_B,
                                           int64_t                   ldb,
                                           I                         batch_count_B,
                                           int64_t                   batch_stride_B,
                                           rocsparse_order           order_B,
                                           U                         beta_device_host,
                                           C*                        dense_C,
                                           int64_t                   ldc,
                                           I                         batch_count_C,
                                           int64_t                   batch_stride_C,
                                           rocsparse_order           order_C);

    template <typename T, typename I, typename A, typename B, typename C, typename U>
    rocsparse_status coomm_template_segmented_atomic(rocsparse_handle          handle,
                                                     rocsparse_operation       trans_A,
                                                     rocsparse_operation       trans_B,
                                                     I                         m,
                                                     I                         n,
                                                     I                         k,
                                                     int64_t                   nnz,
                                                     I                         batch_count_A,
                                                     int64_t                   batch_stride_A,
                                                     U                         alpha_device_host,
                                                     const rocsparse_mat_descr descr,
                                                     const A*                  coo_val,
                                                     const I*                  coo_row_ind,
                                                     const I*                  coo_col_ind,
                                                     const B*                  dense_B,
                                                     int64_t                   ldb,
                                                     I                         batch_count_B,
                                                     int64_t                   batch_stride_B,
                                                     rocsparse_order           order_B,
                                                     U                         beta_device_host,
                                                     C*                        dense_C,
                                                     int64_t                   ldc,
                                                     I                         batch_count_C,
                                                     int64_t                   batch_stride_C,
                                                     rocsparse_order           order_C);

    template <typename T, typename I, typename A, typename B, typename C, typename U>
    rocsparse_status coomm_template_segmented(rocsparse_handle          handle,
                                              rocsparse_operation       trans_A,
                                              rocsparse_operation       trans_B,
                                              I                         m,
                                              I                         n,
                                              I                         k,
                                              int64_t                   nnz,
                                              I                         batch_count_A,
                                              int64_t                   batch_stride_A,
                                              U                         alpha_device_host,
                                              const rocsparse_mat_descr descr,
                                              const A*                  coo_val,
                                              const I*                  coo_row_ind,
                                              const I*                  coo_col_ind,
                                              const B*                  dense_B,
                                              int64_t                   ldb,
                                              I                         batch_count_B,
                                              int64_t                   batch_stride_B,
                                              rocsparse_order           order_B,
                                              U                         beta_device_host,
                                              C*                        dense_C,
                                              int64_t                   ldc,
                                              I                         batch_count_C,
                                              int64_t                   batch_stride_C,
                                              rocsparse_order           order_C,
                                              void*                     temp_buffer);
}

template <typename T, typename I, typename A, typename B, typename C, typename U>
rocsparse_status rocsparse::coomm_template_dispatch(rocsparse_handle          handle,
                                                    rocsparse_operation       trans_A,
                                                    rocsparse_operation       trans_B,
                                                    rocsparse_coomm_alg       alg,
                                                    I                         m,
                                                    I                         n,
                                                    I                         k,
                                                    int64_t                   nnz,
                                                    I                         batch_count_A,
                                                    int64_t                   batch_stride_A,
                                                    U                         alpha_device_host,
                                                    const rocsparse_mat_descr descr,
                                                    const A*                  coo_val,
                                                    const I*                  coo_row_ind,
                                                    const I*                  coo_col_ind,
                                                    const B*                  dense_B,
                                                    int64_t                   ldb,
                                                    I                         batch_count_B,
                                                    int64_t                   batch_stride_B,
                                                    rocsparse_order           order_B,
                                                    U                         beta_device_host,
                                                    C*                        dense_C,
                                                    int64_t                   ldc,
                                                    I                         batch_count_C,
                                                    int64_t                   batch_stride_C,
                                                    rocsparse_order           order_C,
                                                    void*                     temp_buffer)
{
    if(trans_A == rocsparse_operation_none)
    {
        RETURN_IF_HIPLAUNCHKERNELGGL_ERROR((rocsparse::coommnn_scale_kernel<256>),
                                           dim3((int64_t(m) * n - 1) / 256 + 1, batch_count_C),
                                           dim3(256),
                                           0,
                                           handle->stream,
                                           m,
                                           n,
                                           beta_device_host,
                                           dense_C,
                                           ldc,
                                           batch_stride_C,
                                           order_C);
    }
    else
    {
        RETURN_IF_HIPLAUNCHKERNELGGL_ERROR((rocsparse::coommnn_scale_kernel<256>),
                                           dim3((int64_t(k) * n - 1) / 256 + 1, batch_count_C),
                                           dim3(256),
                                           0,
                                           handle->stream,
                                           k,
                                           n,
                                           beta_device_host,
                                           dense_C,
                                           ldc,
                                           batch_stride_C,
                                           order_C);
    }

    switch(alg)
    {
    case rocsparse_coomm_alg_default:
    case rocsparse_coomm_alg_atomic:
    {
        RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_template_atomic<T>(handle,
                                                                      trans_A,
                                                                      trans_B,
                                                                      m,
                                                                      n,
                                                                      k,
                                                                      nnz,
                                                                      batch_count_A,
                                                                      batch_stride_A,
                                                                      alpha_device_host,
                                                                      descr,
                                                                      coo_val,
                                                                      coo_row_ind,
                                                                      coo_col_ind,
                                                                      dense_B,
                                                                      ldb,
                                                                      batch_count_B,
                                                                      batch_stride_B,
                                                                      order_B,
                                                                      beta_device_host,
                                                                      dense_C,
                                                                      ldc,
                                                                      batch_count_C,
                                                                      batch_stride_C,
                                                                      order_C));
        return rocsparse_status_success;
    }

    case rocsparse_coomm_alg_segmented:
    {
        switch(trans_A)
        {
        case rocsparse_operation_none:
        {

            RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_template_segmented<T>(handle,
                                                                             trans_A,
                                                                             trans_B,
                                                                             m,
                                                                             n,
                                                                             k,
                                                                             nnz,
                                                                             batch_count_A,
                                                                             batch_stride_A,
                                                                             alpha_device_host,
                                                                             descr,
                                                                             coo_val,
                                                                             coo_row_ind,
                                                                             coo_col_ind,
                                                                             dense_B,
                                                                             ldb,
                                                                             batch_count_B,
                                                                             batch_stride_B,
                                                                             order_B,
                                                                             beta_device_host,
                                                                             dense_C,
                                                                             ldc,
                                                                             batch_count_C,
                                                                             batch_stride_C,
                                                                             order_C,
                                                                             temp_buffer));
            return rocsparse_status_success;
        }
        case rocsparse_operation_transpose:
        case rocsparse_operation_conjugate_transpose:
        {
            RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_template_atomic<T>(handle,
                                                                          trans_A,
                                                                          trans_B,
                                                                          m,
                                                                          n,
                                                                          k,
                                                                          nnz,
                                                                          batch_count_A,
                                                                          batch_stride_A,
                                                                          alpha_device_host,
                                                                          descr,
                                                                          coo_val,
                                                                          coo_row_ind,
                                                                          coo_col_ind,
                                                                          dense_B,
                                                                          ldb,
                                                                          batch_count_B,
                                                                          batch_stride_B,
                                                                          order_B,
                                                                          beta_device_host,
                                                                          dense_C,
                                                                          ldc,
                                                                          batch_count_C,
                                                                          batch_stride_C,
                                                                          order_C));
            return rocsparse_status_success;
        }
        }
    }

    case rocsparse_coomm_alg_segmented_atomic:
    {
        switch(trans_A)
        {
        case rocsparse_operation_none:
        {
            RETURN_IF_ROCSPARSE_ERROR(
                rocsparse::coomm_template_segmented_atomic<T>(handle,
                                                              trans_A,
                                                              trans_B,
                                                              m,
                                                              n,
                                                              k,
                                                              nnz,
                                                              batch_count_A,
                                                              batch_stride_A,
                                                              alpha_device_host,
                                                              descr,
                                                              coo_val,
                                                              coo_row_ind,
                                                              coo_col_ind,
                                                              dense_B,
                                                              ldb,
                                                              batch_count_B,
                                                              batch_stride_B,
                                                              order_B,
                                                              beta_device_host,
                                                              dense_C,
                                                              ldc,
                                                              batch_count_C,
                                                              batch_stride_C,
                                                              order_C));
            return rocsparse_status_success;
        }
        case rocsparse_operation_transpose:
        case rocsparse_operation_conjugate_transpose:
        {
            RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_template_atomic<T>(handle,
                                                                          trans_A,
                                                                          trans_B,
                                                                          m,
                                                                          n,
                                                                          k,
                                                                          nnz,
                                                                          batch_count_A,
                                                                          batch_stride_A,
                                                                          alpha_device_host,
                                                                          descr,
                                                                          coo_val,
                                                                          coo_row_ind,
                                                                          coo_col_ind,
                                                                          dense_B,
                                                                          ldb,
                                                                          batch_count_B,
                                                                          batch_stride_B,
                                                                          order_B,
                                                                          beta_device_host,
                                                                          dense_C,
                                                                          ldc,
                                                                          batch_count_C,
                                                                          batch_stride_C,
                                                                          order_C));
            return rocsparse_status_success;
        }
        }
    }
    }
    RETURN_IF_ROCSPARSE_ERROR(rocsparse_status_invalid_value);
}

namespace rocsparse
{
    template <typename T, typename I, typename A, typename B, typename C>
    static rocsparse_status coomm_core(rocsparse_handle          handle,
                                       rocsparse_operation       trans_A,
                                       rocsparse_operation       trans_B,
                                       rocsparse_coomm_alg       alg,
                                       I                         m,
                                       I                         n,
                                       I                         k,
                                       int64_t                   nnz,
                                       I                         batch_count_A,
                                       int64_t                   batch_stride_A,
                                       const T*                  alpha_device_host,
                                       const rocsparse_mat_descr descr,
                                       const A*                  coo_val,
                                       const I*                  coo_row_ind,
                                       const I*                  coo_col_ind,
                                       const B*                  dense_B,
                                       int64_t                   ldb,
                                       I                         batch_count_B,
                                       int64_t                   batch_stride_B,
                                       rocsparse_order           order_B,
                                       const T*                  beta_device_host,
                                       C*                        dense_C,
                                       int64_t                   ldc,
                                       I                         batch_count_C,
                                       int64_t                   batch_stride_C,
                                       rocsparse_order           order_C,
                                       void*                     temp_buffer)
    {
        if(handle->pointer_mode == rocsparse_pointer_mode_device)
        {
            RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_template_dispatch<T>(handle,
                                                                            trans_A,
                                                                            trans_B,
                                                                            alg,
                                                                            m,
                                                                            n,
                                                                            k,
                                                                            nnz,
                                                                            batch_count_A,
                                                                            batch_stride_A,
                                                                            alpha_device_host,
                                                                            descr,
                                                                            coo_val,
                                                                            coo_row_ind,
                                                                            coo_col_ind,
                                                                            dense_B,
                                                                            ldb,
                                                                            batch_count_B,
                                                                            batch_stride_B,
                                                                            order_B,
                                                                            beta_device_host,
                                                                            dense_C,
                                                                            ldc,
                                                                            batch_count_C,
                                                                            batch_stride_C,
                                                                            order_C,
                                                                            temp_buffer));
            return rocsparse_status_success;
        }
        else
        {
            RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_template_dispatch<T>(handle,
                                                                            trans_A,
                                                                            trans_B,
                                                                            alg,
                                                                            m,
                                                                            n,
                                                                            k,
                                                                            nnz,
                                                                            batch_count_A,
                                                                            batch_stride_A,
                                                                            *alpha_device_host,
                                                                            descr,
                                                                            coo_val,
                                                                            coo_row_ind,
                                                                            coo_col_ind,
                                                                            dense_B,
                                                                            ldb,
                                                                            batch_count_B,
                                                                            batch_stride_B,
                                                                            order_B,
                                                                            *beta_device_host,
                                                                            dense_C,
                                                                            ldc,
                                                                            batch_count_C,
                                                                            batch_stride_C,
                                                                            order_C,
                                                                            temp_buffer));
            return rocsparse_status_success;
        }
    }

    template <typename T, typename I, typename A, typename B, typename C>
    static rocsparse_status coomm_quickreturn(rocsparse_handle          handle,
                                              rocsparse_operation       trans_A,
                                              rocsparse_operation       trans_B,
                                              rocsparse_coomm_alg       alg,
                                              I                         m,
                                              I                         n,
                                              I                         k,
                                              int64_t                   nnz,
                                              I                         batch_count_A,
                                              int64_t                   batch_stride_A,
                                              const T*                  alpha_device_host,
                                              const rocsparse_mat_descr descr,
                                              const A*                  coo_val,
                                              const I*                  coo_row_ind,
                                              const I*                  coo_col_ind,
                                              const B*                  dense_B,
                                              int64_t                   ldb,
                                              I                         batch_count_B,
                                              int64_t                   batch_stride_B,
                                              rocsparse_order           order_B,
                                              const T*                  beta_device_host,
                                              C*                        dense_C,
                                              int64_t                   ldc,
                                              I                         batch_count_C,
                                              int64_t                   batch_stride_C,
                                              rocsparse_order           order_C,
                                              void*                     temp_buffer)
    {
        if(m == 0 || n == 0 || k == 0)
        {

            // matrix never accessed however still need to update C matrix
            const rocsparse_int Csize = (trans_A == rocsparse_operation_none) ? m * n : k * n;
            if(Csize > 0)
            {
                if(dense_C == nullptr && beta_device_host == nullptr)
                {
                    RETURN_IF_ROCSPARSE_ERROR(rocsparse_status_invalid_pointer);
                }

                if(handle->pointer_mode == rocsparse_pointer_mode_device)
                {
                    RETURN_IF_HIPLAUNCHKERNELGGL_ERROR((rocsparse::scale_array_2d<256>),
                                                       dim3((Csize - 1) / 256 + 1, batch_count_C),
                                                       dim3(256),
                                                       0,
                                                       handle->stream,
                                                       (trans_A == rocsparse_operation_none) ? m
                                                                                             : k,
                                                       n,
                                                       ldc,
                                                       batch_stride_C,
                                                       dense_C,
                                                       beta_device_host,
                                                       order_C);
                }
                else
                {
                    RETURN_IF_HIPLAUNCHKERNELGGL_ERROR((rocsparse::scale_array_2d<256>),
                                                       dim3((Csize - 1) / 256 + 1, batch_count_C),
                                                       dim3(256),
                                                       0,
                                                       handle->stream,
                                                       (trans_A == rocsparse_operation_none) ? m
                                                                                             : k,
                                                       n,
                                                       ldc,
                                                       batch_stride_C,
                                                       dense_C,
                                                       *beta_device_host,
                                                       order_C);
                }
            }
            return rocsparse_status_success;
        }

        if(handle->pointer_mode == rocsparse_pointer_mode_host
           && ((alpha_device_host != nullptr) && (*alpha_device_host == static_cast<T>(0)))
           && ((beta_device_host != nullptr) && (*beta_device_host == static_cast<T>(1))))
        {
            return rocsparse_status_success;
        }

        return rocsparse_status_continue;
    }

    template <typename T, typename I, typename A, typename B, typename C>
    static rocsparse_status coomm_checkarg(rocsparse_handle          handle, //0
                                           rocsparse_operation       trans_A, //1
                                           rocsparse_operation       trans_B, //2
                                           rocsparse_coomm_alg       alg, //3
                                           I                         m, //4
                                           I                         n, //5
                                           I                         k, //6
                                           int64_t                   nnz, //7
                                           I                         batch_count_A, //8
                                           int64_t                   batch_stride_A, //9
                                           const T*                  alpha_device_host, //10
                                           const rocsparse_mat_descr descr, //11
                                           const A*                  coo_val, //12
                                           const I*                  coo_row_ind, //13
                                           const I*                  coo_col_ind, //14
                                           const B*                  dense_B, //15
                                           int64_t                   ldb, //16
                                           I                         batch_count_B, //17
                                           int64_t                   batch_stride_B, //18
                                           rocsparse_order           order_B, //19
                                           const T*                  beta_device_host, //20
                                           C*                        dense_C, //21
                                           int64_t                   ldc, //22
                                           I                         batch_count_C, //23
                                           int64_t                   batch_stride_C, //24
                                           rocsparse_order           order_C, //25
                                           void*                     temp_buffer) //26
    {
        ROCSPARSE_CHECKARG_HANDLE(0, handle);
        ROCSPARSE_CHECKARG_ENUM(1, trans_A);
        ROCSPARSE_CHECKARG_ENUM(2, trans_B);
        ROCSPARSE_CHECKARG_ENUM(19, order_B);
        ROCSPARSE_CHECKARG_ENUM(25, order_C);

        ROCSPARSE_CHECKARG(25, order_C, (order_C != order_B), rocsparse_status_invalid_value);

        ROCSPARSE_CHECKARG_ENUM(3, alg);
        ROCSPARSE_CHECKARG_SIZE(4, m);
        ROCSPARSE_CHECKARG_SIZE(5, n);
        ROCSPARSE_CHECKARG_SIZE(6, k);
        ROCSPARSE_CHECKARG_SIZE(7, nnz);
        ROCSPARSE_CHECKARG_POINTER(11, descr);
        ROCSPARSE_CHECKARG(11,
                           descr,
                           (descr->type != rocsparse_matrix_type_general),
                           rocsparse_status_not_implemented);

        ROCSPARSE_CHECKARG(11,
                           descr,
                           (descr->storage_mode != rocsparse_storage_mode_sorted),
                           rocsparse_status_requires_sorted_storage);

        ROCSPARSE_CHECKARG_POINTER(10, alpha_device_host);
        ROCSPARSE_CHECKARG_POINTER(20, beta_device_host);
        const rocsparse_status status = rocsparse::coomm_quickreturn<T>(handle,
                                                                        trans_A,
                                                                        trans_B,
                                                                        alg,
                                                                        m,
                                                                        n,
                                                                        k,
                                                                        nnz,
                                                                        batch_count_A,
                                                                        batch_stride_A,
                                                                        alpha_device_host,
                                                                        descr,
                                                                        coo_val,
                                                                        coo_row_ind,
                                                                        coo_col_ind,
                                                                        dense_B,
                                                                        ldb,
                                                                        batch_count_B,
                                                                        batch_stride_B,
                                                                        order_B,
                                                                        beta_device_host,
                                                                        dense_C,
                                                                        ldc,
                                                                        batch_count_C,
                                                                        batch_stride_C,
                                                                        order_C,
                                                                        temp_buffer);

        if(status != rocsparse_status_continue)
        {
            RETURN_IF_ROCSPARSE_ERROR(status);
            return rocsparse_status_success;
        }

        ROCSPARSE_CHECKARG_ARRAY(11, nnz, coo_val);
        ROCSPARSE_CHECKARG_ARRAY(13, nnz, coo_row_ind);
        ROCSPARSE_CHECKARG_ARRAY(14, nnz, coo_col_ind);

        ROCSPARSE_CHECKARG_POINTER(15, dense_B);

        ROCSPARSE_CHECKARG_POINTER(21, dense_C);

        // Check leading dimension of matrices
        static constexpr I s_one = static_cast<I>(1);
        switch(trans_A)
        {
        case rocsparse_operation_none:
        {
            ROCSPARSE_CHECKARG(
                22,
                ldc,
                (ldc < std::max(s_one, ((order_C == rocsparse_order_column) ? m : n))),
                rocsparse_status_invalid_size);

            // Check leading dimension of B
            switch(trans_B)
            {
            case rocsparse_operation_none:
            {
                ROCSPARSE_CHECKARG(
                    16,
                    ldb,
                    (ldb < std::max(s_one, ((order_B == rocsparse_order_column) ? k : n))),
                    rocsparse_status_invalid_size);
                break;
            }
            case rocsparse_operation_transpose:
            case rocsparse_operation_conjugate_transpose:
            {
                ROCSPARSE_CHECKARG(
                    16,
                    ldb,
                    (ldb < std::max(s_one, ((order_B == rocsparse_order_column) ? n : k))),
                    rocsparse_status_invalid_size);
                break;
            }
            }
            break;
        }

        case rocsparse_operation_transpose:
        case rocsparse_operation_conjugate_transpose:
        {
            ROCSPARSE_CHECKARG(
                22,
                ldc,
                (ldc < std::max(s_one, ((order_C == rocsparse_order_column) ? k : n))),
                rocsparse_status_invalid_size);

            switch(trans_B)
            {
            case rocsparse_operation_none:
            {
                ROCSPARSE_CHECKARG(
                    16,
                    ldb,
                    (ldb < std::max(s_one, ((order_B == rocsparse_order_column) ? m : n))),
                    rocsparse_status_invalid_size);
                break;
            }
            case rocsparse_operation_transpose:
            case rocsparse_operation_conjugate_transpose:
            {
                ROCSPARSE_CHECKARG(
                    16,
                    ldb,
                    (ldb < std::max(s_one, ((order_B == rocsparse_order_column) ? n : m))),
                    rocsparse_status_invalid_size);
                break;
            }
            }
            break;
        }
        }

        ROCSPARSE_CHECKARG(23,
                           batch_count_C,
                           ((batch_count_A == 1) && (batch_count_C != batch_count_B)),
                           rocsparse_status_invalid_value);

        ROCSPARSE_CHECKARG(23,
                           batch_count_C,
                           ((batch_count_B == 1) && (batch_count_C != batch_count_A)),
                           rocsparse_status_invalid_value);

        ROCSPARSE_CHECKARG(
            9,
            batch_count_A,
            (((batch_count_A > 1) && (batch_count_B > 1))
             && ((batch_count_A != batch_count_B) || (batch_count_A != batch_count_C))),
            rocsparse_status_invalid_value);

        return rocsparse_status_continue;
    }
}

template <typename T, typename I, typename A, typename B, typename C>
rocsparse_status rocsparse::coomm_template(rocsparse_handle          handle,
                                           rocsparse_operation       trans_A,
                                           rocsparse_operation       trans_B,
                                           rocsparse_coomm_alg       alg,
                                           I                         m,
                                           I                         n,
                                           I                         k,
                                           int64_t                   nnz,
                                           I                         batch_count_A,
                                           int64_t                   batch_stride_A,
                                           const T*                  alpha_device_host,
                                           const rocsparse_mat_descr descr,
                                           const A*                  coo_val,
                                           const I*                  coo_row_ind,
                                           const I*                  coo_col_ind,
                                           const B*                  dense_B,
                                           int64_t                   ldb,
                                           I                         batch_count_B,
                                           int64_t                   batch_stride_B,
                                           rocsparse_order           order_B,
                                           const T*                  beta_device_host,
                                           C*                        dense_C,
                                           int64_t                   ldc,
                                           I                         batch_count_C,
                                           int64_t                   batch_stride_C,
                                           rocsparse_order           order_C,
                                           void*                     temp_buffer)
{

    const rocsparse_status status = rocsparse::coomm_quickreturn<T>(handle,
                                                                    trans_A,
                                                                    trans_B,
                                                                    alg,
                                                                    m,
                                                                    n,
                                                                    k,
                                                                    nnz,
                                                                    batch_count_A,
                                                                    batch_stride_A,
                                                                    alpha_device_host,
                                                                    descr,
                                                                    coo_val,
                                                                    coo_row_ind,
                                                                    coo_col_ind,
                                                                    dense_B,
                                                                    ldb,
                                                                    batch_count_B,
                                                                    batch_stride_B,
                                                                    order_B,
                                                                    beta_device_host,
                                                                    dense_C,
                                                                    ldc,
                                                                    batch_count_C,
                                                                    batch_stride_C,
                                                                    order_C,
                                                                    temp_buffer);

    if(status != rocsparse_status_continue)
    {
        RETURN_IF_ROCSPARSE_ERROR(status);
        return rocsparse_status_success;
    }

    const bool Ci_A_Bi  = (batch_count_A == 1 && batch_count_B == batch_count_C);
    const bool Ci_Ai_B  = (batch_count_B == 1 && batch_count_A == batch_count_C);
    const bool Ci_Ai_Bi = (batch_count_A == batch_count_C && batch_count_A == batch_count_B);

    if(!Ci_A_Bi && !Ci_Ai_B && !Ci_Ai_Bi)
    {
        RETURN_IF_ROCSPARSE_ERROR(rocsparse_status_invalid_value);
    }

    RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_core<T>(handle,
                                                       trans_A,
                                                       trans_B,
                                                       alg,
                                                       m,
                                                       n,
                                                       k,
                                                       nnz,
                                                       batch_count_A,
                                                       batch_stride_A,
                                                       alpha_device_host,
                                                       descr,
                                                       coo_val,
                                                       coo_row_ind,
                                                       coo_col_ind,
                                                       dense_B,
                                                       ldb,
                                                       batch_count_B,
                                                       batch_stride_B,
                                                       order_B,
                                                       beta_device_host,
                                                       dense_C,
                                                       ldc,
                                                       batch_count_C,
                                                       batch_stride_C,
                                                       order_C,
                                                       temp_buffer));
    return rocsparse_status_success;
}

template <typename T, typename I, typename A, typename B, typename C>
rocsparse_status coomm_impl(rocsparse_handle          handle,
                            rocsparse_operation       trans_A,
                            rocsparse_operation       trans_B,
                            rocsparse_coomm_alg       alg,
                            I                         m,
                            I                         n,
                            I                         k,
                            int64_t                   nnz,
                            I                         batch_count_A,
                            int64_t                   batch_stride_A,
                            const T*                  alpha_device_host,
                            const rocsparse_mat_descr descr,
                            const A*                  coo_val,
                            const I*                  coo_row_ind,
                            const I*                  coo_col_ind,
                            const B*                  dense_B,
                            int64_t                   ldb,
                            I                         batch_count_B,
                            int64_t                   batch_stride_B,
                            rocsparse_order           order_B,
                            const T*                  beta_device_host,
                            C*                        dense_C,
                            int64_t                   ldc,
                            I                         batch_count_C,
                            int64_t                   batch_stride_C,
                            rocsparse_order           order_C,
                            void*                     temp_buffer)
{

    rocsparse::log_trace(handle,
                         rocsparse::replaceX<T>("rocsparse_Xcoomm"),
                         trans_A,
                         trans_B,
                         alg,
                         m,
                         n,
                         k,
                         nnz,
                         batch_count_A,
                         batch_stride_A,
                         LOG_TRACE_SCALAR_VALUE(handle, alpha_device_host),
                         (const void*&)descr,
                         (const void*&)coo_val,
                         (const void*&)coo_row_ind,
                         (const void*&)coo_col_ind,
                         (const void*&)dense_B,
                         ldb,
                         batch_count_B,
                         batch_stride_B,
                         order_B,
                         LOG_TRACE_SCALAR_VALUE(handle, beta_device_host),
                         (const void*&)dense_C,
                         ldc,
                         batch_count_C,
                         batch_stride_C,
                         order_C,
                         temp_buffer);

    const rocsparse_status status = rocsparse::coomm_checkarg<T>(handle,
                                                                 trans_A,
                                                                 trans_B,
                                                                 alg,
                                                                 m,
                                                                 n,
                                                                 k,
                                                                 nnz,
                                                                 batch_count_A,
                                                                 batch_stride_A,
                                                                 alpha_device_host,
                                                                 descr,
                                                                 coo_val,
                                                                 coo_row_ind,
                                                                 coo_col_ind,
                                                                 dense_B,
                                                                 ldb,
                                                                 batch_count_B,
                                                                 batch_stride_B,
                                                                 order_B,
                                                                 beta_device_host,
                                                                 dense_C,
                                                                 ldc,
                                                                 batch_count_C,
                                                                 batch_stride_C,
                                                                 order_C,
                                                                 temp_buffer);

    if(status != rocsparse_status_continue)
    {
        RETURN_IF_ROCSPARSE_ERROR(status);
        return rocsparse_status_success;
    }

    RETURN_IF_ROCSPARSE_ERROR(rocsparse::coomm_core<T>(handle,
                                                       trans_A,
                                                       trans_B,
                                                       alg,
                                                       m,
                                                       n,
                                                       k,
                                                       nnz,
                                                       batch_count_A,
                                                       batch_stride_A,
                                                       alpha_device_host,
                                                       descr,
                                                       coo_val,
                                                       coo_row_ind,
                                                       coo_col_ind,
                                                       dense_B,
                                                       ldb,
                                                       batch_count_B,
                                                       batch_stride_B,
                                                       order_B,
                                                       beta_device_host,
                                                       dense_C,
                                                       ldc,
                                                       batch_count_C,
                                                       batch_stride_C,
                                                       order_C,
                                                       temp_buffer));
    return rocsparse_status_success;
}

#define INSTANTIATE(TTYPE, ITYPE, ATYPE, BTYPE, CTYPE)                                               \
    template rocsparse_status rocsparse::coomm_template(rocsparse_handle          handle,            \
                                                        rocsparse_operation       trans_A,           \
                                                        rocsparse_operation       trans_B,           \
                                                        rocsparse_coomm_alg       alg,               \
                                                        ITYPE                     m,                 \
                                                        ITYPE                     n,                 \
                                                        ITYPE                     k,                 \
                                                        int64_t                   nnz,               \
                                                        ITYPE                     batch_count_A,     \
                                                        int64_t                   batch_stride_A,    \
                                                        const TTYPE*              alpha_device_host, \
                                                        const rocsparse_mat_descr descr,             \
                                                        const ATYPE*              coo_val,           \
                                                        const ITYPE*              coo_row_ind,       \
                                                        const ITYPE*              coo_col_ind,       \
                                                        const BTYPE*              B,                 \
                                                        int64_t                   ldb,               \
                                                        ITYPE                     batch_count_B,     \
                                                        int64_t                   batch_stride_B,    \
                                                        rocsparse_order           order_B,           \
                                                        const TTYPE*              beta_device_host,  \
                                                        CTYPE*                    C,                 \
                                                        int64_t                   ldc,               \
                                                        ITYPE                     batch_count_C,     \
                                                        int64_t                   batch_stride_C,    \
                                                        rocsparse_order           order_C,           \
                                                        void*                     temp_buffer);

// Uniform precisions
INSTANTIATE(float, int32_t, float, float, float);
INSTANTIATE(float, int64_t, float, float, float);
INSTANTIATE(double, int32_t, double, double, double);
INSTANTIATE(double, int64_t, double, double, double);
INSTANTIATE(rocsparse_float_complex,
            int32_t,
            rocsparse_float_complex,
            rocsparse_float_complex,
            rocsparse_float_complex);
INSTANTIATE(rocsparse_float_complex,
            int64_t,
            rocsparse_float_complex,
            rocsparse_float_complex,
            rocsparse_float_complex);
INSTANTIATE(rocsparse_double_complex,
            int32_t,
            rocsparse_double_complex,
            rocsparse_double_complex,
            rocsparse_double_complex);
INSTANTIATE(rocsparse_double_complex,
            int64_t,
            rocsparse_double_complex,
            rocsparse_double_complex,
            rocsparse_double_complex);

// Mixed precisions
INSTANTIATE(int32_t, int32_t, int8_t, int8_t, int32_t);
INSTANTIATE(int32_t, int64_t, int8_t, int8_t, int32_t);
INSTANTIATE(float, int32_t, int8_t, int8_t, float);
INSTANTIATE(float, int64_t, int8_t, int8_t, float);
#undef INSTANTIATE

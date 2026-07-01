// SPDX-License-Identifier: GPL-2.0
// =============================================================================
//  vb2compat -- 为 LS2K0300 板子内核补齐 FRAME_VECTOR 导出符号
//
//  板子内核(4.19.190+)未开启 CONFIG_FRAME_VECTOR, 因此缺少
//  get_vaddr_frames/put_vaddr_frames/frame_vector_create/destroy/to_pages/to_pfns
//  这些符号, 导致 videobuf2-memops/vmalloc 无法加载(Unknown symbol)。
//  本模块把内核源码 mm/frame_vector.c 一起编入并导出这些符号(EXPORT_SYMBOL),
//  加载后即可满足 videobuf2 的依赖。必须【最先】加载。
//
//  实际的函数与 EXPORT_SYMBOL 来自同目录下随构建拷入的 frame_vector.c。
// =============================================================================
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PatrolSystem");
MODULE_DESCRIPTION("frame_vector export shim for uvcvideo on LS2K0300 (kernel lacks CONFIG_FRAME_VECTOR)");
MODULE_VERSION("1.0");

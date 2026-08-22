#pragma once

#include "su/result.hpp"

#include <string>

namespace su {
namespace parse {

// 判断给定 PSF 文件是否为文本格式（PSFASCII）。
// 二进制 BINPSF 头部含 NUL 字节，PSFASCII 为纯文本（HEADER/"PSFversion" 开头）。
bool is_psf_ascii_file(const std::string& path);

// Spectre PSFASCII 结果读取（当前仅面向 Spectre 输出格式）。
// 与 libpsf backend 保持同一公开签名与失败语义。
ReadResult<ScalarResult> read_dc_value_with_ascii(const std::string& result_dir,
                                                  const std::string& signal_name);

ReadResult<DcSweep> read_dc_sweep_with_ascii(const std::string& result_dir,
                                             const std::string& sweep_name,
                                             const std::string& signal_name,
                                             const std::string& filename);

ReadResult<AcResponse> read_ac_response_with_ascii(const std::string& result_dir,
                                                   const std::string& signal_name,
                                                   const std::string& filename);

ReadResult<TranWaveform> read_tran_waveform_with_ascii(const std::string& result_dir,
                                                       const std::string& signal_name,
                                                       const std::string& filename);

}  // namespace parse
}  // namespace su

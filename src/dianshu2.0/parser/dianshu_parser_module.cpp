#include "dianshu_parser_module.h"
#include "dianshu_parser_u.h"
#include "sgx_urts.h"
#include "ypc/corecommon/package.h"
#include <stdexcept>

dianshu_parser_module::dianshu_parser_module(const char *mod_path)
    : ::stbox::sgx_module(mod_path) {}

uint32_t dianshu_parser_module::begin_parse_data_item() {
  auto retval = ecall<uint32_t>(::begin_parse_data_item);
  return retval;
}

uint32_t dianshu_parser_module::parse_data_item(const uint8_t *data,
                                                size_t len) {
  auto retval = ecall<uint32_t>(::parse_data_item, data, (len));
  return retval;
}

uint32_t dianshu_parser_module::end_parse_data_item() {
  auto retval = ecall<uint32_t>(::end_parse_data_item);
  return retval;
}

uint32_t dianshu_parser_module::get_enclave_hash(ypc::bytes &_enclave_hash) {
  uint32_t hash_size;
  uint8_t *enclave_hash;
  stbox::buffer_length_t buf_res(&hash_size, &enclave_hash,
                                 ::get_enclave_hash_size);
  auto t = ecall<uint32_t>(::get_enclave_hash, stbox::xmem(buf_res),
                           stbox::xlen(buf_res));

  _enclave_hash = ypc::bytes(enclave_hash, hash_size);
  return t;
}

uint32_t dianshu_parser_module::get_analyze_result(ypc::bytes &result) {
  uint32_t res_size;
  uint8_t *res;
  stbox::buffer_length_t buf_res(&res_size, &res, ::get_analyze_result_size);
  auto t = ecall<uint32_t>(::get_analyze_result, stbox::xmem(buf_res),
                           stbox::xlen(buf_res));

  if (t == stbox::stx_status::success) {
    result = ypc::bytes(res, res_size);
  }
  return t;
}
uint32_t dianshu_parser_module::init_data_source(const ypc::bytes &info) {
  return ecall<uint32_t>(::init_data_source, info.data(), info.size());
}
uint32_t dianshu_parser_module::init_model(const ypc::bytes &info) {
  LOG(INFO) << "dianshu_parser_module::init_model";
  return ecall<uint32_t>(::init_model, info.data(), info.size());
}
uint32_t dianshu_parser_module::get_parser_type() {
  return ecall<uint32_t>(::get_parser_type);
}
